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

#include "disk.h"


struct vhd_footer_t {

  // "conectix"
  uint64_t cookie;

  // 0  - No features enabled
  // 1  - Temporary
  // 2  - Reserved
  uint32_t features;

  // should be 0x00010000
  uint32_t version;

  // 0xFFFFFFFF for fixed disks
  uint64_t offset;
  uint32_t timestamp;

  // "vs  "  - virtual server
  // "vpc "  - virtual pc
  uint32_t creat_app;
  uint32_t creat_ver;
  uint32_t creat_os;
  uint64_t orig_size;

  // size of hard drive in bytes
  uint64_t size;

  // byte 0-1  - cylinders
  // byte 2    - heads
  // byte 3    - sectors
  uint32_t geometry;

  // 2  - fixed disk
  uint32_t type;

  // inverted sum of hard disk footer bytes (without checksum)
  uint32_t checksum;
  uint8_t  uuid[16];
  uint8_t  state;

  // should be 427 bytes reserved after this
};


bool _disk_vhd_open(
  const uint8_t num, const char *path, struct disk_info_t *out) {

  return false;
}
