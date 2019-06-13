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

#include "cpu.h"
#include "cpu_mod_rm.h"


//
static bool _did_run_temp = true;


// forward declare opcode table
typedef void (*opcode_t)(const uint8_t *code);
static const opcode_t _op_table[];


// shift register used to delay STI until next instruction
static uint8_t _sti_sr = 0;

#define OPCODE(NAME)                                                          \
  static void NAME (const uint8_t *code)

// effective instruction pointer
static inline uint32_t _eip(void) {
  return (cpu_regs.cs << 4) + cpu_regs.ip;
}

// effective stack pointer
static inline uint32_t _esp(void) {
  return (cpu_regs.ss << 4) + cpu_regs.sp;
}

// push byte to stack
static inline void _pushb(const uint8_t val) {
  cpu_regs.sp -= 1;
  *(uint8_t*)(RAM + _esp()) = val;
}

// push word to stack
static inline void _pushw(const uint16_t val) {
  cpu_regs.sp -= 2;
  *(uint16_t*)(RAM + _esp()) = val;
}

// pop byte from stack
static inline uint8_t _popb(void) {
  const uint8_t out = *(const uint8_t*)(RAM + _esp());
  cpu_regs.sp += 1;
  return out;
}

// pop word from stack
static inline uint16_t _popw(void) {
  const uint16_t out = *(const uint16_t*)(RAM + _esp());
  cpu_regs.sp += 2;
  return out;
}

// step instruction pointer
static inline void _step_ip(const int16_t rel) {
  cpu_regs.ip += rel;
}

// TODO: move this to be lazy executed
static const uint8_t parity[0x100] = {
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1};

// set parity flag
static inline void _set_pf(const uint16_t val) {
  cpu_flags.pf = parity[val & 0xff];
}

// get parity flag
static inline uint8_t _get_pf(void) {
  return cpu_flags.pf;
}

// set zero and sign flags
static inline void _set_zf_sf_b(const uint16_t val) {
  cpu_flags.zf = (val == 0);
  cpu_flags.sf = (val & 0x80) ? 1 : 0;
}

// set zero and sign flags
static inline void _set_zf_sf_w(const uint16_t val) {
  cpu_flags.zf = (val == 0);
  cpu_flags.sf = (val & 0x8000) ? 1 : 0;
}

// PUSH ES - push segment register ES
OPCODE(_06) {
  _pushw(cpu_regs.es);
  _step_ip(1);
}

// POP ES - pop segment register ES
OPCODE(_07) {
  cpu_regs.es = _popw();
  _step_ip(1);
}

// PUSH CS - push segment register CS
OPCODE(_0E) {
  _pushw(cpu_regs.cs);
  _step_ip(1);
}

// PUSH SS - push segment register SS
OPCODE(_16) {
  _pushw(cpu_regs.ss);
  _step_ip(1);
}

// POP SS - pop segment register SS
OPCODE(_17) {
  cpu_regs.ss = _popw();
  _step_ip(1);
}

// PUSH DS - push segment register DS
OPCODE(_1E) {
  _pushw(cpu_regs.ds);
  _step_ip(1);
}

// POP DS - pop segment register DS
OPCODE(_1F) {
  cpu_regs.ds = _popw();
  _step_ip(1);
}

// Prefix - Segment Override ES
OPCODE(_26) {
  _did_run_temp = false;  // XXX: remove
  // backup seg regs
  const uint32_t ss = cpu_regs.ss;
  const uint32_t ds = cpu_regs.ds;
  // modify segment register
  cpu_regs.ss = cpu_regs.es;
  cpu_regs.ds = cpu_regs.es;
  // dispatch next opcode
  if (_op_table[code[1]]) {  // XXX: remove
    _step_ip(1);
    _op_table[code[1]](code + 1);
    _did_run_temp = true;  // XXX: remove
  }
  // restore segment addresses
  cpu_regs.ss = ss;
  cpu_regs.ds = ds;
}

// Prefix - Segment Override CS
OPCODE(_2E) {
  _did_run_temp = false;  // XXX: remove
  // backup seg regs
  const uint32_t ss = cpu_regs.ss;
  const uint32_t ds = cpu_regs.ds;
  // modify segment register
  cpu_regs.ss = cpu_regs.cs;
  cpu_regs.ds = cpu_regs.cs;
  // dispatch next opcode
  if (_op_table[code[1]]) {  // XXX: remove
    _step_ip(1);
    _op_table[code[1]](code + 1);
    _did_run_temp = true;  // XXX: remove
  }
  // restore segment addresses
  cpu_regs.ss = ss;
  cpu_regs.ds = ds;
}

// Prefix - Segment Override SS
OPCODE(_36) {
  _did_run_temp = false;  // XXX: remove
  // backup seg regs
  const uint32_t ds = cpu_regs.ds;
  // modify segment register
  cpu_regs.ds = cpu_regs.ss;
  // dispatch next opcode
  if (_op_table[code[1]]) {  // XXX: remove
    _step_ip(1);
    _op_table[code[1]](code + 1);
    _did_run_temp = true;  // XXX: remove
  }
  // restore segment addresses
  cpu_regs.ds = ds;
}

// Prefix - Segment Override DS
OPCODE(_3E) {
  _did_run_temp = false;  // XXX: remove
  // backup seg regs
  const uint32_t ss = cpu_regs.ss;
  // modify segment register
  cpu_regs.ss = cpu_regs.ds;
  // dispatch next opcode
  if (_op_table[code[1]]) {  // XXX: remove
    _step_ip(1);
    _op_table[code[1]](code + 1);
    _did_run_temp = true;  // XXX: remove
  }
  // restore segment addresses
  cpu_regs.ss = ss;
}

#define INC(REG)                                                              \
  {                                                                           \
    cpu_flags.of = (REG == 0x7fff);                                           \
    cpu_flags.af = (REG & 0x0f) == 0x0f;                                      \
    REG += 1;                                                                 \
    _set_zf_sf_w(REG);                                                        \
    _set_pf(REG);                                                             \
    _step_ip(1);                                                              \
  }

// INC AX - increment register
OPCODE(_40) {
  INC(cpu_regs.ax);
}

// INC CX - increment register
OPCODE(_41) {
  INC(cpu_regs.cx);
}

// INC DX - increment register
OPCODE(_42) {
  INC(cpu_regs.dx);
}

// INC BX - increment register
OPCODE(_43) {
  INC(cpu_regs.bx);
}

// INC SP - increment register
OPCODE(_44) {
  INC(cpu_regs.sp);
}

// INC BP - increment register
OPCODE(_45) {
  INC(cpu_regs.bp);
}

// INC SI - increment register
OPCODE(_46) {
  INC(cpu_regs.si);
}

// INC DI - increment register
OPCODE(_47) {
  INC(cpu_regs.di);
}

#undef INC

#define DEC(REG)                                                              \
  {                                                                           \
    cpu_flags.of = (REG == 0x8000);                                           \
    cpu_flags.af = (REG & 0x0f) == 0x0;                                       \
    REG -= 1;                                                                 \
    _set_zf_sf_w(REG);                                                          \
    _set_pf(REG);                                                             \
    _step_ip(1);                                                              \
  }

// DEC AX - increment register
OPCODE(_48) {
  DEC(cpu_regs.ax);
}

// DEC CX - increment register
OPCODE(_49) {
  DEC(cpu_regs.cx);
}

// DEC DX - increment register
OPCODE(_4A) {
  DEC(cpu_regs.dx);
}

// DEC BX - increment register
OPCODE(_4B) {
  DEC(cpu_regs.bx);
}

// DEC SP - increment register
OPCODE(_4C) {
  DEC(cpu_regs.sp);
}

// DEC BP - increment register
OPCODE(_4D) {
  DEC(cpu_regs.bp);
}

// DEC SI - increment register
OPCODE(_4E) {
  DEC(cpu_regs.si);
}

// DEC DI - increment register
OPCODE(_4F) {
  DEC(cpu_regs.di);
}

#undef DEC

// PUSH AX - push register
OPCODE(_50) {
  _pushw(cpu_regs.ax);
  _step_ip(1);
}

// PUSH CX - push register
OPCODE(_51) {
  _pushw(cpu_regs.cx);
  _step_ip(1);
}

// PUSH DX - push register
OPCODE(_52) {
  _pushw(cpu_regs.dx);
  _step_ip(1);
}

// PUSH BX - push register
OPCODE(_53) {
  _pushw(cpu_regs.bx);
  _step_ip(1);
}

// PUSH SP - push register
OPCODE(_54) {
  _pushw(cpu_regs.sp);
  _step_ip(1);
}

// PUSH BP - push register
OPCODE(_55) {
  _pushw(cpu_regs.bp);
  _step_ip(1);
}

// PUSH SI - push register
OPCODE(_56) {
  _pushw(cpu_regs.si);
  _step_ip(1);
}

// PUSH DI - push register
OPCODE(_57) {
  _pushw(cpu_regs.di);
  _step_ip(1);
}

// POP AX - pop register
OPCODE(_58) {
  cpu_regs.ax = _popw();
  _step_ip(1);
}

// POP CX - pop register
OPCODE(_59) {
  cpu_regs.cx = _popw();
  _step_ip(1);
}

// POP DX - pop register
OPCODE(_5A) {
  cpu_regs.dx = _popw();
  _step_ip(1);
}

// POP BX - pop register
OPCODE(_5B) {
  cpu_regs.bx = _popw();
  _step_ip(1);
}

// POP SP - pop register
OPCODE(_5C) {
  cpu_regs.sp = _popw();
  _step_ip(1);
}

// POP BP - pop register
OPCODE(_5D) {
  cpu_regs.bp = _popw();
  _step_ip(1);
}

// POP SI - pop register
OPCODE(_5E) {
  cpu_regs.si = _popw();
  _step_ip(1);
}

// POP DI - pop register
OPCODE(_5F) {
  cpu_regs.di = _popw();
  _step_ip(1);
}

// JO - jump on overflow
OPCODE(_70) {
  _step_ip(2);
  if (cpu_flags.of) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JNO - jump not overflow
OPCODE(_71) {
  _step_ip(2);
  if (!cpu_flags.of) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JB - jump if below
OPCODE(_72) {
  _step_ip(2);
  if (cpu_flags.cf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JAE - jump above or equal
OPCODE(_73) {
  _step_ip(2);
  if (!cpu_flags.cf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JZ - jump not zero
OPCODE(_74) {
  _step_ip(2);
  if (cpu_flags.zf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JNZ - jump not zero
OPCODE(_75) {
  _step_ip(2);
  if (!cpu_flags.zf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JBE - jump below or equal
OPCODE(_76) {
  _step_ip(2);
  if (cpu_flags.cf || cpu_flags.zf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JS - jump if not below or equal
OPCODE(_77) {
  _step_ip(2);
  if (!cpu_flags.cf && !cpu_flags.zf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JS - jump if sign
OPCODE(_78) {
  _step_ip(2);
  if (cpu_flags.sf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JNS - jump not sign
OPCODE(_79) {
  _step_ip(2);
  if (!cpu_flags.sf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JP - jump parity
OPCODE(_7A) {
  _step_ip(2);
  if (cpu_flags.pf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JNP - jump not parity
OPCODE(_7B) {
  _step_ip(2);
  if (!cpu_flags.pf) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// JLE - jump if less or equal
OPCODE(_7E) {
  _step_ip(2);
  if (cpu_flags.zf || (cpu_flags.sf != cpu_flags.of)) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

#define TEST_B(TMP)                                                           \
  {                                                                           \
    _set_zf_sf_b(TMP);                                                        \
    _set_pf(TMP);                                                             \
    cpu_flags.cf = 0;                                                         \
    cpu_flags.of = 0;                                                         \
  }

// TEST - r/m8, r8
OPCODE(_84) {
  struct cpu_mod_rm_t m;
  _decode_mod_rm(code, &m);
  const uint8_t lhs = _read_rm_b(&m);
  const uint8_t rhs = _get_regb(m.reg);
  const uint8_t res = lhs & rhs;
  TEST_B(res);
  // step instruction pointer
  _step_ip(1 + m.num_bytes);
}

#define TEST_W(TMP)                                                           \
  {                                                                           \
    _set_zf_sf_w(TMP);                                                        \
    _set_pf(TMP);                                                             \
    cpu_flags.cf = 0;                                                         \
    cpu_flags.of = 0;                                                         \
  }

// TEST - r/m16, r16
OPCODE(_85) {
  struct cpu_mod_rm_t m;
  _decode_mod_rm(code, &m);
  const uint16_t lhs = _read_rm_w(&m);
  const uint16_t rhs = _get_regw(m.reg);
  const uint16_t res = lhs & rhs;
  TEST_W(res);
  // step instruction pointer
  _step_ip(1 + m.num_bytes);
}

// MOV - r/m8, r8
OPCODE(_88) {
  struct cpu_mod_rm_t m;
  _decode_mod_rm(code, &m);
  // do the transfer
  _write_rm_b(&m, _get_regb(m.reg));
  // step instruction pointer
  _step_ip(1 + m.num_bytes);
}

// MOV - r/m16, r16
OPCODE(_89) {
  struct cpu_mod_rm_t m;
  _decode_mod_rm(code, &m);
  // do the transfer
  _write_rm_w(&m, _get_regw(m.reg));
  // step instruction pointer
  _step_ip(1 + m.num_bytes);
}

// NOP - no operation (XCHG AX AX)
OPCODE(_90) {
  _step_ip(1);
}

#define XCHG(REG)                                                             \
  {                                                                           \
    const uint16_t t = REG;                                                   \
    REG = cpu_regs.ax;                                                        \
    cpu_regs.ax = t;                                                          \
    _step_ip(1);                                                              \
  }

// XCHG CX - exchange AX and CX
OPCODE(_91) {
  XCHG(cpu_regs.cx);
}

// XCHG DX - exchange AX and DX
OPCODE(_92) {
  XCHG(cpu_regs.dx);
}

// XCHG BX - exchange AX and BX
OPCODE(_93) {
  XCHG(cpu_regs.bx);
}

// XCHG SP - exchange AX and SP
OPCODE(_94) {
  XCHG(cpu_regs.sp);
}

// XCHG BP - exchange AX and BP
OPCODE(_95) {
  XCHG(cpu_regs.bp);
}

// XCHG SI - exchange AX and SI
OPCODE(_96) {
  XCHG(cpu_regs.si);
}

// XCHG DI - exchange AX and DI
OPCODE(_97) {
  XCHG(cpu_regs.di);
}

#undef XCHG

// CBW - convert byte to word
OPCODE(_98) {
  cpu_regs.ah = (cpu_regs.al & 0x80) ? 0xff : 0x00;
  _step_ip(1);
}

// CWD - convert word to dword
OPCODE(_99) {
  cpu_regs.dx = (cpu_regs.ax & 0x8000) ? 0xffff : 0x0000;
  _step_ip(1);
}

// WAIT - wait for test pin assertion
OPCODE(_9B) {
  _step_ip(1);
}

// TEST AL, imm8
OPCODE(_A8) {
  const uint8_t imm = GET_CODE(uint8_t, 1);
  const uint8_t res = cpu_regs.al & imm;
  TEST_B(res);
  _step_ip(2);
}

// TEST AX, imm16
OPCODE(_A9) {
  const uint16_t imm = GET_CODE(uint16_t, 1);
  const uint16_t res = cpu_regs.ax & imm;
  TEST_W(res);
  _step_ip(3);
}

// RET - near return and add to stack pointer
OPCODE(_C2) {
  const uint16_t disp16 = GET_CODE(uint16_t, 1);
  cpu_regs.ip = _popw();
  cpu_regs.sp += disp16;
}

// RET - near return
OPCODE(_C3) {
  cpu_regs.ip = _popw();
}

// RETF - far return and add to stack pointer
OPCODE(_CA) {
  const uint16_t disp16 = GET_CODE(uint16_t, 1);
  cpu_regs.ip = _popw();
  cpu_regs.cs = _popw();
  cpu_regs.sp += disp16;
}

// RETF - far return
OPCODE(_CB) {
  cpu_regs.ip = _popw();
  cpu_regs.cs = _popw();
}

// JCXZ - jump if CX is zero
OPCODE(_E3) {
  _step_ip(2);
  if (cpu_regs.cx == 0) {
    cpu_regs.ip += GET_CODE(int8_t, 1);
  }
}

// CALL disp16
OPCODE(_E8) {
  // step over call
  _step_ip(3);
  // push return address
  _pushw(cpu_regs.ip);
  // set new ip
  cpu_regs.ip += GET_CODE(uint16_t, 1);
}

// JMP disp16 - jump with signed word displacement
OPCODE(_E9) {
  cpu_regs.ip += 3 + GET_CODE(uint16_t, 1);
}

// JMP far - intersegment jump
OPCODE(_EA) {
  cpu_regs.ip = GET_CODE(uint16_t, 1);
  cpu_regs.cs = GET_CODE(uint16_t, 3);
}

// JMP disp8 - jump with signed byte displacement
OPCODE(_EB) {
  cpu_regs.ip += 2 + GET_CODE(int8_t, 1);
}

// LOCK - lock prefix
OPCODE(_F0) {
  _step_ip(1);
}

// CMC - compliment carry flag
OPCODE(_F5) {
  cpu_flags.cf ^= 1;
  _step_ip(1);
}

// CLC - clear carry flag
OPCODE(_F8) {
  cpu_flags.cf = 0;
  _step_ip(1);
}

// STC - set carry flag
OPCODE(_F9) {
  cpu_flags.cf = 1;
  _step_ip(1);
}

// CLI - clear interrupt flag
OPCODE(_FA) {
  cpu_flags.ifl = 0;
  _sti_sr = 0x0;
  _step_ip(1);
}

// STI - set interrupt flag
OPCODE(_FB) {
  // STI is delayed one instruction
  _sti_sr = 0x3;
  _step_ip(1);
}

// CLD - clear direction flag
OPCODE(_FC) {
  // STI is delayed one instruction
  cpu_flags.df = 0;
  _step_ip(1);
}

// STD - set direction flag
OPCODE(_FD) {
  cpu_flags.df = 1;
  _step_ip(1);
}

#define ___ 0
static const opcode_t _op_table[256] = {
// 00   01   02   03   04   05   06   07   08   09   0A   0B   0C   0D   0E   0F
  ___, ___, ___, ___, ___, ___, _06, _07, ___, ___, ___, ___, ___, ___, _0E, ___, // 00
  ___, ___, ___, ___, ___, ___, _16, _17, ___, ___, ___, ___, ___, ___, _1E, _1F, // 10
  ___, ___, ___, ___, ___, ___, _26, ___, ___, ___, ___, ___, ___, ___, _2E, ___, // 20
  ___, ___, ___, ___, ___, ___, _36, ___, ___, ___, ___, ___, ___, ___, _3E, ___, // 30
  _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4A, _4B, _4C, _4D, _4E, _4F, // 40
  _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5A, _5B, _5C, _5D, _5E, _5F, // 50
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 60
  _70, _71, _72, _73, _74, _75, _76, _77, _78, _79, _7A, _7B, ___, ___, _7E, ___, // 70
  ___, ___, ___, ___, _84, _85, ___, ___, _88, _89, ___, ___, ___, ___, ___, ___, // 80
  _90, _91, _92, _93, _94, _95, _96, _97, _98, _99, ___, _9B, ___, ___, ___, ___, // 90
  ___, ___, ___, ___, ___, ___, ___, ___, _A8, _A9, ___, ___, ___, ___, ___, ___, // A0
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // B0
  ___, ___, _C2, _C3, ___, ___, ___, ___, ___, ___, _CA, _CB, ___, ___, ___, ___, // C0
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // D0
  ___, ___, ___, _E3, ___, ___, ___, ___, _E8, _E9, _EA, _EB, ___, ___, ___, ___, // E0
  _F0, ___, ___, ___, ___, _F5, ___, ___, _F8, _F9, _FA, _FB, _FC, _FD, ___, ___, // F0
};
#undef ___

bool cpu_redux_exec(void) {

  // delay setting IFL for one instruction after STI
  _sti_sr >>= 1;
  cpu_flags.ifl |= _sti_sr & 1;

  // get effective pc
  const uint32_t eip = (cpu_regs.cs << 4) + cpu_regs.ip;
  // find the code stream
  const uint8_t *code = RAM + eip;
  // lookup opcode handler
  const opcode_t op = _op_table[*code];
  if (op == NULL) {
    return false;
  }

  // execute opcode
  op(code);

  // this is a little hack for the segment override prefix so we can bail out
  // if the next opcode wasnt implemented for us yet
  if (_did_run_temp == false) {  // XXX: remove
    return false;
  }

  return true;
}
