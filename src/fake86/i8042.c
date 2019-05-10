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
#include "common.h"

static uint8_t _key_buf;
static bool _out_full;

void i8042_key_push(uint8_t key) {
  _key_buf = key;
  _out_full = true;
  i8259_doirq(1);
}

uint8_t i8042_port_read(uint16_t port) {
  // data port (scancode)
  if (port == 0x60) {
    _out_full = false;
    return _key_buf;
  }
  // status
  if (port == 0x64) {
    __debugbreak();
    // input buffer is sent from host to keyboard
    // output buffer is sent from keyboard to host
    uint8_t status;
    // output buffer full
    status |= _out_full ? 0x01 : 0x00;
    // system flag
    status |= 0x40;
    // ?? why should this always be set
    status |= 2;
    return status;
  }
  return 0;
}

void i8042_port_write(uint16_t port, uint8_t value) {
  // data port (scancode)
  if (port == 0x60) {
    _key_buf = value;
  }
  // command
  if (port == 0x64) {
    __debugbreak();
  }
}

void i8042_reset() {
  _out_full = false;
}

void i8042_tick() {
}

void i8042_init() {
  i8042_reset();
}
