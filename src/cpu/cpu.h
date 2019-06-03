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

typedef void (*cpu_intcall_t)(const uint8_t int_num);
void cpu_set_intcall_handler(cpu_intcall_t handler);

extern uint16_t segregs[4];

// cpu instruction pointer
extern uint16_t ip;

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

void cpu_udis_exec(const uint8_t *stream);
