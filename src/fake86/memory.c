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


// ?.c
extern uint8_t vidmode;
extern uint8_t updatedscreen;
extern uint16_t VGA_SC[0x100];

// cpu.c
extern uint8_t didbootstrap;
extern uint8_t hdcount;

// video.c
void writeVGA(uint32_t addr32, uint8_t value);
uint8_t readVGA(uint32_t addr32);

uint8_t RAM[0x100000];
uint8_t readonly[0x100000];

void mem_init(void) {
  // its static so not required
  memset(readonly, 0, sizeof(readonly));
  memset(RAM, 0, sizeof(RAM));
}

static uint8_t mem_read_8(uint32_t addr) {
  return RAM[addr];
}

static uint16_t mem_read_16(uint32_t addr) {
  return RAM[addr] | (RAM[addr + 1] << 8);
}

static void mem_write_8(uint32_t addr, uint8_t data) {
  RAM[addr] = data;
}

static void mem_write_16(uint32_t addr, uint16_t data) {
  RAM[addr + 0] = (0x00ff & data) >> 0;
  RAM[addr + 1] = (0xff00 & data) >> 8;
}

void write86(uint32_t addr32, uint8_t value) {
  addr32 &= 0xFFFFF;

  if (readonly[addr32] || (addr32 >= 0xC0000)) {
    return;
  }
#if USE_VIDEO_NEO
  mem_write_8(addr32, value);
  // pass write on to video neo
#else
  if ((addr32 >= 0xA0000) && (addr32 <= 0xBFFFF)) {
    if ((vidmode != 0x13) && (vidmode != 0x12) && (vidmode != 0xD) &&
        (vidmode != 0x10)) {
      mem_write_8(addr32, value);
    } else {
      if (((VGA_SC[4] & 6) == 0) && (vidmode != 0xD) && (vidmode != 0x10) &&
          (vidmode != 0x12)) {
        mem_write_8(addr32, value);
      } else {
        writeVGA(addr32 - 0xA0000, value);
      }
    }
    updatedscreen = 1;
    return;
  }
#endif
  mem_write_8(addr32, value);
}

void writew86(uint32_t addr32, uint16_t value) {
  addr32 &= 0xFFFFF;
  write86(addr32 + 0, (uint8_t)(value >> 0));
  write86(addr32 + 1, (uint8_t)(value >> 8));
}

uint8_t read86(uint32_t addr32) {

  // wrap address pointer
  addr32 &= 0xFFFFF;

#if USE_VIDEO_NEO
  // invoke video neo if in right address range
  return mem_read_8(addr32);
#else
  // VRAM read
  static const uint32_t VRAM_ADDR = 0xA0000;
  static const uint32_t VRAM_END  = 0xC0000;
  if ((addr32 >= VRAM_ADDR) && (addr32 < VRAM_END)) {
    switch (vidmode) {
    case 0x0D:
    case 0x0E:
    case 0x10:
    case 0x12:
      return readVGA(addr32 - VRAM_ADDR);
    case 0x13:
      if ((VGA_SC[4] & 6) != 0) {
        return readVGA(addr32 - VRAM_ADDR);
      }
    }
  }
#endif

  // bootstraping hackingWW
  if (!didbootstrap) {
    // warning this causes landmark test to fail on memory
#if 1
    // ugly hack to make BIOS always believe we have an EGA/VGA card installed
    mem_write_8(0x410, 0x41);
    // the BIOS doesn't have any concept of hard drives, so here's another hack
    mem_write_8(0x475, hdcount);
#endif
  }

  // normal ram read
  return mem_read_8(addr32);
}

uint16_t readw86(uint32_t addr32) {
  addr32 &= 0xFFFFF;
  return (uint16_t)(read86(addr32 + 0) << 0) |
         (uint16_t)(read86(addr32 + 1) << 8);
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

uint32_t mem_loadrom(uint32_t addr32, const char *filename, uint8_t failure_fatal) {
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
