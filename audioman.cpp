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

#include <assert.h>
#include "audioman.h"
#include "audiostream.h"
#include "rate.h"

AudioManager::AudioManager() {
	_mutex = SDL_CreateMutex();
}

AudioManager::~AudioManager() {
	stopAll();
	SDL_CloseAudio();
	SDL_DestroyMutex(_mutex);
}

bool AudioManager::init() {
	SDL_AudioSpec spec;
	spec.freq = 44100;
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.samples = 4096;
	spec.callback = sdlCallback;
	spec.userdata = this;

	if (SDL_OpenAudio(&spec, &_spec) != 0)
		return false;

	if (_spec.channels != 2 || _spec.format != AUDIO_S16SYS)
		return false;

	SDL_PauseAudio(0);
	return true;
}

void AudioManager::play(uint channel, AudioStream *stream) {
	stop(channel);

	Channel *chan = new Channel();
	chan->stream = stream;
	chan->converter = makeRateConverter(stream->getRate(), _spec.freq, stream->getChannels() == 2);

	SDL_mutexP(_mutex);
	_channels[channel] = chan;
	SDL_mutexV(_mutex);
}

void AudioManager::stop(uint channel) {
	SDL_mutexP(_mutex);

	ChannelMap::iterator it = _channels.find(channel);

	if (it != _channels.end()) {
		Channel *chan = it->second;
		delete chan->stream;
		delete chan->converter;
		delete chan;
		_channels.erase(it);
	}

	SDL_mutexV(_mutex);
}

void AudioManager::stopAll() {
	SDL_mutexP(_mutex);

	for (ChannelMap::iterator it = _channels.begin(); it != _channels.end(); it++) {
		Channel *channel = it->second;
		delete channel->stream;
		delete channel->converter;
		delete channel;
	}

	_channels.clear();

	SDL_mutexV(_mutex);
}

void AudioManager::sdlCallback(void *manager, byte *samples, int len) {
	((AudioManager *)manager)->callbackHandler(samples, len);
}

void AudioManager::callbackHandler(byte *samples, int len) {
	assert((len % 4) == 0);
	memset(samples, 0, len);

	SDL_mutexP(_mutex);

	for (ChannelMap::iterator it = _channels.begin(); it != _channels.end(); it++) {
		Channel *channel = it->second;

		if (channel->stream->endOfData()) {
			// TODO: Remove the channel?
		} else {
			channel->converter->flow(*channel->stream, (int16 *)samples, len >> 2, 255, 255);
		}
	}

	SDL_mutexV(_mutex);
}
