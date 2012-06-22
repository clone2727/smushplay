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

#ifndef AUDIOMAN_H
#define AUDIOMAN_H

#include <SDL.h>
#include <SDL_thread.h>
#include <map>
#include "types.h"

class AudioStream;
class RateConverter;

// TODO: Expand with a handle system similar to ScummVM's
// Maybe eventually just replace with ScummVM's

class AudioManager {
public:
	AudioManager();
	~AudioManager();

	bool init();
	void play(uint channel, AudioStream *stream);
	void stop(uint channel);
	void stopAll();

private:
	void callbackHandler(byte *samples, int len);
	static void sdlCallback(void *manager, byte *samples, int len);

	SDL_AudioSpec _spec;
	SDL_mutex *_mutex;

	struct Channel {
		AudioStream *stream;
		RateConverter *converter;
	};

	typedef std::map<uint, Channel*> ChannelMap;
	ChannelMap _channels;
};

#endif
