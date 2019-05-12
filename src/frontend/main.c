/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers

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

/* main.c: functions to initialize the different components of Fake86,
   load ROM binaries, and kickstart the CPU emulator. */

#include <math.h>

#include "../fake86/common.h"

#include "../80x86/cpu.h"


extern uint8_t running, renderbenchmark;

void exec86(uint32_t execloops);
bool render_init(void);
void tick_events(void);
void handlevideo();

extern bool cursorvisible;
extern volatile bool scrmodechange;
extern uint8_t doaudio;
extern uint8_t updatedscreen;

const char *biosfile = "pcxtbios.bin";

uint8_t *verbose = 0, cgaonly = 0, useconsole = 0;

extern uint8_t insertdisk(uint8_t drivenum, char *filename);
extern void ejectdisk(uint8_t drivenum);
extern uint8_t bootdrive, ethif, net_enabled;
extern void doirq(uint8_t irqnum);

extern bool cl_parse(int argc, char *argv[]);

void timing();
void tickaudio();
void inittiming();
void initaudio();

extern void initVideoPorts();
extern void killaudio();
extern void initsermouse(uint16_t baseport, uint8_t irq);

extern void initadlib(uint16_t baseport);
extern void initsoundsource();
extern void isa_ne2000_init(uint16_t baseport, uint8_t irq);
extern void initBlaster(uint16_t baseport, uint8_t irq);

uint8_t dohardreset = 0;
uint8_t audiobufferfilled();
uint8_t usessource = 0;

#ifdef NETWORKING_ENABLED
extern void initpcap();
extern void dispatch();
#endif

static void inithardware() {
#ifdef NETWORKING_ENABLED
  if (ethif != 254)
    initpcap();
#endif
  printf("Initializing emulated hardware:\n");

  printf("  - Intel 8253 timer: ");
  i8253_init();
  printf("OK\n");

  printf("  - Intel 8259 interrupt controller: ");
  i8259_init();
  printf("OK\n");

  printf("  - Intel 8237 DMA controller: ");
  i8237_init();
  printf("OK\n");

  printf("  - Intel 8255 peripheral interface adapter: ");
  i8255_init();
  printf("OK\n");

  initVideoPorts();

#if 0
  if (usessource) {
    printf("  - Disney Sound Source: ");
    initsoundsource();
    printf("OK\n");
  }
#endif

#if !defined(NETWORKING_OLDCARD) && 0
  printf("  - Novell NE2000 ethernet adapter: ");
  isa_ne2000_init(0x300, 6);
  printf("OK\n");
#endif

#if 0
  printf("  - Adlib FM music card: ");
  initadlib(0x388);
  printf("OK\n");

  printf("  - Creative Labs Sound Blaster 2.0: ");
  initBlaster(0x220, 7);
  printf("OK\n");
#endif

  printf("  - Serial mouse (Microsoft compatible): ");
  mouse_init(0x3F8, 4);
  printf("OK\n");

#if 0
  if (doaudio) {
    initaudio();
  }
#endif

#if 0
  inittiming();
#endif

  render_init();
}

static void exit_handler(void) {
  log_close();
}

void render_update(void);
void render_check_for_mode_change(void);

static int64_t tick_cpu(int64_t num_cycles) {
  if (dohardreset) {
    cpu_reset();
    dohardreset = 0;
  }
  return cpu_exec86((int32_t)num_cycles);
}

static void tick_render() {
  updatedscreen = 1;
  render_check_for_mode_change();
  render_update();
}

static uint64_t get_ticks() {
#if 1
  return SDL_GetTicks();
#else
  static uint64_t ticks = 0;
  return ticks += 10;
#endif
}

static void tick_cursor(uint64_t cycles) {
  static uint64_t cursor_accum;
  cursor_accum += cycles;
  if (cursor_accum >= CYCLES_PER_SECOND) {
    cursor_accum -= CYCLES_PER_SECOND;
  }
  cursorvisible = cursor_accum > (CYCLES_PER_SECOND / 2);
}

extern uint8_t port3da;
uint64_t cga_accum = 0;

static void tick_cga(uint64_t cycles) {

  // 1000000us in a second

  // for (640×480 "60 Hz" non-interlaced) and (320×200 @ 70 Hz)
  //   total htime 31.778us
  //   hblank for   6.356us
  //   total vtime 16.683ms
  //   vblank for   1.430ms

  // hblank for 20.0012587325% of line
  // vblank for 8.57159983216% of frame

  double total_htime =    31.778;
  double total_vtime = 16683.0;

  uint64_t num_lines = (uint64_t)(total_vtime / total_htime);

  const uint64_t cycles_per_frame = CYCLES_PER_SECOND / 60;
  const uint64_t cycles_per_line = cycles_per_frame / num_lines;

  cga_accum = (cga_accum + cycles) % cycles_per_frame;

  const vpos = cga_accum;
  const hpos = cga_accum % cycles_per_line;

  // set hblank bit
  if (hpos < ((cycles_per_line * 20) / 100)) {
    port3da |= 0x01;
  }
  else {
    port3da &= 0xFE;
  }

  // set vblank bit
  if (vpos > ((cycles_per_frame * 91) / 100)) {
    port3da |= 0x08;
  }
  else {
    port3da &= 0xF7;
  }
}

static void tick_hardware(uint64_t cycles) {
  tick_cursor(cycles);
  // dma controller
  i8237_tick(cycles);
  // PIC controller
  i8259_tick(cycles);
  // PIA
  i8255_tick(cycles);
}

// fine grained timing
void timing(uint64_t cycles) {
  // PIT timer
  i8253_tick(cycles);
  // CGA status register
  tick_cga(cycles);
}

static void emulate_loop(void) {

#define TICK_SLICES (100)
#define CYCLES_PER_SLICE (CYCLES_PER_SECOND / TICK_SLICES)
#define CYCLES_PER_REFRESH (CYCLES_PER_SECOND / 30)
#define MSTOCYCLES(X) ((X) * (CYCLES_PER_SECOND / 1000))

  int64_t cpu_acc = 0;
  int64_t video_acc = 0;
  uint64_t bench_acc = 0;

  uint64_t old_ms = get_ticks();

  uint64_t old_bench = get_ticks();

  while (running) {

    // diff cycle time
    const uint64_t new_ms = get_ticks();
    const uint64_t new_cycles = MSTOCYCLES(new_ms - old_ms);

    // avoid fastforwaring on lag
    if (new_ms - old_ms > 1000) {
      old_ms = new_ms;
      continue;
    }

    bool video_redraw = false;

    // update cpu
    while (cpu_acc <= 0) {
      const uint64_t executed = tick_cpu(CYCLES_PER_SLICE);
      bench_acc += executed;
      cpu_acc += executed;
      video_acc += executed;

      // update video
      if (video_acc >= CYCLES_PER_REFRESH) {
        video_acc -= CYCLES_PER_REFRESH;
        video_redraw = true;
      }

      // tick peripherals
      tick_hardware(executed);
    }

    // refresh the screen buffer
    if (video_redraw) {
      tick_render();
    }

    // parse events from host
    tick_events();

    // advance cpu or sleep
    if ((int64_t)new_cycles >= cpu_acc) {
      cpu_acc -= new_cycles;
      old_ms = new_ms;
    }
    else {
      SDL_Delay(1);
    }

#if 1
    // benchmark cps
    if ((get_ticks() - old_bench) > 1000) {
      old_bench += 1000;
      printf("cps: %d\n", (uint32_t)bench_acc);
      bench_acc = 0;
    }
#endif

#ifdef NETWORKING_ENABLED
    if (ethif < 254)
      dispatch();
#endif
  }
}

int main(int argc, char *argv[]) {

  // setup out exit handler
  atexit(exit_handler);
  // initialize the log file
  log_init();
  // parse the input command line
  if (!cl_parse(argc, argv)) {
    return 1;
  }
  // initialize memory
  mem_init();
  // load bios
  const uint32_t biossize = mem_loadbios(biosfile);
  if (!biossize)
    return -1;
#ifdef DISK_CONTROLLER_ATA
  if (!mem_loadrom(0xD0000UL, PATH_DATAFILES "ide_xt.bin", 1))
    return (-1);
#endif
  // load other roms
  if (biossize <= (1024 * 8)) {
    mem_loadrom(0xF6000UL, PATH_DATAFILES "rombasic.bin", 0);
    if (!mem_loadrom(0xC0000UL, PATH_DATAFILES "videorom.bin", 1))
      return (-1);
  }
  running = 1;
  cpu_reset();
  cpu_set_intcall_handler(intcall86);

  inithardware();

  // enter the emulation loop
  emulate_loop();

#if 0
  killaudio();
#endif

  return 0;
}
