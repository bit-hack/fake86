/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
  USA.
*/

#include "../fake86/common.h"

static SDL_Surface *_surface;

// hack for just now
extern SDL_Surface *screen;


bool neo_render_init() {

  _surface = SDL_SetVideoMode(640, 480, 32, 0);
  if (!_surface) {
    log_printf(LOG_CHAN_VIDEO, "SDL_SetVideoMode failed");
    return false;
  }

  SDL_WM_SetCaption(BUILD_STRING, NULL);

  // hack for now
  screen = _surface;

  return true;
}

void neo_render_tick() {

  // 12C000 bytes max

  memcpy(_surface->pixels, RAM + 0xA0000, 0xC0000);

  SDL_Flip(_surface);
}
