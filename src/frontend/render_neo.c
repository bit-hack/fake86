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

// see:
// http://www.minuszerodegrees.net/video/bios_video_modes.htm


#include "../fake86/common.h"
#include "../fake86/video.h"


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

// render a grey/black dither pattern
static void _neo_render_mode_unknown(void) {
  uint32_t *dsty = (uint32_t*)_surface->pixels;
  for (uint32_t y = 0; y < _surface->h; ++y) {
    uint32_t *dstx = dsty;
    for (uint32_t x = 0; x < _surface->w; ++x) {
      dstx[x] = (1 & (x ^ y)) ? 0x000000 : 0x808080;
     }
    dsty += (_surface->pitch) / sizeof(uint32_t);
  }
}

// 80x25 16-colour text mode
static void _neo_render_mode_3(void) {
  // text mode buffer address
  uint32_t src = 0xB8000;
  // cga/PCjr = 8x8  char px
  // EGA      = 8x14 char px
  // MCGA     = 8x16 char px
  // VGA      = 9x16 char px
  const int chw = 8, chh = 16;
  // step through VGA text-mode buffer
  const int rows = 25, cols = 80;
  // screen buffer position
  const uint32_t pitch = _surface->pitch / sizeof(uint32_t);
  uint32_t *dsty = (uint32_t*)_surface->pixels;
  dsty += pitch * ((_surface->h - (chh * rows)) / 2);
  // blit loop
  for (int y=0; y<rows; ++y) {
    uint32_t *dstx = dsty;
    for (int x=0; x<cols; ++x) {
      // grab character and attribute
      const uint8_t ch = RAM[src + 0];
      const uint8_t at = RAM[src + 1];
      // draw the glyph
      font_draw_glyph_8x16(dstx, pitch, ch, -1, 0);
      // step over to next glyph
      dstx += chw;
      // step over character and attribute
      src += 2;
    }
    // step over glyph line
    dsty += pitch * chh;
  }
}

void neo_render_tick(void) {
  switch (neo_get_video_mode()) {
  case 0x03:
    _neo_render_mode_3();
    break;
  default:
    _neo_render_mode_unknown();
    break;
  }
  SDL_Flip(_surface);
}
