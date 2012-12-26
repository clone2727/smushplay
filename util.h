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

#ifndef UTIL_H
#define UTIL_H

#include <SDL_endian.h>
#include "types.h"

#define ARRAYSIZE(x) ((int)(sizeof(x) / sizeof(x[0])))

#define MKTAG(a, b, c, d) ((uint32)(((a) << 24) | ((b) << 16) | ((c) << 8) | (d)))
#define LISTTAG(a) (((a) >> 24) & 0xFF), (((a) >> 16) & 0xFF), (((a) >> 8) & 0xFF), (((a) & 0xFF))

uint16 READ_LE_UINT16(const void *ptr);
uint32 READ_LE_UINT32(const void *ptr);
uint16 READ_BE_UINT16(const void *ptr);
uint32 READ_BE_UINT32(const void *ptr);

inline uint16 SWAP_BYTES_16(const uint16 a) {
	return (a >> 8) | (a << 8);
}

inline uint32 SWAP_BYTES_32(uint32 a) {
	const uint16 low = (uint16)a, high = (uint16)(a >> 16);
	return ((uint32)(uint16)((low >> 8) | (low << 8)) << 16)
			| (uint16)((high >> 8) | (high << 8));
}

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	#define FROM_LE_16(a) ((uint16)(a))
	#define FROM_LE_32(a) ((uint32)(a))

	#define FROM_BE_16(a) SWAP_BYTES_16(a)
	#define FROM_BE_32(a) SWAP_BYTES_32(a)
#else
	#define FROM_LE_16(a) SWAP_BYTES_16(a)
	#define FROM_LE_32(a) SWAP_BYTES_32(a)

	#define FROM_BE_16(a) ((uint16)(a))
	#define FROM_BE_32(a) ((uint32)(a))
#endif

#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

template<typename T> inline T ABS(T x) { return (x >= 0) ? x : -x; }
template<typename T> inline T MIN (T a, T b) { return (a<b) ? a : b; }
template<typename T> inline T MAX (T a, T b) { return (a>b) ? a : b; }
template<typename T> inline void SWAP(T &a, T &b) { T tmp = a; a = b; b = tmp; }
template<typename T> inline T CLIP(T v, T amin, T amax) {
	if (v < amin)
		return amin;
	else if (v > amax)
		return amax;

	return v;
}

#endif
