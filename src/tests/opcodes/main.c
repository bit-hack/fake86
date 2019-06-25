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

static _rng_seed = _root_seed;

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

bool _compare_flags(uint16_t emu, uint16_t ref, uint16_t mask) {

  const uint16_t diff = (emu ^ ref) & mask;
  if (diff == 0) {
    return true;
  }

  printf("%c%c%c%c %c%c%c. .%c.%c\n",
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

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t TEST_MASK = CF | OF | SF | ZF | PF;

static uint16_t ref_test_b(uint8_t a, uint8_t b) {
  uint16_t res = 0;
  __asm {
    mov al, a;
    mov bl, b;
    test al, bl;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static uint16_t ref_test_w(uint16_t a, uint16_t b) {
  uint16_t res = 0;
  __asm {
    mov ax, a;
    mov bx, b;
    test ax, bx;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static bool _check_test_b(void) {
  cpu_reset();
  const uint8_t o1 = _rand8();
  const uint8_t o2 = _rand8();
  cpu_regs.al = o1;
  cpu_regs.bl = o2;
  const uint8_t prog[] = {0x84, 0xD8}; // TEST AL, BL
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_test_b(o1, o2);

  return _compare_flags(emu_flags, ref_flags, TEST_MASK);
}

static bool _check_test_w(void) {
  cpu_reset();
  const uint16_t o1 = _rand16();
  const uint16_t o2 = _rand16();
  cpu_regs.ax = o1;
  cpu_regs.bx = o2;
  const uint8_t prog[] = {0x85, 0xD8}; // TEST AX, BX
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_test_w(o1, o2);

  return _compare_flags(emu_flags, ref_flags, TEST_MASK);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t CMP_MASK = CF | OF | SF | ZF | AF | PF;

static uint16_t ref_cmp_b(uint8_t a, uint8_t b) {
  uint16_t res = 0;
  __asm {
    mov al, a;
    mov bl, b;
    cmp al, bl;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static uint16_t ref_cmp_w(uint16_t a, uint16_t b) {
  uint16_t res = 0;
  __asm {
    mov ax, a;
    mov bx, b;
    cmp ax, bx;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static bool _check_cmp_b(void) {
  cpu_reset();
  const uint8_t o1 = _rand8();
  const uint8_t o2 = _rand8();
  cpu_regs.al = o1;
  cpu_regs.bl = o2;
  const uint8_t prog[] = {0x38, 0xD8}; // CMP AL, BL
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_cmp_b(o1, o2);

  return _compare_flags(emu_flags, ref_flags, CMP_MASK);
}

static bool _check_cmp_w(void) {
  cpu_reset();
  const uint16_t o1 = _rand16();
  const uint16_t o2 = _rand16();
  cpu_regs.ax = o1;
  cpu_regs.bx = o2;
  const uint8_t prog[] = {0x39, 0xD8}; // CMP AX, BX
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_cmp_w(o1, o2);

  return _compare_flags(emu_flags, ref_flags, CMP_MASK);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t ADD_MASK = CF | OF | SF | ZF | AF | PF;

static uint16_t ref_add_b(uint8_t a, uint8_t b) {
  uint16_t res = 0;
  __asm {
    mov al, a;
    mov bl, b;
    add al, bl;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static uint16_t ref_add_w(uint16_t a, uint16_t b) {
  uint16_t res = 0;
  __asm {
    mov ax, a;
    mov bx, b;
    add ax, bx;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static bool _check_add_b(void) {
  cpu_reset();
  const uint8_t o1 = _rand8();
  const uint8_t o2 = _rand8();
  cpu_regs.al = o1;
  cpu_regs.bl = o2;
  const uint8_t prog[] = {0x00, 0xD8}; // ADD AL, BL
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_add_b(o1, o2);

  return _compare_flags(emu_flags, ref_flags, ADD_MASK);
}

static bool _check_add_w(void) {
  cpu_reset();
  const uint16_t o1 = _rand16();
  const uint16_t o2 = _rand16();
  cpu_regs.ax = o1;
  cpu_regs.bx = o2;
  const uint8_t prog[] = {0x01, 0xD8}; // ADD AX, BX
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_add_w(o1, o2);

  return _compare_flags(emu_flags, ref_flags, ADD_MASK);
}

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

  return _compare_flags(emu_flags, ref_flags, ADC_MASK);
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

  return _compare_flags(emu_flags, ref_flags, ADC_MASK);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t SUB_MASK = CF | OF | SF | ZF | AF | PF;

static uint16_t ref_sub_b(uint8_t a, uint8_t b) {
  uint16_t res = 0;
  __asm {
    mov al, a;
    mov bl, b;
    sub al, bl;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static uint16_t ref_sub_w(uint16_t a, uint16_t b) {
  uint16_t res = 0;
  __asm {
    mov ax, a;
    mov bx, b;
    sub ax, bx;
    pushf;
    pop ax;
    mov res, ax;
  };
  return res;
}

static bool _check_sub_b(void) {
  cpu_reset();
  const uint8_t o1 = _rand8();
  const uint8_t o2 = _rand8();
  cpu_regs.al = o1;
  cpu_regs.bl = o2;

  const uint8_t prog[] = {0x28, 0xD8}; // SUB AL, BL
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_sub_b(o1, o2);

  return _compare_flags(emu_flags, ref_flags, SUB_MASK);
}

static bool _check_sub_w(void) {
  cpu_reset();
  const uint16_t o1 = _rand16();
  const uint16_t o2 = _rand16();
  cpu_regs.ax = o1;
  cpu_regs.bx = o2;

  const uint8_t prog[] = {0x29, 0xD8}; // SUB AX, BX
  _cpu_exec(prog, sizeof(prog));

  const uint16_t emu_flags = cpu_get_flags();
  const uint16_t ref_flags = ref_sub_w(o1, o2);

  return _compare_flags(emu_flags, ref_flags, ADC_MASK);
}

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

  return _compare_flags(emu_flags, ref_flags, SBB_MASK);
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

  return _compare_flags(emu_flags, ref_flags, SBB_MASK);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static const uint32_t JMP_MASK = CF | OF | SF | ZF | AF | PF;

static uint16_t ref_jo(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jo target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jo(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x70, 0x01}; // JO 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jo(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jno(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jno target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jno(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x71, 0x01}; // JNO 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jno(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_js(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    js target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_js(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x78, 0x01}; // JS 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_js(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jns(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jns target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jns(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x79, 0x01}; // JNS 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jns(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jz(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jz target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jz(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x74, 0x01}; // JZ 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jz(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jnz(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jnz target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jnz(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x75, 0x01}; // JNZ 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jnz(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jb(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jb target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jb(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x72, 0x01}; // JB 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jb(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jnb(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jnb target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jnb(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x73, 0x01}; // JNB 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jnb(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jbe(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jbe target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jbe(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x76, 0x01}; // JBE 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jbe(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_ja(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    ja target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_ja(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x77, 0x01}; // JA 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_ja(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jl(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jl target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jl(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x7C, 0x01}; // JL 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jl(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jge(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jge target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jge(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x7D, 0x01}; // JGE 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jge(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jle(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jle target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jle(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x7E, 0x01}; // JLE 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jle(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}


static uint16_t ref_jg(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jg target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jg(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x7F, 0x01}; // JG 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jg(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jp(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jp target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jp(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x7A, 0x01}; // JP 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jp(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

static uint16_t ref_jnp(uint16_t flags) {
  uint16_t res = 0;
  __asm {
    xor ax, ax
    push flags
    popf
    jnp target
    inc ax
target:
    mov res, ax;
  };
  return res ^ 1;
}

static bool _check_jnp(void) {
  cpu_reset();
  const uint16_t flags = _rand16() & JMP_MASK;
  cpu_set_flags(flags);
  const uint8_t prog[] = {0x7B, 0x01}; // JNP 1
  _cpu_exec(prog, sizeof(prog));
  const uint16_t res = ref_jnp(flags);
  const uint16_t tak = (cpu_regs.ip == 3);
  return res == tak;
}

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
        printf("fail (%08u)", seed);
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
  printf("%d of %d passed\n", num_tests, num_passed);

  getchar();

  return num_tests == num_passed;
}
