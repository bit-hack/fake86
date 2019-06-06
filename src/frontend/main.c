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

#include "../common/common.h"
#include "frontend.h"
#include "../cpu/cpu.h"


const char *biosfile = "pcxtbios.bin";

static void exit_handler(void) {
  log_close();
}

static int64_t tick_cpu(int64_t num_cycles) {
  return cpu_exec86((int32_t)num_cycles);
}

static void tick_render() {
  win_render();
}

static uint64_t get_ticks() {
#if 1
  return SDL_GetTicks();
#else
  static uint64_t ticks = 0;
  return ticks += 10;
#endif
}

static void tick_hardware(uint64_t cycles) {
  // dma controller
  i8237_tick(cycles);
  // PIC controller
  i8259_tick(cycles);
  // PIA
  i8255_tick(cycles);
  // 
  vga_timing_advance(cycles);
  // PIT timer
  i8253_tick(cycles);
  // tick audio event stream
  audio_tick(cycles);
  //
  neo_tick(cycles);
}

static void emulate_loop(void) {

#define CYCLES_PER_REFRESH (CYCLES_PER_SECOND / 30)
#define MSTOCYCLES(X) ((X) * (CYCLES_PER_SECOND / 1000))

  int64_t cpu_acc = 0;
  uint64_t old_ms = get_ticks();

  // enter main emulation loop
  while (cpu_running) {

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
      // set ourselves some cycle targets
      const uint64_t target =
        SDL_max(1, SDL_min(i8253_cycles_before_irq(), CYCLES_PER_SLICE));
      // run for some cycles
      const uint64_t executed = tick_cpu(target);
      cpu_acc += executed;
      // keep track of if the video needs refreshed
      if (vga_timing_should_flip()) {
        video_redraw = true;
        vga_timing_did_flip();
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
  }
}

static void sdl_audio_callback(void *userdata, Uint8 *stream, int len) {
  int16_t *samples = (int16_t*)stream;
  uint32_t todo = len / sizeof(int16_t);
  while (todo) {
    const uint32_t done = audio_callback(samples, todo);
    samples += done;
    todo -= done;
  }
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
    audio_enable = false;
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
  cmos_init();
  mouse_init(0x3F8, 4);
  // initalize vga refresh timing
  vga_timing_init();
  // initalize new video renderer
  if (!win_init()) {
    return false;
  }
  if (!neo_init()) {
    return false;
  }
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
    const char *rom_basic = "rombasic.bin";
    if (!mem_loadrom(0xF6000UL, rom_basic, 0)) {
      log_printf(LOG_CHAN_MEM, "unable to load '%s'", rom_basic);
    }
  }
  // load the video bios
  const char *rom_video = false ? "ibm_vga.bin" : "videorom.bin";
  if (!mem_loadrom(0xC0000UL, rom_video, 1)) {
    log_printf(LOG_CHAN_MEM, "unable to load '%s'", rom_video);
  }
  return true;
}

int main(int argc, const char *argv[]) {
  // setup exit handler
  atexit(exit_handler);
  // initialize the log file
  log_init();
  // parse the input command line
  if (!cl_parse(argc, argv)) {
    return 1;
  }
  // initalize SDL
  const int flags = SDL_INIT_VIDEO | (audio_enable ? SDL_INIT_AUDIO : 0);
  if (SDL_Init(flags)) {
    log_printf(LOG_CHAN_SDL, "unable to init sdl");
    return 1;
  }
  else {
    log_printf(LOG_CHAN_SDL, "initalized sdl");
  }
  // enable key repeat
  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
                      SDL_DEFAULT_REPEAT_INTERVAL);
  // initalize the audio stream
  if (audio_enable) {
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
  cpu_running = true;
  emulate_loop();

  // close the audio device
  if (audio_enable) {
    SDL_CloseAudio();
  }

  SDL_Quit();
  return 0;
}
