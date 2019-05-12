/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2012 Mike Chambers

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

// i8255 Peripheral Interface Adapter

#include "common.h"


struct i8255_t i8255;

static void write_ctrl_word(uint8_t value) {

  const int mode_set    = 1 & (value >> 7);
  const int mode        = 3 & (value >> 5);
  const int io_porta    = 1 & (value >> 4);
  const int io_portc_hi = 1 & (value >> 3);
  const int mode_portb  = 1 & (value >> 2);
  const int io_portb    = 1 & (value >> 1);
  const int io_portc_lo = 1 & (value >> 0);

  i8255.ctrl_word = value;
}

static void i8255_port_write(uint16_t port, uint8_t value) {
  switch (port) {
  // PORTA/B
  case 0x60:
  case 0x61:
    i8255.port_out[port & 0x3] = value;
    break;
  // PORTC
  case 0x62:
    if (value & 0x80) {
      const uint8_t bit = 1 << ((value >> 1) & 0x0f);
      if (value & 0x01) {
        i8255.port_out[2] &= ~bit;
      }
      else {
        i8255.port_out[2] |= bit;
      }
    }
    else {
      i8255.port_out[2] &= 0xf0;
      i8255.port_out[2] |= value & 0x0f;
    }
  case 0x63:
    i8255.ctrl_word = value;
    break;
  }
}

static uint8_t i8255_port_read(uint16_t port) {
  switch (port) {
  case 0x60:
    return i8255.ctrl_word & 0x10 ?
        i8255.port_in[0] :
        i8255.port_out[0];
  case 0x61:
    return i8255.ctrl_word & 0x02 ?
        i8255.port_in[1] :
        i8255.port_out[1];
  case 0x62:
  {
    uint8_t out = 0;
    out |= (i8255.ctrl_word & 0x01 ?
        i8255.port_in[3]:
        i8255.port_out[3]);
    out |= (i8255.ctrl_word & 0x08 ?
        i8255.port_in[3]:
        i8255.port_out[3]) & 0xf0;
    return out;
  }
  case 0x63:
    return i8255.ctrl_word;
  }
  return 0;
}

bool i8255_init() {
  i8255_reset();
  set_port_write_redirector(0x60, 0x63, i8255_port_write);
  set_port_read_redirector(0x60, 0x63, i8255_port_read);
  return true;
}

void i8255_reset() {
  memset(&i8255, 0, sizeof(i8255));
  // PORTA is input
  i8255.ctrl_word |= 0x10;
}

void i8255_tick(uint64_t cycles) {
  (void)cycles;
}
