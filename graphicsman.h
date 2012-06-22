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

#ifndef GRAPHICSMAN_H
#define GRAPHICSMAN_H

#include "types.h"

struct SDL_Surface;

class GraphicsManager {
public:
	GraphicsManager();
	~GraphicsManager();

	bool init(uint width, uint height, bool highColor);
	void blit(const byte *ptr, uint x, uint y, uint width, uint height, uint pitch);
	void update();
	void setPalette(const byte *ptr, uint start, uint count);

private:
	SDL_Surface *_mainScreen;
	SDL_Surface *_workingScreen;
};

#endif
