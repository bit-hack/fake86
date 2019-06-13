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

// http://bochs.sourceforge.net/techspec/CMOS-reference.txt

#include "../common/common.h"


static bool _enable_nmi;
static uint8_t _cmos_addr;
static uint8_t _cmos_ram[128];


static uint8_t _cmos_rtc_read(uint16_t port) {
  switch (port) {
  case 0x00: // seconds
  case 0x01: // second alarm
  case 0x02: // minuites
  case 0x03: // minuite alarm
  case 0x04: // hours
  case 0x05: // hour alarm
  case 0x06: // day of week
  case 0x07: // date of month
  case 0x08: // month
  case 0x09: // year
    return 0x00;
  case 0x0A: // status register A r/w
  case 0x0B: // status register B r/w
    return _cmos_ram[0x0A];
  case 0x0C: // status register C r/w
  case 0x0D: // status register D r/w
    return 0x00;
  case 0x0E: // diagnostic status byte
    return 0x64; // incorrect checksum and time
  default:
    return 0x00;
  }
}

static uint8_t _cmos_read(uint16_t port) {
  switch (port) {
  case 0x70:
    return _cmos_addr;
  case 0x71:
    if (_cmos_addr >= 0x00 && _cmos_addr <= 0x0E) {
      return _cmos_rtc_read(port);
    }
    else {
      return _cmos_ram[_cmos_addr];
    }
  default:
    UNREACHABLE();
  }
}

static void _cmos_write(uint16_t port, uint8_t value) {
  switch (port) {
  case 0x70:
    _cmos_addr = value & 0x7F;
    _enable_nmi = value & 0x80 ? false : true;
    break;
  case 0x71:
    _cmos_ram[_cmos_addr] = value;
    break;
  }
}

void cmos_init(void) {
  set_port_read_redirector(0x70, 0x71, _cmos_read);
  set_port_write_redirector(0x70, 0x71, _cmos_write);
}

bool cmos_nmi_enabled(void) {
  return _enable_nmi;
}
