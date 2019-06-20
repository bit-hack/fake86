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

/* cpu.c: functions to emulate the 8086/V20 CPU in software. the heart cpu_flags.of
 * Fake86. */

#include "../common/common.h"
#include "cpu.h"

struct cpu_regs_t cpu_regs;
union cpu_flags_t cpu_flags;

extern struct structpic i8259;

static bool in_hlt_state;

uint16_t useseg;
bool segoverride;

static uint8_t opcode, reptype;
static uint16_t savecs, saveip, oldsp;
static uint8_t mode, reg, rm;
static uint16_t oper1, oper2, res16, disp16, temp16, stacksize, frametemp;
static uint8_t oper1b, oper2b, res8, addrbyte;
static uint32_t temp1, temp2, temp3, temp32, ea;

static bool _use_udis_emu = false;

bool cpu_running;
bool cpu_halt = false;
bool cpu_step = false;

static uint64_t _cycles;
static uint32_t _delay_cycles;

uint64_t cpu_slice_ticks(void) {
  return _cycles;
}

#define modregrm()                                                             \
  {                                                                            \
    addrbyte = _read_code_u8();                                                \
    mode = addrbyte >> 6;                                                      \
    reg = (addrbyte >> 3) & 7;                                                 \
    rm = addrbyte & 7;                                                         \
    switch (mode) {                                                            \
    case 0:                                                                    \
      if (rm == 6) {                                                           \
        disp16 = _read_code_u16();                                             \
      }                                                                        \
      if (((rm == 2) || (rm == 3)) && !segoverride) {                          \
        useseg = cpu_regs.ss;                                                  \
      }                                                                        \
      break;                                                                   \
                                                                               \
    case 1:                                                                    \
      disp16 = signext(_read_code_u8());                                       \
      if (((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) {             \
        useseg = cpu_regs.ss;                                                  \
      }                                                                        \
      break;                                                                   \
                                                                               \
    case 2:                                                                    \
      disp16 = _read_code_u16();                                               \
      if (((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) {             \
        useseg = cpu_regs.ss;                                                  \
      }                                                                        \
      break;                                                                   \
                                                                               \
    default:                                                                   \
      disp16 = 0;                                                              \
    }                                                                          \
  }

static void _default_intcall_handler(const int16_t intnum) {
  assert(!"Error");
}

static cpu_intcall_t _intcall_handler =
  (cpu_intcall_t)_default_intcall_handler;

void cpu_set_intcall_handler(cpu_intcall_t handler) {
  _intcall_handler = handler;
}

#define segbase(x) ((uint32_t)x << 4)

#define getmem8(x, y) read86(segbase(x) + y)
#define getmem16(x, y) readw86(segbase(x) + y)

#define putmem8(x, y, z) write86(segbase(x) + y, z)
#define putmem16(x, y, z) writew86(segbase(x) + y, z)

#define signext(value) ((int16_t)(int8_t)(value))
#define signext32(value) ((int32_t)(int16_t)(value))

static inline uint16_t _read_code_u16(void) {
  const uint16_t out = getmem16(cpu_regs.cs, cpu_regs.ip);
  cpu_regs.ip += 2;
  return out;
}

static inline uint8_t _read_code_u8(void) {
  const uint8_t out = getmem8(cpu_regs.cs, cpu_regs.ip);
  cpu_regs.ip += 1;
  return out;
}

void cpu_delay(uint32_t cycles) {
#if USE_DISK_DELAY
  _delay_cycles += cycles;
#endif
}

static inline uint16_t getsegreg(const int regid) {
  switch (regid) {
  case 0: return cpu_regs.es;
  case 1: return cpu_regs.cs;
  case 2: return cpu_regs.ss;
  case 3: return cpu_regs.ds;
  case 4: return cpu_regs.es;
  case 5: return cpu_regs.es;
  case 6: return cpu_regs.es;
  case 7: return cpu_regs.es;
  }
  UNREACHABLE();
}

static inline void putsegreg(const int regid, const uint16_t val) {
  switch (regid) {
  case 0: cpu_regs.es = val; return;
  case 1: cpu_regs.cs = val; return;
  case 2: cpu_regs.ss = val; return;
  case 3: cpu_regs.ds = val; return;
  case 4: cpu_regs.es = val; return;
  case 5: cpu_regs.es = val; return;
  case 6: cpu_regs.es = val; return;
  case 7: cpu_regs.es = val; return;
  }
  UNREACHABLE();
}

// set register based on mod-reg-rm bit layout
static inline void cpu_setreg8(const int regid, const uint8_t val) {
  switch (regid) {
  case 0: cpu_regs.al = val; return;
  case 1: cpu_regs.cl = val; return;
  case 2: cpu_regs.dl = val; return;
  case 3: cpu_regs.bl = val; return;
  case 4: cpu_regs.ah = val; return;
  case 5: cpu_regs.ch = val; return;
  case 6: cpu_regs.dh = val; return;
  case 7: cpu_regs.bh = val; return;
  }
  UNREACHABLE();
}

// set register based on mod-reg-rm bit index
static inline void cpu_setreg16(const int regid, const uint16_t val) {
  switch (regid) {
  case 0: cpu_regs.ax = val; return;
  case 1: cpu_regs.cx = val; return;
  case 2: cpu_regs.dx = val; return;
  case 3: cpu_regs.bx = val; return;
  case 4: cpu_regs.sp = val; return;
  case 5: cpu_regs.bp = val; return;
  case 6: cpu_regs.si = val; return;
  case 7: cpu_regs.di = val; return;
  }
  UNREACHABLE();
}

// get register based on mod-reg-rm bit index
static inline uint8_t cpu_getreg8(const int regid)  {
  switch (regid) {
  case 0: return cpu_regs.al;
  case 1: return cpu_regs.cl;
  case 2: return cpu_regs.dl;
  case 3: return cpu_regs.bl;
  case 4: return cpu_regs.ah;
  case 5: return cpu_regs.ch;
  case 6: return cpu_regs.dh;
  case 7: return cpu_regs.bh;
  }
  UNREACHABLE();
}

// get register based on mod-reg-rm bit index
static inline uint16_t cpu_getreg16(const int regid) {
  switch (regid) {
  case 0: return cpu_regs.ax;
  case 1: return cpu_regs.cx;
  case 2: return cpu_regs.dx;
  case 3: return cpu_regs.bx;
  case 4: return cpu_regs.sp;
  case 5: return cpu_regs.bp;
  case 6: return cpu_regs.si;
  case 7: return cpu_regs.di;
  }
  UNREACHABLE();
}

static const uint8_t parity[0x100] = {
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1};

static inline uint16_t makeflagsword(void) {
  return
    (cpu_flags.cf  <<  0) |
    (1             <<  1) |  // reserved
    (cpu_flags.pf  <<  2) |
    (cpu_flags.af  <<  4) |
    (cpu_flags.zf  <<  6) |
    (cpu_flags.sf  <<  7) |
    (cpu_flags.tf  <<  8) |
    (cpu_flags.ifl <<  9) |
    (cpu_flags.df  << 10) |
    (cpu_flags.of  << 11) |
    ((CPU <= CPU_186) ? 0x8000 : 0);
}

static inline void decodeflagsword(const uint16_t x) {
  cpu_flags.cf  = (x >>  0) & 1;
  cpu_flags.pf  = (x >>  2) & 1;
  cpu_flags.af  = (x >>  4) & 1;
  cpu_flags.zf  = (x >>  6) & 1;
  cpu_flags.sf  = (x >>  7) & 1;
  cpu_flags.tf  = (x >>  8) & 1;
  cpu_flags.ifl = (x >>  9) & 1;
  cpu_flags.df  = (x >> 10) & 1;
  cpu_flags.of  = (x >> 11) & 1;
}

static void flag_szp8(uint8_t value) {
  if (!value) {
    cpu_flags.zf = 1;
  } else {
    cpu_flags.zf = 0; /* set or clear zero flag */
  }

  if (value & 0x80) {
    cpu_flags.sf = 1;
  } else {
    cpu_flags.sf = 0; /* set or clear sign flag */
  }

  cpu_flags.pf = parity[value]; /* retrieve parity state from lookup table */
}

static void flag_szp16(uint16_t value) {
  if (!value) {
    cpu_flags.zf = 1;
  } else {
    cpu_flags.zf = 0; /* set or clear zero flag */
  }

  if (value & 0x8000) {
    cpu_flags.sf = 1;
  } else {
    cpu_flags.sf = 0; /* set or clear sign flag */
  }

  cpu_flags.pf = parity[value & 0xff]; /* retrieve parity state from lookup table */
}

static void flag_log8(uint8_t value) {
  flag_szp8(value);
  cpu_flags.cf = 0;
  cpu_flags.of = 0; /* bitwise logic ops always clear carry and overflow */
}

static void flag_log16(uint16_t value) {
  flag_szp16(value);
  cpu_flags.cf = 0;
  cpu_flags.of = 0; /* bitwise logic ops always clear carry and overflow */
}

static void flag_adc8(uint8_t v1, uint8_t v2, uint8_t v3) {

  /* v1 = destination operand, v2 = source operand, v3 = carry flag */
  uint16_t dst;

  dst = (uint16_t)v1 + (uint16_t)v2 + (uint16_t)v3;
  flag_szp8((uint8_t)dst);
  if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
    cpu_flags.of = 1;
  } else {
    cpu_flags.of = 0; /* set or clear overflow flag */
  }

  if (dst & 0xFF00) {
    cpu_flags.cf = 1;
  } else {
    cpu_flags.cf = 0; /* set or clear carry flag */
  }

  if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
    cpu_flags.af = 1;
  } else {
    cpu_flags.af = 0; /* set or clear auxilliary flag */
  }
}

static void flag_adc16(uint16_t v1, uint16_t v2, uint16_t v3) {

  const uint32_t dst = (uint32_t)v1 + (uint32_t)v2 + (uint32_t)v3;
  flag_szp16((uint16_t)dst);
  if ((((dst ^ v1) & (dst ^ v2)) & 0x8000) == 0x8000) {
    cpu_flags.of = 1;
  } else {
    cpu_flags.of = 0;
  }

  if (dst & 0xFFFF0000) {
    cpu_flags.cf = 1;
  } else {
    cpu_flags.cf = 0;
  }

  if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
    cpu_flags.af = 1;
  } else {
    cpu_flags.af = 0;
  }
}

static void flag_add8(uint8_t v1, uint8_t v2) {
  /* v1 = destination operand, v2 = source operand */
  uint16_t dst;

  dst = (uint16_t)v1 + (uint16_t)v2;
  flag_szp8((uint8_t)dst);
  if (dst & 0xFF00) {
    cpu_flags.cf = 1;
  } else {
    cpu_flags.cf = 0;
  }

  if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
    cpu_flags.of = 1;
  } else {
    cpu_flags.of = 0;
  }

  if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
    cpu_flags.af = 1;
  } else {
    cpu_flags.af = 0;
  }
}

static void flag_add16(uint16_t v1, uint16_t v2) {
  /* v1 = destination operand, v2 = source operand */
  uint32_t dst;

  dst = (uint32_t)v1 + (uint32_t)v2;
  flag_szp16((uint16_t)dst);
  if (dst & 0xFFFF0000) {
    cpu_flags.cf = 1;
  } else {
    cpu_flags.cf = 0;
  }

  if (((dst ^ v1) & (dst ^ v2) & 0x8000) == 0x8000) {
    cpu_flags.of = 1;
  } else {
    cpu_flags.of = 0;
  }

  if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
    cpu_flags.af = 1;
  } else {
    cpu_flags.af = 0;
  }
}

static void flag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3) {

  /* v1 = destination operand, v2 = source operand, v3 = carry flag */
  uint16_t dst;

  v2 += v3;
  dst = (uint16_t)v1 - (uint16_t)v2;
  flag_szp8((uint8_t)dst);
  if (dst & 0xFF00) {
    cpu_flags.cf = 1;
  } else {
    cpu_flags.cf = 0;
  }

  if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
    cpu_flags.of = 1;
  } else {
    cpu_flags.of = 0;
  }

  if ((v1 ^ v2 ^ dst) & 0x10) {
    cpu_flags.af = 1;
  } else {
    cpu_flags.af = 0;
  }
}

static void flag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3) {

  /* v1 = destination operand, v2 = source operand, v3 = carry flag */
  uint32_t dst;

  v2 += v3;
  dst = (uint32_t)v1 - (uint32_t)v2;
  flag_szp16((uint16_t)dst);
  if (dst & 0xFFFF0000) {
    cpu_flags.cf = 1;
  } else {
    cpu_flags.cf = 0;
  }

  if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
    cpu_flags.of = 1;
  } else {
    cpu_flags.of = 0;
  }

  if ((v1 ^ v2 ^ dst) & 0x10) {
    cpu_flags.af = 1;
  } else {
    cpu_flags.af = 0;
  }
}

static void flag_sub8(uint8_t v1, uint8_t v2) {

  /* v1 = destination operand, v2 = source operand */
  uint16_t dst;

  dst = (uint16_t)v1 - (uint16_t)v2;
  flag_szp8((uint8_t)dst);
  if (dst & 0xFF00) {
    cpu_flags.cf = 1;
  } else {
    cpu_flags.cf = 0;
  }

  if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
    cpu_flags.of = 1;
  } else {
    cpu_flags.of = 0;
  }

  if ((v1 ^ v2 ^ dst) & 0x10) {
    cpu_flags.af = 1;
  } else {
    cpu_flags.af = 0;
  }
}

static void flag_sub16(uint16_t v1, uint16_t v2) {

  /* v1 = destination operand, v2 = source operand */
  uint32_t dst;

  dst = (uint32_t)v1 - (uint32_t)v2;
  flag_szp16((uint16_t)dst);
  if (dst & 0xFFFF0000) {
    cpu_flags.cf = 1;
  } else {
    cpu_flags.cf = 0;
  }

  if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
    cpu_flags.of = 1;
  } else {
    cpu_flags.of = 0;
  }

  if ((v1 ^ v2 ^ dst) & 0x10) {
    cpu_flags.af = 1;
  } else {
    cpu_flags.af = 0;
  }
}

static void op_adc8() {
  res8 = oper1b + oper2b + cpu_flags.cf;
  flag_adc8(oper1b, oper2b, cpu_flags.cf);
}

static void op_adc16() {
  res16 = oper1 + oper2 + cpu_flags.cf;
  flag_adc16(oper1, oper2, cpu_flags.cf);
}

static void op_add8() {
  res8 = oper1b + oper2b;
  flag_add8(oper1b, oper2b);
}

static void op_add16() {
  res16 = oper1 + oper2;
  flag_add16(oper1, oper2);
}

static void op_and8() {
  res8 = oper1b & oper2b;
  flag_log8(res8);
}

static void op_and16() {
  res16 = oper1 & oper2;
  flag_log16(res16);
}

static void op_or8() {
  res8 = oper1b | oper2b;
  flag_log8(res8);
}

static void op_or16() {
  res16 = oper1 | oper2;
  flag_log16(res16);
}

static void op_xor8() {
  res8 = oper1b ^ oper2b;
  flag_log8(res8);
}

static void op_xor16() {
  res16 = oper1 ^ oper2;
  flag_log16(res16);
}

static void op_sub8() {
  res8 = oper1b - oper2b;
  flag_sub8(oper1b, oper2b);
}

static void op_sub16() {
  res16 = oper1 - oper2;
  flag_sub16(oper1, oper2);
}

static void op_sbb8() {
  res8 = oper1b - (oper2b + cpu_flags.cf);
  flag_sbb8(oper1b, oper2b, cpu_flags.cf);
}

static void op_sbb16() {
  res16 = oper1 - (oper2 + cpu_flags.cf);
  flag_sbb16(oper1, oper2, cpu_flags.cf);
}

static void getea(uint8_t rmval) {
  uint32_t tempea;
  tempea = 0;
  switch (mode) {
  case 0:
    switch (rmval) {
    case 0:
      tempea = (uint32_t)cpu_regs.bx + (uint32_t)cpu_regs.si;
      break;
    case 1:
      tempea = (uint32_t)cpu_regs.bx + (uint32_t)cpu_regs.di;
      break;
    case 2:
      tempea = cpu_regs.bp + cpu_regs.si;
      break;
    case 3:
      tempea = cpu_regs.bp + cpu_regs.di;
      break;
    case 4:
      tempea = cpu_regs.si;
      break;
    case 5:
      tempea = cpu_regs.di;
      break;
    case 6:
      tempea = disp16;
      break;
    case 7:
      tempea = cpu_regs.bx;
      break;
    }
    break;

  case 1:
  case 2:
    switch (rmval) {
    case 0:
      tempea = cpu_regs.bx + cpu_regs.si + disp16;
      break;
    case 1:
      tempea = cpu_regs.bx + cpu_regs.di + disp16;
      break;
    case 2:
      tempea = cpu_regs.bp + cpu_regs.si + disp16;
      break;
    case 3:
      tempea = cpu_regs.bp + cpu_regs.di + disp16;
      break;
    case 4:
      tempea = cpu_regs.si + disp16;
      break;
    case 5:
      tempea = cpu_regs.di + disp16;
      break;
    case 6:
      tempea = cpu_regs.bp + disp16;
      break;
    case 7:
      tempea = cpu_regs.bx + disp16;
      break;
    }
    break;
  }

  ea = (tempea & 0xFFFF) + (useseg << 4);
}

void cpu_push(uint16_t pushval) {
  cpu_regs.sp = cpu_regs.sp - 2;
  putmem16(cpu_regs.ss, cpu_regs.sp, pushval);
}

uint16_t cpu_pop() {
  uint16_t tempval = getmem16(cpu_regs.ss, cpu_regs.sp);
  cpu_regs.sp = cpu_regs.sp + 2;
  return tempval;
}

void cpu_reset() {
  log_printf(LOG_CHAN_CPU, "reset");
  cpu_regs.cs = 0xFFFF;
  cpu_regs.ip = 0x0000;
  in_hlt_state = false;
  _delay_cycles = 0;
}

static uint16_t readrm16(uint8_t rmval) {
  if (mode < 3) {
    getea(rmval);
    return readw86(ea);
  } else {
    return cpu_getreg16(rmval);
  }
}

static uint8_t readrm8(uint8_t rmval) {
  if (mode < 3) {
    getea(rmval);
    return read86(ea);
  } else {
    return cpu_getreg8(rmval);
  }
}

static void writerm16(uint8_t rmval, uint16_t value) {
  if (mode < 3) {
    getea(rmval);
    writew86(ea, value);
  } else {
    cpu_setreg16(rmval, value);
  }
}

// write to rm specified location
static void writerm8(uint8_t rmval, uint8_t value) {
  if (mode < 3) {
    getea(rmval);
    write86(ea, value);
  } else {
    cpu_setreg8(rmval, value);
  }
}

// 8 bit shifts
static uint8_t op_grp2_8(uint8_t cnt) {

  uint16_t s = oper1b;

  cnt = (cnt & 0x1f);
  
  switch (reg) {
  case 0: /* ROL r/m8 */
    for (int i = 1; i <= cnt; i++) {
      cpu_flags.cf = (s & 0x80) ? 1 : 0;
      s = (s << 1) | cpu_flags.cf;
    }
    if (cnt == 1) {
      cpu_flags.of = cpu_flags.cf ^ ((s & 0x80) ? 1 : 0);
    }
    return s & 0xff;

  case 1: /* ROR r/m8 */
    for (int i = 1; i <= cnt; i++) {
      cpu_flags.cf = s & 1;
      s = (s >> 1) | ((s & 1) ? 0x80 : 0);
    }
    if (cnt == 1) {
      cpu_flags.of = ((s ^ (s << 1)) & 0x80) ? 1 : 0;
    }
    return s & 0xff;

  case 2: /* RCL r/m8 */
    for (int i = 1; i <= cnt; i++) {
      const uint8_t c = cpu_flags.cf;
      cpu_flags.cf = (s & 0x80) ? 1 : 0;
      s = (s << 1) | c;
    }
    if (cnt == 1) {
      cpu_flags.of = cpu_flags.cf ^ ((s & 0x80) ? 1 : 0);
    }
    return s & 0xff;

  case 3: /* RCR r/m8 */
    for (int i = 1; i <= cnt; i++) {
      const uint8_t c = cpu_flags.cf;
      cpu_flags.cf = s & 1;
      s = (s >> 1) | (c ? 0x80 : 0);
    }
    if (cnt == 1) {
      cpu_flags.of = ((s ^ (s << 1)) & 0x80) ? 1 : 0;
    }
    return s & 0xff;

  case 4: /* SHL r/m8 */
    if (cnt != 0) {
      for (int i = 1; i <= cnt; i++) {
        cpu_flags.cf = (s & 0x80) ? 1 : 0;
        s = (s << 1) & 0xFF;
      }
      if (cnt == 1) {
        cpu_flags.of = (cpu_flags.cf != ((s & 0x80) ? 1 : 0));
      }
      flag_szp8((uint8_t)s);
    }
    return s & 0xff;

  case 5: /* SHR r/m8 */
    if (cnt != 0) {
      cpu_flags.of = ((cnt == 1) && (s & 0x80)) ? 1 : 0;
      for (int i = 1; i <= cnt; i++) {
        cpu_flags.cf = s & 1;
        s = s >> 1;
      }
      flag_szp8((uint8_t)s);
    }
    return s & 0xff;

  case 6:
    UNREACHABLE();

  case 7: /* SAR r/m8 */
    for (int i = 1; i <= cnt; i++) {
      cpu_flags.cf = s & 1;
      s = (s >> 1) | (s & 0x80);
    }
    cpu_flags.of = 0;
    flag_szp8((uint8_t)s);
    return s & 0xff;

  default:
    UNREACHABLE();
  }
}

// 16 bit shifts
static uint16_t op_grp2_16(uint8_t cnt) {

  uint32_t s = oper1;

  cnt = (cnt & 0x1f) % 17;

  switch (reg) {
  case 0: /* ROL r/m8 */
    for (int i = 1; i <= cnt; i++) {
      cpu_flags.cf = (s & 0x8000) ? 1 : 0;
      s = (s << 1) | (cpu_flags.cf);
    }
    if (cnt == 1) {
      cpu_flags.of = cpu_flags.cf ^ ((s & 0x8000) ? 1 : 0);
    }
    return s & 0xffff;

  case 1: /* ROR r/m8 */
    for (int i = 1; i <= cnt; i++) {
      cpu_flags.cf = s & 1;
      s = (s >> 1) | ((s & 1) ? 0x8000 : 0);
    }
    if (cnt == 1) {
      cpu_flags.of = ((s ^ (s << 1)) & 0x8000) ? 1 : 0;
    }
    return s & 0xffff;

  case 2: /* RCL r/m8 */
    for (int i = 1; i <= cnt; i++) {
      const uint16_t c = cpu_flags.cf;
      cpu_flags.cf = (s & 0x8000) ? 1 : 0;
      s = (s << 1) | c;
    }
    if (cnt == 1) {
      cpu_flags.of = cpu_flags.cf ^ ((s & 0x8000) ? 1 : 0);
    }
    return s & 0xffff;

  case 3: /* RCR r/m8 */
    for (int i = 1; i <= cnt; i++) {
      const uint16_t c = cpu_flags.cf;
      cpu_flags.cf = s & 1;
      s = (s >> 1) | (c ? 0x8000 : 0);
    }
    if (cnt == 1) {
      cpu_flags.of = ((s ^ (s << 1)) & 0x8000) ? 1 : 0;
    }
    return s & 0xffff;

  case 4: /* SHL r/m8 */
    if (cnt != 0) {
      for (int i = 1; i <= cnt; i++) {
        cpu_flags.cf = (s & 0x8000) ? 1 : 0;
        s = (s << 1) & 0xFFFF;
      }
      if (cnt == 1) {
        cpu_flags.of = (cpu_flags.cf != ((s & 0x8000) ? 1 : 0));
      }
      flag_szp16((uint16_t)s);
    }
    return s & 0xffff;

  case 5: /* SHR r/m8 */
    if (cnt != 0) {
      cpu_flags.of = ((cnt == 1) && (s & 0x8000)) ? 1 : 0;
      for (int i = 1; i <= cnt; i++) {
        cpu_flags.cf = s & 1;
        s = s >> 1;
      }
      flag_szp16((uint16_t)s);
    }
    return s & 0xffff;

  case 6:
    UNREACHABLE();

  case 7: /* SAR r/m8 */
    for (int i = 1; i <= cnt; i++) {
      cpu_flags.cf = s & 1;
      s = (s >> 1) | (s & 0x8000);
    }
    cpu_flags.of = 0;
    flag_szp16((uint16_t)s);
    return s & 0xffff;

  default:
    UNREACHABLE();
  }
}

static void op_div8(uint16_t valdiv, uint8_t divisor) {
  if (divisor == 0) {
    _intcall_handler(0);
    return;
  }

  if ((valdiv / (uint16_t)divisor) > 0xFF) {
    _intcall_handler(0);
    return;
  }

  cpu_regs.ah = valdiv % (uint16_t)divisor;
  cpu_regs.al = valdiv / (uint16_t)divisor;
}

static void op_idiv8(uint16_t valdiv, uint8_t divisor) {

  if (divisor == 0) {
    _intcall_handler(0);
    return;
  }

  uint16_t s1 = valdiv;
  uint16_t s2 = divisor;
  int sign = (((s1 ^ s2) & 0x8000) != 0);
  s1 = (s1 < 0x8000) ? s1 : ((~s1 + 1) & 0xffff);
  s2 = (s2 < 0x8000) ? s2 : ((~s2 + 1) & 0xffff);
  uint16_t d1 = s1 / s2;
  uint16_t d2 = s1 % s2;
  if (d1 & 0xFF00) {
    _intcall_handler(0);
    return;
  }

  if (sign) {
    d1 = (~d1 + 1) & 0xff;
    d2 = (~d2 + 1) & 0xff;
  }

  cpu_regs.ah = (uint8_t)d2;
  cpu_regs.al = (uint8_t)d1;
}

// opcode group 0xF6 ...
static void op_grp3_8() {
  oper1 = signext(oper1b);
  oper2 = signext(oper2b);
  switch (reg) {
  case 0:
  case 1: /* TEST */
    {
      const uint8_t opr = _read_code_u8();
      flag_log8(oper1b & opr);
    }
    break;

  case 2: /* NOT */
    res8 = ~oper1b;
    break;

  case 3: /* NEG */
    res8 = (~oper1b) + 1;
    flag_sub8(0, oper1b);
    if (res8 == 0) {
      cpu_flags.cf = 0;
    } else {
      cpu_flags.cf = 1;
    }
    break;

  case 4: /* MUL */
  {
    const uint32_t t = (uint32_t)oper1b * (uint32_t)cpu_regs.al;
    cpu_regs.ax = t & 0xFFFF;
    flag_szp8((uint8_t)t);
    cpu_flags.cf = cpu_flags.of = (cpu_regs.ah ? 1 : 0);
#ifdef CPU_CLEAR_ZF_ON_MUL
    cpu_flags.zf = 0;
#endif
  }
    break;

  case 5: /* IMUL */
  {
    const int16_t x = signext(oper1b);
    const int16_t y = signext(cpu_regs.al);
    const int16_t z = x * y;
    cpu_regs.ax = (uint16_t)z;
#if MUL_EXT
    cpu_flags.cf = cpu_flags.of =
      (cpu_regs.ah != 0x00 && cpu_regs.ah != 0xff);
#else
    cpu_flags.cf = cpu_flags.of =
      (cpu_regs.ah != 0xff);
#endif
#ifdef CPU_CLEAR_ZF_ON_MUL
    cpu_flags.zf = 0;
#endif
  }
    break;

  case 6: /* DIV */
    op_div8(cpu_regs.ax, oper1b);
    break;

  case 7: /* IDIV */
    op_idiv8(cpu_regs.ax, oper1b);
    break;
  }
}

static void op_div16(uint32_t valdiv, uint16_t divisor) {
  if (divisor == 0) {
    _intcall_handler(0);
    return;
  }

  if ((valdiv / (uint32_t)divisor) > 0xFFFF) {
    _intcall_handler(0);
    return;
  }

  cpu_regs.dx = valdiv % (uint32_t)divisor;
  cpu_regs.ax = valdiv / (uint32_t)divisor;
}

static void op_idiv16(uint32_t valdiv, uint16_t divisor) {

  uint32_t d1;
  uint32_t d2;
  uint32_t s1;
  uint32_t s2;
  int sign;

  if (divisor == 0) {
    _intcall_handler(0);
    return;
  }

  s1 = valdiv;
  s2 = divisor;
  s2 = (s2 & 0x8000) ? (s2 | 0xffff0000) : s2;
  sign = (((s1 ^ s2) & 0x80000000) != 0);
  s1 = (s1 < 0x80000000) ? s1 : ((~s1 + 1) & 0xffffffff);
  s2 = (s2 < 0x80000000) ? s2 : ((~s2 + 1) & 0xffffffff);
  d1 = s1 / s2;
  d2 = s1 % s2;
  if (d1 & 0xFFFF0000) {
    _intcall_handler(0);
    return;
  }

  if (sign) {
    d1 = (~d1 + 1) & 0xffff;
    d2 = (~d2 + 1) & 0xffff;
  }

  cpu_regs.ax = d1;
  cpu_regs.dx = d2;
}

static void op_grp3_16() {
  switch (reg) {
  case 0:
  case 1: /* TEST */
  {
    const uint16_t opr = _read_code_u16();
    flag_log16(oper1 & opr);
  }
    break;

  case 2: /* NOT */
    res16 = ~oper1;
    break;

  case 3: /* NEG */
    res16 = (~oper1) + 1;
    flag_sub16(0, oper1);
    if (res16) {
      cpu_flags.cf = 1;
    } else {
      cpu_flags.cf = 0;
    }
    break;

  case 4: /* MUL */
    temp1 = (uint32_t)oper1 * (uint32_t)cpu_regs.ax;
    cpu_regs.ax = temp1 & 0xFFFF;
    cpu_regs.dx = temp1 >> 16;
    flag_szp16((uint16_t)temp1);
    cpu_flags.cf = cpu_flags.of = (cpu_regs.dx ? 1 : 0);
#ifdef CPU_CLEAR_ZF_ON_MUL
    cpu_flags.zf = 0;
#endif
    break;

  case 5: /* IMUL */
    temp1 = signext32(cpu_regs.ax);
    temp2 = signext32(oper1);
    temp3 = temp1 * temp2;
    cpu_regs.ax = temp3 & 0xFFFF; /* into register ax */
    cpu_regs.dx = temp3 >> 16;    /* into register dx */
    cpu_flags.cf = cpu_flags.of = (cpu_regs.dx != 0x0000 && cpu_regs.dx != 0xffff);
#ifdef CPU_CLEAR_ZF_ON_MUL
    cpu_flags.zf = 0;
#endif
    break;

  case 6: /* DIV */
    op_div16(((uint32_t)cpu_regs.dx << 16) + cpu_regs.ax,
             oper1);
    break;

  case 7: /* DIV */
    op_idiv16(((uint32_t)cpu_regs.dx << 16) + cpu_regs.ax,
              oper1);
    break;
  }
}

static void op_grp5() {
  int tempcf = cpu_flags.cf;
  switch (reg) {
  case 0: /* INC Ev */
    oper2 = 1;
    op_add16();
    cpu_flags.cf = tempcf;
    writerm16(rm, res16);
    break;

  case 1: /* DEC Ev */
    oper2 = 1;
    op_sub16();
    cpu_flags.cf = tempcf;
    writerm16(rm, res16);
    break;

  case 2: /* CALL Ev */
    cpu_push(cpu_regs.ip);
    cpu_regs.ip = oper1;
    break;

  case 3: /* CALL Mp */
    cpu_push(cpu_regs.cs);
    cpu_push(cpu_regs.ip);
    getea(rm);
    cpu_regs.ip = readw86(ea + 0);
    cpu_regs.cs = readw86(ea + 2);
    break;

  case 4: /* JMP Ev */
    cpu_regs.ip = oper1;
    break;

  case 5: /* JMP Mp */
    getea(rm);
    cpu_regs.ip = readw86(ea + 0);
    cpu_regs.cs = readw86(ea + 2);
    break;

  case 6: /* PUSH Ev */
    cpu_push(oper1);
    break;
  }
}

bool cpu_in_hlt_state(void) {
  return in_hlt_state;
}

static void _on_illegal_instruction(void) {
#ifdef CPU_ALLOW_ILLEGAL_OP_EXCEPTION
  // trip invalid opcode exception (this occurs on the 80186+,
  // 8086/8088 CPUs treat them as NOPs.
  _intcall_handler(6);
  // technically they aren't exactly like NOPs in most cases,
  // but for our pursoses, that's accurate enough.
#endif
  log_printf(LOG_CHAN_CPU, "unknown opcode:");
  log_printf(LOG_CHAN_CPU, "  @ %04x:%04x", savecs, saveip);

  for (int i = 0; i < 5; ++i) {
    log_printf(LOG_CHAN_CPU, "  %02x", getmem8(savecs, saveip + i));
  }

#ifndef NDEBUG
//  __debugbreak();
#endif
}

// cycles is target cycles
// return executed cycles
int32_t cpu_exec86(int32_t target) {

  if (target == 0) {
    return 0;
  }

  static uint16_t trap_toggle = 0;
  _cycles = 0;

  const bool in_cpu_halt = cpu_halt;

  while (cpu_running && _cycles < target) {

    if (in_cpu_halt != cpu_halt) {
      break;
    }

#if 0
    const uint32_t eip = (cpu_regs.cs << 4) + cpu_regs.ip;
    if (false && eip == 0x96b1 && !cpu_halt) {
      cpu_halt = true;
      log_printf(LOG_CHAN_CPU, "cpu exec breakpoint hit");
      break;
    }
#endif

    // if trap is asserted
    if (trap_toggle) {
      _intcall_handler(1);
    }

    trap_toggle = cpu_flags.tf;

    const bool pending_irq = cpu_flags.ifl && (i8259.irr & (~i8259.imr));
    if (!trap_toggle && pending_irq) {
      in_hlt_state = false;
      const int next_int = i8259_nextintr();
      // get next interrupt from the i8259, if any
      _intcall_handler(next_int);
    }

    if (in_hlt_state) {
      _cycles = target;
      break;
    }

#if USE_DISK_DELAY
    // if needed, delay when we are not handling an interupt
    if (_delay_cycles) {
      --_delay_cycles;
      if (!cpu_flags.ifl) {
        ++_cycles;
        continue;
      }
    }
#endif

    if (USE_CPU_REDUX) {
      if (cpu_redux_exec()) {
        ++_cycles;
        continue;
      }
    }

    reptype = 0;
    segoverride = false;
    useseg = cpu_regs.ds;
    uint8_t docontinue = 0;
    const uint16_t firstip = cpu_regs.ip;

    // handle prefix bytes
    while (!docontinue) {
      cpu_regs.cs &= 0xFFFF;
      cpu_regs.ip &= 0xFFFF;
      savecs = cpu_regs.cs;
      saveip = cpu_regs.ip;
      opcode = _read_code_u8();

      switch (opcode) {
      /* segment prefix check */
      case 0x2E: /* segment cpu_regs.cs */
        useseg = cpu_regs.cs;
        segoverride = true;
        break;

      case 0x3E: /* segment cpu_regs.ds */
        useseg = cpu_regs.ds;
        segoverride = true;
        break;

      case 0x26: /* segment cpu_regs.es */
        useseg = cpu_regs.es;
        segoverride = true;
        break;

      case 0x36: /* segment cpu_regs.ss */
        useseg = cpu_regs.ss;
        segoverride = true;
        break;

      /* repetition prefix check */
      case 0xF3: /* REP/REPE/REPZ */
        reptype = 1;
        break;

      case 0xF2: /* REPNE/REPNZ */
        reptype = 2;
        break;

      default:
        docontinue = 1;
        break;
      }
    } // while

    ++_cycles;

    switch (opcode) {
    case 0x0: /* 00 ADD Eb Gb */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = cpu_getreg8(reg);
      op_add8();
      writerm8(rm, res8);
      break;

    case 0x1: /* 01 ADD Ev Gv */
      modregrm();
      oper1 = readrm16(rm);
      oper2 = cpu_getreg16(reg);
      op_add16();
      writerm16(rm, res16);
      break;

    case 0x2: /* 02 ADD Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      oper2b = readrm8(rm);
      op_add8();
      cpu_setreg8(reg, res8);
      break;

    case 0x3: /* 03 ADD Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      oper2 = readrm16(rm);
      op_add16();
      cpu_setreg16(reg, res16);
      break;

    case 0x4: /* 04 ADD cpu_regs.al] Ib */
      oper1b = cpu_regs.al;
      oper2b = _read_code_u8();
      op_add8();
      cpu_regs.al = res8;
      break;

    case 0x5: /* 05 ADD eAX Iv */
      oper1 = cpu_regs.ax;
      oper2 = _read_code_u16();
      op_add16();
      cpu_regs.ax = res16;
      break;

    case 0x6: /* 06 PUSH cpu_regs.es */
      if (cpu_regs.cs == 0xD800) {
        __debugbreak();
      }
      cpu_push(cpu_regs.es);
      break;

    case 0x7: /* 07 POP cpu_regs.es */
      cpu_regs.es = cpu_pop();
      break;

    case 0x8: /* 08 OR Eb Gb */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = cpu_getreg8(reg);
      op_or8();
      writerm8(rm, res8);
      break;

    case 0x9: /* 09 OR Ev Gv */
      modregrm();
      oper1 = readrm16(rm);
      oper2 = cpu_getreg16(reg);
      op_or16();
      writerm16(rm, res16);
      break;

    case 0xA: /* 0A OR Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      oper2b = readrm8(rm);
      op_or8();
      cpu_setreg8(reg, res8);
      break;

    case 0xB: /* 0B OR Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      oper2 = readrm16(rm);
      op_or16();
      cpu_setreg16(reg, res16);
      break;

    case 0xC: /* 0C OR cpu_regs.al Ib */
      oper1b = cpu_regs.al;
      oper2b = _read_code_u8();
      op_or8();
      cpu_regs.al = res8;
      break;

    case 0xD: /* 0D OR eAX Iv */
      oper1 = cpu_regs.ax;
      oper2 = _read_code_u16();
      op_or16();
      cpu_regs.ax = res16;
      break;

    case 0xE: /* 0E PUSH cpu_regs.cs */
      cpu_push(cpu_regs.cs);
      break;

#ifdef CPU_ALLOW_POP_CS // only the 8086/8088 does this.
    case 0xF: // 0F POP CS
      cpu_regs.cs = cpu_pop();
      break;
#endif

    case 0x10: /* 10 ADC Eb Gb */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = cpu_getreg8(reg);
      op_adc8();
      writerm8(rm, res8);
      break;

    case 0x11: /* 11 ADC Ev Gv */
      modregrm();
      oper1 = readrm16(rm);
      oper2 = cpu_getreg16(reg);
      op_adc16();
      writerm16(rm, res16);
      break;

    case 0x12: /* 12 ADC Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      oper2b = readrm8(rm);
      op_adc8();
      cpu_setreg8(reg, res8);
      break;

    case 0x13: /* 13 ADC Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      oper2 = readrm16(rm);
      op_adc16();
      cpu_setreg16(reg, res16);
      break;

    case 0x14: /* 14 ADC cpu_regs.al Ib */
      oper1b = cpu_regs.al;
      oper2b = _read_code_u8();
      op_adc8();
      cpu_regs.al = res8;
      break;

    case 0x15: /* 15 ADC eAX Iv */
      oper1 = cpu_regs.ax;
      oper2 = _read_code_u16();
      op_adc16();
      cpu_regs.ax = res16;
      break;

    case 0x16: /* 16 PUSH cpu_regs.ss */
      cpu_push(cpu_regs.ss);
      break;

    case 0x17: /* 17 POP cpu_regs.ss */
      cpu_regs.ss = cpu_pop();
      break;

    case 0x18: /* 18 SBB Eb Gb */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = cpu_getreg8(reg);
      op_sbb8();
      writerm8(rm, res8);
      break;

    case 0x19: /* 19 SBB Ev Gv */
      modregrm();
      oper1 = readrm16(rm);
      oper2 = cpu_getreg16(reg);
      op_sbb16();
      writerm16(rm, res16);
      break;

    case 0x1A: /* 1A SBB Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      oper2b = readrm8(rm);
      op_sbb8();
      cpu_setreg8(reg, res8);
      break;

    case 0x1B: /* 1B SBB Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      oper2 = readrm16(rm);
      op_sbb16();
      cpu_setreg16(reg, res16);
      break;

    case 0x1C: /* 1C SBB cpu_regs.al Ib */
      oper1b = cpu_regs.al;
      oper2b = _read_code_u8();
      op_sbb8();
      cpu_regs.al = res8;
      break;

    case 0x1D: /* 1D SBB eAX Iv */
      oper1 = cpu_regs.ax;
      oper2 = _read_code_u16();
      op_sbb16();
      cpu_regs.ax = res16;
      break;

    case 0x1E: /* 1E PUSH cpu_regs.ds */
      cpu_push(cpu_regs.ds);
      break;

    case 0x1F: /* 1F POP cpu_regs.ds */
      cpu_regs.ds = cpu_pop();
      break;

    case 0x20: /* 20 AND Eb Gb */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = cpu_getreg8(reg);
      op_and8();
      writerm8(rm, res8);
      break;

    case 0x21: /* 21 AND Ev Gv */
      modregrm();
      oper1 = readrm16(rm);
      oper2 = cpu_getreg16(reg);
      op_and16();
      writerm16(rm, res16);
      break;

    case 0x22: /* 22 AND Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      oper2b = readrm8(rm);
      op_and8();
      cpu_setreg8(reg, res8);
      break;

    case 0x23: /* 23 AND Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      oper2 = readrm16(rm);
      op_and16();
      cpu_setreg16(reg, res16);
      break;

    case 0x24: /* 24 AND cpu_regs.al] Ib */
      oper1b = cpu_regs.al;
      oper2b = _read_code_u8();
      op_and8();
      cpu_regs.al = res8;
      break;

    case 0x25: /* 25 AND eAX Iv */
      oper1 = cpu_regs.ax;
      oper2 = _read_code_u16();
      op_and16();
      cpu_regs.ax = res16;
      break;

    case 0x27: /* 27 DAA */
      {
        const uint8_t c = cpu_flags.cf;
        const uint8_t al = cpu_regs.al;
        if (((cpu_regs.al & 0xF) > 9) || (cpu_flags.af == 1)) {
          const uint16_t temp = cpu_regs.al + 6;
          cpu_regs.al = temp & 0xff;
          cpu_flags.cf = c | (temp > 255);
        }
        cpu_flags.cf = 0;
        if (al > 0x99 || c == 1) {
          cpu_regs.al += 0x60;
          cpu_flags.cf = 1;
        }
        flag_szp8(cpu_regs.al);
      }
      break;

    case 0x28: /* 28 SUB Eb Gb */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = cpu_getreg8(reg);
      op_sub8();
      writerm8(rm, res8);
      break;

    case 0x29: /* 29 SUB Ev Gv */
      modregrm();
      oper1 = readrm16(rm);
      oper2 = cpu_getreg16(reg);
      op_sub16();
      writerm16(rm, res16);
      break;

    case 0x2A: /* 2A SUB Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      oper2b = readrm8(rm);
      op_sub8();
      cpu_setreg8(reg, res8);
      break;

    case 0x2B: /* 2B SUB Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      oper2 = readrm16(rm);
      op_sub16();
      cpu_setreg16(reg, res16);
      break;

    case 0x2C: /* 2C SUB cpu_regs.al Ib */
      oper1b = cpu_regs.al;
      oper2b = _read_code_u8();
      op_sub8();
      cpu_regs.al = res8;
      break;

    case 0x2D: /* 2D SUB eAX Iv */
      oper1 = cpu_regs.ax;
      oper2 = _read_code_u16();
      op_sub16();
      cpu_regs.ax = res16;
      break;

    case 0x2F: /* 2F DAS */
      if (((cpu_regs.al & 15) > 9) || (cpu_flags.af == 1)) {
        oper1 = cpu_regs.al - 6;
        cpu_regs.al = oper1 & 255;
        if (oper1 & 0xFF00) {
          cpu_flags.cf = 1;
        } else {
          cpu_flags.cf = 0;
        }
        cpu_flags.af = 1;
      } else {
        cpu_flags.af = 0;
      }
      if (((cpu_regs.al & 0xF0) > 0x90) || cpu_flags.cf) {
        cpu_regs.al = cpu_regs.al - 0x60;
        cpu_flags.cf = 1;
      } else {
        cpu_flags.cf = 0;
      }
      flag_szp8(cpu_regs.al);
      break;

    case 0x30: /* 30 XOR Eb Gb */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = cpu_getreg8(reg);
      op_xor8();
      writerm8(rm, res8);
      break;

    case 0x31: /* 31 XOR Ev Gv */
      modregrm();
      oper1 = readrm16(rm);
      oper2 = cpu_getreg16(reg);
      op_xor16();
      writerm16(rm, res16);
      break;

    case 0x32: /* 32 XOR Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      oper2b = readrm8(rm);
      op_xor8();
      cpu_setreg8(reg, res8);
      break;

    case 0x33: /* 33 XOR Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      oper2 = readrm16(rm);
      op_xor16();
      cpu_setreg16(reg, res16);
      break;

    case 0x34: /* 34 XOR cpu_regs.al Ib */
      oper1b = cpu_regs.al;
      oper2b = _read_code_u8();
      op_xor8();
      cpu_regs.al = res8;
      break;

    case 0x35: /* 35 XOR eAX Iv */
      oper1 = cpu_regs.ax;
      oper2 = _read_code_u16();
      op_xor16();
      cpu_regs.ax = res16;
      break;

    case 0x37: /* 37 AAA ASCII */
      if (((cpu_regs.al & 0xF) > 9) || (cpu_flags.af == 1)) {
        cpu_regs.al = cpu_regs.al + 6;
        cpu_regs.ah = cpu_regs.ah + 1;
        cpu_flags.af = 1;
        cpu_flags.cf = 1;
      } else {
        cpu_flags.af = 0;
        cpu_flags.cf = 0;
      }
      cpu_regs.al = cpu_regs.al & 0xF;
      break;

    case 0x38: /* 38 CMP Eb Gb */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = cpu_getreg8(reg);
      flag_sub8(oper1b, oper2b);
      break;

    case 0x39: /* 39 CMP Ev Gv */
      modregrm();
      oper1 = readrm16(rm);
      oper2 = cpu_getreg16(reg);
      flag_sub16(oper1, oper2);
      break;

    case 0x3A: /* 3A CMP Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      oper2b = readrm8(rm);
      flag_sub8(oper1b, oper2b);
      break;

    case 0x3B: /* 3B CMP Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      oper2 = readrm16(rm);
      flag_sub16(oper1, oper2);
      break;

    case 0x3C: /* 3C CMP cpu_regs.al Ib */
      oper1b = cpu_regs.al;
      oper2b = _read_code_u8();
      flag_sub8(oper1b, oper2b);
      break;

    case 0x3D: /* 3D CMP eAX Iv */
      oper1 = cpu_regs.ax;
      oper2 = _read_code_u16();
      flag_sub16(oper1, oper2);
      break;

    case 0x3F: /* 3F AAS ASCII */
      if (((cpu_regs.al & 0xF) > 9) || (cpu_flags.af == 1)) {
        cpu_regs.al = cpu_regs.al - 6;
        cpu_regs.ah = cpu_regs.ah - 1;
        cpu_flags.af = 1;
        cpu_flags.cf = 1;
      } else {
        cpu_flags.af = 0;
        cpu_flags.cf = 0;
      }
      cpu_regs.al = cpu_regs.al & 0xF;
      break;

    case 0x40: /* 40 INC eAX */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.ax;
      oper2 = 1;
      op_add16();
      cpu_flags.cf = oldcf;
      cpu_regs.ax = res16;
    }
      break;

    case 0x41: /* 41 INC eCX */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.cx;
      oper2 = 1;
      op_add16();
      cpu_flags.cf = oldcf;
      cpu_regs.cx = res16;
    }
      break;

    case 0x42: /* 42 INC eDX */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.dx;
      oper2 = 1;
      op_add16();
      cpu_flags.cf = oldcf;
      cpu_regs.dx = res16;
    }
      break;

    case 0x43: /* 43 INC eBX */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.bx;
      oper2 = 1;
      op_add16();
      cpu_flags.cf = oldcf;
      cpu_regs.bx = res16;
    }
      break;

    case 0x44: /* 44 INC eSP */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.sp;
      oper2 = 1;
      op_add16();
      cpu_flags.cf = oldcf;
      cpu_regs.sp = res16;
    }
      break;

    case 0x45: /* 45 INC eBP */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.bp;
      oper2 = 1;
      op_add16();
      cpu_flags.cf = oldcf;
      cpu_regs.bp = res16;
    }
      break;

    case 0x46: /* 46 INC eSI */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.si;
      oper2 = 1;
      op_add16();
      cpu_flags.cf = oldcf;
      cpu_regs.si = res16;
    }
      break;

    case 0x47: /* 47 INC eDI */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.di;
      oper2 = 1;
      op_add16();
      cpu_flags.cf = oldcf;
      cpu_regs.di = res16;
    }
      break;

    case 0x48: /* 48 DEC eAX */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.ax;
      oper2 = 1;
      op_sub16();
      cpu_flags.cf = oldcf;
      cpu_regs.ax = res16;
    }
      break;

    case 0x49: /* 49 DEC eCX */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.cx;
      oper2 = 1;
      op_sub16();
      cpu_flags.cf = oldcf;
      cpu_regs.cx = res16;
    }
      break;

    case 0x4A: /* 4A DEC eDX */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.dx;
      oper2 = 1;
      op_sub16();
      cpu_flags.cf = oldcf;
      cpu_regs.dx = res16;
    }
      break;

    case 0x4B: /* 4B DEC eBX */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.bx;
      oper2 = 1;
      op_sub16();
      cpu_flags.cf = oldcf;
      cpu_regs.bx = res16;
    }
      break;

    case 0x4C: /* 4C DEC eSP */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.sp;
      oper2 = 1;
      op_sub16();
      cpu_flags.cf = oldcf;
      cpu_regs.sp = res16;
    }
      break;

    case 0x4D: /* 4D DEC eBP */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.bp;
      oper2 = 1;
      op_sub16();
      cpu_flags.cf = oldcf;
      cpu_regs.bp = res16;
    }
      break;

    case 0x4E: /* 4E DEC eSI */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.si;
      oper2 = 1;
      op_sub16();
      cpu_flags.cf = oldcf;
      cpu_regs.si = res16;
    }
      break;

    case 0x4F: /* 4F DEC eDI */
    {
      const uint8_t oldcf = cpu_flags.cf;
      oper1 = cpu_regs.di;
      oper2 = 1;
      op_sub16();
      cpu_flags.cf = oldcf;
      cpu_regs.di = res16;
    }
      break;

    case 0x50: /* 50 PUSH eAX */
      cpu_push(cpu_regs.ax);
      break;

    case 0x51: /* 51 PUSH eCX */
      cpu_push(cpu_regs.cx);
      break;

    case 0x52: /* 52 PUSH eDX */
      cpu_push(cpu_regs.dx);
      break;

    case 0x53: /* 53 PUSH eBX */
      cpu_push(cpu_regs.bx);
      break;

    case 0x54: /* 54 PUSH eSP */
#ifdef CPU_USE_286_STYLE_PUSH_SP
      cpu_push(cpu_regs.sp);
#else
      cpu_push(cpu_regs.sp - 2);
#endif
      break;

    case 0x55: /* 55 PUSH eBP */
      cpu_push(cpu_regs.bp);
      break;

    case 0x56: /* 56 PUSH eSI */
      cpu_push(cpu_regs.si);
      break;

    case 0x57: /* 57 PUSH eDI */
      cpu_push(cpu_regs.di);
      break;

    case 0x58: /* 58 POP eAX */
      cpu_regs.ax = cpu_pop();
      break;

    case 0x59: /* 59 POP eCX */
      cpu_regs.cx = cpu_pop();
      break;

    case 0x5A: /* 5A POP eDX */
      cpu_regs.dx = cpu_pop();
      break;

    case 0x5B: /* 5B POP eBX */
      cpu_regs.bx = cpu_pop();
      break;

    case 0x5C: /* 5C POP eSP */
      cpu_regs.sp = cpu_pop();
      break;

    case 0x5D: /* 5D POP eBP */
      cpu_regs.bp = cpu_pop();
      break;

    case 0x5E: /* 5E POP eSI */
      cpu_regs.si = cpu_pop();
      break;

    case 0x5F: /* 5F POP eDI */
      cpu_regs.di = cpu_pop();
      break;

#if (CPU != CPU_8086)
    case 0x60: /* 60 PUSHA (80186+) */
    {
      const uint16_t sp = cpu_regs.sp;
      cpu_push(cpu_regs.ax);
      cpu_push(cpu_regs.cx);
      cpu_push(cpu_regs.dx);
      cpu_push(cpu_regs.bx);
      cpu_push(sp);
      cpu_push(cpu_regs.bp);
      cpu_push(cpu_regs.si);
      cpu_push(cpu_regs.di);
    }
      break;

    case 0x61: /* 61 POPA (80186+) */
      cpu_regs.di = cpu_pop();
      cpu_regs.si = cpu_pop();
      cpu_regs.bp = cpu_pop();
      cpu_pop();
      cpu_regs.bx = cpu_pop();
      cpu_regs.dx = cpu_pop();
      cpu_regs.cx = cpu_pop();
      cpu_regs.ax = cpu_pop();
      break;

    case 0x62: /* 62 BOUND Gv, Ev (80186+) */
      modregrm();
      getea(rm);
      if (signext32(cpu_getreg16(reg)) < signext32(getmem16(ea >> 4, ea & 15))) {
        _intcall_handler(5); // bounds check exception
      } else {
        ea += 2;
        if (signext32(cpu_getreg16(reg)) > signext32(getmem16(ea >> 4, ea & 15))) {
          _intcall_handler(5); // bounds check exception
        }
      }
      break;

    case 0x68: /* 68 PUSH Iv (80186+) */
      cpu_push(_read_code_u16());
      break;

    case 0x69: /* 69 IMUL Gv Ev Iv (80186+) */
      // https://c9x.me/x86/html/file_module_x86_id_138.html
    {
      modregrm();
      const int16_t t1 = readrm16(rm);
      const int16_t t2 = _read_code_u16();
      const int32_t t3 = (int32_t)t1 * (int32_t)t2;
      const int16_t t4 = t1 * t2;
      cpu_setreg16(reg, t3 & 0xFFFF);
      if (t3 != t4) {
        cpu_flags.cf = 1;
        cpu_flags.of = 1;
      } else {
        cpu_flags.cf = 0;
        cpu_flags.of = 0;
      }
    }
      break;

    case 0x6A: /* 6A PUSH Ib (80186+) */
      cpu_push(_read_code_u8());
      break;

    case 0x6B: /* 6B IMUL Gv Eb Ib (80186+) */
      // https://c9x.me/x86/html/file_module_x86_id_138.html
    {
      modregrm();
      const int16_t t1 = readrm16(rm);
      const int16_t t2 = signext(_read_code_u8());
      const int32_t t3 = (int32_t)t1 * (int32_t)t2;
      const int16_t t4 = t1 * t2;
      cpu_setreg16(reg, t3 & 0xFFFF);
      if (t3 != t4) {
        cpu_flags.cf = 1;
        cpu_flags.of = 1;
      } else {
        cpu_flags.cf = 0;
        cpu_flags.of = 0;
      }
    }
      break;

    case 0x6C: /* 6E INSB */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      putmem8(useseg, cpu_regs.si, portin(cpu_regs.dx));
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 1;
        cpu_regs.di = cpu_regs.di - 1;
      } else {
        cpu_regs.si = cpu_regs.si + 1;
        cpu_regs.di = cpu_regs.di + 1;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0x6D: /* 6F INSW */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      putmem16(useseg, cpu_regs.si, portin16(cpu_regs.dx));
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 2;
        cpu_regs.di = cpu_regs.di - 2;
      } else {
        cpu_regs.si = cpu_regs.si + 2;
        cpu_regs.di = cpu_regs.di + 2;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0x6E: /* 6E OUTSB */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      portout(cpu_regs.dx, getmem8(useseg, cpu_regs.si));
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 1;
        cpu_regs.di = cpu_regs.di - 1;
      } else {
        cpu_regs.si = cpu_regs.si + 1;
        cpu_regs.di = cpu_regs.di + 1;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0x6F: /* 6F OUTSW */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      portout16(cpu_regs.dx, getmem16(useseg, cpu_regs.si));
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 2;
        cpu_regs.di = cpu_regs.di - 2;
      } else {
        cpu_regs.si = cpu_regs.si + 2;
        cpu_regs.di = cpu_regs.di + 2;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;
#endif

    case 0x70: /* 70 JO Jb */
      temp16 = signext(_read_code_u8());
      if (cpu_flags.of) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x71: /* 71 JNO Jb */
      temp16 = signext(_read_code_u8());
      if (!cpu_flags.of) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x72: /* 72 JB Jb */
      temp16 = signext(_read_code_u8());
      if (cpu_flags.cf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x73: /* 73 JNB Jb */
      temp16 = signext(_read_code_u8());
      if (!cpu_flags.cf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x74: /* 74 JZ Jb */
      temp16 = signext(_read_code_u8());
      if (cpu_flags.zf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x75: /* 75 JNZ Jb */
      temp16 = signext(_read_code_u8());
      if (!cpu_flags.zf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x76: /* 76 JBE Jb */
      temp16 = signext(_read_code_u8());
      if (cpu_flags.cf || cpu_flags.zf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x77: /* 77 JA Jb */
      temp16 = signext(_read_code_u8());
      if (!cpu_flags.cf && !cpu_flags.zf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x78: /* 78 JS Jb */
      temp16 = signext(_read_code_u8());
      if (cpu_flags.sf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x79: /* 79 JNS Jb */
      temp16 = signext(_read_code_u8());
      if (!cpu_flags.sf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x7A: /* 7A JPE Jb */
      temp16 = signext(_read_code_u8());
      if (cpu_flags.pf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x7B: /* 7B JPO Jb */
      temp16 = signext(_read_code_u8());
      if (!cpu_flags.pf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x7C: /* 7C JL Jb */
      temp16 = signext(_read_code_u8());
      if (cpu_flags.sf != cpu_flags.of) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x7D: /* 7D JGE Jb */
      temp16 = signext(_read_code_u8());
      if (cpu_flags.sf == cpu_flags.of) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x7E: /* 7E JLE Jb */
      temp16 = signext(_read_code_u8());
      if ((cpu_flags.sf != cpu_flags.of) || cpu_flags.zf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x7F: /* 7F JG Jb */
      temp16 = signext(_read_code_u8());
      if (!cpu_flags.zf && (cpu_flags.sf == cpu_flags.of)) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0x80:
    case 0x82: /* 80/82 GRP1 Eb Ib */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = _read_code_u8();
      switch (reg) {
      case 0:
        op_add8();
        break;
      case 1:
        op_or8();
        break;
      case 2:
        op_adc8();
        break;
      case 3:
        op_sbb8();
        break;
      case 4:
        op_and8();
        break;
      case 5:
        op_sub8();
        break;
      case 6:
        op_xor8();
        break;
      case 7:
        flag_sub8(oper1b, oper2b);
        break;
      default:
        UNREACHABLE();
      }

      if (reg < 7) {
        writerm8(rm, res8);
      }
      break;

    case 0x81: /* 81 GRP1 Ev Iv */
    case 0x83: /* 83 GRP1 Ev Ib */
      modregrm();
      oper1 = readrm16(rm);
      if (opcode == 0x81) {
        oper2 = _read_code_u16();
      } else {
        oper2 = signext(_read_code_u8());
      }

      switch (reg) {
      case 0:
        op_add16();
        break;
      case 1:
        op_or16();
        break;
      case 2:
        op_adc16();
        break;
      case 3:
        op_sbb16();
        break;
      case 4:
        op_and16();
        break;
      case 5:
        op_sub16();
        break;
      case 6:
        op_xor16();
        break;
      case 7:
        flag_sub16(oper1, oper2);
        break;
      default:
        break; /* to avoid compiler warnings */
      }

      // XXX: would reg ever be >= 7
      if (reg < 7) {
        writerm16(rm, res16);
      }
      break;

    case 0x84: /* 84 TEST Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      oper2b = readrm8(rm);
      flag_log8(oper1b & oper2b);
      break;

    case 0x85: /* 85 TEST Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      oper2 = readrm16(rm);
      flag_log16(oper1 & oper2);
      break;

    case 0x86: /* 86 XCHG Gb Eb */
      modregrm();
      oper1b = cpu_getreg8(reg);
      cpu_setreg8(reg, readrm8(rm));
      writerm8(rm, oper1b);
      break;

    case 0x87: /* 87 XCHG Gv Ev */
      modregrm();
      oper1 = cpu_getreg16(reg);
      cpu_setreg16(reg, readrm16(rm));
      writerm16(rm, oper1);
      break;

    case 0x88: /* 88 MOV Eb Gb */
      modregrm();
      writerm8(rm, cpu_getreg8(reg));
      break;

    case 0x89: /* 89 MOV Ev Gv */
      modregrm();
      writerm16(rm, cpu_getreg16(reg));
      break;

    case 0x8A: /* 8A MOV Gb Eb */
      modregrm();
      cpu_setreg8(reg, readrm8(rm));
      break;

    case 0x8B: /* 8B MOV Gv Ev */
      modregrm();
      cpu_setreg16(reg, readrm16(rm));
      break;

    case 0x8C: /* 8C MOV Ew Sw */
      modregrm();
      writerm16(rm, getsegreg(reg));
      break;

    case 0x8D: /* 8D LEA Gv M */
      modregrm();
      getea(rm);
      cpu_setreg16(reg, ea - segbase(useseg));
      break;

    case 0x8E: /* 8E MOV Sw Ew */
      modregrm();
      putsegreg(reg, readrm16(rm));
      break;

    case 0x8F: /* 8F POP Ev */
      modregrm();
      writerm16(rm, cpu_pop());
      break;

    case 0x90: /* 90 NOP */
      break;

    case 0x91: /* 91 XCHG eCX eAX */
      oper1 = cpu_regs.cx;
      cpu_regs.cx = cpu_regs.ax;
      cpu_regs.ax = oper1;
      break;

    case 0x92: /* 92 XCHG eDX eAX */
      oper1 = cpu_regs.dx;
      cpu_regs.dx = cpu_regs.ax;
      cpu_regs.ax = oper1;
      break;

    case 0x93: /* 93 XCHG eBX eAX */
      oper1 = cpu_regs.bx;
      cpu_regs.bx = cpu_regs.ax;
      cpu_regs.ax = oper1;
      break;

    case 0x94: /* 94 XCHG eSP eAX */
      oper1 = cpu_regs.sp;
      cpu_regs.sp = cpu_regs.ax;
      cpu_regs.ax = oper1;
      break;

    case 0x95: /* 95 XCHG eBP eAX */
      oper1 = cpu_regs.bp;
      cpu_regs.bp = cpu_regs.ax;
      cpu_regs.ax = oper1;
      break;

    case 0x96: /* 96 XCHG eSI eAX */
      oper1 = cpu_regs.si;
      cpu_regs.si = cpu_regs.ax;
      cpu_regs.ax = oper1;
      break;

    case 0x97: /* 97 XCHG eDI eAX */
      oper1 = cpu_regs.di;
      cpu_regs.di = cpu_regs.ax;
      cpu_regs.ax = oper1;
      break;

    case 0x98: /* 98 CBW */
      if ((cpu_regs.al & 0x80) == 0x80) {
        cpu_regs.ah = 0xFF;
      } else {
        cpu_regs.ah = 0;
      }
      break;

    case 0x99: /* 99 CWD */
      if ((cpu_regs.ah & 0x80) == 0x80) {
        cpu_regs.dx = 0xFFFF;
      } else {
        cpu_regs.dx = 0;
      }
      break;

    case 0x9A: /* 9A CALL Ap */
      oper1 = _read_code_u16();
      oper2 = _read_code_u16();
      cpu_push(cpu_regs.cs);
      cpu_push(cpu_regs.ip);
      cpu_regs.ip = oper1;
      cpu_regs.cs = oper2;
      break;

    case 0x9B: /* 9B WAIT */
      break;

    case 0x9C: /* 9C PUSHF */
#ifdef CPU_SET_HIGH_FLAGS
      cpu_push(makeflagsword() | 0xF800);
#else
      cpu_push(makeflagsword() | 0x0800);
#endif
      break;

    case 0x9D: /* 9D POPF */
      temp16 = cpu_pop();
      decodeflagsword(temp16);
      break;

    case 0x9E: /* 9E SAHF */
      decodeflagsword((makeflagsword() & 0xFF00) | cpu_regs.ah);
      break;

    case 0x9F: /* 9F LAHF */
      cpu_regs.ah = makeflagsword() & 0xFF;
      break;

    case 0xA0: /* A0 MOV cpu_regs.al Ob */
      cpu_regs.al = getmem8(useseg, _read_code_u16());
      break;

    case 0xA1: /* A1 MOV eAX Ov */
      oper1 = getmem16(useseg, _read_code_u16());
      cpu_regs.ax = oper1;
      break;

    case 0xA2: /* A2 MOV Ob cpu_regs.al */
      putmem8(useseg, _read_code_u16(), cpu_regs.al);
      break;

    case 0xA3: /* A3 MOV Ov eAX */
      putmem16(useseg, _read_code_u16(), cpu_regs.ax);
      break;

    case 0xA4: /* A4 MOVSB */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      putmem8(cpu_regs.es, cpu_regs.di,
              getmem8(useseg, cpu_regs.si));
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 1;
        cpu_regs.di = cpu_regs.di - 1;
      } else {
        cpu_regs.si = cpu_regs.si + 1;
        cpu_regs.di = cpu_regs.di + 1;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xA5: /* A5 MOVSW */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      putmem16(cpu_regs.es, cpu_regs.di,
               getmem16(useseg, cpu_regs.si));
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 2;
        cpu_regs.di = cpu_regs.di - 2;
      } else {
        cpu_regs.si = cpu_regs.si + 2;
        cpu_regs.di = cpu_regs.di + 2;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xA6: /* A6 CMPSB */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      oper1b = getmem8(useseg, cpu_regs.si);
      oper2b = getmem8(cpu_regs.es, cpu_regs.di);
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 1;
        cpu_regs.di = cpu_regs.di - 1;
      } else {
        cpu_regs.si = cpu_regs.si + 1;
        cpu_regs.di = cpu_regs.di + 1;
      }

      flag_sub8(oper1b, oper2b);
      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      if ((reptype == 1) && !cpu_flags.zf) {
        break;
      } else if ((reptype == 2) && (cpu_flags.zf == 1)) {
        break;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xA7: /* A7 CMPSW */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      oper1 = getmem16(useseg, cpu_regs.si);
      oper2 = getmem16(cpu_regs.es, cpu_regs.di);
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 2;
        cpu_regs.di = cpu_regs.di - 2;
      } else {
        cpu_regs.si = cpu_regs.si + 2;
        cpu_regs.di = cpu_regs.di + 2;
      }

      flag_sub16(oper1, oper2);
      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      if ((reptype == 1) && !cpu_flags.zf) {
        break;
      }

      if ((reptype == 2) && (cpu_flags.zf == 1)) {
        break;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xA8: /* A8 TEST cpu_regs.al Ib */
      oper1b = cpu_regs.al;
      oper2b = _read_code_u8();
      flag_log8(oper1b & oper2b);
      break;

    case 0xA9: /* A9 TEST eAX Iv */
      oper1 = cpu_regs.ax;
      oper2 = _read_code_u16();
      flag_log16(oper1 & oper2);
      break;

    case 0xAA: /* AA STOSB */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      putmem8(cpu_regs.es, cpu_regs.di, cpu_regs.al);
      if (cpu_flags.df) {
        cpu_regs.di = cpu_regs.di - 1;
      } else {
        cpu_regs.di = cpu_regs.di + 1;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xAB: /* AB STOSW */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      putmem16(cpu_regs.es, cpu_regs.di, cpu_regs.ax);
      if (cpu_flags.df) {
        cpu_regs.di = cpu_regs.di - 2;
      } else {
        cpu_regs.di = cpu_regs.di + 2;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xAC: /* AC LODSB */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      cpu_regs.al = getmem8(useseg, cpu_regs.si);
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 1;
      } else {
        cpu_regs.si = cpu_regs.si + 1;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xAD: /* AD LODSW */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      oper1 = getmem16(useseg, cpu_regs.si);
      cpu_regs.ax = oper1;
      if (cpu_flags.df) {
        cpu_regs.si = cpu_regs.si - 2;
      } else {
        cpu_regs.si = cpu_regs.si + 2;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xAE: /* AE SCASB */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      oper1b = cpu_regs.al;
      oper2b = getmem8(cpu_regs.es, cpu_regs.di);
      flag_sub8(oper1b, oper2b);
      if (cpu_flags.df) {
        cpu_regs.di = cpu_regs.di - 1;
      } else {
        cpu_regs.di = cpu_regs.di + 1;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      if ((reptype == 1) && !cpu_flags.zf) {
        break;
      } else if ((reptype == 2) && (cpu_flags.zf == 1)) {
        break;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xAF: /* AF SCASW */
      if (reptype && (cpu_regs.cx == 0)) {
        break;
      }

      oper1 = cpu_regs.ax;
      oper2 = getmem16(cpu_regs.es, cpu_regs.di);
      flag_sub16(oper1, oper2);
      if (cpu_flags.df) {
        cpu_regs.di = cpu_regs.di - 2;
      } else {
        cpu_regs.di = cpu_regs.di + 2;
      }

      if (reptype) {
        cpu_regs.cx = cpu_regs.cx - 1;
      }

      if ((reptype == 1) && !cpu_flags.zf) {
        break;
      } else if ((reptype == 2) & (cpu_flags.zf == 1)) {
        break;
      }

      ++_cycles;
      if (!reptype) {
        break;
      }

      cpu_regs.ip = firstip;
      break;

    case 0xB0: /* B0 MOV cpu_regs.al Ib */
      cpu_regs.al = _read_code_u8();
      break;

    case 0xB1: /* B1 MOV cpu_regs.cl Ib */
      cpu_regs.cl = _read_code_u8();
      break;

    case 0xB2: /* B2 MOV cpu_regs.dl Ib */
      cpu_regs.dl = _read_code_u8();
      break;

    case 0xB3: /* B3 MOV cpu_regs.bl Ib */
      cpu_regs.bl = _read_code_u8();
      break;

    case 0xB4: /* B4 MOV cpu_regs.ah Ib */
      cpu_regs.ah = _read_code_u8();
      break;

    case 0xB5: /* B5 MOV cpu_regs.ch Ib */
      cpu_regs.ch = _read_code_u8();
      break;

    case 0xB6: /* B6 MOV cpu_regs.dh Ib */
      cpu_regs.dh = _read_code_u8();
      break;

    case 0xB7: /* B7 MOV cpu_regs.bh Ib */
      cpu_regs.bh = _read_code_u8();
      break;

    case 0xB8: /* B8 MOV eAX Iv */
      oper1 = _read_code_u16();
      cpu_regs.ax = oper1;
      break;

    case 0xB9: /* B9 MOV eCX Iv */
      oper1 = _read_code_u16();
      cpu_regs.cx = oper1;
      break;

    case 0xBA: /* BA MOV eDX Iv */
      oper1 = _read_code_u16();
      cpu_regs.dx = oper1;
      break;

    case 0xBB: /* BB MOV eBX Iv */
      oper1 = _read_code_u16();
      cpu_regs.bx = oper1;
      break;

    case 0xBC: /* BC MOV eSP Iv */
      cpu_regs.sp = _read_code_u16();
      break;

    case 0xBD: /* BD MOV eBP Iv */
      cpu_regs.bp = _read_code_u16();
      break;

    case 0xBE: /* BE MOV eSI Iv */
      cpu_regs.si = _read_code_u16();
      break;

    case 0xBF: /* BF MOV eDI Iv */
      cpu_regs.di = _read_code_u16();
      break;

#if (CPU >= CPU_186)
    case 0xC0: /* C0 GRP2 byte imm8 (80186+) */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = _read_code_u8();
      writerm8(rm, op_grp2_8(oper2b));
      break;

    case 0xC1: /* C1 GRP2 word imm8 (80186+) */
      modregrm();
      oper1 = readrm16(rm);
      oper2 = _read_code_u8();
      writerm16(rm, op_grp2_16((uint8_t)oper2));
      break;
#endif

    case 0xC2: /* C2 RET Iw */
      // TODO: _read_code_u16();
      oper1 = getmem16(cpu_regs.cs, cpu_regs.ip);
      cpu_regs.ip = cpu_pop();
      cpu_regs.sp = cpu_regs.sp + oper1;
      break;

    case 0xC3: /* C3 RET */
      cpu_regs.ip = cpu_pop();
      break;

    case 0xC4: /* C4 LES Gv Mp */
      modregrm();
      getea(rm);
      cpu_setreg16(reg, readw86(ea));
      cpu_regs.es = readw86(ea + 2);
      break;

    case 0xC5: /* C5 LDS Gv Mp */
      modregrm();
      getea(rm);
      cpu_setreg16(reg, readw86(ea));
      cpu_regs.ds = readw86(ea + 2);
      break;

    case 0xC6: /* C6 MOV Eb Ib */
      modregrm();
      writerm8(rm, _read_code_u8());
      break;

    case 0xC7: /* C7 MOV Ev Iv */
      modregrm();
      writerm16(rm, _read_code_u16());
      break;

    case 0xC8: /* C8 ENTER (80186+) */
    {
      const uint16_t stacksize = _read_code_u16();
      const uint8_t nestlev = _read_code_u8();
      cpu_push(cpu_regs.bp);

      frametemp = cpu_regs.sp;
      if (nestlev) {
        for (temp16 = 1; temp16 < nestlev; temp16++) {
          cpu_regs.bp -= 2;
          cpu_push(cpu_regs.bp);
        }
        cpu_push(frametemp);
      }

      cpu_regs.bp = frametemp;
      cpu_regs.sp = cpu_regs.bp - stacksize;
    }
      break;

    case 0xC9: /* C9 LEAVE (80186+) */
      cpu_regs.sp = cpu_regs.bp;
      cpu_regs.bp = cpu_pop();
      break;

    case 0xCA: /* CA RETF Iw */
      // TODO: _read_code_u16();
      oper1 = getmem16(cpu_regs.cs, cpu_regs.ip);
      cpu_regs.ip = cpu_pop();
      cpu_regs.cs = cpu_pop();
      cpu_regs.sp = cpu_regs.sp + oper1;
      break;

    case 0xCB: /* CB RETF */
      cpu_regs.ip = cpu_pop();
      cpu_regs.cs = cpu_pop();
      break;

    case 0xCC: /* CC INT 3 */
      _intcall_handler(3);
      break;

    case 0xCD: /* CD INT Ib */
      oper1b = _read_code_u8();
      _intcall_handler(oper1b);
      break;

    case 0xCE: /* CE INTO */
      if (cpu_flags.of) {
        _intcall_handler(4);
      }
      break;

    case 0xCF: /* CF IRET */
      cpu_regs.ip = cpu_pop();
      cpu_regs.cs = cpu_pop();
      decodeflagsword(cpu_pop());
      break;

    case 0xD0: /* D0 GRP2 Eb 1 */
      modregrm();
      oper1b = readrm8(rm);
      writerm8(rm, op_grp2_8(1));
      break;

    case 0xD1: /* D1 GRP2 Ev 1 */
      modregrm();
      oper1 = readrm16(rm);
      writerm16(rm, op_grp2_16(1));
      break;

    case 0xD2: /* D2 GRP2 Eb cpu_regs.cl */
      modregrm();
      oper1b = readrm8(rm);
      writerm8(rm, op_grp2_8(cpu_regs.cl));
      break;

    case 0xD3: /* D3 GRP2 Ev cpu_regs.cl */
      modregrm();
      oper1 = readrm16(rm);
      writerm16(rm, op_grp2_16(cpu_regs.cl));
      break;

    case 0xD4: /* D4 AAM I0 */
      oper1 = _read_code_u8();
      // division by zero!
      if (!oper1) {
        _intcall_handler(0);
        break;
      }

      cpu_regs.ah = (cpu_regs.al / oper1) & 0xff;
      cpu_regs.al = (cpu_regs.al % oper1) & 0xff;
      flag_szp16(cpu_regs.ax);
      break;

    case 0xD5: /* D5 AAD I0 */
      oper1 = _read_code_u8();
      cpu_regs.al = (cpu_regs.ah * oper1 + cpu_regs.al) & 0xff;
      cpu_regs.ah = 0;
      flag_szp16(cpu_regs.ah * oper1 + cpu_regs.al);
      cpu_flags.sf = 0;
      break;

    case 0xD6: /* D6 XLAT on V20/V30, SALC on 8086/8088 */
#ifndef CPU_NO_SALC
      cpu_regs.al = cpu_flags.cf ? 0xFF : 0x00;
      break;
#endif

    case 0xD7: /* D7 XLAT */
      cpu_regs.al = 
          read86(segbase(useseg) + (cpu_regs.bx) + cpu_regs.al);
      break;

#if 1
    case 0xD8:
    case 0xD9:
    case 0xDA:
    case 0xDB:
    case 0xDC:
    case 0xDE:
    case 0xDD:
    case 0xDF: /* escape to x87 FPU (unsupported) */
      modregrm();
      break;
#endif

    case 0xE0: /* E0 LOOPNZ Jb */
      temp16 = signext(_read_code_u8());
      cpu_regs.cx = cpu_regs.cx - 1;
      if (cpu_regs.cx && !cpu_flags.zf) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0xE1: /* E1 LOOPZ Jb */
      temp16 = signext(_read_code_u8());
      cpu_regs.cx = cpu_regs.cx - 1;
      if (cpu_regs.cx && (cpu_flags.zf == 1)) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0xE2: /* E2 LOOP Jb */
      temp16 = signext(_read_code_u8());
      cpu_regs.cx = cpu_regs.cx - 1;
      if (cpu_regs.cx) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0xE3: /* E3 JCXZ Jb */
      temp16 = signext(_read_code_u8());
      if (!cpu_regs.cx) {
        cpu_regs.ip += temp16;
      }
      break;

    case 0xE4: /* E4 IN cpu_regs.al Ib */
      oper1b = _read_code_u8();
      cpu_regs.al = (uint8_t)portin(oper1b);
      break;

    case 0xE5: /* E5 IN AX Ib */
      oper1b = _read_code_u8();
      cpu_regs.ax = portin16(oper1b);
      break;

    case 0xE6: /* E6 OUT Ib cpu_regs.al */
      oper1b = _read_code_u8();
      portout(oper1b, cpu_regs.al);
      break;

    case 0xE7: /* E7 OUT Ib eAX */
      oper1b = _read_code_u8();
      portout16(oper1b, cpu_regs.ax);
      break;

    case 0xE8: /* E8 CALL Jv */
      oper1 = _read_code_u16();
      cpu_push(cpu_regs.ip);
      cpu_regs.ip += oper1;
      break;

    case 0xE9: /* E9 JMP Jv */
      oper1 = _read_code_u16();
      cpu_regs.ip += oper1;
      break;

    case 0xEA: /* EA JMP Ap */
      oper1 = _read_code_u16();
      // TODO: _read_code_u16();
      oper2 = getmem16(cpu_regs.cs, cpu_regs.ip);
      cpu_regs.ip = oper1;
      cpu_regs.cs = oper2;
      break;

    case 0xEB: /* EB JMP Jb */
      oper1 = signext(_read_code_u8());
      cpu_regs.ip += oper1;
      break;

    case 0xEC: /* EC IN cpu_regs.al regdx */
      oper1 = cpu_regs.dx;
      cpu_regs.al = (uint8_t)portin(oper1);
      break;

    case 0xED: /* ED IN eAX regdx */
      oper1 = cpu_regs.dx;
      cpu_regs.ax = portin16(oper1);
      break;

    case 0xEE: /* EE OUT regdx cpu_regs.al */
      oper1 = cpu_regs.dx;
      portout(oper1, cpu_regs.al);
      break;

    case 0xEF: /* EF OUT regdx eAX */
      oper1 = cpu_regs.dx;
      portout16(oper1, cpu_regs.ax);
      break;

    case 0xF0: /* F0 LOCK */
      break;

    case 0xF4: /* F4 HLT */
      in_hlt_state = true;
      break;

    case 0xF5: /* F5 CMC */
      if (!cpu_flags.cf) {
        cpu_flags.cf = 1;
      } else {
        cpu_flags.cf = 0;
      }
      break;

    case 0xF6: /* F6 GRP3a Eb */
      modregrm();
      oper1b = readrm8(rm);
      op_grp3_8();
      if ((reg > 1) && (reg < 4)) {
        writerm8(rm, res8);
      }
      break;

    case 0xF7: /* F7 GRP3b Ev */
      modregrm();
      oper1 = readrm16(rm);
      op_grp3_16();
      if ((reg > 1) && (reg < 4)) {
        writerm16(rm, res16);
      }
      break;

    case 0xF8: /* F8 CLC */
      cpu_flags.cf = 0;
      break;

    case 0xF9: /* F9 STC */
      cpu_flags.cf = 1;
      break;

    case 0xFA: /* FA CLI */
      cpu_flags.ifl = 0;
      break;

    case 0xFB: /* FB STI */
      cpu_flags.ifl = 1;
      break;

    case 0xFC: /* FC CLD */
      cpu_flags.df = 0;
      break;

    case 0xFD: /* FD STD */
      cpu_flags.df = 1;
      break;

    case 0xFE: /* FE GRP4 Eb */
      modregrm();
      oper1b = readrm8(rm);
      oper2b = 1;
      const uint8_t tempcf = cpu_flags.cf;
      if (!reg) {
        res8 = oper1b + oper2b;
        flag_add8(oper1b, oper2b);
      } else {
        res8 = oper1b - oper2b;
        flag_sub8(oper1b, oper2b);
      }
      cpu_flags.cf = tempcf;
      writerm8(rm, res8);
      break;

    case 0xFF: /* FF GRP5 Ev */
      modregrm();
      oper1 = readrm16(rm);
      op_grp5();
      break;

    default:
      _on_illegal_instruction();
      break;
    }
  }
  // retired cycles
  const uint32_t out = (uint32_t)_cycles;
  _cycles = 0;
  return out;
}

void cpu_prep_interupt(uint16_t intnum) {
  // push flags
  cpu_push(makeflagsword());
  // push cs register
  cpu_push(cpu_regs.cs);
  // push ip
  cpu_push(cpu_regs.ip);
  // new cs register
  cpu_regs.cs = getmem16(0, (uint16_t)intnum * 4 + 2);
  // new ip
  cpu_regs.ip = getmem16(0, (uint16_t)intnum * 4 + 0);
  // clear flags
  cpu_flags.ifl = 0;
  cpu_flags.tf = 0;
}

void cpu_state_save(FILE *fd) {
  fwrite(&cpu_regs, 1, sizeof(cpu_regs), fd);
  fwrite(&cpu_flags, 1, sizeof(cpu_flags), fd);
  fwrite(&in_hlt_state, 1, sizeof(in_hlt_state), fd);
  fwrite(&_delay_cycles, 1, sizeof(_delay_cycles), fd);
}

void cpu_state_load(FILE *fd) {
  fread(&cpu_regs, 1, sizeof(cpu_regs), fd);
  fread(&cpu_flags, 1, sizeof(cpu_flags), fd);
  fread(&in_hlt_state, 1, sizeof(in_hlt_state), fd);
  fread(&_delay_cycles, 1, sizeof(_delay_cycles), fd);
}

void cpu_dump_state(FILE *fd) {
  fprintf(fd, "CPU state:\n");
  fprintf(fd, "  AX %04x\n", (int)cpu_regs.ax);
  fprintf(fd, "  CX %04x\n", (int)cpu_regs.cx);
  fprintf(fd, "  DX %04x\n", (int)cpu_regs.dx);
  fprintf(fd, "  BX %04x\n", (int)cpu_regs.bx);
  fprintf(fd, "  SP %04x\n", (int)cpu_regs.sp);
  fprintf(fd, "  BP %04x\n", (int)cpu_regs.bp);
  fprintf(fd, "  SI %04x\n", (int)cpu_regs.si);
  fprintf(fd, "  DI %04x\n", (int)cpu_regs.di);
  fprintf(fd, "  CS %04x\n", (int)cpu_regs.cs);
  fprintf(fd, "  SS %04x\n", (int)cpu_regs.ss);
  fprintf(fd, "  ES %04x\n", (int)cpu_regs.es);
  fprintf(fd, "  DS %04x\n", (int)cpu_regs.ds);
  fprintf(fd, "  IP %04x\n", (int)cpu_regs.ip);
  fflush(fd);
}
