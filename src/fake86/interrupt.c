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

#include "common.h"

#include "../80x86/cpu.h"

bool video_int_handler(int intnum);

void intcall86(uint8_t intnum) {

  switch (intnum) {
  // Video services
  case 0x10:
    if (video_int_handler(intnum)) {
      return;
    }
    break;
#ifndef DISK_CONTROLLER_ATA
  // Bootstrap loader interupt
  case 0x19:
    disk_bootstrap(intnum);
    return;
  // Disk services
  case 0x13:
  case 0xFD:
    disk_int_handler(intnum);
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

  if (intnum == 4) {
    printf("doing IRQ 4\n");
  }

  // prepare an interupt?
  cpu_push(makeflagsword());
  cpu_push(segregs[regcs]);
  cpu_push(ip);
  segregs[regcs] = getmem16(0, (uint16_t)intnum * 4 + 2);
  ip = getmem16(0, (uint16_t)intnum * 4);
  ifl = 0;
  tf = 0;
}
