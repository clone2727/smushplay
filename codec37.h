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

#ifndef CODEC37_H
#define CODEC37_H

#include "types.h"

class Codec37Decoder {
public:
	Codec37Decoder(int width, int height);
	~Codec37Decoder();

	void decode(byte *dst, const byte *src);

private:
	void makeTable(int, int);
	void proc1(byte *dst, const byte *src, int32, int, int, int, int16 *);
	void proc3WithFDFE(byte *dst, const byte *src, int32, int, int, int, int16 *);
	void proc3WithoutFDFE(byte *dst, const byte *src, int32, int, int, int, int16 *);
	void proc4WithFDFE(byte *dst, const byte *src, int32, int, int, int, int16 *);
	void proc4WithoutFDFE(byte *dst, const byte *src, int32, int, int, int, int16 *);
	void bompDecodeLine(byte *dst, const byte *src, int len);

	int32 _deltaSize;
	byte *_deltaBufs[2];
	byte *_deltaBuf;
	int16 *_offsetTable;
	int _curTable;
	uint16 _prevSeqNb;
	int _tableLastPitch;
	int _tableLastIndex;
	int32 _frameSize;
	int _width, _height;
};

#endif
