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


// video.c
void writeVGA(uint32_t addr32, uint8_t value);
uint8_t readVGA(uint32_t addr32);
extern uint8_t vidmode;
extern uint8_t updatedscreen;
extern uint16_t VGA_SC[0x100];

uint8_t RAM[0x100000];
uint8_t readonly[0x100000];


void mem_init(void) {
  // its static so not required
  memset(readonly, 0, sizeof(readonly));
  memset(RAM, 0, sizeof(RAM));
}

static void mem_write_8(uint32_t addr, uint8_t data) {
  RAM[addr] = data;
}

static void mem_write_16(uint32_t addr, uint16_t data) {
  RAM[addr + 0] = (0x00ff & data) >> 0;
  RAM[addr + 1] = (0xff00 & data) >> 8;
}

void write86(uint32_t addr, uint8_t value) {
  addr &= 0xFFFFF;

  if (addr >= 0xC0000) {
    return;
  }
  if (addr >= 0xA0000) {
    if (addr >= 0xB0000) {
      RAM[addr] = value;
      return;
    }
    neo_mem_write_A0000(addr, value); // vga/ega
    return;
  }
  else {
    RAM[addr] = value;
  }
}

void writew86(uint32_t addr32, uint16_t value) {
  addr32 &= 0xFFFFF;
  if (addr32 >= 0xC0000) {
    // read only
    return;
  }
  if (addr32 < 0xA0000) {
    *(uint16_t*)(RAM + addr32) = value;
  }
  else {
    write86(addr32 + 0, (uint8_t)(value >> 0));
    write86(addr32 + 1, (uint8_t)(value >> 8));
  }
}

uint8_t read86(uint32_t addr) {
  addr &= 0xFFFFF;

  if (addr >= 0xA0000 && addr <= 0xC0000) {
    if (addr >= 0xB0000) {
      return RAM[addr];
    }
    return neo_mem_read_A0000(addr); // vga/ega
  }
  else {
    return RAM[addr];
  }
}

uint16_t readw86(uint32_t addr32) {
  addr32 &= 0xFFFFF;
  if (addr32 < 0xA0000 || addr32 >= 0xC0000) {
    return *(const uint16_t*)(RAM + addr32);
  }
  else {
    return (uint16_t)(read86(addr32 + 0) << 0) |
           (uint16_t)(read86(addr32 + 1) << 8);
  }
}

uint32_t mem_loadbinary(uint32_t addr32, const char *filename, uint8_t roflag) {
  FILE *binfile = fopen(filename, "rb");
  if (binfile == NULL) {
    log_printf(LOG_CHAN_MEM, "unable to open rom file '%s'", filename);
    return 0;
  }
  log_printf(LOG_CHAN_MEM, "loading rom '%s' at %08x", filename, addr32);
  // get filesize
  fseek(binfile, 0, SEEK_END);
  uint32_t readsize = ftell(binfile);
  fseek(binfile, 0, SEEK_SET);
  // load into memory
  fread((void *)&RAM[addr32], 1, readsize, binfile);
  fclose(binfile);
  // set read only mask
  memset((void *)&readonly[addr32], roflag, readsize);
  return (readsize);
}

uint32_t mem_loadrom(uint32_t addr32, const char *filename,
                     uint8_t failure_fatal) {
  uint32_t readsize;
  readsize = mem_loadbinary(addr32, filename, 1);
  if (!readsize) {
    return 0;
  } else {
    return readsize;
  }
}

uint32_t mem_loadbios(const char *filename) {
  FILE *binfile = fopen(filename, "rb");
  if (binfile == NULL) {
    log_printf(LOG_CHAN_MEM, "unable to open bios file '%s'", filename);
    return (0);
  }
  // get filesize
  fseek(binfile, 0, SEEK_END);
  uint32_t readsize = ftell(binfile);
  fseek(binfile, 0, SEEK_SET);
  const uint32_t addr32 = 0x100000 - readsize;
  // log message
  log_printf(LOG_CHAN_MEM, "loading bios '%s' at %08x", filename, addr32);
  // load into memory
  fread((void *)&RAM[addr32], 1, readsize, binfile);
  fclose(binfile);
  // set readonly mask
  memset((void *)&readonly[addr32], 1, readsize);

  return readsize;
}
