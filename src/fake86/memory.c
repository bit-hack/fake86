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

#include "../common/common.h"
#include "../cpu/cpu.h"


uint8_t RAM[0x100000];


void mem_init(void) {
  // its static so not required
  memset(RAM, 0, sizeof(RAM));
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

void mem_write(uint32_t addr, const uint8_t *src, size_t size) {
  const uint32_t end = addr + size;
  if (end <= 0xA0000) {
    memcpy(RAM + addr, src, size);
  }
  else {
    for (uint32_t i = 0; i < size; i++) {
      write86(addr + i, src[i]);
    }
  }
}

void mem_read(uint8_t *dst, uint32_t addr, size_t size) {
  const uint32_t end = addr + size;
  if (end <= 0xA0000) {
    memcpy(dst, RAM + addr, size);
  }
  else {
    for (uint32_t i = 0; i < size; i++) {
      dst[i] = read86(addr + i);
    }
  }
}

uint8_t read86(uint32_t addr) {
  addr &= 0xFFFFF;

#if 0
  if (addr == 0x29f33) {
    log_printf(LOG_CHAN_MEM, "memory read breakpoint hit");
    cpu_halt = true;
  }
#endif

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

uint16_t readw86(uint32_t addr) {
  addr &= 0xFFFFF;
  if (addr >= 0xA0000 && addr <= 0xC0000) {
    if (addr >= 0xB0000) {
      return *(const uint16_t*)(RAM + addr);
    }
    return (uint16_t)(read86(addr + 0) << 0) |
           (uint16_t)(read86(addr + 1) << 8);
  }
  else {
    return *(const uint16_t*)(RAM + addr);
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

  return readsize;
}

void mem_dump(const char *path) {
  FILE *fd = fopen(path, "wb");
  if (!fd) {
    log_printf(LOG_CHAN_MEM, "error opening dump file");
    return;
  }
  fwrite(RAM, 1, sizeof(RAM), fd);
  fclose(fd);
  log_printf(LOG_CHAN_MEM, "memory dump written to '%s'", path);
}

void mem_state_save(FILE *fd) {
  fwrite(RAM, 1, sizeof(RAM), fd);
}

void mem_state_load(FILE *fd) {
  fread(RAM, 1, sizeof(RAM), fd);
}
