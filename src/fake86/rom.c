/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2012 Mike Chambers

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

#include "common.h"


extern uint8_t hdcount;

// D4000 -> EC000
static int rom_addr = 0xD4000;
static int rom_size = 0x04000;

bool rom_insert() {
  // fill with int3
  memset(RAM + rom_addr, 0xCC, rom_size);

  // add rom signature
  uint8_t *ptr = RAM + rom_addr;
  ptr[0] = 0x55;
  ptr[1] = 0xAA;
  // add rom size
  ptr[2] = rom_size / 512;

  // if the BIOS is happy execution will be given to 0xD4003
  const uint8_t code[] = {
                            // org  4800h
    0x60,                   // pusha

    0xB8, 0x40, 0x00,       // mov ax, 40h
    0x8E, 0xD8,             // mov ds, ax

    0xBE, 0x10, 0x00,       // mov si, 10h
    0xC6, 0x04, 0x41,       // mov byte [si], 41h

    0xBE, 0x75, 0x00,       // mov si, 75h
    0xC6, 0x04, hdcount,    // mov byte [si], hdcount

    0x61,                   // popa
    0xC3,                   // ret
  };
  memcpy(RAM + rom_addr + 3, code, sizeof(code));

  // add checksum to the end of the rom
  uint8_t byte_sum = ptr[0];
  for (int i=1; i<rom_size; ++i) {
    byte_sum += ptr[i];
  }
  ptr[rom_size - 1] = ~byte_sum;

  return true;
}
