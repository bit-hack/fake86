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
  return _rng_seed = x;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static void _cpu_exec(const uint8_t *prog, const size_t size) {
  cpu_regs.ip = 0x0;
  cpu_regs.cs = 0x100;
  memcpy(RAM + 0x1000, prog, size);
  cpu_exec86(1);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static bool _check_test_b(void) {
  cpu_reset();
  const uint8_t o1 = _rand16();
  const uint8_t o2 = _rand16();
  cpu_regs.al = o1;
  cpu_regs.bl = o2;
  const uint8_t prog[]= {0x84, 0xD8}; // TEST AL, BL
  _cpu_exec(prog, sizeof(prog));
  return true;
}

static bool _check_test_w(void) {
  cpu_reset();
  const uint16_t o1 = _rand16();
  const uint16_t o2 = _rand16();
  cpu_regs.ax = o1;
  cpu_regs.bx = o2;
  const uint8_t prog[]= {0x85, 0xD8}; // TEST AX, BX
  _cpu_exec(prog, sizeof(prog));
  return true;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static bool _check_cmp_b(void) {
  cpu_reset();
  const uint8_t o1 = _rand16();
  const uint8_t o2 = _rand16();
  cpu_regs.al = o1;
  cpu_regs.bl = o2;
  const uint8_t prog[]= {0x38, 0xD8}; // CMP AL, BL
  _cpu_exec(prog, sizeof(prog));
  return true;
}

static bool _check_cmp_w(void) {
  cpu_reset();
  const uint16_t o1 = _rand16();
  const uint16_t o2 = _rand16();
  cpu_regs.ax = o1;
  cpu_regs.bx = o2;
  const uint8_t prog[]= {0x39, 0xD8}; // CMP AX, BX
  _cpu_exec(prog, sizeof(prog));
  return true;
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

  return num_tests == num_passed;
}
