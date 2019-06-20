#include <stdio.h>

#include "../cpu/cpu.h"
#include "../common/common.h"


#define ASSERT(X) { if (!(X)) return false; }


static int32_t _cpu_run(const uint32_t cycles) {
  cpu_running = true;
  return cpu_exec86(cycles);
}

static int32_t _cpu_setup(const uint8_t *src, uint32_t size) {
  memset(RAM, 0, sizeof(RAM));
  memcpy(RAM + 0x7100, src, size);
  cpu_regs.cs = 0x700;
  cpu_regs.ip = 0x100;
}

// TEST: mov [bx + di], dl
bool test_opcode_88_0(void) {
  const uint8_t code[] = {0x88, 0x11};
  _cpu_setup(code, sizeof(code));
  cpu_regs.dl = 0x69;
  cpu_regs.ds = 0x100;
  cpu_regs.bx = 0x100;
  cpu_regs.di = 0x10;
  const uint16_t ip = cpu_regs.ip;
  ASSERT(_cpu_run(1) == 1);
  ASSERT((cpu_regs.ip - ip) == 2);
  ASSERT(RAM[0x1110] == 0x69);
  return true;
}

// TEST: mov [0x0110], al
bool test_opcode_88_1(void) {
  const uint8_t code[] = {0x88, 0x06, 0x10, 0x01};
  _cpu_setup(code, sizeof(code));
  cpu_regs.al = 0x69;
  cpu_regs.ds = 0x100;
  const uint16_t ip = cpu_regs.ip;
  ASSERT(_cpu_run(1) == 1);
  ASSERT((cpu_regs.ip - ip) == 4);
  ASSERT(RAM[0x1110] == 0x69);
  return true;
}

typedef bool (*test_t)(void);

struct test_info_t {
  test_t func;
  const char *name;
};

static const struct test_info_t tests[] = {
  { test_opcode_88_0, "opcode 88 # 1" },
  { test_opcode_88_1, "opcode 88 # 2" },
  {0, 0}
};

int main(int argc, char **args) {

  uint32_t passed = 0;
  uint32_t total = 0;

  const struct test_info_t *test = tests;
  for (; test->func; ++test) {
    ++total;
    cpu_reset();
    if (test->func()) {
      ++passed;
      fprintf(stderr, "passed: %s\n", test->name);
    }
    else {
      fprintf(stderr, "failed: %s\n", test->name);
    }
  }

  printf("%d of %d passed\n", (int)passed, (int)total);
  return passed == total;
}
