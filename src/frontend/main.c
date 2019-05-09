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

#include "../fake86/common.h"

#include "../80x86/cpu.h"


SDL_Thread *thread_console = NULL;
SDL_Thread *thread_emu = NULL;

extern uint8_t running, renderbenchmark;

void exec86(uint32_t execloops);
bool initscreen();

void handleinput();
void handlevideo();

#ifdef CPU_ADDR_MODE_CACHE
extern uint64_t cached_access_count, uncached_access_count;
#endif

extern volatile bool scrmodechange;
extern uint8_t doaudio;
extern uint64_t totalexec, totalframes;
uint64_t starttick, endtick, lasttick;

uint8_t *biosfile = NULL, verbose = 0, cgaonly = 0, useconsole = 0;
uint32_t speed = 0;

extern uint8_t insertdisk(uint8_t drivenum, char *filename);
extern void ejectdisk(uint8_t drivenum);
extern uint8_t bootdrive, ethif, net_enabled;
extern void doirq(uint8_t irqnum);
extern void parsecl(int argc, char *argv[]);
void timing();
void tickaudio();
void inittiming();
void initaudio();
void init8253();
void init8259();
extern void init8237();
extern void initVideoPorts();
extern void killaudio();
extern void initsermouse(uint16_t baseport, uint8_t irq);
extern void *port_write_callback[0x10000];
extern void *port_read_callback[0x10000];
extern void *port_write_callback16[0x10000];
extern void *port_read_callback16[0x10000];
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

// console.c
// Console handler thread
extern int ConsoleThread(void *);

extern void bufsermousedata(uint8_t value);

static void inithardware() {
#ifdef NETWORKING_ENABLED
  if (ethif != 254)
    initpcap();
#endif
  printf("Initializing emulated hardware:\n");
  memset(port_write_callback, 0, sizeof(port_write_callback));
  memset(port_read_callback, 0, sizeof(port_read_callback));
  memset(port_write_callback16, 0, sizeof(port_write_callback16));
  memset(port_read_callback16, 0, sizeof(port_read_callback16));
  
  printf("  - Intel 8042 keyboard controller: ");
  i8042_init();
  printf("OK\n");

  printf("  - Intel 8253 timer: ");
  i8253_init();
  printf("OK\n");

  printf("  - Intel 8259 interrupt controller: ");
  i8259_init();
  printf("OK\n");

  printf("  - Intel 8237 DMA controller: ");
  init8237();
  printf("OK\n");

  initVideoPorts();
  if (usessource) {
    printf("  - Disney Sound Source: ");
    initsoundsource();
    printf("OK\n");
  }

#ifndef NETWORKING_OLDCARD
  printf("  - Novell NE2000 ethernet adapter: ");
  isa_ne2000_init(0x300, 6);
  printf("OK\n");
#endif

  printf("  - Adlib FM music card: ");
  initadlib(0x388);
  printf("OK\n");

  printf("  - Creative Labs Sound Blaster 2.0: ");
  initBlaster(0x220, 7);
  printf("OK\n");

  printf("  - Serial mouse (Microsoft compatible): ");
  mouse_init(0x3F8, 4);
  printf("OK\n");

  if (doaudio) {
    initaudio();
  }

  inittiming();
  initscreen();
}

// Emulation therad
static int EmuThread(void *dummy) {
  while (running) {
    if (!speed)
      exec86(10000);
    else {
      exec86(speed / 100);
      while (!audiobufferfilled()) {
        timing();
        tickaudio();
      }
      SDL_Delay(10);
    }
    if (dohardreset) {
      cpu_reset();
      dohardreset = 0;
    }
  }
  return 0;
}

static void on_exit(void) {
  log_close();
}

int main(int argc, char *argv[]) {
  // setup out exit handler
  atexit(on_exit);
  // initalize the log file
  log_init();
  // parse the input command line
  parsecl(argc, argv);
  // initalize memory
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
  if (biossize <= 8192) {
    mem_loadrom(0xF6000UL, PATH_DATAFILES "rombasic.bin", 0);
    if (!mem_loadrom(0xC0000UL, PATH_DATAFILES "videorom.bin", 1))
      return (-1);
  }
  running = 1;
  cpu_reset();

  inithardware();

  if (useconsole) {
    thread_console = SDL_CreateThread(ConsoleThread, NULL);
    assert(thread_console);
  }

  thread_emu = SDL_CreateThread(EmuThread, NULL);
  assert(thread_emu);

  lasttick = starttick = SDL_GetTicks();
  while (running) {
    i8042_tick();
    handleinput();
    handlevideo();
#ifdef NETWORKING_ENABLED
    if (ethif < 254)
      dispatch();
#endif
    SDL_Delay(1);
  }

  endtick = (SDL_GetTicks() - starttick) / 1000;
  if (endtick == 0)
    endtick = 1; // avoid divide-by-zero exception in the code below, if ran for
                 // less than 1 second

  killaudio();

  if (renderbenchmark) {
    printf("\n%llu frames rendered in %llu seconds.\n", totalframes, endtick);
    printf("Average framerate: %llu FPS.\n", totalframes / endtick);
  }

  printf("\n%llu instructions executed in %llu seconds.\n", totalexec, endtick);
  printf("Average speed: %llu instructions/second.\n", totalexec / endtick);

#ifdef CPU_ADDR_MODE_CACHE
  printf("\n  Cached modregrm data access count: %llu\n", cached_access_count);
  printf("Uncached modregrm data access count: %llu\n", uncached_access_count);
#endif

  if (useconsole)
    exit(0); // makes sure console thread quits even if blocking

  return 0;
}
