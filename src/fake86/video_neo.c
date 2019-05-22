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

// MDA
uint8_t neo_mem_read_B0000(uint32_t addr) {
  return RAM[addr];
}

// MDA
void neo_mem_write_B0000(uint32_t addr, uint8_t value) {
  RAM[addr] = value;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// CGA
uint8_t neo_mem_read_B8000(uint32_t addr) {
  return RAM[addr];
}

// CGA
void neo_mem_write_B8000(uint32_t addr, uint8_t value) {
  RAM[addr] = value;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ports 03B0-03BF

static uint8_t mda_port_read(uint16_t portnum) {
  return 0;
}

static void mda_port_write(uint16_t portnum, uint8_t value) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ports 03C0-03CF

static uint8_t ega_port_read(uint16_t portnum) {
  return 0;
}

static void ega_port_write(uint16_t portnum, uint8_t value) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ports 03D0-03DF

static uint8_t cga_port_read(uint16_t portnum) {
  return 0;
}

static void cga_port_write(uint16_t portnum, uint8_t value) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// update neo display adapter
void neo_tick(uint64_t cycles) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static void neo_set_video_mode(uint8_t al) {

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
  }
  if (al >= 0x0D && al <= 0x13) {
    _base = 0xA0000;
  }
}

// set cursor size
static do_int10_01() {
}

// set cursor position
static do_int10_02() {
}

// get cursor mode and shape
static do_int10_03(int page_num) {
  cpu_regs.ax = 0;
  cpu_regs.ch = 0;
  cpu_regs.cl = 0;
  cpu_regs.dh = 0;
  cpu_regs.dl = 0;
}

// select active display page 
static do_int10_05() {
  switch (cpu_regs.al) {
  case 0x81: // cpu page regs
  case 0x82: // crt page regs
  case 0x83: // both
    break;
  }
}

// scroll window up
static do_int10_06() {
}

// scroll window down
static do_int10_07() {
}

// read character and attribute at cursor position
static do_int10_08() {
}

// write character and attribute at cursor position 
static do_int10_09() {
}

// write character only at cursor position
static do_int10_0A() {
}

// teletype output
static do_int10_0E() {
}

static do_int10_0F() {
  cpu_regs.ah = _cols;
  cpu_regs.al = _video_mode | (no_blanking ? 0x80 : 0x00);
  cpu_regs.bh = _active_page;
}

// write to dac registers (vga) or Alternate Select (ega)?
static do_int10_12() {
}

// write string (EGA+)
static do_int10_13() {
}

// get/set display combination
static do_int10_1AXX() {
}

static do_int10_30XX() {
  cpu_regs.cx = 0;
  cpu_regs.dx = 0;
}

// BIOS int 10h Video Services handler
void neo_int10_handler() {
  switch (cpu_regs.ah) {
  case 0x00: neo_set_video_mode(cpu_regs.al); return;
  case 0x01: do_int10_01(); return;
  case 0x02: do_int10_02(); return;
  case 0x03: do_int10_03(cpu_regs.bh); return;
  case 0x05: do_int10_05(); return;
  case 0x06: do_int10_06(); return;
  case 0x07: do_int10_07(); return;
  case 0x08: do_int10_08(); return;
  case 0x09: do_int10_09(); return;
  case 0x0A: do_int10_0A(); return;
  case 0x0E: do_int10_0E(); return;
  case 0x0F: do_int10_0F(); return;
  case 0x12: do_int10_12(); return;
  case 0x13: do_int10_13(); return;
  case 0x1A: do_int10_1AXX(); return;
  case 0x30: do_int10_30XX(); return;
  default:
    // handle me please
    __debugbreak();
  }
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
