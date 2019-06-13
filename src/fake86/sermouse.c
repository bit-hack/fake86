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

/* sermouse.c: functions to emulate a standard Microsoft-compatible serial
 * mouse. */

#include "../common/common.h"


struct sermouse_s sermouse;

// buffer some serial mouse data
void bufsermousedata(uint8_t value) {
  if (sermouse.bufptr == 16) {
    return;
  }
  if (sermouse.bufptr == 0) {
    i8259_doirq(4);
  }
  sermouse.buf[sermouse.bufptr++] = value;
}

// port write
void mouse_port_write(uint16_t portnum, uint8_t value) {
  uint8_t oldreg;
  // printf("[DEBUG] Serial mouse, port %X out: %02X\n", portnum, value);
  portnum &= 7;
  oldreg = sermouse.reg[portnum];
  sermouse.reg[portnum] = value;
  switch (portnum) {
  case 4:                              // modem control register
    if ((value & 1) != (oldreg & 1)) { // software toggling of this register
      sermouse.bufptr = 0;  // causes the mouse to reset and fill the buffer
      bufsermousedata('M'); // with a bunch of ASCII 'M' characters.
      bufsermousedata('M'); // this is intended to be a way for
      bufsermousedata('M'); // drivers to verify that there is
      bufsermousedata('M'); // actually a mouse connected to the port.
      bufsermousedata('M');
      bufsermousedata('M');
    }
    break;
  }
}

// port read
uint8_t mouse_port_read(uint16_t portnum) {
  uint8_t temp;
  portnum &= 7;
  switch (portnum) {
  case 0: // data receive
    temp = sermouse.buf[0];
    for (int i = 0; i < 15; ++i) {
      sermouse.buf[i] = sermouse.buf[i + 1];
    }
    if (sermouse.bufptr > 0) {
      --sermouse.bufptr;
    }
    if (sermouse.bufptr > 0) {
#if 1
      i8259_doirq(4);
#endif
    }
    sermouse.reg[4] = ~sermouse.reg[4] & 1;
    return temp;

  case 5: // line status register (read-only)
    if (sermouse.bufptr > 0) {
      temp = 1;
    } else {
      return 0x1;
    }
    return 0x60 | temp;
  }
  return (sermouse.reg[portnum & 7]);
}

void mouse_init(uint16_t baseport, uint8_t irq) {
  sermouse.bufptr = 0;
  set_port_write_redirector(baseport, baseport + 7, mouse_port_write);
  set_port_read_redirector(baseport, baseport + 7, mouse_port_read);
}

void mouse_post_event(uint8_t lmb, uint8_t rmb, int32_t xrel, int32_t yrel) {
  // cast to 8 bit signed
  uint8_t rx = xrel, ry = yrel;
  // high two bits of each relative motion axis
  uint8_t high_bits = (((ry >> 6) & 0x03) << 2) |
                      (((rx >> 6) & 0x03) << 0);
  // button encodings
  const uint8_t buttons = (lmb ? 0x20 : 0) | (rmb ? 0x10 : 0);
  // first byte
  const uint8_t first_char_flag = 0x80 | 0x40;
  bufsermousedata(first_char_flag | buttons | high_bits);
  // second horizontal
  bufsermousedata(rx & 0x3f);
  // third vertical
  bufsermousedata(ry & 0x3f);
}
