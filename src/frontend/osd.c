/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms cpu_flags.of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  cpu_flags.of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty cpu_flags.of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy cpu_flags.of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
  USA.
*/

#include "frontend.h"
#include "../video/video.h"


// TODO: just render the cmd buffer under the ring buffer it will be simpler


static uint32_t _last_disk_tick;
static bool _is_active;

#define WIDTH  80
#define HEIGHT 29

// text screen buffer
static char _buffer[WIDTH * HEIGHT];
static uint32_t _head;

void _new_line(void) {
  ++_head;
  const uint32_t y = (_head - 2) % HEIGHT;
  memset(_buffer + y * WIDTH, 0, WIDTH);
}

void osd_disk_fdd_used(void) {
  _last_disk_tick = SDL_GetTicks();
}

void osd_disk_hdd_used(void) {
  _last_disk_tick = SDL_GetTicks();
}

bool osd_should_draw_disk(void) {
  return (SDL_GetTicks() - _last_disk_tick) < 250;
}

bool osd_is_active(void) {
  return _is_active;
}

void osd_open(void) {
  _is_active = true;

  char *src = _buffer;
#if 0
  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      *(src++) = rand() & 0xff;
    }
  }
#endif
}

void osd_close(void) {
  _is_active = false;
}

uint8_t _cmd_buf[WIDTH];
uint32_t _cmd_head;

static void _exec(void) {
  _new_line();
}

static void _buf_on_char(const uint8_t ch) {
  _cmd_buf[_cmd_head] = ch;
  _cmd_head += (_cmd_head < (WIDTH-1));
}

static void _buf_on_del() {
  if (_cmd_head > 0) {
    --_cmd_head;
    _cmd_buf[_cmd_head] = ' ';
  }
}

void osd_on_event(const SDL_Event *t) {
  assert(t);
  if (t->type == SDL_KEYDOWN) {

    const uint8_t ch = t->key.keysym.sym;
    if (ch <= 0x7f && ch >= 0x20) {
      // printable ascii
      _buf_on_char(ch);
    }

    if (t->key.keysym.sym == SDLK_BACKSPACE) {
      _buf_on_del();
    }

    if (t->key.keysym.sym == SDLK_RETURN) {
      // execute command
      _exec();
    }

    if (t->key.keysym.sym == SDLK_F12) {
      osd_close();
    }
  }
}

void osd_render(const struct render_target_t *target) {
  if (!_is_active) {
    return;
  }

  uint32_t *dst = target->dst;
  const uint32_t offs = (target->h - ((HEIGHT + 1) * 16)) / 2;
  dst += offs;

  for (int y = 0; y < HEIGHT; ++y) {
    uint32_t *dstx = dst;
    const uint32_t index = ((y + _head) % HEIGHT) * WIDTH;
    const uint8_t *ch = _buffer + index;
    for (int x = 0; x < WIDTH; ++x) {
      font_draw_glyph_8x16_gliss(dstx, target->pitch, ch[x], ~0);
      dstx += 8;
    }
    dst += target->pitch * 16;
    ch += WIDTH;
  }
#if 1
  // render the cmd buffer
  for (int x = 0; x < WIDTH; ++x) {
    font_draw_glyph_8x16_gliss(dst + x*8, target->pitch, _cmd_buf[x], ~0);
  }
#endif
}
