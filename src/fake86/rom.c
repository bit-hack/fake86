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

// custom option rom

// currently this is used to set the number of hard disks attached
// and set the video mode to something we expect.

// info on bios data area:
// http://stanislavs.org/helppc/bios_data_area.html

// option rom info:
// http://etherboot.sourceforge.net/doc/html/devman/extension.html

#include "../common/common.h"


extern uint8_t hdcount;

static int rom_addr = 0xD8000;
static int rom_size = 0x02000;

bool rom_insert(void) {
  // fill with int3
  memset(RAM + rom_addr, 0xCC, rom_size);

  // add rom signature
  uint8_t *ptr = RAM + rom_addr;
  ptr[0] = 0x55;
  ptr[1] = 0xAA;
  // add rom size
  ptr[2] = rom_size / 512;

  uint8_t elist1 = 0;
  elist1 |= 0x01;  // floppy drives present
  elist1 |= 0x0C;  // +64kb ram
  elist1 |= 0x00;  // 'reserved' video mode (vga/ega)
  elist1 |= (fdcount & 0x3) << 6;

  uint8_t elist2 = 0;
  elist2 |= 0x01;  // dma hardware present

  // if the BIOS is happy execution will be given to (rom_addr + 3)
  const uint8_t code[] = {
#if 0
    0xF4,                   // halt
#endif
    // prologue
                            // org  4800h
    0x60,                   // pusha

    // set ds to bios data area
    0xB8, 0x40, 0x00,       // mov ax, 40h
    0x8E, 0xD8,             // mov ds, ax

    // set equipment list
    0xBE, 0x10, 0x00,       // mov si, 10h
    0xC6, 0x04, elist1,     // mov byte [si], 41h
    0xBE, 0x11, 0x00,       // mov si, 11h
    0xC6, 0x04, elist2,     // mov byte [si], 41h

    // set number of hard disks
    0xBE, 0x75, 0x00,       // mov si, 75h
    0xC6, 0x04, hdcount,    // mov byte [si], hdcount

    // epilogue
    0x61,                   // popa
    0xCB,                   // far ret
  };
  memcpy(RAM + rom_addr + 3, code, sizeof(code));

  // add checksum to the end of the rom
  ptr[rom_size - 1] = 0;
  uint8_t byte_sum = 0;
  for (int i=0; i<rom_size; ++i) {
    byte_sum += ptr[i];
  }
  ptr[rom_size - 1] = 0x100 - byte_sum;

  return true;
}
