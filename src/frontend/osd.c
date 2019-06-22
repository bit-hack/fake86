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
#include "../cpu/cpu.h"
#include "../video/video.h"

#include "../external/udis86/udis86.h"


static uint32_t _last_disk_tick;
static bool _is_active;

#define WIDTH  80
#define HEIGHT 29

// text screen buffer
static char _buf[WIDTH * HEIGHT];
static uint32_t _buf_head;

uint8_t _cmd_buf_last[WIDTH];
uint8_t _cmd_buf[WIDTH];
uint32_t _cmd_head;

static char *_get_line(uint32_t index) {
  const uint32_t y = (index + _buf_head) % HEIGHT;
  return _buf + y * WIDTH;
}

static void _new_line(void) {
  ++_buf_head;
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
  memset(_cmd_buf, ' ', WIDTH);
  memset(_cmd_buf_last, ' ', WIDTH);
  _cmd_head = 0;
}

void osd_close(void) {
  _is_active = false;
}

static int _parse(const char *line, char out[32][32]) {
  char prev = ' ';
  int item = -1;
  int pos = 0;
  for (int i=0; i<WIDTH && item < 32; ++i) {
    const char ch = line[i];
    if (prev == ' ' && ch != ' ') {
      if (item >= 0) {
        out[item][pos++] = '\0';
      }
      ++item;
      pos = 0;
    }
    if (ch != ' ') {
      assert(item >= 0);
      out[item][pos++] = ch;
    }
    prev = ch;
  }
  return item + 1;
}

static bool _pstrcmp(const char *input, const char *word) {
  for (;; ++input, ++word) {
    if (*word == '\0')
      return true;
    if (*input != *word)
      return false;
  }
  UNREACHABLE();
}

static void _on_cmd_exit(int num, const char **tokens) {
  osd_printf("cpu being stopped");
  cpu_running = false;
}

static bool _drive_num(const char *token, uint8_t *out) {
  if (_pstrcmp(token, "fd")) {
    *out = atoi(token + 2);
    return true;
  }
  if (_pstrcmp(token, "hd")) {
    *out = atoi(token + 2) + 0x80;
    return true;
  }
  osd_printf("'%s' is not a valid drive specifier", token);
  return false;
}

static void _on_cmd_disk_eject(int num, const char **tokens) {
  if (num <= 0) {
    osd_printf("usage: disk eject [fd0,hd0...]");
    return;
  }
  uint8_t drive = 0;
  if (_drive_num(tokens[0], &drive)) {
    disk_eject(drive);
  }
}

static void _on_cmd_disk_insert(int num, const char **tokens) {
  if (num <= 1) {
    osd_printf("usage: disk insert [fd0,hd0...] [path]");
    return;
  }
  const char *path  = tokens[1];
  uint8_t drive = 0;
  if (_drive_num(tokens[0], &drive)) {
    disk_insert(drive, path);
  }
}

static void _on_cmd_disk_info(int num, const char **tokens) {
  // show which disks are inserted, etc
}

static void _on_cmd_disk(int num, const char **tokens) {
  if (num <= 0) {
    return;
  }
  const char *tok = *tokens;
  switch (*tok) {
  case 'e':
    if (_pstrcmp(tok, "eject")) {
      _on_cmd_disk_eject(num - 1, tokens + 1);
    }
    break;
  case 'i':
    if (_pstrcmp(tok, "insert")) {
      _on_cmd_disk_insert(num - 1, tokens + 1);
    }
    if (_pstrcmp(tok, "info")) {
      _on_cmd_disk_info(num - 1, tokens + 1);
    }
    break;
  default:
    osd_printf("unexpected input '%s'", tok);
    break;
  }}

static void _on_cmd_cpu(int num, const char **tokens) {
  if (num <= 0) {
    return;
  }
  const char *tok = *tokens;
  switch (*tok) {
  case 'r':
    if (_pstrcmp(tok, "reset")) {
      cpu_reset();
    }
    if (_pstrcmp(tok, "run")) {
      // start execution
      cpu_step = false;
      cpu_halt = false;
    }
    break;
  case 'h':
    if (_pstrcmp(tok, "halt")) {
      // stop execution
      cpu_halt = true;
      cpu_step = false;
    }
    break;
  case 's':
    if (_pstrcmp(tok, "state")) {
      osd_printf("cs %04x   ip %04x  (%06x)",
                 cpu_regs.cs, cpu_regs.ip, (cpu_regs.cs << 4) + cpu_regs.ip);
      osd_printf("ss %04x   sp %04x  (%06x)   bp %04x  (%06x)",
                 cpu_regs.ss,
                 cpu_regs.sp, (cpu_regs.ss << 4) + cpu_regs.sp,
                 cpu_regs.bp, (cpu_regs.ss << 4) + cpu_regs.bp);

      osd_printf("ds %04x   si %04x   di %04x",
                 cpu_regs.ds, cpu_regs.si, cpu_regs.di);
      osd_printf("es %04x   ax %04x   bx %04x   cx %04x   dx %04x",
                 cpu_regs.es, cpu_regs.ax, cpu_regs.bx, cpu_regs.cx, cpu_regs.dx);

      osd_printf("....%c%c%c%c%c%c.%c.%c.%c",
                 cpu_flags.of  ? 'o' : '.',
                 cpu_flags.df  ? 'd' : '.',
                 cpu_flags.ifl ? 'i' : '.',
                 cpu_flags.tf  ? 't' : '.',
                 cpu_flags.sf  ? 's' : '.',
                 cpu_flags.zf  ? 'z' : '.',
                 cpu_flags.af  ? 'a' : '.',
                 cpu_flags.pf  ? 'p' : '.',
                 cpu_flags.cf  ? 'c' : '.');
    }
    if (_pstrcmp(tok, "step")) {
      // single step
      cpu_step = true;
      cpu_halt = true;
    }
    break;
  default:
    osd_printf("unexpected input '%s'", tok);
    break;
  }
}

static void _on_cmd_memory_dis(int num, const char **tokens) {
  uint32_t ip = 0xFFFFF & (cpu_regs.cs << 4) + cpu_regs.ip;
  const uint8_t *code = RAM + ip;

  ud_t ud_obj;
  ud_init(&ud_obj);
  ud_set_input_buffer(&ud_obj, code, 16);
  ud_set_mode(&ud_obj, 16);
  ud_set_syntax(&ud_obj, UD_SYN_INTEL);

  for (int i = 0; i < 8; ++i) {
    if (!ud_disassemble(&ud_obj)) {
      osd_printf("unable to disassemble");
      break;
    }
    osd_printf("%06x    %s", ip, ud_insn_asm(&ud_obj));
    ip += ud_obj.pc;
  }

}

static void _on_cmd_memory_dump(int num, const char **tokens) {
  if (num <= 0) {
    osd_printf("usage: memory dump [path]");
    return;
  }
  const char *path  = tokens[0];
  mem_dump(path);
}

static void _on_cmd_memory(int num, const char **tokens) {
  if (num <= 0) {
    return;
  }
  const char *tok = *tokens;
  switch (*tok) {
  case 'd':
    if (_pstrcmp(tok, "dump")) {
      _on_cmd_memory_dump(num - 1, tokens + 1);
    }
    if (_pstrcmp(tok, "dis")) {
      _on_cmd_memory_dis(num - 1, tokens + 1);
    }
    break;
  case 'r':
    if (_pstrcmp(tok, "read")) {
      osd_printf("doing memory read");
    }
    break;
  default:
    osd_printf("unexpected input '%s'", tok);
    break;
  }
}

static void _on_cmd_state(int num, const char **tokens) {
  if (num <= 0) {
    return;
  }

  const char *tok  = tokens[0];
  const char *path = tokens[1];

  switch (*tok) {
  case 'l':
    if (_pstrcmp(tok, "load")) {
      state_load(path);
      osd_printf("state loaded");
    }
    break;
  case 's':
    if (_pstrcmp(tok, "save")) {
      state_save(path);
      osd_printf("state saved");
    }
    break;
  default:
    osd_printf("unexpected input '%s'", tok);
    break;
  }
}

// root level command handler
static void _on_cmd(int num, const char **tokens) {
  if (num <= 0) {
    return;
  }
  const char *tok = *tokens;
  switch (*tok) {
  case 'c':
    if (_pstrcmp(tok, "cpu")) {
      _on_cmd_cpu(num-1, tokens+1);
    }
    break;
  case 'd':
    if (_pstrcmp(tok, "disk")) {
      _on_cmd_disk(num-1, tokens+1);
    }
  case 'e':
    if (_pstrcmp(tok, "exit")) {
      _on_cmd_exit(num-1, tokens+1);
    }
    break;
  case 'm':
    if (_pstrcmp(tok, "memory")) {
      _on_cmd_memory(num-1, tokens+1);
    }
    break;
  case 's':
    if (_pstrcmp(tok, "state")) {
      _on_cmd_state(num - 1, tokens + 1);
    }
    break;
  default:
    osd_printf("unexpected input '%s'", tok);
    break;
  }
}

static void _exec(void) {

  // store last command
  memcpy(_cmd_buf_last, _cmd_buf, WIDTH);

  // put the executed line in the output
  memcpy(_get_line(0), _cmd_buf, WIDTH);
  _new_line();

  // parse executed line into tokens
  char tokens[32][32];
  memset(tokens, 0, sizeof(tokens));
  const int num = _parse(_cmd_buf, tokens);

  char *input[32];
  for (int i = 0; i < 32; ++i) {
    input[i] = tokens[i];
  }
  _on_cmd(num, input);

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

    if (t->key.keysym.sym == SDLK_UP) {
      // restore previous command
      memcpy(_cmd_buf, _cmd_buf_last, WIDTH);
      for (int i = 0; i < WIDTH; ++i) {
        _cmd_head = (_cmd_buf[i] != ' ') ? (i+1) : _cmd_head;
      }
    }

    if (t->key.keysym.sym == SDLK_DOWN) {
      // clear current command
      memset(_cmd_buf, 0, WIDTH);
      _cmd_head = 0;
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
    const uint8_t *ch = (const uint8_t*)_get_line(y);
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
  char *line = _get_line(0);
  memset(line, 0, WIDTH);
  vsnprintf(line + 2, WIDTH - 2, fmt, list);
  ++_buf_head;
}

void osd_printf(const char *fmt, ...) {
  va_list list;
  va_start(list, fmt);
  osd_vprintf(fmt, list);
  va_end(list);
}
