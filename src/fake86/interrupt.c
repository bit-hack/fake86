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

#include "../common/common.h"
#include "../cpu/cpu.h"
#include "../disk/disk.h"


void intcall86(uint16_t intnum) {

#if 0
  if (intnum != 0x16) {
    log_printf(LOG_CHAN_ALL, "int %02x", (int)intnum);
  }
#endif

  assert(intnum <= 0xff);

  switch (intnum) {
  // Video services
  case 0x10:
    if (neo_int10_handler()) {
      return;
    }
    break;
  // Bootstrap loader interupt
  case 0x19:
    disk_bootstrap(intnum);
    return;
  // Disk services
  case 0x13:
  case 0xFD:
    disk_int_handler(intnum);
    return;
  // DOS services
  case 0x21:
    if (on_dos_int()) {
      return;
    }
    break;
  }

  // if interupt was not handled then let the CPU
  // jump to the interupt handler 
  cpu_prep_interupt(intnum);
}
