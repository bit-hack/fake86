#include <stdint.h>

#include "../80x86/cpu.h"

#include "memory.h"

uint8_t didintr = 0;

// cpu.c
extern uint8_t didbootstrap;
extern union _bytewordregs_ regs;
extern uint16_t segregs[4];
extern uint8_t bootdrive, hdcount;
extern uint16_t ip;
extern uint8_t cf, pf, af, zf, sf, tf, ifl, df, of;
extern void push(uint16_t pushval);

// video.c
extern uint8_t updatedscreen;
extern void vidinterrupt();
extern uint8_t vidmode;

// disk.c
extern void diskhandler();
extern void readdisk(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff,
                     uint16_t cyl, uint16_t sect, uint16_t head,
                     uint16_t sectcount);

void intcall86(uint8_t intnum) {
  static uint16_t lastint10ax;
  uint16_t oldregax;
  didintr = 1;

  if (intnum == 0x19) {
    didbootstrap = 1;
  }

  switch (intnum) {
  case 0x10:
    updatedscreen = 1;
    /*if (regs.byteregs[regah]!=0x0E) {
            printf("Int 10h AX = %04X\n", regs.wordregs[regax]);
    }*/
    if ((regs.byteregs[regah] == 0x00) || (regs.byteregs[regah] == 0x10)) {
      oldregax = regs.wordregs[regax];
      vidinterrupt();
      regs.wordregs[regax] = oldregax;
      if (regs.byteregs[regah] == 0x10)
        return;
      if (vidmode == 9)
        return;
    }
    if ((regs.byteregs[regah] == 0x1A) &&
        (lastint10ax != 0x0100)) { // the 0x0100 is a cheap hack to make it not
                                   // do this if DOS EDIT/QBASIC
      regs.byteregs[regal] = 0x1A;
      regs.byteregs[regbl] = 0x8;
      return;
    }
    lastint10ax = regs.wordregs[regax];
    if (regs.byteregs[regah] == 0x1B) {
      regs.byteregs[regal] = 0x1B;
      segregs[reges] = 0xC800;
      regs.wordregs[regdi] = 0x0000;
      writew86(0xC8000, 0x0000);
      writew86(0xC8002, 0xC900);
      write86(0xC9000, 0x00);
      write86(0xC9001, 0x00);
      write86(0xC9002, 0x01);
      return;
    }
    break;

#ifndef DISK_CONTROLLER_ATA
  case 0x19: // bootstrap
#ifdef BENCHMARK_BIOS
    running = 0;
#endif
    if (bootdrive <
        255) { // read first sector of boot drive into 07C0:0000 and execute it
      regs.byteregs[regdl] = bootdrive;
      readdisk(regs.byteregs[regdl], 0x07C0, 0x0000, 0, 1, 0, 1);
      segregs[regcs] = 0x0000;
      ip = 0x7C00;
    } else {
      segregs[regcs] = 0xF600; // start ROM BASIC at bootstrap if requested
      ip = 0x0000;
    }
    return;

  case 0x13:
  case 0xFD:
    diskhandler();
    return;
#endif
#ifdef NETWORKING_OLDCARD
  case 0xFC:
#ifdef NETWORKING_ENABLED
    nethandler();
#endif
    return;
#endif
  }

  cpu_push(makeflagsword());
  cpu_push(segregs[regcs]);
  cpu_push(ip);
  segregs[regcs] = getmem16(0, (uint16_t)intnum * 4 + 2);
  ip = getmem16(0, (uint16_t)intnum * 4);
  ifl = 0;
  tf = 0;
}
