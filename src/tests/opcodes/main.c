#include "../../cpu/cpu.h"


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

#define _root_seed 12345
#define _test_runs 1024

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

uint8_t RAM[1 << 20];

static uint8_t _mem_read_8(uint32_t addr) { return RAM[addr]; }
static uint16_t _mem_read_16(uint32_t addr) {
  return RAM[addr + 0] | (RAM[addr + 1] << 8);
}
static void _mem_write_8(uint32_t addr, uint8_t value) { RAM[addr] = value; }
static void _mem_write_16(uint32_t addr, uint16_t value) {
  memcpy(RAM + addr, &value, 2);
}
static uint8_t _port_read_8(uint16_t port) { return 0; }
static uint16_t _port_read_16(uint16_t port) { return 0; }
static void _port_write_8(uint16_t port, uint8_t value) {}
static void _port_write_16(uint16_t port, uint16_t value) {}
static void _int_call(uint16_t num) {}

uint8_t i8259_nextintr(void) {
  return 0; 
}
bool i8259_irq_pending(void) {
  return false;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

struct res_b_t {
  uint16_t flags;
  uint8_t val;
};

struct res_w_t {
  uint16_t flags;
  uint16_t val;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static uint32_t _rng_seed = _root_seed;

static _rand16(void) {
  uint32_t x = _rng_seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return (_rng_seed = x) & 0xffff;
}

static _rand8(void) {
  return _rand16() & 0xff;
}

static _rand1(void) {
  return _rand8() & 0x1;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static void _cpu_exec(const uint8_t *prog, const size_t size) {
  cpu_regs.ip = 0x0;
  cpu_regs.cs = 0x100;
  memcpy(RAM + 0x1000, prog, size);
  cpu_running = true;
  cpu_exec86(1);
}

enum {
  CF = (1 << 0),
  PF = (1 << 2),
  AF = (1 << 4),
  ZF = (1 << 6),
  SF = (1 << 7),
  TF = (1 << 8),
  IF = (1 << 9),
  DF = (1 << 10),
  OF = (1 << 11)
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

bool _compare_flags(uint16_t lhs, uint16_t rhs, uint16_t emu, uint16_t ref, uint16_t mask) {

  const uint16_t diff = (emu ^ ref) & mask;
  if (diff == 0) {
    return true;
  }

  printf("lhs:%04x rhs:%04x  ", lhs, rhs);

  printf("%c%c%c%c %c%c%c. .%c.%c",
    ((diff & OF) ? 'O' : '.'),
    ((diff & DF) ? 'D' : '.'),
    ((diff & IF) ? 'I' : '.'),
    ((diff & TF) ? 'T' : '.'),
    ((diff & SF) ? 'S' : '.'),
    ((diff & ZF) ? 'Z' : '.'),
    ((diff & AF) ? 'A' : '.'),
    ((diff & PF) ? 'P' : '.'),
    ((diff & CF) ? 'C' : '.')
  );

  return false;
}

bool _compare_val(const uint16_t lhs,
                  const uint16_t rhs,
                  const uint16_t emu,
                  const uint16_t ref) {

  if (emu == ref) {
    return true;
  }

  printf("lhs:%04x rhs:%04x got:%04x exp:%04x", lhs, rhs, emu, ref);

  return false;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

#define ref_op_b(NAME, OP)                                                    \
  static void NAME(uint8_t a, uint8_t b, struct res_b_t *out) {               \
    uint16_t fl = 0;                                                          \
    uint8_t  v  = 0;                                                          \
    __asm {                                                                   \
    __asm mov al, a                                                           \
    __asm mov bl, b                                                           \
    __asm OP al, bl                                                           \
    __asm pushf                                                               \
    __asm mov v, al                                                           \
    __asm pop ax                                                              \
    __asm mov fl, ax                                                          \
    }                                                                         \
    out->flags = fl;                                                          \
    out->val = v;                                                             \
  }

#define ref_op_w(NAME, OP)                                                    \
  static void NAME(uint16_t a, uint16_t b, struct res_w_t *out) {             \
    uint16_t fl = 0;                                                          \
    uint16_t v  = 0;                                                          \
    __asm {                                                                   \
    __asm mov ax, a                                                           \
    __asm mov bx, b                                                           \
    __asm OP ax, bx                                                           \
    __asm pushf                                                               \
    __asm mov v, ax                                                           \
    __asm pop ax                                                              \
    __asm mov fl, ax                                                          \
    }                                                                         \
    out->flags = fl;                                                          \
    out->val = v;                                                             \
  }

#define check_op_b(NAME, REF, MASK, ...)                                      \
  static bool NAME(void) {                                                    \
    cpu_reset();                                                              \
    const uint8_t o1 = _rand8();                                              \
    const uint8_t o2 = _rand8();                                              \
    cpu_regs.al = o1;                                                         \
    cpu_regs.bl = o2;                                                         \
                                                                              \
    const uint8_t prog[] = {__VA_ARGS__};                                     \
    _cpu_exec(prog, sizeof(prog));                                            \
                                                                              \
    const uint16_t emu_flags = cpu_get_flags();                               \
    struct res_b_t res;                                                       \
    REF(o1, o2, &res);                                                        \
                                                                              \
    if (!_compare_val(o1, o2, cpu_regs.al, res.val)) {                        \
      return false;                                                           \
    }                                                                         \
                                                                              \
    return _compare_flags(o1, o2, emu_flags, res.flags, MASK);                \
  }

#define check_op_w(NAME, REF, MASK, ...)                                      \
  static bool NAME(void) {                                                    \
    cpu_reset();                                                              \
    const uint16_t o1 = _rand16();                                            \
    const uint16_t o2 = _rand16();                                            \
    cpu_regs.ax = o1;                                                         \
    cpu_regs.bx = o2;                                                         \
                                                                              \
    const uint8_t prog[] = {__VA_ARGS__};                                     \
    _cpu_exec(prog, sizeof(prog));                                            \
                                                                              \
    const uint16_t emu_flags = cpu_get_flags();                               \
    struct res_w_t res;                                                       \
    REF(o1, o2, &res);                                                        \
                                                                              \
    if (!_compare_val(o1, o2, cpu_regs.ax, res.val)) {                        \
      return false;                                                           \
    }                                                                         \
                                                                              \
    return _compare_flags(o1, o2, emu_flags, res.flags, MASK);                \
  }

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t TEST_MASK = CF | OF | SF | ZF | PF;

ref_op_b(ref_test_b, test)
ref_op_w(ref_test_w, test)

check_op_b(_check_test_b, ref_test_b, TEST_MASK, 0x84, 0xd8);
check_op_w(_check_test_w, ref_test_w, TEST_MASK, 0x85, 0xd8);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t CMP_MASK = CF | OF | SF | ZF | AF | PF;

ref_op_b(ref_cmp_b, cmp)
ref_op_w(ref_cmp_w, cmp)

check_op_b(_check_cmp_b, ref_cmp_b, CMP_MASK, 0x38, 0xd8);
check_op_w(_check_cmp_w, ref_cmp_w, CMP_MASK, 0x39, 0xd8);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t ADD_MASK = CF | OF | SF | ZF | AF | PF;

ref_op_b(ref_add_b, add);
ref_op_w(ref_add_w, add);

check_op_b(_check_add_b, ref_add_b, ADD_MASK, 0x00, 0xd8);
check_op_w(_check_add_w, ref_add_w, ADD_MASK, 0x01, 0xd8);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t ADC_MASK = CF | OF | SF | ZF | AF | PF;

static uint16_t ref_adc_b(uint8_t a, uint8_t b, uint8_t c) {
  uint16_t res = 0;
  __asm {
    mov al, c
    test al, al
    clc
    jz no_carry
    stc
no_carry:
    mov al, a;
    mov bl, b;
    adc al, bl;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static uint16_t ref_adc_w(uint16_t a, uint16_t b, uint8_t c) {
  uint16_t res = 0;
  __asm {
    mov al, c
    test al, al
    clc
    jz no_carry
    stc
no_carry:
    mov ax, a;
    mov bx, b;
    adc ax, bx;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static bool _check_adc_b(void) {
  cpu_reset();
  const uint8_t o1 = _rand8();
  const uint8_t o2 = _rand8();
  const uint8_t c = _rand1();
  cpu_regs.al = o1;
  cpu_regs.bl = o2;
  cpu_flags.cf = c;

  const uint8_t prog[] = {0x10, 0xD8}; // ADC AL, BL
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_adc_b(o1, o2, c);

  return _compare_flags(o1, o2, emu_flags, ref_flags, ADC_MASK);
}

static bool _check_adc_w(void) {
  cpu_reset();
  const uint16_t o1 = _rand16();
  const uint16_t o2 = _rand16();
  const uint8_t c = _rand1();
  cpu_regs.ax = o1;
  cpu_regs.bx = o2;
  cpu_flags.cf = c;

  const uint8_t prog[] = {0x11, 0xD8}; // ADC AX, BX
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_adc_w(o1, o2, c);

  return _compare_flags(o1, o2, emu_flags, ref_flags, ADC_MASK);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t SUB_MASK = CF | OF | SF | ZF | AF | PF;

ref_op_b(ref_sub_b, sub)
ref_op_w(ref_sub_w, sub)

check_op_b(_check_sub_b, ref_sub_b, SUB_MASK, 0x28, 0xD8);
check_op_w(_check_sub_w, ref_sub_w, SUB_MASK, 0x29, 0xD8);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t SBB_MASK = CF | OF | SF | ZF | AF | PF;

static uint16_t ref_sbb_b(uint8_t a, uint8_t b, uint8_t c) {
  uint16_t res = 0;
  __asm {
    mov al, c
    test al, al
    clc
    jz no_carry
    stc
no_carry:
    mov al, a;
    mov bl, b;
    sbb al, bl;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static uint16_t ref_sbb_w(uint16_t a, uint16_t b, uint8_t c) {
  uint16_t res = 0;
  __asm {
    mov al, c
    test al, al
    clc
    jz no_carry
    stc
no_carry:
    mov ax, a;
    mov bx, b;
    sbb ax, bx;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static bool _check_sbb_b(void) {
  cpu_reset();
  const uint8_t o1 = _rand8();
  const uint8_t o2 = _rand8();
  const uint8_t c = _rand1();
  cpu_regs.al = o1;
  cpu_regs.bl = o2;
  cpu_flags.cf = c;

  const uint8_t prog[] = {0x18, 0xD8}; // SBB AL, BL
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_sbb_b(o1, o2, c);

  return _compare_flags(o1, o2, emu_flags, ref_flags, SBB_MASK);
}

static bool _check_sbb_w(void) {
  cpu_reset();
  const uint16_t o1 = _rand16();
  const uint16_t o2 = _rand16();
  const uint8_t c = _rand1();
  cpu_regs.ax = o1;
  cpu_regs.bx = o2;
  cpu_flags.cf = c;

  const uint8_t prog[] = {0x19, 0xD8}; // SBB AX, BX
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_sbb_w(o1, o2, c);

  return _compare_flags(o1, o2, emu_flags, ref_flags, SBB_MASK);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t AND_MASK = CF | OF | SF | ZF | PF;

ref_op_b(ref_and_b, and);
ref_op_w(ref_and_w, and);

check_op_b(_check_and_b, ref_and_b, AND_MASK, 0x20, 0xD8);
check_op_w(_check_and_w, ref_and_w, AND_MASK, 0x21, 0xD8);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t OR_MASK = CF | OF | SF | ZF | PF;

ref_op_b(ref_or_b, or);
ref_op_w(ref_or_w, or);

check_op_b(_check_or_b, ref_or_b, OR_MASK, 0x08, 0xD8);
check_op_w(_check_or_w, ref_or_w, OR_MASK, 0x09, 0xD8);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t XOR_MASK = CF | OF | SF | ZF | PF;

ref_op_b(ref_xor_b, xor);
ref_op_w(ref_xor_w, xor);

check_op_b(_check_xor_b, ref_xor_b, XOR_MASK, 0x30, 0xD8);
check_op_w(_check_xor_w, ref_xor_w, XOR_MASK, 0x31, 0xD8);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

#define ref_shift_b(NAME, OP)                                                           \
  void NAME(uint8_t val, uint8_t s, struct res_b_t *res) {                              \
    uint16_t fl = 0;                                                                    \
    uint8_t  v  = 0;                                                                    \
    __asm {                                                                             \
    __asm xor eax, eax                                                                  \
    __asm push ax                                                                       \
    __asm popf                                                                          \
    __asm mov al, val                                                                   \
    __asm mov cl, s                                                                     \
    __asm OP al, cl                                                                     \
    __asm pushf                                                                         \
    __asm mov v, al                                                                     \
    __asm pop ax                                                                        \
    __asm mov fl, ax                                                                    \
    }                                                                                   \
    res->flags = fl;                                                                    \
    res->val = v;                                                                       \
  }                                                                                     \

#define ref_shift_w(NAME, OP)                                                           \
  void NAME(uint16_t val, uint8_t s, struct res_w_t *res) {                             \
    uint16_t fl = 0;                                                                    \
    uint16_t  v  = 0;                                                                   \
    __asm {                                                                             \
    __asm xor eax, eax                                                                  \
    __asm push ax                                                                       \
    __asm popf                                                                          \
    __asm mov ax, val                                                                   \
    __asm mov cl, s                                                                     \
    __asm OP ax, cl                                                                     \
    __asm pushf                                                                         \
    __asm mov v, ax                                                                     \
    __asm pop ax                                                                        \
    __asm mov fl, ax                                                                    \
    }                                                                                   \
    res->flags = fl;                                                                    \
    res->val = v;                                                                       \
  }

#define _check_shift_b(NAME, REF, MASK, ...)                                            \
  static bool NAME(void) {                                                              \
    cpu_reset();                                                                        \
                                                                                        \
    const uint8_t al = _rand8();                                                        \
    const uint8_t cl = _rand8() & 0x1f;                                                 \
    cpu_regs.al = al;                                                                   \
    cpu_regs.cl = cl;                                                                   \
                                                                                        \
    const uint8_t code[] = { __VA_ARGS__ };                                             \
    _cpu_exec(code, sizeof(code));                                                      \
                                                                                        \
    struct res_b_t res;                                                                 \
    REF(al, cl, &res);                                                                  \
    const uint16_t emu = cpu_get_flags();                                               \
                                                                                        \
    if (!_compare_val(al, cl, cpu_regs.al, res.val)) {                                  \
      return false;                                                                     \
    }                                                                                   \
                                                                                        \
    uint16_t mask = SF | ZF | PF;                                                       \
    if (cl == 0) { mask |= AF; }                                                        \
    if (cl == 1) { mask |= OF; }                                                        \
    if (cl < 8)  { mask |= CF; }                                                        \
    return _compare_flags(al, cl, emu, res.flags, mask & MASK);                         \
  }

#define _check_shift_w(NAME, OP, MASK, ...)                                             \
  bool NAME(void) {                                                                     \
    cpu_reset();                                                                        \
                                                                                        \
    const uint16_t ax = _rand8();                                                       \
    const uint8_t cl = _rand8() & 0x1f;                                                 \
    cpu_regs.ax = ax;                                                                   \
    cpu_regs.cl = cl;                                                                   \
                                                                                        \
    const uint8_t code[] = { __VA_ARGS__ };                                             \
    _cpu_exec(code, sizeof(code));                                                      \
                                                                                        \
    struct res_w_t res;                                                                 \
    OP(ax, cl, &res);                                                                   \
    const uint16_t emu = cpu_get_flags();                                               \
                                                                                        \
    if (!_compare_val(ax, cl, cpu_regs.ax, res.val)) {                                  \
      return false;                                                                     \
    }                                                                                   \
                                                                                        \
    uint16_t mask =  SF | ZF | PF;                                                      \
    if (cl == 0) { mask |= AF; }                                                        \
    if (cl == 1) { mask |= OF; }                                                        \
    if (cl < 8)  { mask |= CF; }                                                        \
    return _compare_flags(ax, cl, emu, res.flags, mask & MASK);                         \
  }

ref_shift_b(ref_shl_b, shl)
ref_shift_w(ref_shl_w, shl)

_check_shift_b(_check_shl_b, ref_shl_b, -1, 0xD2, 0xE0)
_check_shift_w(_check_shl_w, ref_shl_w, -1, 0xD3, 0xE0)

ref_shift_b(ref_shr_b, shr)
ref_shift_w(ref_shr_w, shr)

_check_shift_b(_check_shr_b, ref_shr_b, -1, 0xD2, 0xE8)
_check_shift_w(_check_shr_w, ref_shr_w, -1, 0xD3, 0xE8)

ref_shift_b(ref_sar_b, sar)
ref_shift_w(ref_sar_w, sar)

_check_shift_b(_check_sar_b, ref_sar_b, -1, 0xD2, 0xF8)
_check_shift_w(_check_sar_w, ref_sar_w, -1, 0xD3, 0xF8)

ref_shift_b(ref_rol_b, rol)
ref_shift_w(ref_rol_w, rol)

_check_shift_b(_check_rol_b, ref_rol_b, CF | OF, 0xD2, 0xC0)
_check_shift_w(_check_rol_w, ref_rol_w, CF | OF, 0xD3, 0xC0)

ref_shift_b(ref_ror_b, ror)
ref_shift_w(ref_ror_w, ror)

_check_shift_b(_check_ror_b, ref_ror_b, CF | OF, 0xD2, 0xC8)
_check_shift_w(_check_ror_w, ref_ror_w, CF | OF, 0xD3, 0xC8)

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t MUL_MASK = CF | OF;

uint16_t ref_mul_b(const uint8_t lhs, const uint8_t rhs) {
  uint16_t flags = 0;
  __asm {
  __asm xor eax, eax
  __asm xor ebx, ebx
  __asm mov al, lhs
  __asm mov bl, rhs
  __asm mul bl
  __asm pushf
  __asm pop flags
  }
  return flags;
}

uint16_t ref_mul_w(const uint16_t lhs, const uint16_t rhs) {
  uint16_t flags = 0;
  __asm {
  __asm xor eax, eax
  __asm xor ebx, ebx
  __asm mov ax, lhs
  __asm mov bx, rhs
  __asm mul bx
  __asm pushf
  __asm pop flags
  }
  return flags;
}

static bool _check_mul_b(void) {
  cpu_reset();
  const uint8_t al = _rand8();
  const uint8_t bl = _rand8();
  cpu_regs.al = al;
  cpu_regs.bl = bl;
  const uint8_t code[] = {0xF6, 0xE3}; // mul bl
  _cpu_exec(code, sizeof(code));
  const uint16_t emu = cpu_get_flags();
  const uint16_t ref = ref_mul_b(al, bl);
  return _compare_flags(al, bl, emu, ref, MUL_MASK);
}

static bool _check_mul_w(void) {
  cpu_reset();
  const uint8_t ax = _rand8();
  const uint8_t bx = _rand8();
  cpu_regs.ax = ax;
  cpu_regs.bx = bx;
  const uint8_t code[] = {0xF7, 0xE3}; // mul bx
  _cpu_exec(code, sizeof(code));
  const uint16_t emu = cpu_get_flags();
  const uint16_t ref = ref_mul_w(ax, bx);
  return _compare_flags(ax, bx, emu, ref, MUL_MASK);
}

static const uint32_t IMUL_MASK = CF | OF;

static uint16_t ref_imul_b(const uint8_t lhs, const uint8_t rhs) {
  uint16_t flags = 0;
  __asm {
  __asm mov al, lhs
  __asm mov bl, rhs
  __asm imul bl
  __asm pushf
  __asm pop flags
  }
  return flags;
}

static uint16_t ref_imul_w(const uint16_t lhs, const uint16_t rhs) {
  uint16_t flags = 0;
  __asm {
  __asm mov ax, lhs
  __asm mov bx, rhs
  __asm imul bx
  __asm pushf
  __asm pop flags
  }
  return flags;
}

static bool _check_imul_b(void) {
  cpu_reset();
  const uint8_t al = _rand8();
  const uint8_t bl = _rand8();
  cpu_regs.al = al;
  cpu_regs.bl = bl;
  const uint8_t code[] = {0xF6, 0xEB}; // imul bl
  _cpu_exec(code, sizeof(code));
  const uint16_t emu = cpu_get_flags();
  const uint16_t ref = ref_imul_b(al, bl);
  return _compare_flags(al, bl, emu, ref, IMUL_MASK);
}

static bool _check_imul_w(void) {
  cpu_reset();
  const uint8_t ax = _rand8();
  const uint8_t bx = _rand8();
  cpu_regs.ax = ax;
  cpu_regs.bx = bx;
  const uint8_t code[] = {0xF7, 0xEB}; // imul bx
  _cpu_exec(code, sizeof(code));
  const uint16_t emu = cpu_get_flags();
  const uint16_t ref = ref_imul_w(ax, bx);
  return _compare_flags(ax, bx, emu, ref, IMUL_MASK);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t DIV_MASK = CF | OF | SF | ZF | PF;

static uint16_t ref_div_b(const uint8_t lhs, const uint8_t rhs) {
  uint16_t flags = 0;
  __asm {
  __asm mov al, lhs
  __asm mov bl, rhs
  __asm div bl
  __asm pushf
  __asm pop flags
  }
  return flags;
}

static bool _check_div_b(void) {
  cpu_reset();
  const uint8_t al = _rand8();
  const uint8_t bl = _rand8();
  if (bl == 0) return true;
  cpu_regs.al = al;
  cpu_regs.bl = bl;
  const uint8_t code[] = {0xF6, 0xF3}; // div bl
  _cpu_exec(code, sizeof(code));
  const uint16_t emu = cpu_get_flags();
  const uint16_t ref = ref_div_b(al, bl);
  return _compare_flags(al, bl, emu, ref, DIV_MASK);
}

static uint16_t ref_div_w(const uint16_t lhs, const uint16_t rhs) {
  uint16_t flags = 0;
  __asm {
  __asm mov ax, lhs
  __asm mov bx, rhs
  __asm div bx
  __asm pushf
  __asm pop flags
  }
  return flags;
}

static bool _check_div_w(void) {
  cpu_reset();
  const uint8_t ax = _rand8();
  const uint8_t bx = _rand8();
  if (bx == 0) return true;
  cpu_regs.ax = ax;
  cpu_regs.bx = bx;
  const uint8_t code[] = {0xF7, 0xF3}; // div bx
  _cpu_exec(code, sizeof(code));
  const uint16_t emu = cpu_get_flags();
  const uint16_t ref = ref_div_w(ax, bx);
  return _compare_flags(ax, bx, emu, ref, DIV_MASK);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t JMP_MASK = CF | OF | SF | ZF | AF | PF;

#define ref_jump(NAME, OP)                            \
  static uint16_t NAME(uint16_t flags) {              \
    uint16_t res = 0;                                 \
    __asm {                                           \
    __asm xor ax, ax                                  \
    __asm push flags                                  \
    __asm popf                                        \
    __asm OP target                                   \
    __asm inc ax                                      \
    __asm target:                                     \
    __asm mov res, ax                                 \
    };                                                \
    return res ^ 1;                                   \
  }

#define _check_jmp(NAME, REF, ...)                    \
  static bool NAME(void) {                            \
    cpu_reset();                                      \
    const uint16_t flags = _rand16() & JMP_MASK;      \
    cpu_set_flags(flags);                             \
    const uint8_t prog[] = {__VA_ARGS__};             \
    _cpu_exec(prog, sizeof(prog));                    \
    const uint16_t res = REF(flags);                  \
    const uint16_t tak = (cpu_regs.ip == 3);          \
    return res == tak;                                \
  }

ref_jump(ref_jo, jo);
_check_jmp(_check_jo, ref_jo, 0x70, 0x01);

ref_jump(ref_jno, jno);
_check_jmp(_check_jno, ref_jno, 0x71, 0x01);

ref_jump(ref_js, js);
_check_jmp(_check_js, ref_js, 0x78, 0x01);

ref_jump(ref_jns, jns);
_check_jmp(_check_jns, ref_jns, 0x79, 0x01);

ref_jump(ref_jz, jz);
_check_jmp(_check_jz, ref_jz, 0x74, 0x01);

ref_jump(ref_jnz, jnz);
_check_jmp(_check_jnz, ref_jnz, 0x75, 0x01);

ref_jump(ref_jb, jb);
_check_jmp(_check_jb, ref_jb, 0x72, 0x01);

ref_jump(ref_jnb, jnb);
_check_jmp(_check_jnb, ref_jnb, 0x73, 0x01);

ref_jump(ref_jbe, jbe);
_check_jmp(_check_jbe, ref_jbe, 0x76, 0x01);

ref_jump(ref_ja, ja);
_check_jmp(_check_ja, ref_ja, 0x77, 0x01);

ref_jump(ref_jl, jl);
_check_jmp(_check_jl, ref_jl, 0x7c, 0x01);

ref_jump(ref_jge, jge);
_check_jmp(_check_jge, ref_jge, 0x7d, 0x01);

ref_jump(ref_jle, jle);
_check_jmp(_check_jle, ref_jle, 0x7e, 0x01);

ref_jump(ref_jg, jg);
_check_jmp(_check_jg, ref_jg, 0x7f, 0x01);

ref_jump(ref_jp, jp);
_check_jmp(_check_jp, ref_jp, 0x7a, 0x01);

ref_jump(ref_jnp, jnp);
_check_jmp(_check_jnp, ref_jnp, 0x7b, 0x01);

static uint16_t ref_jcxz(uint16_t val) {
  uint16_t res = 0;
  __asm {
    mov cx, val
    xor ax, ax
    jcxz target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jcxz(void) {
  cpu_reset();
  const uint16_t cx = _rand16() >> (_rand8() % 16);
  cpu_regs.cx = cx;
  const uint8_t prog[] = {0xE3, 0x01}; // JXCZ 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jcxz(cx);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

typedef bool (*test_t)(void);

struct test_info_t {
  test_t func;
  const char *name;
};

static struct test_info_t test[] = {

  {_check_test_b, "TEST byte"},
  {_check_test_w, "TEST word"},
  {_check_cmp_b,  "CMP byte"},
  {_check_cmp_w,  "CMP word"},

  {_check_add_b,  "ADD byte"},
  {_check_add_w,  "ADD word"},
  {_check_adc_b,  "ADC byte"},
  {_check_adc_w,  "ADC word"},
  {_check_sub_b,  "SUB byte"},
  {_check_sub_w,  "SUB word"},
  {_check_sbb_b,  "SBB byte"},
  {_check_sbb_w,  "SBB word"},
  {_check_and_b,  "AND byte"},
  {_check_and_w,  "AND word"},
  {_check_or_b,   "OR byte"},
  {_check_or_w,   "OR word"},
  {_check_or_b,   "XOR byte"},
  {_check_or_w,   "XOR word"},
  {_check_shl_b,  "SHL byte"},
  {_check_shl_w,  "SHL word"},
  {_check_shr_b,  "SHR byte"},
  {_check_shr_w,  "SHR word"},
  {_check_sar_b,  "SAR byte"},
  {_check_sar_w,  "SAR word"},
  {_check_rol_b,  "ROL byte"},
  {_check_rol_w,  "ROL word"},
  {_check_ror_b,  "ROR byte"},
  {_check_ror_w,  "ROR word"},

  {_check_mul_b,  "MUL byte"},
  {_check_mul_w,  "MUL word"},
  {_check_imul_b, "IMUL byte"},
  {_check_imul_w, "IMUL word"},
//  {_check_div_b,  "DIV byte"},
//  {_check_div_w,  "DIV word"},

  {_check_jo,     "JO"},
  {_check_jno,    "JNO"},
  {_check_js,     "JS"},
  {_check_jns,    "JNS"},
  {_check_jz,     "JZ"},
  {_check_jnz,    "JNZ"},
  {_check_jb,     "JB"},
  {_check_jnb,    "JNB"},
  {_check_jbe,    "JBE"},
  {_check_ja,     "JA"},
  {_check_jl,     "JL"},
  {_check_jle,    "JLE"},
  {_check_jg,     "JG"},
  {_check_jnp,    "JNP"},

  {_check_jcxz,   "JCXZ"},

  {NULL, NULL},
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

void setup_cpu_io(void) {
  struct cpu_io_t io;
  io.ram = RAM;
  io.mem_read_8 = _mem_read_8;
  io.mem_read_16 = _mem_read_16;
  io.mem_write_8 = _mem_write_8;
  io.mem_write_16 = _mem_write_16;
  io.port_read_8 = _port_read_8;
  io.port_read_16 = _port_read_16;
  io.port_write_8 = _port_write_8;
  io.port_write_16 = _port_write_16;
  io.int_call = _int_call;
  cpu_set_io(&io);
}

int main(int argc, char **args) {

  log_mute(true);
  setup_cpu_io();

  uint32_t num_tests = 0;
  uint32_t num_passed = 0;

  struct test_info_t *info = test;
  for (;info->func; ++info) {

    ++num_tests;

    printf("%20s  ", info->name);
    _rng_seed = _root_seed;
    bool ok = true;
    for (int i=0; i<_test_runs; ++i) {
      const uint32_t seed = _rng_seed;
      if (!info->func()) {
        printf("  fail (%08u)", seed);
        ok = false;
        break;
      }
    }
    if (ok) {
      ++num_passed;
      printf("ok");
    }
    printf("\n");
  }

  printf("\n");
  printf("%d of %d passed\n", num_passed, num_tests);

  getchar();

  return num_tests == num_passed;
}
