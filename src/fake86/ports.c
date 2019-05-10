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

/* ports.c: functions to handle port I/O from the CPU module, as well
   as functions for emulated hardware components to register their
   read/write callback functions across the port address range. */

// for a good list of ports see:
// http://bochs.sourceforge.net/techspec/PORTS.LST

#include "common.h"

#include "../80x86/cpu.h"


static const bool _notify_unknown_ports = false;

uint8_t portram[0x10000];

extern uint8_t speakerenabled;
extern uint8_t keyboardwaitack;

static bool _port_ignore[0x10000];

static port_write_b_t port_write_callback[0x10000];
static port_read_b_t port_read_callback[0x10000];

static port_write_w_t port_write_callback16[0x10000];
static port_read_w_t port_read_callback16[0x10000];

extern uint8_t verbose;


static void _ignore_range(uint16_t start, uint16_t end) {
  for (int i = start; i <= end; ++i) {
    _port_ignore[i] = true;
  }
}

// 8bit port write
void portout(uint16_t portnum, uint8_t value) {

  portram[portnum] = value;

  switch (portnum) {
  // (Programmable Interrupt Controller 8259)
  case 0x20:
  case 0x21:
    i8259_port_write(portnum, value);
    return;

  // 0040-005F ----	PIT  (Programmable Interrupt Timer  8253, 8254)
  case 0x40:
  case 0x41:
  case 0x42:
  case 0x43:
    i8253_port_write(portnum, value);
    return;

  // 0060-006F ----	Keyboard controller 804x (8041, 8042) (or PPI (8255) on PC,XT)
  case 0x61:
    speakerenabled = 0;
    // timer 2 gate to speaker enable
    if (value & 1) {
      // speaker data enable
      if (value & 2) {
        speakerenabled = 1;
      }
    }
    return;
  case 0x60:
  case 0x64:
    i8042_port_write(portnum, value);
    return;
  }

  port_write_b_t cb = port_write_callback[portnum];
  if (cb) {
    cb(portnum, value);
  }
  else {
    // notify of unhandled port access
    if (_notify_unknown_ports && !_port_ignore[portnum]) {
      log_printf(LOG_CHAN_PORT, "byte write to unknown port %03xh", portnum);
      _port_ignore[portnum] = true;
    }
  }
}

// 8bit port read
uint8_t portin(uint16_t portnum) {
  switch (portnum) {
  case 0x20:
  case 0x21:
    return i8259_port_read(portnum);

  case 0x40:
  case 0x41:
  case 0x42:
  case 0x43:
    return i8253_port_read(portnum);

  case 0x62:
    return 0x00;
  case 0x61:
  case 0x63:
    return portram[portnum];

  case 0x60:
  case 0x64:
    return i8042_port_read(portnum);
  }

  port_read_b_t cb = port_read_callback[portnum];
  if (cb) {
    return cb(portnum);
  }

  // notify of unhandled port access
  if (_notify_unknown_ports && !_port_ignore[portnum]) {
    log_printf(LOG_CHAN_PORT, "byte read from unknown port %03xh", portnum);
    _port_ignore[portnum] = true;
  }

  // send back dummy value
  return 0xff;
}

// 16bit port write
void portout16(uint16_t portnum, uint16_t value) {
  port_write_w_t cb = port_write_callback16[portnum];
  if (cb) {
    cb(portnum, value);
  }
  else {
    portout(portnum, (uint8_t)value);
    portout(portnum + 1, (uint8_t)(value >> 8));
  }
}

// 16bit port read
uint16_t portin16(uint16_t portnum) {
  port_read_w_t cb = port_read_callback16[portnum];
  if (cb) {
    return cb(portnum);
  }
  else {
    // do dual 8bit read
    const uint16_t lo = (uint16_t)portin(portnum + 0);
    const uint16_t hi = (uint16_t)portin(portnum + 1);
    return (hi << 8) | lo;
  }
}

void set_port_write_redirector(uint16_t startport, uint16_t endport,
                               void *callback) {
  for (uint16_t i = startport; i <= endport; i++) {
    port_write_callback[i] = callback;
  }
}

void set_port_read_redirector(uint16_t startport, uint16_t endport,
                              void *callback) {
  for (uint16_t i = startport; i <= endport; i++) {
    port_read_callback[i] = callback;
  }
}

void set_port_write_redirector_16(uint16_t startport, uint16_t endport,
                                  void *callback) {
  for (uint16_t i = startport; i <= endport; i++) {
    port_write_callback16[i] = callback;
  }
}

void set_port_read_redirector_16(uint16_t startport, uint16_t endport,
                                 void *callback) {
  for (uint16_t i = startport; i <= endport; i++) {
    port_read_callback16[i] = callback;
  }
}

void port_init() {

  // TODO
  // byte write to unknown port 0a0h        PIC 2	(Programmable Interrupt Controller 8259)
  // byte write to unknown port 063h        PPI (XT only) command mode register  (read dipswitches)
  // byte write to unknown port 213h        Expansion unit (XT)
  // byte write to unknown port 0c0h        SN746496 pcjr
  // byte write to unknown port 378h        printer
  // byte write to unknown port 278h        parallel printer
  // byte write to unknown port 2fbh        async coms
  // byte read from unknown port 201h       game control
  // byte read from unknown port 241h       Gravis Ultra Sound by Advanced Gravis
  // byte read from unknown port 341h       Gravis Ultra Sound by Advanced Gravis
  // byte write to unknown port 3f2h        diskette controller

  _ignore_range(0x2f0, 0x2f7); // reserved
}
