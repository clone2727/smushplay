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

// Partially based on ScummVM's SmushChannel code (GPLv2+)

#include <assert.h>
#include "audiostream.h"
#include "smushchannel.h"

SMUSHChannel::SMUSHChannel(AudioManager *audio, uint track, uint maxFrames) {
	_audio = audio;
	_track = track;
	_maxFrames = maxFrames;
	_stream = 0;
	_data = 0;
	_dataSize = 0;
	_dataConsumed = 0;
	_totalDataUsed = 0;
	_totalDataSize = 0;
	_index = -1;
}

SMUSHChannel::~SMUSHChannel() {
	_audio->stop(_handle);
}

void SMUSHChannel::setVolume(uint volume) {
	_volume = volume;
	_audio->setVolume(_handle, _volume);
}

void SMUSHChannel::setBalance(int8 balance) {
	_balance = balance;
	_audio->setBalance(_handle, _balance);
}

void SMUSHChannel::appendData(uint index, byte *data, uint32 size) {
	if (done())
		return;

	if (_index + 1 != (int)index)
		fprintf(stderr, "Invalid SMUSH channel index (%d, should be %d)\n", index, _index + 1);

	_index = index;

	if (_dataConsumed == _dataSize) {
		delete[] _data;
		_data = data;
		_dataSize = size;
		_dataConsumed = 0;
	} else {
		uint32 dataLeft = _dataSize - _dataConsumed;
		uint32 newSize = dataLeft + size;
		byte *newData = new byte[newSize];
		memcpy(newData, _data + _dataConsumed, dataLeft);
		delete[] _data;
		memcpy(newData + dataLeft, data, size);
		delete[] data;
		_data = newData;
		_dataSize = newSize;
		_dataConsumed = 0;
	}

	update();
}

bool SMUSHChannel::done() const {
	return _stream && _totalDataUsed >= _totalDataSize;
}

void SMUSHChannel::startStream() {
	assert(_stream);
	_audio->play(_stream, _handle, _volume, _balance);
}

