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

// Partially based on ScummVM's SaudChannel code (GPLv2+)

#include <assert.h>
#include "audiostream.h"
#include "pcm.h"
#include "smushchannel.h"
#include "util.h"

SAUDChannel::SAUDChannel(AudioManager *audio, uint track, uint maxFrames, uint rate) : SMUSHChannel(audio, track, maxFrames) {
	_rate = rate;
}

void SAUDChannel::update() {
	if (!_stream)
		readHeader();

	if (_stream)
		queueSamples();
}

void SAUDChannel::readHeader() {
	assert(_dataConsumed == 0);

	if (_dataSize < 16)
		return;

	byte *ptr = _data;
	assert(READ_BE_UINT32(ptr) == MKTAG('S', 'A', 'U', 'D'));
	ptr += 8;

	bool foundData = false;
	uint32 dataConsumed = 8;

	while (!foundData) {
		uint32 tag = READ_BE_UINT32(ptr);
		uint32 size = READ_BE_UINT32(ptr + 4);
		ptr += 8;
		dataConsumed += 8;

		if (tag == MKTAG('S', 'D', 'A', 'T')) {
			foundData = true;
			_dataConsumed = dataConsumed;
			_totalDataSize = size;
			break;
		}

		if (size + 8 > _dataSize - dataConsumed)
			return;

		// The movie can override the rate
		// However, it only seems certain sizes of the chunk have the info
		// Mortimer makes heavy use of this, RA2 sporadic, and FT minor
		if (tag == MKTAG('S', 'T', 'R', 'K') && size == 14)
			_rate = READ_BE_UINT16(ptr + 12);

		ptr += size;
		dataConsumed += size;
	}

	_stream = makeQueuingAudioStream(_rate, 1);
	startStream();
}

void SAUDChannel::queueSamples() {
	uint bytesLeft = _dataSize - _dataConsumed;

	// Clip it to what we have left
	if (bytesLeft > _totalDataSize - _totalDataUsed)
		bytesLeft = _totalDataSize - _totalDataUsed;

	if (bytesLeft == 0)
		return;

	byte *buffer = new byte[bytesLeft];
	memcpy(buffer, _data + _dataConsumed, bytesLeft);
	_stream->queueAudioStream(makePCMStream(buffer, bytesLeft, _rate, 1, FLAG_UNSIGNED));
	_dataConsumed += bytesLeft;
	_totalDataUsed += bytesLeft;
}
