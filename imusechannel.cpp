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

// Partially based on ScummVM's ImuseChannel code (GPLv2+)

#include <assert.h>
#include <SDL_endian.h>
#include "audiostream.h"
#include "pcm.h"
#include "smushchannel.h"
#include "util.h"

IMuseChannel::IMuseChannel(AudioManager *audio, uint track, uint maxFrames) : SMUSHChannel(audio, track, maxFrames) {
}

void IMuseChannel::setVolume(uint volume) {
	if (volume == 1 || volume == 2 || volume == 3) {
		volume = 127;
	} else if (volume >= 100 && volume <= 163) {
		volume = volume * 2 - 200;
	} else if (volume >= 200 && volume <= 263) {
		volume = volume * 2 - 400;
	} else if (volume >= 300 && volume <= 363) {
		volume = volume * 2 - 600;
	} else {
		fprintf(stderr, "IMuseChannel::setVolume(): bad flags: %d", volume);
		volume = 127;
	}

	SMUSHChannel::setVolume(volume);
}

void IMuseChannel::update() {
	if (!_stream)
		readHeader();

	if (_stream)
		queueSamples();
}

void IMuseChannel::readHeader() {
	assert(_dataConsumed == 0);

	if (_dataSize < 8)
		return;

	byte *ptr = _data;
	uint32 dataConsumed = 0;

	if (READ_BE_UINT32(ptr) != MKTAG('i', 'M', 'U', 'S')) {
		fprintf(stderr, "Failed to find iMuse header\n");
		return;
	}

	ptr += 8;
	dataConsumed += 8;

	if (READ_BE_UINT32(ptr) != MKTAG('M', 'A', 'P', ' ')) {
		fprintf(stderr, "Failed to find iMuse map\n");
		return;
	}

	uint32 mapSize = READ_BE_UINT32(ptr + 4);

	ptr += 8;
	dataConsumed += 8;

	if (mapSize + 8 > _dataSize - dataConsumed)
		return;

	uint32 mapBytesLeft = mapSize;

	while (mapBytesLeft > 0) {
		uint32 subTag = READ_BE_UINT32(ptr);
		uint32 subSize = READ_BE_UINT32(ptr + 4);
		ptr += 8;
		mapBytesLeft -= 8;

		switch (subTag) {
		case MKTAG('F', 'R', 'M', 'T'):
			assert(subSize == 20);
			_bitsPerSample = READ_BE_UINT32(ptr + 8);
			assert(_bitsPerSample == 8 || _bitsPerSample == 12 || _bitsPerSample == 16);
			_rate = READ_BE_UINT32(ptr + 12);
			_channels = READ_BE_UINT32(ptr + 16);
			assert(_channels == 1 || _channels == 2);
			break;
		case MKTAG('T', 'E', 'X', 'T'):
			// Ignored
			break;
		case MKTAG('R', 'E', 'G', 'N'):
			assert(subSize == 8);
			break;
		case MKTAG('S', 'T', 'O', 'P'):
			assert(subSize == 4);
			break;
		default:
			fprintf(stderr, "Unknown iMuse MAP tag '%c%c%c%c'\n", LISTTAG(subTag));
		}

		ptr += subSize;
		mapBytesLeft -= subSize;
	}

	assert(READ_BE_UINT32(ptr) == MKTAG('D', 'A', 'T', 'A'));
	_totalDataSize = READ_BE_UINT32(ptr + 4);

	_dataConsumed = dataConsumed + mapSize + 8;
	_stream = makeQueuingAudioStream(_rate, _channels);
	startStream();
}

void IMuseChannel::queueSamples() {
	uint bytesLeft = _dataSize - _dataConsumed;

	if (_bitsPerSample != 8)
		bytesLeft -= (bytesLeft % (_channels * (_bitsPerSample == 12 ? 3 : 2)));

	if (bytesLeft == 0)
		return;

	if (_bitsPerSample == 8) {
		byte *buffer = new byte[bytesLeft];
		memcpy(buffer, _data + _dataConsumed, bytesLeft);
		_stream->queueAudioStream(makePCMStream(buffer, bytesLeft, _rate, _channels, FLAG_UNSIGNED));
	} else if (_bitsPerSample == 12) {
		uint decompressedSize = bytesLeft / 3 * 2;
		int16 *buffer = new int16[decompressedSize];
		decode12(_data + _dataConsumed, buffer, bytesLeft / 3);

		int flags = FLAG_16BITS;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		flags |= FLAG_LITTLE_ENDIAN;
#endif

		_stream->queueAudioStream(makePCMStream((byte *)buffer, decompressedSize * 2, _rate, _channels, flags));
	} else if (_bitsPerSample == 16) {
		byte *buffer = new byte[bytesLeft];
		memcpy(buffer, _data + _dataConsumed, bytesLeft);
		_stream->queueAudioStream(makePCMStream(buffer, bytesLeft, _rate, _channels, FLAG_16BITS));
	}

	_dataConsumed += bytesLeft;
	_totalDataUsed += bytesLeft;
}

void IMuseChannel::decode12(const byte *src, int16 *dst, uint32 size) {
	while (size--) {
		byte v1 = *src++;
		byte v2 = *src++;
		byte v3 = *src++;
		*dst++ = ((((v2 & 0xF) << 8) | v1) << 4) - 0x8000;
		*dst++ = ((((v2 & 0xF0) << 4) | v3) << 4) - 0x8000;
	}
}
