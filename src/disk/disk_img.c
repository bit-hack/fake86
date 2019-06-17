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

#include <stdio.h>

#include "disk.h"


struct disk_img_t {
  FILE *fd;
  uint32_t filesize;
  uint32_t seek_pos;
};


static bool _disk_img_eject(
  void *self) {
  assert(self);
  struct disk_img_t *img = (struct disk_img_t*)self;
  fclose(img->fd);
  free(img);
  return true;
}

static bool _disk_img_seek(
  void *self, const uint32_t offset) {
  assert(self);
  struct disk_img_t *img = (struct disk_img_t*)self;
  fseek(img->fd, offset, SEEK_SET);
  img->seek_pos = offset;
}

static bool _disk_img_read(
  void *self, uint8_t *dst, const uint32_t count) {
  assert(self);
  struct disk_img_t *img = (struct disk_img_t*)self;
  return fread(dst, 1, count, img->fd) == count;
}

static bool _disk_img_write(
  void *self, const uint8_t *src, const uint32_t count) {
  assert(self);
  struct disk_img_t *img = (struct disk_img_t*)self;
  return fwrite(src, 1, count, img->fd) == count;
}

bool _disk_img_open(
  const uint8_t num, const char *path, struct disk_info_t *out) {
  assert(path && out);

  // open disk image file
  FILE *fd = fopen(path, "r+b");
  if (!fd) {
    return false;
  }

  // get drive size
  fseek(fd, 0, SEEK_END);
  long size = ftell(fd);
  fseek(fd, 0, SEEK_SET);

  // allocate self structure
  struct disk_img_t *img =
    (struct disk_img_t*)malloc(sizeof(struct disk_img_t));
  memset(img, 0, sizeof(struct disk_img_t));

  img->fd = fd;
  img->filesize = size;
  img->seek_pos = 0;

  // populate disk structure
  out->self = img;
  out->eject = _disk_img_eject;
  out->seek  = _disk_img_seek;
  out->read  = _disk_img_read;
  out->write = _disk_img_write;
  out->drive_num = num;

  return true;
}
