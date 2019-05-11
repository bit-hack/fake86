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

enum {
  reges = 0,
  regcs = 1,
  regss = 2,
  regds = 3,
};

#if 0
enum {
  regax = 0,
  regcx = 1,
  regdx = 2,
  regbx = 3,
  regsp = 4,
  regbp = 5,
  regsi = 6,
  regdi = 7,
  reges = 0,
  regcs = 1,
  regss = 2,
  regds = 3,
};

enum {
  regal = 0,
  regah = 1,
  regcl = 2,
  regch = 3,
  regdl = 4,
  regdh = 5,
  regbl = 6,
  regbh = 7,
};
#endif


#pragma pack(push, 1)
struct cpu_regs_t {
  union {
    struct {
      uint16_t ax, cx, dx, bx;
    };
    struct {
      uint8_t al, ah, cl, ch, dl, dh, bl, bh;
    };
  };
  uint16_t sp, bp, si, di;
};
extern struct cpu_regs_t cpu_regs;
#pragma pack(pop)

void cpu_push(uint16_t pushval);
uint16_t cpu_pop();
void cpu_reset();
void cpu_prep_interupt(uint16_t intnum);
void cpu_exec86(int32_t cycles);

extern uint16_t segregs[4];

extern uint8_t didbootstrap;

// cpu instruction pointer
extern uint16_t ip;

// cpu flags
extern uint8_t cf, pf, af, zf, sf, tf, ifl, df, of;
