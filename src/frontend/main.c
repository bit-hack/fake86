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

bool render_init(void);
void tick_events(void);
void handlevideo();

bool neo_render_init();
void neo_render_tick();

extern bool cursorvisible;
extern uint8_t doaudio;
extern uint8_t updatedscreen;

const char *biosfile = "pcxtbios.bin";

extern bool cl_parse(int argc, char *argv[]);
extern void initVideoPorts();
extern void initsermouse(uint16_t baseport, uint8_t irq);

void render_update(void);
void render_check_for_mode_change(void);

static void exit_handler(void) {
  log_close();
}

static int64_t tick_cpu(int64_t num_cycles) {
  return cpu_exec86((int32_t)num_cycles);
}

static void tick_render() {
#if USE_VIDEO_NEO
  neo_render_tick();
#else
  updatedscreen = 1;
  render_check_for_mode_change();
  render_update();
#endif
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

  const uint64_t vpos = cga_accum;
  const uint64_t hpos = cga_accum % cycles_per_line;

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
void tick_hardware_fast(uint64_t cycles) {
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

#if 0
    // benchmark cps
    if ((get_ticks() - old_bench) > 1000) {
      old_bench += 1000;
      printf("cps: %d\n", (uint32_t)bench_acc);
      bench_acc = 0;
    }
#endif
  }
}

static void sdl_audio_callback(void *userdata, Uint8 *stream, int len) {
  int16_t *samples = (int16_t*)stream;
  audio_callback(samples, len / 2);
}

static bool sdl_audio_init(void) {
  SDL_AudioSpec desired, obtained;
  memset(&desired, 0, sizeof(desired));

  desired.channels = 2;
  desired.freq = 22050;
  desired.callback = sdl_audio_callback;
  desired.format = AUDIO_S16;
  desired.samples = 1024;

  if (SDL_OpenAudio(&desired, &obtained)) {
    log_printf(LOG_CHAN_AUDIO, "SDL_OpenAudio failed");
    doaudio = false;
    return false;
  }

  if (obtained.channels != desired.channels) {
    return false;
  }
  if (obtained.format != desired.format) {
    return false;
  }

  audio_init(obtained.freq);
  return true;
}

static bool emulate_init() {
  // initialize memory
  mem_init();
  // insert our option rom
  rom_insert();
  // initalize the cpu
  cpu_reset();
  cpu_set_intcall_handler(intcall86);
  // initalize hardware
  i8253_init();
  i8259_init();
  i8237_init();
  i8255_init();
  initVideoPorts();
  mouse_init(0x3F8, 4);
#if USE_VIDEO_NEO
  if (!neo_render_init()) {
    return false;
  }
#else
  if (!render_init()) {
    return false;
  }
#endif
  return true;
}

static bool load_roms(void) {
  // load bios
  const uint32_t biossize = mem_loadbios(biosfile);
  if (!biossize) {
    return false;
  }
  // load other roms
  if (biossize <= (1024 * 8)) {
    if (!mem_loadrom(0xF6000UL, PATH_DATAFILES "rombasic.bin", 0)) {
      return false;
    }
    if (!mem_loadrom(0xC0000UL, PATH_DATAFILES "videorom.bin", 1)) {
      return false;
    }
  }
  return true;
}

int main(int argc, char *argv[]) {
  // setup exit handler
  atexit(exit_handler);
  // initialize the log file
  log_init();
  // parse the input command line
  if (!cl_parse(argc, argv)) {
    return 1;
  }
  // initalize SDL
  const int flags = SDL_INIT_VIDEO | (doaudio ? SDL_INIT_AUDIO : 0);
  if (SDL_Init(flags)) {
    log_printf(LOG_CHAN_SDL, "unable to init sdl");
    return 1;
  }
  else {
    log_printf(LOG_CHAN_SDL, "initalized sdl");
  }
  // initalize the audio stream
  if (doaudio) {
    if (!sdl_audio_init()) {
      return 1;
    }
  }
  // init the emulator
  if (!emulate_init()) {
    return -1;
  }
  // load roms needed by the emulator
  if (!load_roms()) {
    return -1;
  }
  // enter the emulation loop
  SDL_PauseAudio(0);
  running = 1;
  emulate_loop();

  //
  if (doaudio) {
    SDL_CloseAudio();
  }

  SDL_Quit();
  return 0;
}
