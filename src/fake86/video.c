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

/* video.c: many various functions to emulate bits of the video controller.
   a lot of this code is inefficient, and just plain ugly. i plan to rework
   large sections of it soon. */

#include "common.h"

#include "../80x86/cpu.h"


extern volatile bool scrmodechange;

static uint16_t lastint10ax;
uint16_t vtotal = 0;

// video ram at base address 0xA0000
uint8_t VRAM[0x40000];
// text mode size
uint16_t cols = 80, rows = 25;
// current BIOS video mode
uint8_t vidmode;
// cursor variables
uint16_t cursorposition, cursorvisible;

uint8_t cgabg, blankattr, vidgfxmode, vidcolor;
uint16_t cursx, cursy, vgapage;
uint8_t updatedscreen, clocksafe, port3da;
uint16_t VGA_SC[0x100], VGA_CRTC[0x100], VGA_ATTR[0x100], VGA_GC[0x100];
uint32_t videobase = 0xB8000, textbase = 0xB8000, x, y;

uint8_t fontcga[0x8000];

uint8_t readmode;
uint32_t readmap;

// palette.c
extern uint32_t palettecga[16], palettevga[256];

uint8_t latchRGB = 0, latchPal = 0, VGA_latch[4], stateDAC = 0;
uint8_t latchReadRGB = 0, latchReadPal = 0;
uint32_t tempRGB;
uint16_t oldw, oldh; // used when restoring screen mode

static inline uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) {
  // TODO: Flip this to native format ARGB
  return r | (g << 8) | (b << 16);
}

extern uint32_t nw, nh;

// int handler for int10h (ah=00h set video mode)
static void video_int_set_mode_handler() {
  updatedscreen = 1;
  // what video interrupt function
  switch (cpu_regs.ah) {
  case 0: // set video mode
    log_printf(LOG_CHAN_VIDEO, "set video mode %02Xh", cpu_regs.al);
    // VGA modes are in chained mode by default after a mode switch
    VGA_SC[0x4] = 0;
    // cpu_regs.al = 3;
    switch (cpu_regs.al & 0x7F) {
    case 0: // 40x25 mono text
      videobase = textbase;
      cols = 40;
      rows = 25;
      vidcolor = 0;
      vidgfxmode = 0;
      blankattr = 7;
      for (uint32_t i = videobase; i < videobase + 16384; i += 2) {
        RAM[i] = 0;
        RAM[i + 1] = blankattr;
      }
      break;
    case 1: // 40x25 color text
      videobase = textbase;
      cols = 40;
      rows = 25;
      vidcolor = 1;
      vidgfxmode = 0;
      blankattr = 7;
      for (uint32_t i = videobase; i < videobase + 16384; i += 2) {
        RAM[i] = 0;
        RAM[i + 1] = blankattr;
      }
      portram[0x3D8] = portram[0x3D8] & 0xFE;
      break;
    case 2: // 80x25 mono text
      videobase = textbase;
      cols = 80;
      rows = 25;
      vidcolor = 1;
      vidgfxmode = 0;
      blankattr = 7;
      for (uint32_t i = videobase; i < videobase + 16384; i += 2) {
        RAM[i] = 0;
        RAM[i + 1] = blankattr;
      }
      portram[0x3D8] = portram[0x3D8] & 0xFE;
      break;
    case 3: // 80x25 color text
      videobase = textbase;
      cols = 80;
      rows = 25;
      vidcolor = 1;
      vidgfxmode = 0;
      blankattr = 7;
      for (uint32_t i = videobase; i < videobase + 16384; i += 2) {
        RAM[i] = 0;
        RAM[i + 1] = blankattr;
      }
      portram[0x3D8] = portram[0x3D8] & 0xFE;
      break;
    case 4:
    case 5: // 80x25 color text
      videobase = textbase;
      cols = 40;
      rows = 25;
      vidcolor = 1;
      vidgfxmode = 1;
      blankattr = 7;
      for (uint32_t i = videobase; i < videobase + 16384; i += 2) {
        RAM[i] = 0;
        RAM[i + 1] = blankattr;
      }
      if (cpu_regs.al == 4)
        portram[0x3D9] = 48;
      else
        portram[0x3D9] = 0;
      break;
    case 6:
      videobase = textbase;
      cols = 80;
      rows = 25;
      vidcolor = 0;
      vidgfxmode = 1;
      blankattr = 7;
      for (uint32_t i = videobase; i < videobase + 16384; i += 2) {
        RAM[i] = 0;
        RAM[i + 1] = blankattr;
      }
      portram[0x3D8] = portram[0x3D8] & 0xFE;
      break;
    case 127:
      videobase = 0xB8000;
      cols = 90;
      rows = 25;
      vidcolor = 0;
      vidgfxmode = 1;
      for (uint32_t i = videobase; i < videobase + 16384; i++) {
        RAM[i] = 0;
      }
      portram[0x3D8] = portram[0x3D8] & 0xFE;
      break;
    case 0x9: // 320x200 16-color
      videobase = 0xB8000;
      cols = 40;
      rows = 25;
      vidcolor = 1;
      vidgfxmode = 1;
      blankattr = 0;
      if ((cpu_regs.al & 0x80) == 0)
        for (uint32_t i = videobase; i < videobase + 65535; i += 2) {
          RAM[i + 0] = 0;
          RAM[i + 1] = blankattr;
        }
      portram[0x3D8] = portram[0x3D8] & 0xFE;
      break;
    case 0xD:  // 320x200 16-color
    case 0x12: // 640x480 16-color
    case 0x13: // 320x200 256-color
      videobase = 0xA0000;
      cols = 40;
      rows = 25;
      vidcolor = 1;
      vidgfxmode = 1;
      blankattr = 0;
      for (uint32_t i = videobase; i < videobase + 0xffff; i += 2) {
        RAM[i + 0] = 0;
        RAM[i + 1] = blankattr;
      }
      portram[0x3D8] = portram[0x3D8] & 0xFE;
      break;
    }

    const int new_vidmode = cpu_regs.al & 0x7F;
    vidmode = new_vidmode;
    RAM[0x449] = vidmode;
    RAM[0x44A] = (uint8_t)cols;
    RAM[0x44B] = 0;
    RAM[0x484] = (uint8_t)(rows - 1);
    cursx = 0;
    cursy = 0;
    if ((cpu_regs.al & 0x80) == 0x00) {
      memset(&RAM[0xA0000], 0, 0x1FFFF);
      memset(VRAM, 0, 262144);
    }
    switch (vidmode) {
    case 127: // hercules
      nw = oldw = 720;
      nh = oldh = 348;
      scrmodechange = true;
      break;
    case 0x12:
      nw = oldw = 640;
      nh = oldh = 480;
      scrmodechange = true;
      break;
    case 0x13:
      oldw = 640;
      oldh = 400;
      nw = 320;
      nh = 200;
      scrmodechange = true;
      break;
    default:
      nw = oldw = 640;
      nh = oldh = 400;
      scrmodechange = true;
      break;
    }
    break;
  case 0x10: // VGA DAC functions
    switch (cpu_regs.al) {
    case 0x10: // set individual DAC register
      palettevga[cpu_regs.bx] = rgb((cpu_regs.dh & 63) << 2,
                                    (cpu_regs.ch & 63) << 2,
                                    (cpu_regs.cl & 63) << 2);
      break;
    case 0x12: // set block of DAC registers
    {
      uint32_t memloc = (cpu_regs.es << 4) + cpu_regs.dx;
      for (uint32_t n = cpu_regs.bx; n < (uint32_t)(cpu_regs.bx + cpu_regs.cx); n++) {
        palettevga[n] = rgb(read86(memloc + 0) << 2,
                            read86(memloc + 1) << 2,
                            read86(memloc + 2) << 2);
        memloc += 3;
      }
    }
    }
    break;
  case 0x1A: // get display combination code (ps, vga/mcga)
    cpu_regs.al = 0x1A;
    cpu_regs.bl = 0x8;
    break;
  }
}

void initcga() {
  FILE *fontfile;
  fontfile = fopen(PATH_DATAFILES "asciivga.dat", "rb");
  if (fontfile == NULL) {
    printf("FATAL: Cannot open " PATH_DATAFILES "asciivga!\n");
    exit(1);
  }
  fread(&fontcga[0], 32768, 1, fontfile);
  fclose(fontfile);
}

// monochrome display graphics
// port 3B0h - 3BBh
static void port_write_mda(uint16_t portnum, uint8_t value) {
  switch (portnum) {
  case 0x3B8: // hercules support
    if (((value & 2) == 2) && (vidmode != 127)) {
      const uint16_t ax = cpu_regs.ax;
      // force set video mode
      cpu_regs.ah = 0;
      cpu_regs.al = 127;
      video_int_set_mode_handler();
      cpu_regs.ax = ax;
    }
    if (value & 0x80) {
      videobase = 0xB8000;
    } else {
      videobase = 0xB0000;
    }
    break;
  default:
    portram[portnum] = value;
    //    UNREACHABLE();
  }
  updatedscreen = 1;
}

// VGA / EGA graphics
// port 3C0h - 3CFh
static void port_write_xga(uint16_t portnum, uint8_t value) {
  uint8_t flip3c0 = 0;
  switch (portnum) {
  case 0x3C0:
    if (flip3c0) {
      flip3c0 = 0;
      portram[0x3C0] = value & 0xff;
    } else {
      flip3c0 = 1;
      VGA_ATTR[portram[0x3C0]] = value & 0xff;
    }
    break;
  // 3C3 Video subsystem enable
  case 0x3C3:
    portram[0x3C3] = value;
    break;
  case 0x3C4: // sequence controller index
    portram[0x3C4] = value & 0xff;
    // if (portout16) VGA_SC[value & 0xff] = value >> 8;
    break;
  case 0x3C5: // sequence controller data
    VGA_SC[portram[0x3C4]] = value & 0xff;
    /*if (portram[0x3C4] == 2) {
    printf("VGA_SC[2] = %02X\n", value);
    }*/
    break;
  case 0x3C7: // color index register (read operations)
    latchReadPal = value & 0xff;
    latchReadRGB = 0;
    stateDAC = 0;
    break;
  case 0x3C8: // color index register (write operations)
    latchPal = value & 0xff;
    latchRGB = 0;
    tempRGB = 0;
    stateDAC = 3;
    break;
  case 0x3C9: // RGB data register
    value = value & 63;
    switch (latchRGB) {
    case 0: // red
      tempRGB = value << 2;
      break;
    case 1: // green
      tempRGB |= value << 10;
      break;
    case 2: // blue
      tempRGB |= value << 18;
      palettevga[latchPal] = tempRGB;
      latchPal = latchPal + 1;
      break;
    }
    latchRGB = (latchRGB + 1) % 3;
    break;
  // other vga graphics
  case 0x3CF:
    VGA_GC[portram[0x3CE]] = value;
    break;
  }
  updatedscreen = 1;
}

static void port_write_cga(uint16_t portnum, uint8_t value) {
  switch (portnum) {
  case 0x3D5: // cursor position latch
    VGA_CRTC[portram[0x3D4]] = value & 0xff;
    if (portram[0x3D4] == 0xE)
      cursorposition = (cursorposition & 0xFF) | (value << 8);
    else if (portram[0x3D4] == 0xF)
      cursorposition = (cursorposition & 0xFF00) | value;
    cursy = cursorposition / cols;
    cursx = cursorposition % cols;
    if (portram[0x3D4] == 6) {
      vtotal = value | (((uint16_t)VGA_GC[7] & 1) << 8) |
               (((VGA_GC[7] & 32) ? 1 : 0) << 9);
    }
    break;
  default:
    portram[portnum] = value;
  }
  updatedscreen = 1;
}

static uint8_t port_read_mda(uint16_t portnum) {
  switch (portnum) {
  default:
    return portram[portnum];
  }
}

static uint8_t port_read_3C9h() {
  // TODO: flip these so that palettevga can be in native format ARGB
  switch (latchReadRGB++) {
  case 0: // blue
    return (palettevga[latchReadPal] >> 2) & 0x3f;
  case 1: // green
    return (palettevga[latchReadPal] >> 10) & 0x3f;
  case 2: // red
    latchReadRGB = 0;
    return (palettevga[latchReadPal++] >> 18) & 0x3f;
  }
  UNREACHABLE();
}

static uint8_t port_read_xga(uint16_t portnum) {
  switch (portnum) {
  case 0x3C1:
    return (uint8_t)VGA_ATTR[portram[0x3C0]];
  case 0x3C5:
    return (uint8_t)VGA_SC[portram[0x3C4]];
  // DAC state
  case 0x3C7:
    return stateDAC;
  // palette index
  case 0x3C8:
    return latchReadPal;
  // RGB data register
  case 0x3C9:
    return port_read_3C9h();
  default:
    return portram[portnum];
  }
}

static uint8_t port_read_cga(uint16_t portnum) {
  switch (portnum) {
  case 0x3D5:
    return (uint8_t)VGA_CRTC[portram[0x3D4]];
  // status register
  case 0x3DA:
    // this is updated in main.c
    return port3da;
  default:
    return portram[portnum];
  }
}

#define shiftVGA(value)                                                        \
  {                                                                            \
    for (cnt = 0; cnt < (VGA_GC[3] & 7); cnt++) {                              \
      value = (value >> 1) | ((value & 1) << 7);                               \
    }                                                                          \
  }

#define logicVGA(curval, latchval)                                             \
  {                                                                            \
    switch ((VGA_GC[3] >> 3) & 3) {                                            \
    case 1:                                                                    \
      curval &= latchval;                                                      \
      break;                                                                   \
    case 2:                                                                    \
      curval |= latchval;                                                      \
      break;                                                                   \
    case 3:                                                                    \
      curval ^= latchval;                                                      \
      break;                                                                   \
    }                                                                          \
  }

void writeVGA(uint32_t addr32, uint8_t value) {
  uint32_t planesize;
  uint8_t curval, tempand, cnt;
  updatedscreen = 1;
  planesize = 0x10000;
  switch (VGA_GC[5] & 3) { // get write mode
  case 0:
    shiftVGA(value);
    if (VGA_SC[2] & 1) {
      if (VGA_GC[1] & 1)
        if (VGA_GC[0] & 1)
          curval = 255;
        else
          curval = 0;
      else
        curval = value;
      logicVGA(curval, VGA_latch[0]);
      curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[0]);
      VRAM[addr32] = curval;
    }
    if (VGA_SC[2] & 2) {
      if (VGA_GC[1] & 2)
        if (VGA_GC[0] & 2)
          curval = 255;
        else
          curval = 0;
      else
        curval = value;
      logicVGA(curval, VGA_latch[1]);
      curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[1]);
      VRAM[addr32 + planesize] = curval;
    }
    if (VGA_SC[2] & 4) {
      if (VGA_GC[1] & 4)
        if (VGA_GC[0] & 4)
          curval = 255;
        else
          curval = 0;
      else
        curval = value;
      logicVGA(curval, VGA_latch[2]);
      curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[2]);
      VRAM[addr32 + planesize * 2] = curval;
    }
    if (VGA_SC[2] & 8) {
      if (VGA_GC[1] & 8)
        if (VGA_GC[0] & 8)
          curval = 255;
        else
          curval = 0;
      else
        curval = value;
      logicVGA(curval, VGA_latch[3]);
      curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[3]);
      VRAM[addr32 + planesize * 3] = curval;
    }
    break;
  case 1:
    if (VGA_SC[2] & 1)
      VRAM[addr32] = VGA_latch[0];
    if (VGA_SC[2] & 2)
      VRAM[addr32 + planesize] = VGA_latch[1];
    if (VGA_SC[2] & 4)
      VRAM[addr32 + planesize * 2] = VGA_latch[2];
    if (VGA_SC[2] & 8)
      VRAM[addr32 + planesize * 3] = VGA_latch[3];
    break;
  case 2:
    if (VGA_SC[2] & 1) {
      if (VGA_GC[1] & 1)
        if (value & 1)
          curval = 255;
        else
          curval = 0;
      else
        curval = value;
      logicVGA(curval, VGA_latch[0]);
      curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[0]);
      VRAM[addr32] = curval;
    }
    if (VGA_SC[2] & 2) {
      if (VGA_GC[1] & 2)
        if (value & 2)
          curval = 255;
        else
          curval = 0;
      else
        curval = value;
      logicVGA(curval, VGA_latch[1]);
      curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[1]);
      VRAM[addr32 + planesize] = curval;
    }
    if (VGA_SC[2] & 4) {
      if (VGA_GC[1] & 4)
        if (value & 4)
          curval = 255;
        else
          curval = 0;
      else
        curval = value;
      logicVGA(curval, VGA_latch[2]);
      curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[2]);
      VRAM[addr32 + planesize * 2] = curval;
    }
    if (VGA_SC[2] & 8) {
      if (VGA_GC[1] & 8)
        if (value & 8)
          curval = 255;
        else
          curval = 0;
      else
        curval = value;
      logicVGA(curval, VGA_latch[3]);
      curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[3]);
      VRAM[addr32 + planesize * 3] = curval;
    }
    break;
  case 3:
    tempand = value & VGA_GC[8];
    shiftVGA(value);
    if (VGA_SC[2] & 1) {
      if (VGA_GC[0] & 1)
        curval = 255;
      else
        curval = 0;
      // logicVGA (curval, VGA_latch[0]);
      curval = (~tempand & curval) | (tempand & VGA_latch[0]);
      VRAM[addr32] = curval;
    }
    if (VGA_SC[2] & 2) {
      if (VGA_GC[0] & 2)
        curval = 255;
      else
        curval = 0;
      // logicVGA (curval, VGA_latch[1]);
      curval = (~tempand & curval) | (tempand & VGA_latch[1]);
      VRAM[addr32 + planesize] = curval;
    }
    if (VGA_SC[2] & 4) {
      if (VGA_GC[0] & 4)
        curval = 255;
      else
        curval = 0;
      // logicVGA (curval, VGA_latch[2]);
      curval = (~tempand & curval) | (tempand & VGA_latch[2]);
      VRAM[addr32 + planesize * 2] = curval;
    }
    if (VGA_SC[2] & 8) {
      if (VGA_GC[0] & 8)
        curval = 255;
      else
        curval = 0;
      // logicVGA (curval, VGA_latch[3]);
      curval = (~tempand & curval) | (tempand & VGA_latch[3]);
      VRAM[addr32 + planesize * 3] = curval;
    }
    break;
  }
}

// read from video memory base address 0xA0000
uint8_t readVGA(uint32_t addr32) {
  const uint32_t planesize = 0x10000;
  VGA_latch[0] = VRAM[addr32 + planesize * 0];
  VGA_latch[1] = VRAM[addr32 + planesize * 1];
  VGA_latch[2] = VRAM[addr32 + planesize * 2];
  VGA_latch[3] = VRAM[addr32 + planesize * 3];
  if (VGA_SC[2] & 1)
    return VRAM[addr32 + planesize * 0];
  if (VGA_SC[2] & 2)
    return VRAM[addr32 + planesize * 1];
  if (VGA_SC[2] & 4)
    return VRAM[addr32 + planesize * 2];
  if (VGA_SC[2] & 8)
    return VRAM[addr32 + planesize * 3];
  return 0;
}

void initVideoPorts() {
  // 3B0-3BB Monochrome Display Adapter
  // TODO: try this as write only
  set_port_write_redirector(0x3B0, 0x3BB, &port_write_mda);
  set_port_read_redirector(0x3B0, 0x3BB, &port_read_mda);

  // 3C0-3CF EGA/VGA
  set_port_write_redirector(0x3C0, 0x3CF, &port_write_xga);
  set_port_read_redirector(0x3C0, 0x3CF, &port_read_xga);

  // 3D0-3DF Color Graphics Monitor Adapter
  // TODO: 3D0-3DB is write only
  set_port_write_redirector(0x3D0, 0x3DF, &port_write_cga);
  set_port_read_redirector(0x3D0, 0x3DF, &port_read_cga);
}

static bool handle_int_10h() {

  // http://stanislavs.org/helppc/int_10.html

  updatedscreen = 1;

  // set video mode (00h)
  // get/set palette registers?
  if ((cpu_regs.ah == 0x00) || (cpu_regs.ah == 0x10)) {
    const uint16_t oldregax = cpu_regs.ax;
    // delegate to specialized handler
    video_int_set_mode_handler();
    // restore ah and al
    cpu_regs.ax = oldregax;
    // return codes
    if (cpu_regs.ah == 0x10) {
      return true;
    }
    if (vidmode == 9) {
      return true;
    }
    return false;
  }

  // Video Display Combination (1A)
  if ((cpu_regs.ah == 0x1A) &&
      (lastint10ax != 0x0100)) { // the 0x0100 is a cheap hack to make it not
                                 // do this if DOS EDIT/QBASIC

    // http://stanislavs.org/helppc/int_10-1a.html

    // if a valid function was requested in AH
    cpu_regs.al = 0x1A;
    // active display
    // 08  VGA with analog color display
    cpu_regs.bl = 0x8;
    return true;
  }
  lastint10ax = cpu_regs.ax;
  if (cpu_regs.ah == 0x1B) {
    cpu_regs.al = 0x1B;
    cpu_regs.es = 0xC800;
    cpu_regs.di = 0x0000;
    writew86(0xC8000, 0x0000);
    writew86(0xC8002, 0xC900);
    write86(0xC9000, 0x00);
    write86(0xC9001, 0x00);
    write86(0xC9002, 0x01);
    return true;
  }

  return false;
}

bool video_int_handler(int intnum) {
  switch (intnum) {
  // video BIOS services
  case 0x10:
    return handle_int_10h();
  }
  UNREACHABLE();
  return false;
}