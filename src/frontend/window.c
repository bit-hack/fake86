/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers
               2019      Aidan Dodds

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

#include "frontend.h"


// params
bool do_fullscreen;
uint32_t frame_skip;

static SDL_Surface *_surface;
static uint32_t frame_index;


void win_fs_toggle(void) {
  assert(_surface);
  const int flags = _surface->flags ^ SDL_FULLSCREEN;
  _surface = SDL_SetVideoMode(_surface->w, _surface->h, 32, flags);
  if (!_surface) {
    log_printf(LOG_CHAN_VIDEO, "SDL_SetVideoMode failed");
  }
  SDL_WM_SetCaption(BUILD_STRING, NULL);
}

bool win_init(void) {

  const int flags =
    (do_fullscreen ? SDL_FULLSCREEN : 0);

  _surface = SDL_SetVideoMode(640, 480, 32, flags);
  if (!_surface) {
    log_printf(LOG_CHAN_VIDEO, "SDL_SetVideoMode failed");
    return false;
  }
  SDL_WM_SetCaption(BUILD_STRING, NULL);
  return true;
}

void win_render(void) {

  ++frame_index;
  if (frame_index >= frame_skip) {
    frame_index = 0;
  }

  if (frame_index != 0) {
    return;
  }

  SDL_FillRect(_surface, NULL, 0x050505);

  struct render_target_t target = {
    (uint32_t*)_surface->pixels,
    _surface->w,
    _surface->h,
    _surface->pitch / sizeof(uint32_t)
  };

  neo_render_tick(&target);
  osd_render(&target);

  SDL_Flip(_surface);
}

void win_size(uint32_t *w, uint32_t *h) {
  assert(w && h);
  *w = _surface->w;
  *h = _surface->h;
}