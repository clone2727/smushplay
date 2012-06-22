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

#include <SDL.h>
#include <stdio.h>

#include "graphicsman.h"

// TODO: aspect ratio correction option
// TODO: resize/scaling option

GraphicsManager::GraphicsManager() {
	_mainScreen = 0;
	_workingScreen = 0;
}

GraphicsManager::~GraphicsManager() {
	if (_workingScreen)
		SDL_FreeSurface(_workingScreen);
}

bool GraphicsManager::init(uint width, uint height, bool isHighColor) {
	// Try 32bpp first
	_mainScreen = SDL_SetVideoMode(width, height, 32, SDL_SWSURFACE);

	// Fall back on 16bpp
	if (!_mainScreen)
		_mainScreen = SDL_SetVideoMode(width, height, 16, SDL_SWSURFACE);

	if (!_mainScreen)
		return false;

	if (isHighColor)
		_workingScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16, 0xF800, 0x7E0, 0x1F, 0);
	else
		_workingScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 8, 0, 0, 0, 0);

	return true;
}

void GraphicsManager::setPalette(const byte *ptr, uint start, uint count) {
	if (_workingScreen->format->BitsPerPixel != 8 || count == 0 || !ptr || start + count > 256)
		return;

	SDL_Color *colors = new SDL_Color[count];

	for (uint i = 0; i < count; i++) {
		colors[i].r = ptr[i * 3];
		colors[i].g = ptr[i * 3 + 1];
		colors[i].b = ptr[i * 3 + 2];
	}

	SDL_SetColors(_workingScreen, colors, start, count);
	delete[] colors;
}

void GraphicsManager::blit(const byte *ptr, uint x, uint y, uint width, uint height, uint pitch) {
	if (width == 0 || height == 0)
		return;

	if (x + width > (uint)_workingScreen->w)
		width = _workingScreen->w - x;

	if (y + height > (uint)_workingScreen->h)
		height = _workingScreen->h - y;

	SDL_LockSurface(_workingScreen);

	for (uint i = 0; i < height; i++)
		memcpy((byte *)_workingScreen->pixels + (i + y) * _workingScreen->pitch + x, ptr + i * pitch, width * _workingScreen->format->BytesPerPixel);

	SDL_UnlockSurface(_workingScreen);
}

void GraphicsManager::update() {
	// Blit the 8bpp surface to the main screen
	SDL_BlitSurface(_workingScreen, 0, _mainScreen, 0);

	// Then update the whole screen
	SDL_UpdateRect(_mainScreen, 0, 0, _mainScreen->w, _mainScreen->h);
}
