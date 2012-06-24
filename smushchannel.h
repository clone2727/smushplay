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

#ifndef SMUSHCHANNEL_H
#define SMUSHCHANNEL_H

#include "audioman.h"
#include "types.h"

class AudioManager;
class QueuingAudioStream;

class SMUSHChannel {
public:
	SMUSHChannel(AudioManager *audio, uint track, uint maxFrames);
	virtual ~SMUSHChannel();
	void appendData(uint index, byte *data, uint32 size);
	virtual void setVolume(uint volume);
	void setBalance(int8 balance);
	virtual bool done() const;

protected:
	uint _track;
	uint _maxFrames;
	byte _volume;
	int8 _balance;

	byte *_data;
	uint32 _dataSize, _dataConsumed;
	uint32 _totalDataUsed, _totalDataSize;
	int _index;

	AudioManager *_audio;
	QueuingAudioStream *_stream;
	void startStream();

	virtual void update() = 0;

private:
	AudioHandle _handle;
};

class SAUDChannel : public SMUSHChannel {
public:
	SAUDChannel(AudioManager *audio, uint track, uint maxFrames, uint rate);

protected:
	void update();

private:
	void readHeader();
	void queueSamples();

	uint _rate;
};

class IMuseChannel : public SMUSHChannel {
public:
	IMuseChannel(AudioManager *audio, uint track, uint maxFrames);

	void setVolume(uint volume);

protected:
	void update();

private:
	void readHeader();
	void queueSamples();
	void decode12(const byte *src, int16 *dst, uint32 size);

	uint _bitsPerSample;
	uint _rate;
	uint _channels;
};

#endif
