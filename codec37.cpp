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

#include <assert.h>
#include <string.h>
#include "codec37.h"
#include "util.h"

Codec37Decoder::Codec37Decoder(int width, int height) {
	_width = width;
	_height = height;
	_frameSize = _width * _height;
	_deltaSize = _frameSize * 3 + 0x13600;

	_deltaBuf = new byte[_deltaSize];
	memset(_deltaBuf, 0, _deltaSize);

	_deltaBufs[0] = _deltaBuf + 0x4D80;
	_deltaBufs[1] = _deltaBuf + 0xE880 + _frameSize;

	_offsetTable = new int16[255];

	_curTable = 0;
	_prevSeqNb = 0;
	_tableLastPitch = -1;
	_tableLastIndex = -1;
}

Codec37Decoder::~Codec37Decoder() {
	if (_offsetTable) {
		delete[] _offsetTable;
		_offsetTable = 0;
		_tableLastPitch = -1;
		_tableLastIndex = -1;
	}

	if (_deltaBuf) {
		delete[] _deltaBuf;
		_deltaSize = 0;
		_deltaBuf = 0;
		_deltaBufs[0] = 0;
		_deltaBufs[1] = 0;
	}
}

void Codec37Decoder::makeTable(int pitch, int index) {
	static const int8 table[] = {
		0,   0,   1,   0,   2,   0,   3,   0,   5,   0,
		8,   0,  13,   0,  21,   0,  -1,   0,  -2,   0,
		-3,   0,  -5,   0,  -8,   0, -13,   0, -17,   0,
		-21,   0,   0,   1,   1,   1,   2,   1,   3,   1,
		5,   1,   8,   1,  13,   1,  21,   1,  -1,   1,
		-2,   1,  -3,   1,  -5,   1,  -8,   1, -13,   1,
		-17,   1, -21,   1,   0,   2,   1,   2,   2,   2,
		3,   2,   5,   2,   8,   2,  13,   2,  21,   2,
		-1,   2,  -2,   2,  -3,   2,  -5,   2,  -8,   2,
		-13,   2, -17,   2, -21,   2,   0,   3,   1,   3,
		2,   3,   3,   3,   5,   3,   8,   3,  13,   3,
		21,   3,  -1,   3,  -2,   3,  -3,   3,  -5,   3,
		-8,   3, -13,   3, -17,   3, -21,   3,   0,   5,
		1,   5,   2,   5,   3,   5,   5,   5,   8,   5,
		13,   5,  21,   5,  -1,   5,  -2,   5,  -3,   5,
		-5,   5,  -8,   5, -13,   5, -17,   5, -21,   5,
		0,   8,   1,   8,   2,   8,   3,   8,   5,   8,
		8,   8,  13,   8,  21,   8,  -1,   8,  -2,   8,
		-3,   8,  -5,   8,  -8,   8, -13,   8, -17,   8,
		-21,   8,   0,  13,   1,  13,   2,  13,   3,  13,
		5,  13,   8,  13,  13,  13,  21,  13,  -1,  13,
		-2,  13,  -3,  13,  -5,  13,  -8,  13, -13,  13,
		-17,  13, -21,  13,   0,  21,   1,  21,   2,  21,
		3,  21,   5,  21,   8,  21,  13,  21,  21,  21,
		-1,  21,  -2,  21,  -3,  21,  -5,  21,  -8,  21,
		-13,  21, -17,  21, -21,  21,   0,  -1,   1,  -1,
		2,  -1,   3,  -1,   5,  -1,   8,  -1,  13,  -1,
		21,  -1,  -1,  -1,  -2,  -1,  -3,  -1,  -5,  -1,
		-8,  -1, -13,  -1, -17,  -1, -21,  -1,   0,  -2,
		1,  -2,   2,  -2,   3,  -2,   5,  -2,   8,  -2,
		13,  -2,  21,  -2,  -1,  -2,  -2,  -2,  -3,  -2,
		-5,  -2,  -8,  -2, -13,  -2, -17,  -2, -21,  -2,
		0,  -3,   1,  -3,   2,  -3,   3,  -3,   5,  -3,
		8,  -3,  13,  -3,  21,  -3,  -1,  -3,  -2,  -3,
		-3,  -3,  -5,  -3,  -8,  -3, -13,  -3, -17,  -3,
		-21,  -3,   0,  -5,   1,  -5,   2,  -5,   3,  -5,
		5,  -5,   8,  -5,  13,  -5,  21,  -5,  -1,  -5,
		-2,  -5,  -3,  -5,  -5,  -5,  -8,  -5, -13,  -5,
		-17,  -5, -21,  -5,   0,  -8,   1,  -8,   2,  -8,
		3,  -8,   5,  -8,   8,  -8,  13,  -8,  21,  -8,
		-1,  -8,  -2,  -8,  -3,  -8,  -5,  -8,  -8,  -8,
		-13,  -8, -17,  -8, -21,  -8,   0, -13,   1, -13,
		2, -13,   3, -13,   5, -13,   8, -13,  13, -13,
		21, -13,  -1, -13,  -2, -13,  -3, -13,  -5, -13,
		-8, -13, -13, -13, -17, -13, -21, -13,   0, -17,
		1, -17,   2, -17,   3, -17,   5, -17,   8, -17,
		13, -17,  21, -17,  -1, -17,  -2, -17,  -3, -17,
		-5, -17,  -8, -17, -13, -17, -17, -17, -21, -17,
		0, -21,   1, -21,   2, -21,   3, -21,   5, -21,
		8, -21,  13, -21,  21, -21,  -1, -21,  -2, -21,
		-3, -21,  -5, -21,  -8, -21, -13, -21, -17, -21,
		0,   0,  -8, -29,   8, -29, -18, -25,  17, -25,
		0, -23,  -6, -22,   6, -22, -13, -19,  12, -19,
		0, -18,  25, -18, -25, -17,  -5, -17,   5, -17,
		-10, -15,  10, -15,   0, -14,  -4, -13,   4, -13,
		19, -13, -19, -12,  -8, -11,  -2, -11,   0, -11,
		2, -11,   8, -11, -15, -10,  -4, -10,   4, -10,
		15, -10,  -6,  -9,  -1,  -9,   1,  -9,   6,  -9,
		-29,  -8, -11,  -8,  -8,  -8,  -3,  -8,   3,  -8,
		8,  -8,  11,  -8,  29,  -8,  -5,  -7,  -2,  -7,
		0,  -7,   2,  -7,   5,  -7, -22,  -6,  -9,  -6,
		-6,  -6,  -3,  -6,  -1,  -6,   1,  -6,   3,  -6,
		6,  -6,   9,  -6,  22,  -6, -17,  -5,  -7,  -5,
		-4,  -5,  -2,  -5,   0,  -5,   2,  -5,   4,  -5,
		7,  -5,  17,  -5, -13,  -4, -10,  -4,  -5,  -4,
		-3,  -4,  -1,  -4,   0,  -4,   1,  -4,   3,  -4,
		5,  -4,  10,  -4,  13,  -4,  -8,  -3,  -6,  -3,
		-4,  -3,  -3,  -3,  -2,  -3,  -1,  -3,   0,  -3,
		1,  -3,   2,  -3,   4,  -3,   6,  -3,   8,  -3,
		-11,  -2,  -7,  -2,  -5,  -2,  -3,  -2,  -2,  -2,
		-1,  -2,   0,  -2,   1,  -2,   2,  -2,   3,  -2,
		5,  -2,   7,  -2,  11,  -2,  -9,  -1,  -6,  -1,
		-4,  -1,  -3,  -1,  -2,  -1,  -1,  -1,   0,  -1,
		1,  -1,   2,  -1,   3,  -1,   4,  -1,   6,  -1,
		9,  -1, -31,   0, -23,   0, -18,   0, -14,   0,
		-11,   0,  -7,   0,  -5,   0,  -4,   0,  -3,   0,
		-2,   0,  -1,   0,   0, -31,   1,   0,   2,   0,
		3,   0,   4,   0,   5,   0,   7,   0,  11,   0,
		14,   0,  18,   0,  23,   0,  31,   0,  -9,   1,
		-6,   1,  -4,   1,  -3,   1,  -2,   1,  -1,   1,
		0,   1,   1,   1,   2,   1,   3,   1,   4,   1,
		6,   1,   9,   1, -11,   2,  -7,   2,  -5,   2,
		-3,   2,  -2,   2,  -1,   2,   0,   2,   1,   2,
		2,   2,   3,   2,   5,   2,   7,   2,  11,   2,
		-8,   3,  -6,   3,  -4,   3,  -2,   3,  -1,   3,
		0,   3,   1,   3,   2,   3,   3,   3,   4,   3,
		6,   3,   8,   3, -13,   4, -10,   4,  -5,   4,
		-3,   4,  -1,   4,   0,   4,   1,   4,   3,   4,
		5,   4,  10,   4,  13,   4, -17,   5,  -7,   5,
		-4,   5,  -2,   5,   0,   5,   2,   5,   4,   5,
		7,   5,  17,   5, -22,   6,  -9,   6,  -6,   6,
		-3,   6,  -1,   6,   1,   6,   3,   6,   6,   6,
		9,   6,  22,   6,  -5,   7,  -2,   7,   0,   7,
		2,   7,   5,   7, -29,   8, -11,   8,  -8,   8,
		-3,   8,   3,   8,   8,   8,  11,   8,  29,   8,
		-6,   9,  -1,   9,   1,   9,   6,   9, -15,  10,
		-4,  10,   4,  10,  15,  10,  -8,  11,  -2,  11,
		0,  11,   2,  11,   8,  11,  19,  12, -19,  13,
		-4,  13,   4,  13,   0,  14, -10,  15,  10,  15,
		-5,  17,   5,  17,  25,  17, -25,  18,   0,  18,
		-12,  19,  13,  19,  -6,  22,   6,  22,   0,  23,
		-17,  25,  18,  25,  -8,  29,   8,  29,   0,  31,
		0,   0,  -6, -22,   6, -22, -13, -19,  12, -19,
		0, -18,  -5, -17,   5, -17, -10, -15,  10, -15,
		0, -14,  -4, -13,   4, -13,  19, -13, -19, -12,
		-8, -11,  -2, -11,   0, -11,   2, -11,   8, -11,
		-15, -10,  -4, -10,   4, -10,  15, -10,  -6,  -9,
		-1,  -9,   1,  -9,   6,  -9, -11,  -8,  -8,  -8,
		-3,  -8,   0,  -8,   3,  -8,   8,  -8,  11,  -8,
		-5,  -7,  -2,  -7,   0,  -7,   2,  -7,   5,  -7,
		-22,  -6,  -9,  -6,  -6,  -6,  -3,  -6,  -1,  -6,
		1,  -6,   3,  -6,   6,  -6,   9,  -6,  22,  -6,
		-17,  -5,  -7,  -5,  -4,  -5,  -2,  -5,  -1,  -5,
		0,  -5,   1,  -5,   2,  -5,   4,  -5,   7,  -5,
		17,  -5, -13,  -4, -10,  -4,  -5,  -4,  -3,  -4,
		-2,  -4,  -1,  -4,   0,  -4,   1,  -4,   2,  -4,
		3,  -4,   5,  -4,  10,  -4,  13,  -4,  -8,  -3,
		-6,  -3,  -4,  -3,  -3,  -3,  -2,  -3,  -1,  -3,
		0,  -3,   1,  -3,   2,  -3,   3,  -3,   4,  -3,
		6,  -3,   8,  -3, -11,  -2,  -7,  -2,  -5,  -2,
		-4,  -2,  -3,  -2,  -2,  -2,  -1,  -2,   0,  -2,
		1,  -2,   2,  -2,   3,  -2,   4,  -2,   5,  -2,
		7,  -2,  11,  -2,  -9,  -1,  -6,  -1,  -5,  -1,
		-4,  -1,  -3,  -1,  -2,  -1,  -1,  -1,   0,  -1,
		1,  -1,   2,  -1,   3,  -1,   4,  -1,   5,  -1,
		6,  -1,   9,  -1, -23,   0, -18,   0, -14,   0,
		-11,   0,  -7,   0,  -5,   0,  -4,   0,  -3,   0,
		-2,   0,  -1,   0,   0, -23,   1,   0,   2,   0,
		3,   0,   4,   0,   5,   0,   7,   0,  11,   0,
		14,   0,  18,   0,  23,   0,  -9,   1,  -6,   1,
		-5,   1,  -4,   1,  -3,   1,  -2,   1,  -1,   1,
		0,   1,   1,   1,   2,   1,   3,   1,   4,   1,
		5,   1,   6,   1,   9,   1, -11,   2,  -7,   2,
		-5,   2,  -4,   2,  -3,   2,  -2,   2,  -1,   2,
		0,   2,   1,   2,   2,   2,   3,   2,   4,   2,
		5,   2,   7,   2,  11,   2,  -8,   3,  -6,   3,
		-4,   3,  -3,   3,  -2,   3,  -1,   3,   0,   3,
		1,   3,   2,   3,   3,   3,   4,   3,   6,   3,
		8,   3, -13,   4, -10,   4,  -5,   4,  -3,   4,
		-2,   4,  -1,   4,   0,   4,   1,   4,   2,   4,
		3,   4,   5,   4,  10,   4,  13,   4, -17,   5,
		-7,   5,  -4,   5,  -2,   5,  -1,   5,   0,   5,
		1,   5,   2,   5,   4,   5,   7,   5,  17,   5,
		-22,   6,  -9,   6,  -6,   6,  -3,   6,  -1,   6,
		1,   6,   3,   6,   6,   6,   9,   6,  22,   6,
		-5,   7,  -2,   7,   0,   7,   2,   7,   5,   7,
		-11,   8,  -8,   8,  -3,   8,   0,   8,   3,   8,
		8,   8,  11,   8,  -6,   9,  -1,   9,   1,   9,
		6,   9, -15,  10,  -4,  10,   4,  10,  15,  10,
		-8,  11,  -2,  11,   0,  11,   2,  11,   8,  11,
		19,  12, -19,  13,  -4,  13,   4,  13,   0,  14,
		-10,  15,  10,  15,  -5,  17,   5,  17,   0,  18,
		-12,  19,  13,  19,  -6,  22,   6,  22,   0,  23
	};

	if (_tableLastPitch == pitch && _tableLastIndex == index)
		return;

	_tableLastPitch = pitch;
	_tableLastIndex = index;
	index *= 255;
	assert(index + 254 < (int32)(sizeof(table) / 2));

	for (int32 i = 0; i < 255; i++) {
		int32 j = (i + index) * 2;
		_offsetTable[i] = table[j + 1] * pitch + table[j];
	}
}

#define WRITE_4X1_LINE(dst, v) \
	do { \
		for (int j = 0; j < 4; j++) \
			(dst)[j] = v; \
	} while (0)

#define COPY_4X1_LINE(dst, src) \
	do { \
		for (int j = 0; j < 4; j++) \
			(dst)[j] = (src)[j]; \
	} while (0)

// Fill a 4x4 pixel block with a literal pixel value

#define LITERAL_4X4(src, dst, pitch) \
	do { \
		byte t = *src++; \
		for (int x = 0; x < 4; x++) { \
			WRITE_4X1_LINE(dst + pitch * x, t); \
		} \
		dst += 4; \
	} while (0)

// Fill four 4x1 pixel blocks with literal pixel values

#define LITERAL_4X1(src, dst, pitch) \
	do { \
		for (int x = 0; x < 4; x++) { \
			byte t = *src++; \
			WRITE_4X1_LINE(dst + pitch * x, t); \
		} \
		dst += 4; \
	} while (0)

// Fill sixteen 1x1 pixel blocks with literal pixel values

#define LITERAL_1X1(src, dst, pitch) \
	do { \
		for (int x = 0; x < 4; x++) { \
			COPY_4X1_LINE(dst + pitch * x, src); \
			src += 4; \
		} \
		dst += 4; \
	} while (0)

// Copy a 4x4 pixel block from a different place in the framebuffer

#define COPY_4X4(dst2, dst, pitch) \
	do { \
		for (int x = 0; x < 4; x++) { \
			COPY_4X1_LINE(dst + pitch * x, dst2 + pitch * x); \
		} \
		dst += 4; \
	} while (0)

void Codec37Decoder::proc1(byte *dst, const byte *src, int32 nextOffs, int bw, int bh, int pitch, int16 *offset_table) {
	byte code;
	bool filling, skipCode;
	int32 len;
	int i, p;
	uint32 pitches[16];

	i = bw;
	for (p = 0; p < 16; ++p) {
		pitches[p] = (p >> 2) * pitch + (p & 0x3);
	}
	code = 0;
	filling = false;
	len = -1;
	while (1) {
		if (len < 0) {
			filling = (*src & 1) == 1;
			len = *src++ >> 1;
			skipCode = false;
		} else {
			skipCode = true;
		}
		if (!filling || !skipCode) {
			code = *src++;
			if (code == 0xFF) {
				--len;
				for (p = 0; p < 0x10; ++p) {
					if (len < 0) {
						filling = (*src & 1) == 1;
						len = *src++ >> 1;
						if (filling) {
							code = *src++;
						}
					}
					if (filling) {
						*(dst + pitches[p]) = code;
					} else {
						*(dst + pitches[p]) = *src++;
					}
					--len;
				}
				dst += 4;
				--i;
				if (i == 0) {
					dst += pitch * 3;
					--bh;
					if (bh == 0) return;
					i = bw;
				}
				continue;
			}
		}
		byte *dst2 = dst + offset_table[code] + nextOffs;
		COPY_4X4(dst2, dst, pitch);
		--i;
		if (i == 0) {
			dst += pitch * 3;
			--bh;
			if (bh == 0) return;
			i = bw;
		}
		--len;
	}
}

void Codec37Decoder::proc3WithFDFE(byte *dst, const byte *src, int32 nextOffs, int bw, int bh, int pitch, int16 *offset_table) {
	do {
		int32 i = bw;
		do {
			int32 code = *src++;
			if (code == 0xFD) {
				LITERAL_4X4(src, dst, pitch);
			} else if (code == 0xFE) {
				LITERAL_4X1(src, dst, pitch);
			} else if (code == 0xFF) {
				LITERAL_1X1(src, dst, pitch);
			} else {
				byte *dst2 = dst + _offsetTable[code] + nextOffs;
				COPY_4X4(dst2, dst, pitch);
			}
		} while (--i);
		dst += pitch * 3;
	} while (--bh);
}

void Codec37Decoder::proc3WithoutFDFE(byte *dst, const byte *src, int32 nextOffs, int bw, int bh, int pitch, int16 *offset_table) {
	do {
		int32 i = bw;
		do {
			int32 code = *src++;
			if (code == 0xFF) {
				LITERAL_1X1(src, dst, pitch);
			} else {
				byte *dst2 = dst + _offsetTable[code] + nextOffs;
				COPY_4X4(dst2, dst, pitch);
			}
		} while (--i);
		dst += pitch * 3;
	} while (--bh);
}

void Codec37Decoder::proc4WithFDFE(byte *dst, const byte *src, int32 nextOffs, int bw, int bh, int pitch, int16 *offset_table) {
	do {
		int32 i = bw;
		do {
			int32 code = *src++;
			if (code == 0xFD) {
				LITERAL_4X4(src, dst, pitch);
			} else if (code == 0xFE) {
				LITERAL_4X1(src, dst, pitch);
			} else if (code == 0xFF) {
				LITERAL_1X1(src, dst, pitch);
			} else if (code == 0x00) {
				int32 length = *src++ + 1;
				for (int32 l = 0; l < length; l++) {
					byte *dst2 = dst + nextOffs;
					COPY_4X4(dst2, dst, pitch);
					i--;
					if (i == 0) {
						dst += pitch * 3;
						bh--;
						i = bw;
					}
				}
				if (bh == 0) {
					return;
				}
				i++;
			} else {
				byte *dst2 = dst + _offsetTable[code] + nextOffs;
				COPY_4X4(dst2, dst, pitch);
			}
		} while (--i);
		dst += pitch * 3;
	} while (--bh);
}

void Codec37Decoder::proc4WithoutFDFE(byte *dst, const byte *src, int32 nextOffs, int bw, int bh, int pitch, int16 *offset_table) {
	do {
		int32 i = bw;
		do {
			int32 code = *src++;
			if (code == 0xFF) {
				LITERAL_1X1(src, dst, pitch);
			} else if (code == 0x00) {
				int32 length = *src++ + 1;
				for (int32 l = 0; l < length; l++) {
					byte *dst2 = dst + nextOffs;
					COPY_4X4(dst2, dst, pitch);
					i--;
					if (i == 0) {
						dst += pitch * 3;
						bh--;
						i = bw;
					}
				}
				if (bh == 0) {
					return;
				}
				i++;
			} else {
				byte *dst2 = dst + _offsetTable[code] + nextOffs;
				COPY_4X4(dst2, dst, pitch);
			}
		} while (--i);
		dst += pitch * 3;
	} while (--bh);
}

void Codec37Decoder::decode(byte *dst, const byte *src) {
	int32 bw = (_width + 3) / 4, bh = (_height + 3) / 4;
	int32 pitch = bw * 4;

	int16 seq = READ_LE_UINT16(src + 2);
	int32 decodedSize = READ_LE_UINT32(src + 4);
	byte maskFlags = src[12];
	makeTable(pitch, src[1]);
	int32 tmp;

	switch (src[0]) {
	case 0:
		if ((_deltaBufs[_curTable] - _deltaBuf) > 0) {
			memset(_deltaBuf, 0, _deltaBufs[_curTable] - _deltaBuf);
		}
		tmp = (_deltaBuf + _deltaSize) - _deltaBufs[_curTable] - decodedSize;
		if (tmp > 0) {
			memset(_deltaBufs[_curTable] + decodedSize, 0, tmp);
		}
		memcpy(_deltaBufs[_curTable], src + 16, decodedSize);
		break;
	case 1:
		if ((seq & 1) || !(maskFlags & 1)) {
			_curTable ^= 1;
		}
		proc1(_deltaBufs[_curTable], src + 16, _deltaBufs[_curTable ^ 1] - _deltaBufs[_curTable],
										bw, bh, pitch, _offsetTable);
		break;
	case 2:
		bompDecodeLine(_deltaBufs[_curTable], src + 16, decodedSize);
		if ((_deltaBufs[_curTable] - _deltaBuf) > 0) {
			memset(_deltaBuf, 0, _deltaBufs[_curTable] - _deltaBuf);
		}
		tmp = (_deltaBuf + _deltaSize) - _deltaBufs[_curTable] - decodedSize;
		if (tmp > 0) {
			memset(_deltaBufs[_curTable] + decodedSize, 0, tmp);
		}
		break;
	case 3:
		if ((seq & 1) || !(maskFlags & 1)) {
			_curTable ^= 1;
		}

		if ((maskFlags & 4) != 0) {
			proc3WithFDFE(_deltaBufs[_curTable], src + 16,
										_deltaBufs[_curTable ^ 1] - _deltaBufs[_curTable], bw, bh,
										pitch, _offsetTable);
		} else {
			proc3WithoutFDFE(_deltaBufs[_curTable], src + 16,
										_deltaBufs[_curTable ^ 1] - _deltaBufs[_curTable], bw, bh,
										pitch, _offsetTable);
		}
		break;
	case 4:
		if ((seq & 1) || !(maskFlags & 1)) {
			_curTable ^= 1;
		}

		if ((maskFlags & 4) != 0) {
			proc4WithFDFE(_deltaBufs[_curTable], src + 16,
										_deltaBufs[_curTable ^ 1] - _deltaBufs[_curTable], bw, bh,
										pitch, _offsetTable);
		} else {
			proc4WithoutFDFE(_deltaBufs[_curTable], src + 16,
										_deltaBufs[_curTable ^ 1] - _deltaBufs[_curTable], bw, bh,
										pitch, _offsetTable);
		}
		break;
	default:
		break;
	}
	_prevSeqNb = seq;

	memcpy(dst, _deltaBufs[_curTable], _frameSize);
}

void Codec37Decoder::bompDecodeLine(byte *dst, const byte *src, int len) {
	while (len > 0) {
		byte code = *src++;
		byte num = (code >> 1) + 1;

		if (num > len)
			num = len;

		len -= num;

		if (code & 1) {
			byte color = *src++;
			memset(dst, color, num);
		} else {
			memcpy(dst, src, num);
			src += num;
		}

		dst += num;
	}
}
