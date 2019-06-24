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

static void emulate_loop_headless(void) {
  // enter main emulation loop
  while (cpu_running) {
    // set ourselves some cycle targets
    const int64_t target = SDL_min(CYCLES_PER_SLICE, i8253_cycles_before_irq());
    // run for some cycles
    const int64_t executed = tick_cpu(target);
    // tick the hardware
    tick_hardware(executed);

    // exit if we are locked up
    if (cpu_in_hlt_state()) {
      if (cpu_flags.ifl == 0) {
        cpu_running = false;
      }
    }

  }
  log_printf(LOG_CHAN_CPU, "cpu reached halt state");
  cpu_dump_state(stdout);
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
      if (!cpu_halt)
        continue;
    }
    bool video_redraw = false;
    uint64_t executed = 0;
    // update cpu
    while (cpu_acc <= 0 || cpu_halt) {

      // set ourselves some cycle targets
      int64_t target;
      target = cpu_halt ? 0 : CYCLES_PER_SLICE;
      target = cpu_step ? 1 : target;
      target = SDL_min(target, i8253_cycles_before_irq());
      target = SDL_max(target, cpu_halt ? 0 : 1);

      // run for some cycles
      executed = tick_cpu(target);
      cpu_acc += executed;

      // disable the stepping flag
      cpu_step = executed ? false : cpu_step;

      // keep track of if the video needs refreshed
      if (vga_timing_should_flip()) {
        video_redraw = true;
        vga_timing_did_flip();
      }
      // tick peripherals
      tick_hardware(executed);

      if (video_redraw) {
        break;
      }

      if (cpu_halt) {
        SDL_Delay(2);
        break;
      }
    }
    // refresh the screen buffer
    if (video_redraw || cpu_halt) {
      tick_render();
    }
    // parse events from host
    tick_events();
    // advance cpu or sleep
    if ((int64_t)new_cycles >= cpu_acc) {
      cpu_acc -= cpu_halt ? executed : new_cycles;
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

static void cpu_setup(void) {
  struct cpu_io_t io;
  io.ram = RAM;
  io.mem_read_8    = read86;
  io.mem_read_16   = readw86;
  io.mem_write_8   = write86;
  io.mem_write_16  = writew86;
  io.port_read_8   = portin;
  io.port_read_16  = portin16;
  io.port_write_8  = portout;
  io.port_write_16 = portout16;
  io.int_call      = intcall86;
  cpu_set_io(&io);
}

static bool emulate_init(void) {
  // initialize memory
  mem_init();
  // insert our option rom
  rom_insert();
  // initalize the cpu
  cpu_setup();
  cpu_reset();
  // initalize hardware
  i8253_init();
  i8259_init();
  i8237_init();
  i8255_init();
  cmos_init();
  mouse_init(0x3F8, 4);
  // initalize vga refresh timing
  vga_timing_init();
  if (!_cl_headless) {
    // initalize new video renderer
    if (!win_init()) {
      return false;
    }
    if (!neo_init()) {
      return false;
    }
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
  if (!_cl_headless) {
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
  }
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
  if (audio_enable) {
    SDL_PauseAudio(0);
  }
  cpu_running = true;

  if (_cl_headless) {
    emulate_loop_headless();
  }
  else {
    emulate_loop();
  }

  // close the audio device
  if (audio_enable) {
    SDL_CloseAudio();
  }

  SDL_Quit();
  return 0;
}

void state_save(const char *path) {
  FILE *fd = fopen(path, "wb");
  if (!fd) {
    log_printf(LOG_CHAN_FRONTEND, "unable to open file '%s'", path);
    return;
  }

  mem_state_save(fd);
  cpu_state_save(fd);
  neo_state_save(fd);
  i8237_state_save(fd);
  i8253_state_save(fd);
  i8255_state_save(fd);
  i8259_state_save(fd);
  port_state_save(fd);
}

void state_load(const char *path) {
  FILE *fd = fopen(path, "rb");
  if (!fd) {
    log_printf(LOG_CHAN_FRONTEND, "unable to open file '%s'", path);
    return;
  }

  mem_state_load(fd);
  cpu_state_load(fd);
  neo_state_load(fd);
  i8237_state_load(fd);
  i8253_state_load(fd);
  i8255_state_load(fd);
  i8259_state_load(fd);
  port_state_load(fd);
}
