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
void i8259_init();
uint8_t i8259_port_read(uint16_t portnum);
void i8259_port_write(uint16_t portnum, uint8_t value);
void i8259_doirq(uint8_t irqnum);
uint8_t i8259_nextintr();

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- video.c
bool video_int_handler(int intnum);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- sermouse.c
struct sermouse_s {
  uint8_t reg[8];
  uint8_t buf[16];
  int8_t bufptr;
};

void mouse_port_write(uint16_t portnum, uint8_t value);
uint8_t mouse_port_read(uint16_t portnum);
void mouse_init(uint16_t baseport, uint8_t irq);
void mouse_post_event(uint8_t lmb, uint8_t rmb, int32_t xrel, int32_t yrel);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- memory.c
extern uint8_t RAM[0x100000];
extern uint8_t readonly[0x100000];

void write86(uint32_t addr32, uint8_t value);
void writew86(uint32_t addr32, uint16_t value);

uint8_t read86(uint32_t addr32);
uint16_t readw86(uint32_t addr32);

void mem_init();
uint32_t mem_loadbinary(uint32_t addr32, const char *filename, uint8_t roflag);
uint32_t mem_loadrom(uint32_t addr32, const char *filename, uint8_t failure_fatal);
uint32_t mem_loadbios(const char *filename);

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

void i8253_port_write(uint16_t portnum, uint8_t value);
uint8_t i8253_port_read(uint16_t portnum);
void i8253_init();

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- i8042.c
void i8042_key_push(uint8_t key);
uint8_t i8042_port_read(uint16_t port);
void i8042_port_write(uint16_t port, uint8_t value);
void i8042_tick();
void i8042_reset();
void i8042_init();

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- disk.c
bool disk_insert(uint8_t drivenum, const char *filename);

void disk_eject(uint8_t drivenum);

void disk_read(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl,
              uint16_t sect, uint16_t head, uint16_t sectcount);

void disk_write(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl,
               uint16_t sect, uint16_t head, uint16_t sectcount);

void disk_int_handler(int intnum);

void disk_bootstrap(int intnum);

extern uint8_t bootdrive, hdcount;

bool disk_is_inserted(int num);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- log.c

enum {
  LOG_CHAN_ALL,
  LOG_CHAN_DISK,
  LOG_CHAN_FRONTEND,
  LOG_CHAN_SDL,
  LOG_CHAN_CPU,
  LOG_CHAN_MEM,
  LOG_CHAN_VIDEO,
  LOG_CHAN_AUDIO,
};

void log_init();
void log_close();
void log_printf(int channel, const char *fmt, ...);
