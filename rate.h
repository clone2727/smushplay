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

// Based on the ScummVM code of the same name (GPLv2+)

#ifndef RATE_H
#define RATE_H

#include "types.h"

class AudioStream;

class RateConverter {
public:
	RateConverter() {}
	virtual ~RateConverter() {}

	/**
	 * @return Number of sample pairs written into the buffer.
	 */
	virtual int flow(AudioStream &input, int16 *outBuffer, uint32 outSamples, uint16 leftVolume, uint16 rightVolume) = 0;
};

RateConverter *makeRateConverter(uint32 inRate, uint32 outRate, bool stereo, bool reverseStereo = false);

#endif
