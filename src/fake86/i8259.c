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

/* i8259.c: functions to emulate the Intel 8259 prioritized interrupt
   controller.
   note: this is not a very complete 8259 implementation, but for the purposes
   of a PC, it's all we need. */

#include "common.h"


struct structpic i8259;

extern uint32_t makeupticks;

uint8_t i8259_port_read(uint16_t portnum) {
  switch (portnum & 1) {
  case 0:
    if (i8259.readmode == 0)
      return (i8259.irr);
    else
      return (i8259.isr);
  case 1: // read mask register
    return (i8259.imr);
  }
  return (0);
}

void i8259_port_write(uint16_t portnum, uint8_t value) {
  uint8_t i;
  switch (portnum & 1) {
  case 0:
    if (value & 0x10) { // begin initialization sequence
      i8259.icwstep = 1;
      i8259.imr = 0; // clear interrupt mask register
      i8259.icw[i8259.icwstep++] = value;
      return;
    }
    if ((value & 0x98) == 8) { // it's an OCW3
      if (value & 2) {
        i8259.readmode = value & 2;
      }
    }
    if (value & 0x20) { // EOI command
      for (i = 0; i < 8; i++) {
        if ((i8259.isr >> i) & 1) {
          i8259.isr ^= (1 << i);
          if ((i == 0) && (makeupticks > 0)) {
            makeupticks = 0;
            i8259.irr |= 1;
          }
          return;
        }
      }
    }
    break;
  case 1:
    if ((i8259.icwstep == 3) && (i8259.icw[1] & 2))
      i8259.icwstep = 4; // single mode, so don't read ICW3
    if (i8259.icwstep < 5) {
      i8259.icw[i8259.icwstep++] = value;
      return;
    }
    // if we get to this point, this is just a new IMR value
    i8259.imr = value;
    break;
  }
}

uint8_t i8259_nextintr() {
  // XOR request register with inverted mask register
  uint8_t tmpirr = i8259.irr & (~i8259.imr); 
  for (uint8_t i = 0; i < 8; i++) {
    if ((tmpirr >> i) & 1) {
      i8259.irr ^= (1 << i);
      i8259.isr |= (1 << i);
      return i8259.icw[2] + i;
    }
  }
  // this won't be reached, but without it the compiler gives a warning
  return 0; 
}

void i8259_doirq(uint8_t irqnum) {
  i8259.irr |= (1 << irqnum);
}

void i8259_init() {
  memset((void *)&i8259, 0, sizeof(i8259));
}
