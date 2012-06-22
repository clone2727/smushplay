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

// Based on the xoreos code of the same name, which is in turn
// based on the RawStream code of ScummVM (GPLv3+ and GPLv2+,
// respectively).

#include <assert.h>
#include <SDL_thread.h>
#include <queue>
#include "audiostream.h"


class QueuingAudioStreamImpl : public QueuingAudioStream {
private:
	/**
	 * We queue a number of (pointers to) audio stream objects.
	 * In addition, we need to remember for each stream whether
	 * to dispose it after all data has been read from it.
	 * Hence, we don't store pointers to stream objects directly,
	 * but rather StreamHolder structs.
	 */
	struct StreamHolder {
		AudioStream *_stream;
		bool _disposeAfterUse;
		StreamHolder(AudioStream *stream, bool disposeAfterUse)
		    : _stream(stream),
		      _disposeAfterUse(disposeAfterUse) {}
	};

	/**
	 * The sampling rate of this audio stream.
	 */
	const int _rate;

	/**
	 * The number of channels in the stream.
	 */
	const int _channels;

	/**
	 * This flag is set by the finish() method only. See there for more details.
	 */
	bool _finished;

	/**
	 * A mutex to avoid access problems (causing e.g. corruption of
	 * the linked list) in thread aware environments.
	 */
	SDL_mutex *_mutex;

	/**
	 * The queue of audio streams.
	 */
	std::queue<StreamHolder> _queue;

public:
	QueuingAudioStreamImpl(int rate, int channels)  : _rate(rate), _channels(channels), _finished(false) {
		_mutex = SDL_CreateMutex();
	}
	~QueuingAudioStreamImpl();

	// Implement the AudioStream API
	virtual int readBuffer(int16 *buffer, const int numSamples);
	virtual int getChannels() const { return _channels; }
	virtual int getRate() const { return _rate; }
	virtual bool endOfData() const {
		// TODO: Lock mutex?
		return _queue.empty();
	}
	virtual bool endOfStream() const { return _finished && _queue.empty(); }

	// Implement the QueuingAudioStream API
	virtual void queueAudioStream(AudioStream *stream, bool disposeAfterUse);
	virtual void finish() { _finished = true; }

	uint32 getQueuedStreamCount() const {
		// TODO: Lock mutex?
		return _queue.size();
	}
};

QueuingAudioStreamImpl::~QueuingAudioStreamImpl() {
	SDL_DestroyMutex(_mutex);

	while (!_queue.empty()) {
		StreamHolder tmp = _queue.front();
		_queue.pop();
		if (tmp._disposeAfterUse)
			delete tmp._stream;
	}
}

void QueuingAudioStreamImpl::queueAudioStream(AudioStream *stream, bool disposeAfterUse) {
	assert(!_finished);
	assert(stream->getRate() == getRate());
	assert(stream->getChannels() == getChannels());

	SDL_mutexP(_mutex);
	_queue.push(StreamHolder(stream, disposeAfterUse));
	SDL_mutexV(_mutex);
}

int QueuingAudioStreamImpl::readBuffer(int16 *buffer, const int numSamples) {
	int samplesDecoded = 0;

	SDL_mutexP(_mutex);

	while (samplesDecoded < numSamples && !_queue.empty()) {
		AudioStream *stream = _queue.front()._stream;
		samplesDecoded += stream->readBuffer(buffer + samplesDecoded, numSamples - samplesDecoded);

		if (stream->endOfData()) {
			StreamHolder tmp = _queue.front();
			_queue.pop();
			if (tmp._disposeAfterUse)
				delete stream;
		}
	}

	SDL_mutexV(_mutex);

	return samplesDecoded;
}

QueuingAudioStream *makeQueuingAudioStream(int rate, int channels) {
	return new QueuingAudioStreamImpl(rate, channels);
}

