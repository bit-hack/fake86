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

// i8237 Direct Memory Access controller

#include "../common/common.h"


struct dmachan_s dmachan[4];
static uint8_t flipflop = 0;

// used by blaster.c
uint8_t read8237(uint8_t channel) {
  uint8_t ret;
  if (dmachan[channel].masked)
    return (128);
  if (dmachan[channel].autoinit &&
      (dmachan[channel].count > dmachan[channel].reload))
    dmachan[channel].count = 0;
  if (dmachan[channel].count > dmachan[channel].reload)
    return (128);
  if (dmachan[channel].direction == 0)
    ret = RAM[dmachan[channel].page + dmachan[channel].addr +
              dmachan[channel].count];
  else
    ret = RAM[dmachan[channel].page + dmachan[channel].addr -
              dmachan[channel].count];
  dmachan[channel].count++;
  return (ret);
}

// port write
static void i8237_port_write(uint16_t addr, uint8_t value) {
  const uint8_t channel = value & 3;
  switch (addr) {
  case 0x2: // channel 1 address register
    if (flipflop == 1)
      dmachan[1].addr = (dmachan[1].addr & 0x00FF) | ((uint32_t)value << 8);
    else
      dmachan[1].addr = (dmachan[1].addr & 0xFF00) | value;
    flipflop = ~flipflop & 1;
    break;
  case 0x3: // channel 1 count register
    if (flipflop == 1)
      dmachan[1].reload = (dmachan[1].reload & 0x00FF) | ((uint32_t)value << 8);
    else
      dmachan[1].reload = (dmachan[1].reload & 0xFF00) | value;
    if (flipflop == 1) {
      if (dmachan[1].reload == 0)
        dmachan[1].reload = 0x10000;
      dmachan[1].count = 0;
    }
    flipflop = ~flipflop & 1;
    break;
  case 0xA: // write single mask register
    dmachan[channel].masked = (value >> 2) & 1;
    break;
  case 0xB: // write mode register
    dmachan[channel].direction = (value >> 5) & 1;
    dmachan[channel].autoinit = (value >> 4) & 1;
    dmachan[channel].writemode = (value >> 2) & 1; // not quite accurate
    break;
  case 0xC: // clear byte pointer flip-flop
    flipflop = 0;
    break;
  case 0x83: // DMA channel 1 page register
    dmachan[1].page = (uint32_t)value << 16;
    break;
  }
}

// port read
uint8_t i8237_port_read(uint16_t addr) {
  if (addr & 0x80) {
    // this is a DMA page register
  } else {
    switch (addr) {
    case 3:
      if (flipflop == 1) {
        return dmachan[1].reload >> 8;
      }
      else {
        return dmachan[1].reload;
      }
      flipflop = ~flipflop & 1;
      break;
    }
  }
  return 0;
}

void init8237() {
  memset(dmachan, 0, sizeof(dmachan));

  // DMA 1
  set_port_write_redirector(0x00, 0x0F, &i8237_port_write);
  set_port_read_redirector(0x00, 0x0F, &i8237_port_read);

  // DMA Page registers
  set_port_write_redirector(0x80, 0x8F, &i8237_port_write);
  set_port_read_redirector(0x80, 0x8F, &i8237_port_read);
}

void i8237_tick(uint64_t cycles) {
  // dummy
}

bool i8237_init(void) {
  return true;
}

void i8237_state_save(FILE *fd) {
  fwrite(dmachan, 1, sizeof(dmachan), fd);
  fwrite(&flipflop, 1, sizeof(flipflop), fd);
}

void i8237_state_load(FILE *fd) {
  fread(dmachan, 1, sizeof(dmachan), fd);
  fread(&flipflop, 1, sizeof(flipflop), fd);
}
