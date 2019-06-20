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

#pragma once


#include "../common/common.h"


struct disk_info_t {
  // delegates
  bool (*eject)(void *self);
  bool (*seek)(void *self, const uint32_t offset);
  bool (*read)(void *self, uint8_t *dst, const uint32_t count);
  bool (*write)(void *self, const uint8_t *src, const uint32_t count);
  bool (*tell)(void *self, uint32_t *out);

  // drive instance
  void *self;

  // BIOS drive number
  uint8_t drive_num;

  // disk geometry
  uint32_t sector_size;  // 512
  uint32_t cyls;
  uint32_t sects;
  uint32_t heads;

  // raw size in bytes
  uint32_t size_bytes;

  // disk status
  uint8_t last_ah, last_cf;
};


void disk_int_handler(int intnum);

void disk_load_com(const char *path);

bool _disk_img_open(
  const uint8_t num, const char *path, struct disk_info_t *out);

bool _disk_vhd_open(
  const uint8_t num, const char *path, struct disk_info_t *out);


bool _geom_hard_disk(struct disk_info_t *d);
bool _geom_floppy_disk(struct disk_info_t *d);
