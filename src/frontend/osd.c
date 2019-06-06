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


static uint32_t _last_disk_tick;
static bool _is_active;

#define WIDTH  80
#define HEIGHT 29

// text screen buffer
static char _buffer[WIDTH * HEIGHT];
static uint32_t _head;

static char *_get_line(uint32_t index) {
  const uint32_t y = (index + _head) % HEIGHT;
  return _buffer + y * WIDTH;
}

static void _new_line(void) {
  ++_head;
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
}

void osd_close(void) {
  _is_active = false;
}

uint8_t _cmd_buf[WIDTH];
uint32_t _cmd_head;

static int _parse(const char *line, const char *out[32]) {
  memset(out, 0, sizeof(char*) * 32);
  int head = 0;
  char prev = ' ';
  for (int i=0; i<WIDTH && head < 32; ++i) {
    if (prev == ' ' && line[i] != ' ') {
      out[head] = line + i;
      ++head;
    }
  }
  return head;
}

static bool _pstrcmp(const char *input, const char *word) {
  for (;*input; ++input, ++word) {
    if (*word == '\0')
      return true;
    if (*input != *word)
      return false;
  }
}

// root level command handler
static void _on_cmd(int level, int num, const char *tokens[32]) {
  if (num <= level) {
    return;
  }
  const char *t0 = tokens[0];
  switch (*t0) {
  case 'c':
    if (_pstrcmp(t0, "cpu")) {
      osd_printf("_on_cmd_cpu()");
    }
    break;
  case 'd':
    if (_pstrcmp(t0, "disk")) {
      osd_printf("_on_cmd_disk()");
    }
    break;
  case 'e':
    if (_pstrcmp(t0, "exit")) {
      osd_printf("_on_cmd_exit()");
    }
    break;
  case 'm':
    if (_pstrcmp(t0, "memory")) {
      osd_printf("_on_cmd_memory()");
    }
    break;
  case 's':
    if (_pstrcmp(t0, "state")) {
      osd_printf("_on_cmd_state()");
    }
    break;
  default:
    osd_printf("unexpected token '%s'", t0);
    break;
  }
}

static void _exec(void) {

  // put the executed line in the output
  memcpy(_get_line(0), _cmd_buf, WIDTH);
  _new_line();

  // parse executed line into tokens
  const char *tokens[32];
  const int num = _parse(_cmd_buf, tokens);

  _on_cmd(0, num, tokens);

  // last act clear the command buffer (NOT BEFORE)
  memset(_cmd_buf, 0, WIDTH);
  _cmd_head = 0;
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

static uint32_t _get_ascii(const SDL_Event *t) {
  uint32_t ch = t->key.keysym.sym;
  // very crude caps
  if (SDL_GetKeyState(NULL)[SDLK_LSHIFT]) {
    if (ch >= 0x60 && ch <= 0x7f) {
      return ch - 0x20;
    }
    switch (ch) {
    case '1':  return '!';
    case '2':  return '\"';
    case '3':  return 35;
    case '4':  return '$';
    case '5':  return '%';
    case '6':  return '^';
    case '7':  return '&';
    case '8':  return '*';
    case '9':  return '(';
    case '0':  return ')';
    case '[':  return '{';
    case ']':  return '}';
    case '\'': return '@';
    case '#':  return '~';
    case '/':  return '?';
    case '.':  return '>';
    case ',':  return '<';
    case '\\': return '|';
    case '`':  return '~';
    }
  }
  return ch;
}

void osd_on_event(const SDL_Event *t) {
  assert(t);
  if (t->type == SDL_KEYDOWN) {

    const uint32_t ch = _get_ascii(t);

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
  }
#if 1
  // render the cmd buffer
  for (int x = 0; x < WIDTH; ++x) {
    font_draw_glyph_8x16_gliss(dst + x*8, target->pitch, _cmd_buf[x], ~0);
  }
#endif
}

void osd_vprintf(const char *fmt, va_list list) {
  char *line = _get_line(_head);
  memset(line, 0, WIDTH);
  vsnprintf(line, WIDTH, fmt, list);
  ++_head;
}

void osd_printf(const char *fmt, ...) {
  va_list list;
  va_start(list, fmt);
  osd_vprintf(fmt, list);
  va_end(list);
}
