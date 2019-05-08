#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define _SDL_main_h
#include <SDL/SDL.h>

#include "config.h"


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- i8259.c
void doirq(uint8_t irqnum);

struct structpic {
  // mask register
  uint8_t imr;
  // request register
  uint8_t irr; 
  // service register
  uint8_t isr;
  // used during initialization to keep track of which ICW we're at
  uint8_t icwstep; 
  uint8_t icw[5];
  // interrupt vector offset
  uint8_t intoffset;
  // which IRQ has highest priority
  uint8_t priority;
  // automatic EOI mode
  uint8_t autoeoi;
  // remember what to return on read register from OCW3
  uint8_t readmode; 
  uint8_t enabled;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- video.c
bool video_int_handler(int intnum);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- sermouse.c
struct sermouse_s {
  uint8_t reg[8];
  uint8_t buf[16];
  int8_t bufptr;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- memory.c
extern uint8_t RAM[0x100000];
extern uint8_t readonly[0x100000];

void write86(uint32_t addr32, uint8_t value);
void writew86(uint32_t addr32, uint16_t value);

uint8_t read86(uint32_t addr32);
uint16_t readw86(uint32_t addr32);

uint32_t mem_loadbinary(uint32_t addr32, uint8_t *filename, uint8_t roflag);
uint32_t mem_loadrom(uint32_t addr32, uint8_t *filename, uint8_t failure_fatal);
uint32_t mem_loadbios(uint8_t *filename);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- i8237.c
struct dmachan_s {
  uint32_t page;
  uint32_t addr;
  uint32_t reload;
  uint32_t count;
  uint8_t direction;
  uint8_t autoinit;
  uint8_t writemode;
  uint8_t masked;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- i8253.c
#define PIT_MODE_LATCHCOUNT 0
#define PIT_MODE_LOBYTE 1
#define PIT_MODE_HIBYTE 2
#define PIT_MODE_TOGGLE 3

struct i8253_s {
  uint16_t chandata[3];
  uint8_t accessmode[3];
  uint8_t bytetoggle[3];
  uint32_t effectivedata[3];
  float chanfreq[3];
  uint8_t active[3];
  uint16_t counter[3];
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- disk.c
struct struct_drive {
  FILE *diskfile;
  uint32_t filesize;
  uint16_t cyls;
  uint16_t sects;
  uint16_t heads;
  uint8_t inserted;
  char *filename;
};

uint8_t disk_insert(uint8_t drivenum, char *filename);

void disk_eject(uint8_t drivenum);

void disk_read(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl,
              uint16_t sect, uint16_t head, uint16_t sectcount);

void disk_write(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl,
               uint16_t sect, uint16_t head, uint16_t sectcount);

void disk_int_handler(int intnum);

void disk_bootstrap(int intnum);

extern uint8_t bootdrive, hdcount;
