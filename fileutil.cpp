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

#include "fileutil.h"

byte readByte(FILE *file) {
	byte b;
	fread(&b, 1, 1, file);
	return b;
}

uint16 readUint16LE(FILE *file) {
	uint16 x = readByte(file);
	return x | (readByte(file) << 8);
}

int16 readSint16LE(FILE *file) {
	return (int16)readUint16LE(file);
}

uint16 readUint16BE(FILE *file) {
	uint16 x = readByte(file) << 8;
	return x | readByte(file);
}

int16 readSint16BE(FILE *file) {
	return (int16)readUint16BE(file);
}

uint32 readUint32LE(FILE *file) {
	uint32 x = readUint16LE(file);
	return x | (readUint16LE(file) << 16);
}

int32 readSint32LE(FILE *file) {
	return (int32)readUint32LE(file);
}

uint32 readUint32BE(FILE *file) {
	uint32 x = readUint16BE(file) << 16;
	return x | readUint16BE(file);
}

int32 readSint32BE(FILE *file) {
	return (int32)readUint32BE(file);
}
