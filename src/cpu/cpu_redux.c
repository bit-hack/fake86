#include "cpu.h"

#define OPCODE(NAME) static void NAME (const uint8_t *code)

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
static inline void _step_ip(int16_t rel) {
  cpu_regs.ip += rel;
}

// PUSH ES - push segment register ES
OPCODE(_06) {
  _pushw(cpu_regs.es);
  _step_ip(1);
}

// PUSH CS - push segment register CS
OPCODE(_0E) {
  //XXX: suggeted this is an illegal operation?
  _pushw(cpu_regs.cs);
  _step_ip(1);
}

// PUSH SS - push segment register SS
OPCODE(_16) {
  _pushw(cpu_regs.ss);
  _step_ip(1);
}

// PUSH DS - push segment register DS
OPCODE(_1E) {
  _pushw(cpu_regs.ds);
  _step_ip(1);
}

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

// PUSHA - push all registers (+186)
OPCODE(_60) {
  const uint16_t sp = cpu_regs.sp;
  _pushw(cpu_regs.ax);
  _pushw(cpu_regs.cx);
  _pushw(cpu_regs.dx);
  _pushw(cpu_regs.bx);
  _pushw(sp);
  _pushw(cpu_regs.bp);
  _pushw(cpu_regs.si);
  _pushw(cpu_regs.di);
}

// POPA - pop all registers (+186)
OPCODE(_61) {
  cpu_regs.di = _popw();
  cpu_regs.si = _popw();
  cpu_regs.bp = _popw();
                _popw();
  cpu_regs.bx = _popw();
  cpu_regs.dx = _popw();
  cpu_regs.cx = _popw();
  cpu_regs.ax = _popw();
}

// NOP - no operation (XCHG AX AX)
OPCODE(_90) {
  _step_ip(1);
}

#define XCHG(REG)                     \
  {                                   \
    const uint16_t t = REG;           \
    REG = cpu_regs.ax;                \
    cpu_regs.ax = t;                  \
    _step_ip(1);                      \
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

// WAIT - wait for test pin assetion
OPCODE(_9B) {
  _step_ip(1);
}

// RET - near return and add to stack pointer
OPCODE(_C2) {
  const uint16_t disp16 = *(uint16_t*)(code + 1);
  cpu_regs.ip = _popw();
  cpu_regs.sp += disp16;
}

// RET - near return
OPCODE(_C3) {
  cpu_regs.ip = _popw();
}

// RETF - far return
OPCODE(_CB) {
  cpu_regs.ip = _popw();
  cpu_regs.cs = _popw();
}

// RETF - far return and add to stack pointer
OPCODE(_CA) {
  const uint16_t disp16 = *(uint16_t*)(code + 1);
  cpu_regs.ip = _popw();
  cpu_regs.cs = _popw();
  cpu_regs.sp += disp16;
}

// LOCK - lock prefix
OPCODE(_F0) {
  _step_ip(1);
}

// STC - set carry flag
OPCODE(_F9) {
  cpu_flags.cf = 1;
  _step_ip(1);
}

// STI - set interrupt flag
OPCODE(_FB) {
  cpu_flags.ifl = 1;
  _step_ip(1);
}

// STD - set direction flag
OPCODE(_FD) {
  cpu_flags.df = 1;
  _step_ip(1);
}

typedef void (*opcode_t)(const uint8_t *code);

#define ___ 0
static const opcode_t _op_table[256] = {
// 00   01   02   03   04   05   06   07   08   09   0A   0B   0C   0D   0E   0F
  ___, ___, ___, ___, ___, ___, _06, ___, ___, ___, ___, ___, ___, ___, _0E, ___, // 00
  ___, ___, ___, ___, ___, ___, _16, ___, ___, ___, ___, ___, ___, ___, _1E, ___, // 10
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 20
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 30
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 40
  _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5A, _5B, _5C, _5D, _5E, _5F, // 50
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 60
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 70
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // 80
  _90, _91, _92, _93, _94, _95, _96, _97, ___, ___, ___, _9B, ___, ___, ___, ___, // 90
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // A0
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // B0
  ___, ___, _C2, _C3, ___, ___, ___, ___, ___, ___, _CA, _CB, ___, ___, ___, ___, // C0
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // D0
  ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, ___, // E0
  _F0, ___, ___, ___, ___, ___, ___, ___, ___, _F9, ___, _FB, ___, _FD, ___, ___, // F0
};
#undef ___

bool cpu_redux_exec(void) {
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
  return true;
}
