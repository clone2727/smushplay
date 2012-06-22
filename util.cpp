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

// Partially based on ScummVM utility/endian functions (GPLv2+)

#include <SDL_endian.h>
#include "util.h"

uint16 READ_LE_UINT16(const void *ptr) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	const byte *p = (const byte *)ptr;
	return p[0] | (p[1] << 8);
#else
	return *((uint16 *)ptr);
#endif
}

uint32 READ_LE_UINT32(const void *ptr) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	const byte *p = (const byte *)ptr;
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
#else
	return *((uint32 *)ptr);
#endif
}

uint16 READ_BE_UINT16(const void *ptr) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	return *((uint16 *)ptr);
#else
	const byte *p = (const byte *)ptr;
	return (p[0] << 8) | p[1];
#endif
}

uint32 READ_BE_UINT32(const void *ptr) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	return *((uint32 *)ptr);
#else
	const byte *p = (const byte *)ptr;
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
#endif
}
