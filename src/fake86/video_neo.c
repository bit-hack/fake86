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

#include "common.h"

#include "../80x86/cpu.h"


// text mode layout:
//  [[char], [attr]], [[char], [attr]], ...


// cga has 16kb ram at 0xB8000 for framebuffer
// frame buffer is incompletely decoded and is mirrored at 0xBC000
// text mode page is either 2k bytes (40x25x2) or 4k bytes (80x25x2)
#define MAX_PAGES 16

enum system_t {
  video_mda,
  video_cga,
  video_ega,
  video_vga,
};

enum mode_t {
  mode_text,
  mode_graphics,
};

struct cursor_t {
  uint32_t x, y;
  uint8_t size;
};

// cursor per page
static struct cursor_t _cursor[MAX_PAGES];

// current video mode
static uint8_t _video_mode = 0x00;
static enum system_t _system = video_mda;
static enum system_t _mode = mode_text;
// screen resolution
static uint32_t _width = 320, _height = 240;
// text mode rows and columns
static uint32_t _rows = 25, _cols = 40;
// display pages
static uint32_t _pages = 8;
// video memory base
static uint32_t _base = 0xB8000;
//
static uint8_t _active_page = 0;

static bool no_blanking = false;


// CRTC (6845) address register
static uint8_t crt_reg_addr = 0;

// CRTC (6845) data registers
//
// 0 - horz. total
// 1 - horz. displayed
// 2 - horz. sync pos
// 3 - horz. & vert. sync widths
// 4 - vert. total
// 5 - vert. total adjust
// 6 - vert. displayed
// 7 - vert. sync pos
// 8 - interlace and skew
// 9 - max raster address
// 12 - display start address hi
// 13 - display start address lo
// 14 - cursor address hi
// 15 - cursor address lo
//
static uint8_t crt_register[32];

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// EGA/VGA
uint8_t neo_mem_read_A0000(uint32_t addr) {
  return RAM[addr];
}

// EGA/VGA
void neo_mem_write_A0000(uint32_t addr, uint8_t value) {
  RAM[addr] = value;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ports 03B0-03BF

// 5 - blinking     (1 enable, 0 disable)
// 3 - video output (1 enable, 0 disable)
// 1 - black and white
// 0 - high res mode
static uint8_t mda_control = 0;

// 3 - 1 if currently drawing something bright
// 0 - horz. retrace (1 true, 0 false)
static uint8_t mda_status = 0;

static uint8_t mda_port_read(uint16_t portnum) {
  if (portnum >= 0x03B0 && portnum <= 0x03B7) {
    if (portnum & 1) {
      return crt_register[crt_reg_addr];
    } else {
      // write only but lets return it anyway
      return crt_reg_addr;
    }
  }
  else {
    if (portnum == 0x03BA) {
      const uint8_t port_3da = vga_timing_get_3da();
      mda_status = (port_3da & 1) ? 1 : 0;
      return mda_status | 0xf0;
    }
  }
  return 0;
}

static void mda_port_write(uint16_t portnum, uint8_t value) {
  if (portnum >= 0x03B0 && portnum <= 0x03B7) {
    if (portnum & 1) {
      crt_register[crt_reg_addr] = value;
    } else {
      crt_reg_addr = value & 0x1f;
    }
  }
  if (portnum == 0x03b8) {
    mda_control = value;
  }
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ports 03C0-03CF

static uint8_t ega_port_read(uint16_t portnum) {
  return portram[portnum];
}

static void ega_port_write(uint16_t portnum, uint8_t value) {
  portram[portnum] = value;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ports 03D0-03DF

static uint8_t cga_control = 0;
static uint8_t cga_palette = 0;

static uint8_t cga_port_read(uint16_t portnum) {

  if (portnum >= 0x03d0 && portnum <= 0x03d7) {
    if (portnum & 1) {
      return crt_register[crt_reg_addr];
    } else {
      return crt_reg_addr;
    }
  }

  switch (portnum) {
  case 0x3d8: // mode control register
    return cga_control;
  case 0x3d9: // colour control register
    return cga_palette;
  case 0x3da:
    return portram[0x3da] = vga_timing_get_3da();
  default:
    printf("%04x r\n", (int)portnum);
  }
  return portram[portnum];
}

static void cga_port_write(uint16_t portnum, uint8_t value) {
  if (portnum >= 0x03d0 && portnum <= 0x03d7) {
    if (portnum & 1) {
      crt_register[crt_reg_addr] = value;
    } else {
      crt_reg_addr = value & 0x1f;
    }
  }
  else {
    switch (portnum) {
    case 0x3d8:
      cga_control = value;
      break;
    case 0x3d9:
      cga_palette = value;
      break;
    default:
      printf("%04x w %02x\n", (int)portnum, (int)value);
      portram[portnum] = value;
    }
  }
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// update neo display adapter
void neo_tick(uint64_t cycles) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static void _clear_text_buffer(void) {

  const uint32_t mem_size = 1024 * 16;

  for (int i=0; i<mem_size; i+=2) {
    RAM[0xB8000 + i + 0] = 0x0;
    RAM[0xB8000 + i + 1] = 0x0;
  }
}

static void neo_set_video_mode(uint8_t al) {

  log_printf(LOG_CHAN_VIDEO, "set video mode to %02Xh", (int)al);

  // check for no blanking
  no_blanking = ((cpu_regs.al & 0x80) != 0);
  al &= 0x7f;

  // text mode columns and rows
  switch (al) {
  case 0x00:
  case 0x01:
  case 0x04:
  case 0x05:
  case 0x0D:
  case 0x13: _cols = 40; _rows = 25; break;
  case 0x02:
  case 0x03:
  case 0x06:
  case 0x07:
  case 0x0E:
  case 0x0F:
  case 0x10: _cols = 80; _rows = 25; break;
  case 0x11:
  case 0x12: _cols = 80; _rows = 30; break;
  }
  // pixel resolution
  switch (al) {
  case 0x04:
  case 0x05:
  case 0x0D:
  case 0x13: _width = 320; _height = 200; break;
  case 0x06:
  case 0x0E: _width = 640; _height = 200; break;
  case 0x0F:
  case 0x10: _width = 640; _height = 350; break;
  case 0x11:
  case 0x12: _width = 640; _height = 480; break;
  }
  // memory base
  if (al >= 0x00 && al <= 0x07) {
    _base = 0xB8000;
    _clear_text_buffer();
  }
  if (al >= 0x0D && al <= 0x13) {
    _base = 0xA0000;
  }

  _video_mode = al;
}

// set cursor shape
static void do_int10_01(void) {
}

// set cursor position
static void do_int10_02(void) {
  const int page = cpu_regs.bh;
  assert(page <= MAX_PAGES);
  struct cursor_t *c = &_cursor[page];
  c->x = cpu_regs.dl;
  c->y = cpu_regs.dh;
}

// get cursor mode and shape
static void do_int10_03(int page_num) {
  const int page = cpu_regs.bh;
  assert(page <= MAX_PAGES);
  struct cursor_t *c = &_cursor[page];
  cpu_regs.ax = 0;
  cpu_regs.ch = 0; // start scanline
  cpu_regs.cl = 0; // end scanline
  cpu_regs.dh = c->y;
  cpu_regs.dl = c->x;
}

// select active display page 
static void do_int10_05(void) {
  switch (cpu_regs.al) {
  case 0x81: // cpu page regs
  case 0x82: // crt page regs
  case 0x83: // both
    break;
  }
}

// scroll window up
static void do_int10_06(void) {
}

// scroll window down
static void do_int10_07(void) {
}

// read character and attribute at cursor position
static void do_int10_08(void) {
}

// write character and attribute at cursor position 
static void do_int10_09(void) {
}

// write character only at cursor position
static void do_int10_0A(void) {
}

// teletype output
static void do_int10_0E(void) {
}

static void do_int10_0F(void) {
  cpu_regs.ah = _cols;
  cpu_regs.al = _video_mode | (no_blanking ? 0x80 : 0x00);
  cpu_regs.bh = _active_page;
}

// write to dac registers (vga) or Alternate Select (ega)?
static void do_int10_12(void) {
}

// write string (EGA+)
static void do_int10_13(void) {
}

// get/set display combination
static void do_int10_1AXX(void) {
}

static void do_int10_30XX(void) {
  cpu_regs.cx = 0;
  cpu_regs.dx = 0;
}

// BIOS int 10h Video Services handler
bool neo_int10_handler(void) {
  switch (cpu_regs.ah) {
  case 0x00:
    neo_set_video_mode(cpu_regs.al);
    // must return false
    return false;
  }
  return false;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// initalize neo display adapter
bool neo_init(void) {

  // mda
  set_port_read_redirector(0x3B0, 0x3BF, mda_port_read);
  set_port_write_redirector(0x3B0, 0x3BF, mda_port_write);

  // ega
  set_port_read_redirector(0x3C0, 0x3CF, ega_port_read);
  set_port_write_redirector(0x3C0, 0x3CF, ega_port_write);

  // cga
  set_port_read_redirector(0x3D0, 0x3DF, cga_port_read);
  set_port_write_redirector(0x3D0, 0x3DF, cga_port_write);

  return true;
}

int neo_get_video_mode(void) {
  return _video_mode;
}
