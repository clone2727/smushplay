/* smushplay - A simple LucasArts SMUSH video player
 *
 * smushplay is the legal property of its developers, whose names can be
 * found in the AUTHORS file distributed with this source
 * distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

// Based on ScummVM Stream classes (GPLv2+)

#include <assert.h>
#include <stdio.h>
#include <string>
#include <zlib.h>
#include "stream.h"

#if ZLIB_VERNUM < 0x1204
#error Version 1.2.0.4 or newer of zlib is required for this code
#endif

uint32 MemoryReadStream::read(void *dataPtr, uint32 dataSize) {
	// Read at most as many bytes as are still available...
	if (dataSize > _size - _pos) {
		dataSize = _size - _pos;
		_eos = true;
	}
	memcpy(dataPtr, _ptr, dataSize);

	_ptr += dataSize;
	_pos += dataSize;

	return dataSize;
}

bool MemoryReadStream::seek(int32 offs, int whence) {
	// Pre-Condition
	assert(_pos <= _size);
	switch (whence) {
	case SEEK_END:
		// SEEK_END works just like SEEK_SET, only 'reversed',
		// i.e. from the end.
		offs = _size + offs;
		// Fall through
	case SEEK_SET:
		_ptr = _ptrOrig + offs;
		_pos = offs;
		break;

	case SEEK_CUR:
		_ptr += offs;
		_pos += offs;
		break;
	}
	// Post-Condition
	assert(_pos <= _size);

	// Reset end-of-stream flag on a successful seek
	_eos = false;
	return true;	// FIXME: STREAM REWRITE
}

class StdioStream : public SeekableReadStream {
public:
	StdioStream() {}
	StdioStream(FILE *handle);
	~StdioStream();

	bool err() const;
	void clearErr();
	bool eos() const;

	uint32 write(const void *dataPtr, uint32 dataSize);
	bool flush();

	int32 pos() const;
	int32 size() const;
	bool seek(int32 offs, int whence = SEEK_SET);
	uint32 read(void *dataPtr, uint32 dataSize);

private:
	// Prevent copying instances by accident
	StdioStream(const StdioStream &);
	StdioStream &operator=(const StdioStream &);

	/** File handle to the actual file. */
	FILE *_handle;
};

StdioStream::StdioStream(FILE *handle) : _handle(handle) {
	assert(handle);
}

StdioStream::~StdioStream() {
	fclose(_handle);
}

bool StdioStream::err() const {
	return ferror(_handle) != 0;
}

void StdioStream::clearErr() {
	clearerr(_handle);
}

bool StdioStream::eos() const {
	return feof(_handle) != 0;
}

int32 StdioStream::pos() const {
	return ftell(_handle);
}

int32 StdioStream::size() const {
	int32 oldPos = ftell(_handle);
	fseek(_handle, 0, SEEK_END);
	int32 length = ftell(_handle);
	fseek(_handle, oldPos, SEEK_SET);

	return length;
}

bool StdioStream::seek(int32 offs, int whence) {
	return fseek(_handle, offs, whence) == 0;
}

uint32 StdioStream::read(void *ptr, uint32 len) {
	return fread(ptr, 1, len, _handle);
}

uint32 StdioStream::write(const void *ptr, uint32 len) {
	return fwrite(ptr, 1, len, _handle);
}

bool StdioStream::flush() {
	return fflush(_handle) == 0;
}

SeekableReadStream *createReadStream(const char *pathName) {
	FILE *file = fopen(pathName, "rb");

	if (!file)
		return 0;

	return new StdioStream(file);
}


/**
 * A simple wrapper class which can be used to wrap around an arbitrary
 * other SeekableReadStream and will then provide on-the-fly decompression support.
 * Assumes the compressed data to be in gzip format.
 */
class GZipReadStream : public SeekableReadStream {
protected:
	enum {
		BUFSIZE = 16384		// 1 << MAX_WBITS
	};

	byte	_buf[BUFSIZE];

	SeekableReadStream *_wrapped;
	z_stream _stream;
	int _zlibErr;
	uint32 _pos;
	uint32 _origSize;
	bool _eos;

public:

	GZipReadStream(SeekableReadStream *w) : _wrapped(w), _stream() {
		assert(w != 0);

		// Verify file header is correct
		w->seek(0, SEEK_SET);
		uint16 header = w->readUint16BE();
		assert(header == 0x1F8B ||
		       ((header & 0x0F00) == 0x0800 && header % 31 == 0));

		if (header == 0x1F8B) {
			// Retrieve the original file size
			w->seek(-4, SEEK_END);
			_origSize = w->readUint32LE();
		} else {
			// Original size not available in zlib format
			_origSize = 0;
		}
		_pos = 0;
		w->seek(0, SEEK_SET);
		_eos = false;

		// Adding 32 to windowBits indicates to zlib that it is supposed to
		// automatically detect whether gzip or zlib headers are used for
		// the compressed file. This feature was added in zlib 1.2.0.4,
		// released 10 August 2003.
		// Note: This is *crucial* for savegame compatibility, do *not* remove!
		_zlibErr = inflateInit2(&_stream, MAX_WBITS + 32);
		if (_zlibErr != Z_OK)
			return;

		// Setup input buffer
		_stream.next_in = _buf;
		_stream.avail_in = 0;
	}

	~GZipReadStream() {
		inflateEnd(&_stream);
		delete _wrapped;
	}

	bool err() const { return (_zlibErr != Z_OK) && (_zlibErr != Z_STREAM_END); }
	void clearErr() {
		// only reset _eos; I/O errors are not recoverable
		_eos = false;
	}

	uint32 read(void *dataPtr, uint32 dataSize) {
		_stream.next_out = (byte *)dataPtr;
		_stream.avail_out = dataSize;

		// Keep going while we get no error
		while (_zlibErr == Z_OK && _stream.avail_out) {
			if (_stream.avail_in == 0 && !_wrapped->eos()) {
				// If we are out of input data: Read more data, if available.
				_stream.next_in = _buf;
				_stream.avail_in = _wrapped->read(_buf, BUFSIZE);
			}
			_zlibErr = inflate(&_stream, Z_NO_FLUSH);
		}

		// Update the position counter
		_pos += dataSize - _stream.avail_out;

		if (_zlibErr == Z_STREAM_END && _stream.avail_out > 0)
			_eos = true;

		return dataSize - _stream.avail_out;
	}

	bool eos() const {
		return _eos;
	}
	int32 pos() const {
		return _pos;
	}
	int32 size() const {
		return _origSize;
	}
	bool seek(int32 offset, int whence = SEEK_SET) {
		int32 newPos = 0;
		assert(whence != SEEK_END);	// SEEK_END not supported
		switch (whence) {
		case SEEK_SET:
			newPos = offset;
			break;
		case SEEK_CUR:
			newPos = _pos + offset;
		}

		assert(newPos >= 0);

		if ((uint32)newPos < _pos) {
			// To search backward, we have to restart the whole decompression
			// from the start of the file. A rather wasteful operation, best
			// to avoid it. :/
			fprintf(stderr, "Backward seeking in GZipReadStream detected\n");

			_pos = 0;
			_wrapped->seek(0, SEEK_SET);
			_zlibErr = inflateReset(&_stream);
			if (_zlibErr != Z_OK)
				return false;	// FIXME: STREAM REWRITE
			_stream.next_in = _buf;
			_stream.avail_in = 0;
		}

		offset = newPos - _pos;

		// Skip the given amount of data (very inefficient if one tries to skip
		// huge amounts of data, but usually client code will only skip a few
		// bytes, so this should be fine.
		byte tmpBuf[1024];
		while (!err() && offset > 0) {
			offset -= read(tmpBuf, MIN((int32)sizeof(tmpBuf), offset));
		}

		_eos = false;
		return true;	// FIXME: STREAM REWRITE
	}
};

SeekableReadStream *wrapCompressedReadStream(SeekableReadStream *toBeWrapped) {
	if (toBeWrapped) {
		uint16 header = toBeWrapped->readUint16BE();
		bool isCompressed = (header == 0x1F8B ||
				     ((header & 0x0F00) == 0x0800 &&
				      header % 31 == 0));
		toBeWrapped->seek(-2, SEEK_CUR);
		if (isCompressed)
			return new GZipReadStream(toBeWrapped);
	}

	return toBeWrapped;
}
