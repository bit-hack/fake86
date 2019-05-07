#include "memory.h"

// ?.c
extern uint8_t vidmode;
extern uint8_t updatedscreen;
extern uint16_t	VGA_SC[0x100];

// cpu.c
extern uint8_t didbootstrap;
extern uint8_t hdcount;

uint8_t RAM[0x100000];
uint8_t readonly[0x100000];

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
  const uint32_t tempaddr32 = addr32 & 0xFFFFF;
#ifdef CPU_ADDR_MODE_CACHE
  if (!readonly[tempaddr32]) {
    addrcachevalid[tempaddr32] = 0;
  }
#endif
  if (readonly[tempaddr32] || (tempaddr32 >= 0xC0000) ) {
    return;
  }

  if ( (tempaddr32 >= 0xA0000) && (tempaddr32 <= 0xBFFFF) ) {
    if ( (vidmode != 0x13) && (vidmode != 0x12) && (vidmode != 0xD) && (vidmode != 0x10) ) {
      mem_write_8(tempaddr32, value);
      updatedscreen = 1;
    }
    else {
      if (((VGA_SC[4] & 6) == 0) && (vidmode != 0xD) && (vidmode != 0x10) && (vidmode != 0x12)) {
        mem_write_8(tempaddr32, value);
        updatedscreen = 1;
      } else {
        writeVGA(tempaddr32 - 0xA0000, value);
      }
    }
    updatedscreen = 1;
  }
  else {
    mem_write_8(tempaddr32, value);
  }
}

void writew86(uint32_t addr32, uint16_t value) {
  write86(addr32, (uint8_t) value);
  write86(addr32 + 1, (uint8_t) (value >> 8) );
}

uint8_t read86(uint32_t addr32) {

  addr32 &= 0xFFFFF;

  if ( (addr32 >= 0xA0000) && (addr32 <= 0xBFFFF) ) {
    if ((vidmode == 0xD) || (vidmode == 0xE) || (vidmode == 0x10) || (vidmode == 0x12)) {
      return readVGA(addr32 - 0xA0000);
    }
    if ((vidmode != 0x13) && (vidmode != 0x12) && (vidmode != 0xD)) {
      return mem_read_8(addr32);
    }
    if ((VGA_SC[4] & 6) == 0) {
      return mem_read_8(addr32);
    } else {
      return readVGA(addr32 - 0xA0000);
    }
  }

  if (!didbootstrap) {
    // ugly hack to make BIOS always believe we have an EGA/VGA card installed
    mem_write_8(0x410, 0x41);
    // the BIOS doesn't have any concept of hard drives, so here's another hack
    mem_write_8(0x475, hdcount);
  }

  return mem_read_8(addr32);
}

uint16_t readw86(uint32_t addr32) {
  return (uint16_t)(read86(addr32 + 0) << 0) | 
         (uint16_t)(read86(addr32 + 1) << 8);
}