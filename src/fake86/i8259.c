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

// i8253 Prioritized Interrupt Controller

#include "../common/common.h"


struct structpic i8259;

// seems to be redundant even in the original code base
static uint32_t makeupticks;

static uint8_t i8259_port_read(uint16_t portnum) {
  switch (portnum & 1) {
  case 0:
    return (i8259.readmode == 0) ? i8259.irr : i8259.isr;
  case 1: // read mask register
    return i8259.imr;
  }
  return 0;
}

static void i8259_port_write(uint16_t portnum, uint8_t value) {
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
  uint8_t output = 0;
  // XOR request register with inverted mask register
  const uint8_t tmpirr = i8259.irr & (~i8259.imr);
  for (uint8_t i = 0; i < 8; i++) {
    if ((tmpirr >> i) & 1) {
      i8259.irr ^= (1 << i);
      i8259.isr |= (1 << i);
      output = (i8259.icw[2] + i);
      break;
    }
  }
  // keyboard services required
  if (output == 9) {
    i8255_key_required();
  }
  // return the next interrupt number
  return output;
}

void i8259_doirq(uint8_t irqnum) {
  i8259.irr |= (1 << irqnum);
}

bool i8259_irq_pending(void) {
  return (i8259.irr & (~i8259.imr)) != 0;
}

void i8259_init() {
  memset((void *)&i8259, 0, sizeof(i8259));

  set_port_read_redirector(0x20, 0x21, i8259_port_read);
  set_port_write_redirector(0x20, 0x21, i8259_port_write);
}

void i8259_tick(uint64_t cycles) {
  // dummy
}

void i8259_state_save(FILE *fd) {
  fwrite(&i8259, 1, sizeof(i8259), fd);
}

void i8259_state_load(FILE *fd) {
  fread(&i8259, 1, sizeof(i8259), fd);
}
