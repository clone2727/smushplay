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

// Based on the ScummVM and ResidualVM SMUSH code (GPLv2+ and LGPL v2.1,
// respectively).

#include <SDL.h>
#include <SDL_endian.h>
#include "audioman.h"
#include "audiostream.h"
#include "blocky16.h"
#include "codec37.h"
#include "codec47.h"
#include "fileutil.h"
#include "pcm.h"
#include "smushvideo.h"
#include "util.h"
#include "vima.h"

// OVERALL STATUS:
// Basic parsing achieved
// Basic video playback achieved
// Basic audio playback achieved
// Need ANIM v1 frame rate detection
// A/V Sync could be improved

// ANIM:
// Rebel Assault: Decodes a few videos, missing several codecs, missing ghost support, missing negative coordinate handling
// Rebel Assault II: Decodes most videos, missing at least one codec
// The Dig/Full Throttle/CMI/Shadows of the Empire/Grim Demo/Outlaws: Decodes all videos
// Mysteries of the Sith: Missing codec 48
// Mortimer: Some videos work, but looks like it scales up low-res frames; missing codec 23
// IACT audio (CMI/SotE/Grim Demo/Outlaws/MotS) works

// SANM:
// X-Wing Alliance/Grim Fandango/Racer: Should playback video fine
// (May need to be gunzip'd first)
// Infernal Machine: Untested
// VIMA audio works


SMUSHVideo::SMUSHVideo(AudioManager &audio) : _audio(&audio) {
	_file = 0;
	_buffer = _storedFrame = 0;
	_storeFrame = false;
	_codec37 = 0;
	_codec47 = 0;
	_blocky16 = 0;
	_runSoundHeaderCheck = false;
	_audioChannels = 0;
	_width = _height = 0;
	_iactStream = 0;
	_iactBuffer = 0;
	_vimaDestTable = 0;
	_frameRate = 0;
	_audioRate = 0;
}

SMUSHVideo::~SMUSHVideo() {
	close();
}

bool SMUSHVideo::load(const char *fileName) {
	_file = fopen(fileName, "rb");

	if (!_file)
		return false;

	_mainTag = readUint32BE(_file);
	if (_mainTag != MKTAG('A', 'N', 'I', 'M') && _mainTag != MKTAG('S', 'A', 'N', 'M')) {
		fprintf(stderr, "Not a valid SMUSH FourCC\n");
		close();
		return false;
	}

	readUint32BE(_file); // file size

	if (!readHeader()) {
		fprintf(stderr, "Problem while reading SMUSH header\n");
		close();
		return false;
	}

	printf("'%s' Details:\n", fileName);
	printf("\tSMUSH Tag: '%c%c%c%c'\n", LISTTAG(_mainTag));
	printf("\tFrame Count: %d\n", _frameCount);
	printf("\tWidth: %d\n", _width);
	printf("\tHeight: %d\n", _height);
	if (_mainTag == MKTAG('A', 'N', 'I', 'M')) {
		printf("\tVersion: %d\n", _version);
		if (_version == 2) {
			printf("\tFrame Rate: %d\n", _frameRate);
			printf("\tAudio Rate: %dHz\n", _audioRate);
		}
	} else {
		printf("\tFrame Rate: %d\n", (int)(1000000.0f / _frameRate + 0.5f)); // approximate
		if (_audioRate != 0) {
			printf("\tAudio Rate: %dHz\n", _audioRate);
			printf("\tAudio Channels: %d\n", _audioChannels);
		}
	}

	return true;
}

void SMUSHVideo::close() {
	_audio->stopAll();

	if (_file) {
		fclose(_file);
		_file = 0;

		delete[] _buffer;
		_buffer = 0;

		delete[] _storedFrame;
		_storedFrame = 0;

		delete _codec37;
		_codec37 = 0;

		delete _codec47;
		_codec47 = 0;

		delete _blocky16;
		_blocky16 = 0;

		_iactStream = 0;

		delete[] _iactBuffer;
		_iactBuffer = 0;

		delete[] _vimaDestTable;
		_vimaDestTable = 0;

		_runSoundHeaderCheck = false;
		_storeFrame = false;
		_audioChannels = 0;
		_width = _height = 0;
		_frameRate = 0;
		_audioRate = 0;
	}
}

bool SMUSHVideo::isHighColor() const {
	return _mainTag == MKTAG('S', 'A', 'N', 'M');
}

uint SMUSHVideo::getWidth() const {
	return _width;
}

uint SMUSHVideo::getHeight() const {
	return _height;
}

uint32 SMUSHVideo::getNextFrameTime(uint32 curFrame) const {
	// SANM stores the frame rate as time between frames
	if (_mainTag == MKTAG('S', 'A', 'N', 'M'))
		return curFrame * _frameRate / 1000;

	// Otherwise, in terms of frames per second
	return curFrame * 1000 / _frameRate;
}

void SMUSHVideo::play(GraphicsManager &gfx) {
	if (!isLoaded())
		return;

	// Set the palette from the header for 8bpp videos
	if (!isHighColor())
		gfx.setPalette(_palette, 0, 256);

	uint32 startTime = SDL_GetTicks();
	uint curFrame = 0;

	while (curFrame < _frameCount) {
		if (SDL_GetTicks() > startTime + getNextFrameTime(curFrame)) {
			if (!handleFrame(gfx)) {
				fprintf(stderr, "Problem during frame decode\n");
				return;
			}

			gfx.update();
			curFrame++;
		}

		SDL_Event event;
		while (SDL_PollEvent(&event))
			if (event.type == SDL_QUIT)
				return;

		SDL_Delay(10);
	}

	printf("Done!\n");
}

bool SMUSHVideo::readHeader() {
	uint32 tag = readUint32BE(_file);
	uint32 size = readUint32BE(_file);
	uint32 pos = ftell(_file);

	if (tag == MKTAG('A', 'H', 'D', 'R')) {
		if (size < 0x306)
			return false;

		_version = readUint16LE(_file);
		_frameCount = readUint16LE(_file);
		readUint16LE(_file); // unknown

		fread(_palette, 1, 256 * 3, _file);

		if (_version == 2) {
			// This seems to be the only difference between v1 and v2
			if (size < 0x312) {
				fprintf(stderr, "ANIM v2 without extended header\n");
				return false;
			}

			_frameRate = readUint32LE(_file);
			readUint32LE(_file);
			_audioRate = readUint32LE(_file); // This isn't right for CMI? O_o -- Also doesn't guarantee audio
			_audioChannels = 1; // FIXME: Is this right?
		} else {
			// TODO: Figure out proper values
			_frameRate = 15;
			_audioRate = 11025;
			_audioChannels = 1;
		}

		fseek(_file, pos + size + (size & 1), SEEK_SET);
		return detectFrameSize();
	} else if (tag == MKTAG('S', 'H', 'D', 'R')) {
		readUint16LE(_file);
		_frameCount = readUint32LE(_file);
		readUint16LE(_file);
		_width = readUint16LE(_file);
		_pitch = _width * 2;
		_height = readUint16LE(_file);
		readUint16LE(_file);
		_frameRate = readUint32LE(_file);
		/* _flags = */ readUint16LE(_file);
		fseek(_file, pos + size + (size & 1), SEEK_SET);
		return readFrameHeader();
	}

	fprintf(stderr, "Unknown SMUSH header type '%c%c%c%c'\n", LISTTAG(tag));
	return false;
}

bool SMUSHVideo::handleFrame(GraphicsManager &gfx) {
	uint32 tag = readUint32BE(_file);
	uint32 size = readUint32BE(_file);
	uint32 pos = ftell(_file);

	if (tag == MKTAG('A', 'N', 'N', 'O')) {
		// Skip over any ANNO tag
		// (SANM only)
		fseek(_file, pos + size + (size & 1), SEEK_SET);
		tag = readUint32BE(_file);
		size = readUint32BE(_file);
		pos = ftell(_file);
	}

	// Now we have to be at FRME
	if (tag != MKTAG('F', 'R', 'M', 'E'))
		return false;

	uint32 bytesLeft = size;
	while (bytesLeft > 0) {
		uint32 subType = readUint32BE(_file);
		uint32 subSize = readUint32BE(_file);
		uint32 subPos = ftell(_file);

		if (feof(_file)) {
			// HACK: L2PLAY.ANM from Rebel Assault seems to have an unaligned FOBJ :/
			fprintf(stderr, "Unexpected end of file!\n");
			return false;
		}

		bool result = true;

		switch (subType) {
		case MKTAG('B', 'l', '1', '6'):
			result = handleBlocky16(gfx, subSize);
			break;
		case MKTAG('F', 'A', 'D', 'E'):
			// TODO: Seems to not be needed as XPAL is used in v1 instead?
			break;
		case MKTAG('F', 'O', 'B', 'J'):
			result = handleFrameObject(gfx, subSize);
			break;
		case MKTAG('F', 'T', 'C', 'H'):
			result = handleFetch(subSize);
			break;
		case MKTAG('G', 'A', 'M', 'E'):
			// TODO: SMUSH v1 interaction (?)
			break;
		case MKTAG('G', 'A', 'M', '2'):
			// TODO: SMUSH v1 interaction (?)
			break;
		case MKTAG('G', 'O', 'S', 'T'):
			result = handleGhost(subSize);
			break;
		case MKTAG('I', 'A', 'C', 'T'):
			result = handleIACT(subSize);
			break;
		case MKTAG('L', 'O', 'A', 'D'):
			// TODO: Unknown, found in RA2's 06PLAY1.SAN
			break;
		case MKTAG('N', 'P', 'A', 'L'):
			result = handleNewPalette(gfx, subSize);
			break;
		case MKTAG('P', 'S', 'A', 'D'):
		case MKTAG('P', 'S', 'D', '2'):
		case MKTAG('P', 'V', 'O', 'C'):
			result = handleSoundFrame(subSize);
			break;
		case MKTAG('S', 'K', 'I', 'P'):
			// INSANE related
			break;
		case MKTAG('S', 'T', 'O', 'R'):
			result = handleStore(size);
			break;
		case MKTAG('T', 'E', 'X', 'T'):
		case MKTAG('T', 'R', 'E', 'S'):
			// TODO: Text Resource
			break;
		case MKTAG('W', 'a', 'v', 'e'):
			result = handleVIMA(size);
			break;
		case MKTAG('X', 'P', 'A', 'L'):
			result = handleDeltaPalette(gfx, subSize);
			break;
		case MKTAG('Z', 'F', 'O', 'B'):
			// TODO: Zipped Frame Object (ScummVM-compressed)
			fprintf(stderr, "ScummVM-compressed videos not supported\n");
			return false;
		default:
			// TODO: Other types
			printf("\tSub Type: '%c%c%c%c'\n", LISTTAG(subType));
		}

		if (!result)
			return false;

		bytesLeft -= subSize + 8 + (subSize & 1);
		fseek(_file, subPos + subSize + (subSize & 1), SEEK_SET);
	}

	fseek(_file, pos + size + (size & 1), SEEK_SET);
	return true;
}

bool SMUSHVideo::handleNewPalette(GraphicsManager &gfx, uint32 size) {
	// Load a new palette

	if (size < 256 * 3) {
		fprintf(stderr, "Bad NPAL chunk\n");
		return false;
	}

	fread(_palette, 1, 256 * 3, _file);
	gfx.setPalette(_palette, 0, 256);
	return true;
}

static byte deltaColor(byte pal, int16 delta) {
	int t = (pal * 129 + delta) / 128;
	if (t < 0)
		t = 0;
	else if (t > 255)
		t = 255;
	return t;
}

bool SMUSHVideo::handleDeltaPalette(GraphicsManager &gfx, uint32 size) {
	// Decode a delta palette

	if (size == 256 * 3 * 3 + 4) {
		fseek(_file, 4, SEEK_CUR);

		for (uint16 i = 0; i < 256 * 3; i++)
			_deltaPalette[i] = readUint16LE(_file);

		fread(_palette, 1, 256 * 3, _file);
		gfx.setPalette(_palette, 0, 256);
		return true;
	} else if (size == 6 || size == 4) {
		for (uint16 i = 0; i < 256 * 3; i++)
			_palette[i] = deltaColor(_palette[i], _deltaPalette[i]);

		gfx.setPalette(_palette, 0, 256);
		return true;
	} else if (size == 256 * 3 * 2 + 4) {
		// SMUSH v1 only
		fseek(_file, 4, SEEK_CUR);

		for (uint16 i = 0; i < 256 * 3; i++)
			_deltaPalette[i] = readUint16LE(_file);
		return true;
	}

	fprintf(stderr, "Bad XPAL chunk (%d)\n", size);
	return false;
}

bool SMUSHVideo::handleFrameObject(GraphicsManager &gfx, uint32 size) {
	// Decode a frame object

	if (isHighColor()) {
		fprintf(stderr, "Frame object chunk in 16bpp video\n");
		return false;
	}

	if (size < 14)
		return false;

	byte codec = readByte(_file);
	/* byte codecParam = */ readByte(_file);
	int16 left = readSint16LE(_file);
	int16 top = readSint16LE(_file);
	uint16 width = readUint16LE(_file);
	uint16 height = readUint16LE(_file);
	readUint16LE(_file);
	readUint16LE(_file);

	size -= 14;
	
	if (codec == 37 || codec == 47 || codec == 48) {
		// We ignore left/top for these codecs
		if (width != _width || height != _height) {
			// FIXME: The Dig's SQ1.SAN also has extra large frames (seem broken)
			fprintf(stderr, "Modified codec %d coordinates %d, %d\n", codec, width, height);
			return true;
		}
	} else if (left < 0 || top < 0 || left + width > (int)_width || top + height > (int)_height) {
		// TODO: We should be drawing partial frames
		fprintf(stderr, "Bad codec %d coordinates %d, %d, %d, %d\n", codec, left, top, width, height);
		return true;
	}

	switch (codec) {
	case 1:
	case 3:
		decodeCodec1(left, top, width, height);
		break;
	case 21:
	//case 44:
		decodeCodec21(left, top, width, height);
		break;
	case 23:
		// TODO: Used by Rebel Assault, Rebel Assault II, and Mortimer
		// Used for the blue transparent overlays
		printf("Unhandled codec 23 frame object\n");
		break;
	case 37: {
		byte *ptr = new byte[size];
		fread(ptr, 1, size, _file);

		if (!_codec37)
			_codec37 = new Codec37Decoder(width, height);

		_codec37->decode(_buffer, ptr);
		delete[] ptr;
		} break;
	case 45:
		// TODO: Used by RA2's 14PLAY.SAN
		printf("Unhandled codec 45 frame object\n");
		break;
	case 47: {
		// The original "blocky" codec
		byte *ptr = new byte[size];
		fread(ptr, 1, size, _file);

		if (!_codec47)
			_codec47 = new Codec47Decoder(width, height);

		_codec47->decode(_buffer, ptr);
		delete[] ptr;
		} break;
	case 48:
		// TODO: Used by Mysteries of the Sith
		// Seems similar to codec 47
		printf("Unhandled codec 48 frame object\n");
		break;
	default:
		// TODO: Lots of other Rebel Assault ones
		// They look like a terrible compression
		fprintf(stderr, "Unknown codec %d\n", codec);
		//return false;
		break;
	}

	if (_storeFrame) {
		if (!_storedFrame)
			_storedFrame = new byte[_pitch * _height];

		memcpy(_storedFrame, _buffer, _pitch * _height);
		_storeFrame = false;
	}

	// Ideally, this call should be at the end of the FRME block, but it
	// seems that breaks things like the video in Rebel Assault of Cmdr.
	// Farrell coming in to save you.
	gfx.blit(_buffer, 0, 0, _width, _height, _pitch);
	return true;
}

bool SMUSHVideo::handleStore(uint32 size) {
	// Store the next frame object
	_storeFrame = true;
	return size >= 4;
}

bool SMUSHVideo::handleFetch(uint32 size) {
	// Restore an old frame object

	if (_storedFrame && _buffer)
		memcpy(_buffer, _storedFrame, _pitch * _height);

	return size >= 6;
}

void SMUSHVideo::decodeCodec1(int left, int top, uint width, uint height) {
	// This is very similar to the bomp compression
	for (uint y = 0; y < height; y++) {
		uint16 lineSize = readUint16LE(_file);
		byte *dst = _buffer + (top + y) * _pitch + left;

		while (lineSize > 0) {
			byte code = readByte(_file);
			lineSize--;
			byte length = (code >> 1) + 1;

			if (code & 1) {
				byte val = readByte(_file);
				lineSize--;

				if (val != 0)
					memset(dst, val, length);

				dst += length;
			} else {
				lineSize -= length;

				while (length--) {
					byte val = readByte(_file);

					if (val)
						*dst = val;

					dst++;
				}
			}
		}
	}
}

bool SMUSHVideo::handleSoundFrame(uint32 size) {
	// Old PSAD-based sound
	// As used by Rebel Assault, Rebel Assault II, and Full Throttle
	// Rebel Assault I/II are 11025Hz
	// ScummVM uses 22050Hz for Full Throttle

	uint32 track, index, maxFrames;
	uint16 flags = 0;
	byte vol = 127;
	int8 pan = 0;

	// Need a heuristic to detect the sound format, unfortunately.
	// The _version == 1 check fails because some v2 videos erroneously
	// say they're from Rebel Assault (the early trailers for Full
	// Throttle and Rebel Assault II).
	if (!_runSoundHeaderCheck)
		detectSoundHeaderType();

	if (_oldSoundHeader) {
		track = readUint32BE(_file);
		index = readUint32BE(_file);
		maxFrames = readUint32BE(_file);
		size -= 12;
	} else {
		track = readUint16LE(_file);
		index = readUint16LE(_file);
		maxFrames = readUint16LE(_file);
		flags = readUint16LE(_file);
		vol = readByte(_file);
		pan = (int8)readByte(_file);
		size -= 10;
	}

	// TODO: Handle audio

	return true;
}

void SMUSHVideo::detectSoundHeaderType() {
	// We're just assuming that maxFrames and flags are not going to be zero
	// for the newer header and that the first chunk in the old header
	// will have index = 0 (which seems to be pretty safe).

	readUint32BE(_file);
	_oldSoundHeader = (readUint32BE(_file) == 0);

	fseek(_file, -8, SEEK_CUR);
	_runSoundHeaderCheck = true;
}

bool SMUSHVideo::readFrameHeader() {
	// SANM frame header

	if (readUint32BE(_file) != MKTAG('F', 'L', 'H', 'D'))
		return false;

	uint32 size = readUint32BE(_file);
	uint32 pos = ftell(_file);
	uint32 bytesLeft = size;

	while (bytesLeft > 0) {
		uint32 subType = readUint32BE(_file);
		uint32 subSize = readUint32BE(_file);
		uint32 subPos = ftell(_file);

		bool result = true;

		switch (subType) {
		case MKTAG('B', 'l', '1', '6'):
			// Nothing to do
			break;
		case MKTAG('W', 'a', 'v', 'e'):
			_audioRate = readUint32LE(_file);
			_audioChannels = readUint32LE(_file);

			// HACK: Based on what Residual does
			// Seems the size is always 12 even when it's not :P
			subSize = 12;
			break;
		default:
			fprintf(stderr, "Invalid SANM frame header type '%c%c%c%c'\n", LISTTAG(subType));
			result = false;
		}

		if (!result)
			return false;

		bytesLeft -= subSize + 8 + (subSize & 1);
		fseek(_file, subPos + subSize + (subSize & 1), SEEK_SET);
	}

	fseek(_file, pos + size + (size & 1), SEEK_SET);
	return true;
}

bool SMUSHVideo::handleIACT(uint32 size) {
	// Handle interactive sequences

	if (size < 8)
		return false;

	uint16 code = readUint16LE(_file);
	uint16 flags = readUint16LE(_file);
	/* int16 unknown = */ readSint16LE(_file);
	uint16 trackFlags = readUint16LE(_file);

	if (code == 8 && flags == 46 && trackFlags != 1000) {
		// Audio track
		if (trackFlags == 0)
			return bufferIACTAudio(size);

		return bufferIMuseAudio(trackFlags);
	} if (code == 6 && flags == 38) {
		// Clear frame? Seems to fix some RA2 videos
		//if (_buffer)
		//	memset(_buffer, 0, _pitch * _height);
	} else {
		// Otherwise, the data is meant for INSANE
	}

	return true;
}

bool SMUSHVideo::bufferIMuseAudio(uint16 trackFlags) {
	// TODO: Queue iMuse audio (22050Hz)
	// (As used by The Dig (only?))
	return true;
}

bool SMUSHVideo::bufferIACTAudio(uint32 size) {
	// Queue IACT audio (22050Hz)

	if (!_iactStream) {
		// Ignore _audioRate since it's always 22050Hz
		// and CMI often lies and says 11025Hz
		_iactStream = makeQueuingAudioStream(22050, 2);
		_audio->play(_iactStream);
		_iactPos = 0;
		_iactBuffer = new byte[4096];
	}

	/* uint16 trackID = */ readUint16LE(_file);
	/* uint16 index = */ readUint16LE(_file);
	/* uint16 frameCount = */ readUint16LE(_file);
	/* uint32 bytesLeft = */ readUint32LE(_file);
	size -= 18;

	while (size > 0) {
		if (_iactPos >= 2) {
			uint32 length = READ_BE_UINT16(_iactBuffer) + 2;
			length -= _iactPos;

			if (length > size) {
				fread(_iactBuffer + _iactPos, 1, size, _file);
				_iactPos += size;
				size = 0;
			} else {
				byte *output = new byte[4096];

				fread(_iactBuffer + _iactPos, 1, length, _file);

				byte *dst = output;
				byte *src = _iactBuffer + 2;

				int count = 1024;
				byte var1 = *src++;
				byte var2 = var1 >> 4;
				var1 &= 0xF;

				while (count--) {
					byte value = *src++;
					if (value == 0x80) {
						*dst++ = *src++;
						*dst++ = *src++;
					} else {
						int16 val = (int8)value << var2;
						*dst++ = val >> 8;
						*dst++ = (byte)val;
					}

					value = *src++;
					if (value == 0x80) {
						*dst++ = *src++;
						*dst++ = *src++;
					} else {
						int16 val = (int8)value << var1;
						*dst++ = val >> 8;
						*dst++ = (byte)val;
					}
				}

				_iactStream->queueAudioStream(makePCMStream(output, 4096, _iactStream->getRate(), _iactStream->getChannels(), FLAG_16BITS));
				size -= length;
				_iactPos = 0;
			}
		} else {
			if (size > 1 && _iactPos == 0) {
				_iactBuffer[0] = readByte(_file);
				_iactPos = 1;
				size--;
			}

			_iactBuffer[_iactPos] = readByte(_file);
			_iactPos++;
			size--;
		}
	}

	return true;
}

bool SMUSHVideo::handleGhost(uint32 size) {
	if (size != 12) {
		fprintf(stderr, "Invalid ghost chunk (%d)\n", size);
		return false;
	}

	// There are only a few examples in Rebel Assault of this, the most
	// prominent in FNFINAL.ANM. It looks like it's used for mirroring
	// ("ghosting") since it's in the scene where people are clapping.
	// Its other usage is in several level 5 animations.

	// FNFINAL.ANM: 28, 182, 0
	// Level 5: 28, -190, 20

	/* uint32 unk1 = */ readUint32BE(_file);
	/* int32 unk2 = */ readSint32BE(_file);
	/* int32 unk3 = */ readSint32BE(_file);

	// unk2 seems to be the 'startX' parameter at least in FNFINAL.
	// It copies to startX through _width from (_width - startX) to 0
	// unk3 is possibly startY.

	// However, it works off of *only* what was decoded in this frame.
	// This code will need to be extended not to draw to the main buffer
	// for each FOBJ.

	return true;
}

void SMUSHVideo::decodeCodec21(int left, int top, uint width, uint height) {
	for (uint y = 0; y < height; y++) {
		byte *dst = _buffer + _pitch * (y + top) + left;
		uint16 lineSize = readUint16LE(_file);
		uint32 pos = ftell(_file);

		int len = width;
		do {
			int offs = readUint16LE(_file);
			dst += offs;
			len -= offs;
			if (len <= 0)
				break;

			int w = readUint16LE(_file) + 1;
			len -= w;
			if (len < 0)
				w += len;

			for (int i = 0; i < w; i++) {
				byte color = readByte(_file);

				if (color != 0)
					*dst = color;

				dst++;
			}
		} while (len > 0);

		fseek(_file, pos + lineSize, SEEK_SET);
	}
}

bool SMUSHVideo::handleBlocky16(GraphicsManager &gfx, uint32 size) {
	if (!isHighColor()) {
		fprintf(stderr, "Blocky16 chunk in 8bpp video\n");
		return false;
	}

	byte *ptr = new byte[size];
	fread(ptr, 1, size, _file);

	if (!_blocky16)
		_blocky16 = new Blocky16(_width, _height);

	if (!_buffer) {
		_buffer = new byte[_pitch * _height];
		memset(_buffer, 0, _pitch * _height);
	}

	_blocky16->decode(_buffer, ptr);

	delete[] ptr;

	gfx.blit(_buffer, 0, 0, _width, _height, _pitch);
	return true;
}

bool SMUSHVideo::detectFrameSize() {
	// There is no frame size, so we'll be using a heuristic to detect it.

	// Basically, codecs 37, 47, and 48 work directly off of the whole frame
	// so they generally will always show the correct size. (Except for
	// Mortimer which does some funky frame scaling/resizing). There we'll
	// have to resize based on the dimensions of 37 and scale appropriately
	// to 640x480.

	// Most of this is for detecting the total frame size of a Rebel Assault
	// video which is a lot harder.

	uint32 startPos = ftell(_file);
	bool done = false;

	// Only go through a certain amount of frames
	uint32 maxFrames = 20;
	if (maxFrames > _frameCount)
		maxFrames = _frameCount;

	for (uint i = 0; i < maxFrames && !done; i++) {
		if (readUint32BE(_file) != MKTAG('F', 'R', 'M', 'E'))
			return false;

		uint32 frameSize = readUint32BE(_file);
		uint32 bytesLeft = frameSize;

		while (bytesLeft > 0) {
			uint32 subType = readUint32BE(_file);
			uint32 subSize = readUint32BE(_file);
			uint32 subPos = ftell(_file);

			if (feof(_file)) {
				// HACK: L2PLAY.ANM from Rebel Assault seems to have an unaligned FOBJ :/
				fprintf(stderr, "Unexpected end of file!\n");
				return false;
			}

			if (subType == MKTAG('F', 'O', 'B', 'J')) {
				byte codec = readByte(_file);
				/* byte codecParam = */ readByte(_file);
				int16 left = readSint16LE(_file);
				int16 top = readSint16LE(_file);
				uint16 width = readUint16LE(_file);
				uint16 height = readUint16LE(_file);
				readUint16LE(_file);
				readUint16LE(_file);

				if (width != 1 && height != 1) {
					// HACK: Some Full Throttle videos start off with this. Don't
					// want our algorithm to be thrown off.

					// Codecs 37, 47, and 48 should be telling the truth
					if (codec == 37 || codec == 47 || codec == 48) {
						_width = width;
						_height = height;
						done = true;
						break;
					}

					// FIXME: Just take other codecs at face value for now too
					// (This basically only affects Rebel Assault and NUT files)
					_width = width;
					if (left > 0)
						_width += left;

					_height = height;
					if (top > 0)
						_height += top;

					// Try to figure how close we are to 320x200 and see if maybe
					// this object is a partial frame object.
					// TODO: Not ready for primetime yet
					/*if (_width < 320 && _width > 310)
						_width = 320;
					if (height < 200 && _height > 190)
						_height = 200;*/

					done = true;
					break;
				}
			} else if (subType == MKTAG('Z', 'F', 'O', 'B')) {
				fprintf(stderr, "ScummVM-compressed videos not supported\n");
				return false;
			}

			bytesLeft -= subSize + 8 + (subSize & 1);
			fseek(_file, subPos + subSize + (subSize & 1), SEEK_SET);
		}
	}

	if (_width == 0 || _height == 0)
		return false;

	fseek(_file, startPos, SEEK_SET);
	_pitch = _width;
	_buffer = new byte[_pitch * _height];
	memset(_buffer, 0, _pitch * _height); // FIXME: Is this right?
	return true;
}

bool SMUSHVideo::handleVIMA(uint32 size) {
	// VIMA Audio (SANM-only)
	if (!_vimaDestTable) {
		_vimaDestTable = new uint16[5786];
		initVIMA(_vimaDestTable);
	}

	int flags = FLAG_16BITS;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	flags |= FLAG_LITTLE_ENDIAN;
#endif

	if (!_iactStream) {
		_iactStream = makeQueuingAudioStream(_audioRate, _audioChannels);
		_audio->play(_iactStream);
	}

	uint32 decompressedSize = readUint32BE(_file);
	if ((int32)decompressedSize < 0) {
		// Residual is mum on documentation, but this seems to be some
		// sort of extended-info chunk.
		readUint32BE(_file);
		decompressedSize = readUint32BE(_file);
	}

	byte *src = new byte[size];
	fread(src, 1, size, _file);

	int16 *dst = new int16[decompressedSize * _audioChannels];
	decompressVIMA(src, dst, decompressedSize * _audioChannels * 2, _vimaDestTable);

	delete[] src;

	_iactStream->queueAudioStream(makePCMStream((byte *)dst, decompressedSize * _audioChannels * 2, _audioRate, _iactStream->getChannels(), flags));
	return true;
}
