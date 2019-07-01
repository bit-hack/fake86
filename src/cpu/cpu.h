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

#pragma once

#include "../common/common.h"


#pragma pack(push, 1)
struct cpu_regs_t {
  union {
    // 16bit registers
    struct {
      uint16_t ax, cx, dx, bx;
    };
    // 8bit registers
    struct {
      uint8_t al, ah, cl, ch, dl, dh, bl, bh;
    };
  };
  // stack and index registers
  uint16_t sp, bp, si, di;
  // segment registers
  uint16_t es, cs, ss, ds;
  // instruction pointer
  uint16_t ip;
};
extern struct cpu_regs_t cpu_regs;
#pragma pack(pop)

void cpu_push(uint16_t pushval);
uint16_t cpu_pop();
void cpu_reset();
void cpu_prep_interupt(uint16_t intnum);
int32_t cpu_exec86(int32_t cycle_target);

// get the current tick count of this slice
uint64_t cpu_slice_ticks(void);

bool cpu_in_hlt_state(void);

typedef void (*cpu_intcall_t)(const uint16_t int_num);
void cpu_set_intcall_handler(cpu_intcall_t handler);

union cpu_flags_t {
  struct {
    uint32_t cf:1, pf:1, af:1, zf:1, sf:1, tf:1, ifl:1, df:1, of:1;
  };
};

extern union cpu_flags_t cpu_flags;

extern bool cpu_running;

// delay non ISR routines for number of cycles
// used to simulate disk drive latency
void cpu_delay(uint32_t cycles);

// state save/load
void cpu_state_save(FILE *fd);
void cpu_state_load(FILE *fd);

extern bool cpu_halt;
extern bool cpu_step;

#define CPU_ADDR(SEG, OFF) \
  (((SEG) << 4) + (OFF))

struct cpu_io_t {
  uint8_t *ram;
  uint8_t  (*mem_read_8   )(uint32_t addr);
  uint16_t (*mem_read_16  )(uint32_t addr);
  void     (*mem_write_8  )(uint32_t addr,   uint8_t  value);
  void     (*mem_write_16 )(uint32_t addr,   uint16_t value);
  uint8_t  (*port_read_8  )(uint16_t port);
  uint16_t (*port_read_16 )(uint16_t port);
  void     (*port_write_8 )(uint16_t port, uint8_t  value);
  void     (*port_write_16)(uint16_t port, uint16_t value);
  void     (*int_call     )(uint16_t num);
};

void cpu_set_io(const struct cpu_io_t *io);
uint16_t cpu_get_flags(void);
void cpu_set_flags(const uint16_t flags);
void cpu_mod_flags(uint16_t in, uint16_t mask);
