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
  uint32_t seek_pos;
};


static bool _disk_img_eject(
  void *self) {
  assert(self);
  struct disk_img_t *img = (struct disk_img_t*)self;
  if (img->fd) {
    fclose(img->fd);
  }
  free(img);
  return true;
}

static bool _disk_img_seek(
  void *self, const uint32_t offset) {
  assert(self);
  struct disk_img_t *img = (struct disk_img_t*)self;
  fseek(img->fd, offset, SEEK_SET);
  img->seek_pos = offset;
  return true;
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

bool _disk_img_tell(void *self, uint32_t *out) {
  assert(self);
  struct disk_img_t *img = (struct disk_img_t*)self;
  *out = img->seek_pos;
  return true;
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
  img->seek_pos = 0;

  // populate disk structure
  out->self = img;
  out->eject = _disk_img_eject;
  out->seek  = _disk_img_seek;
  out->read  = _disk_img_read;
  out->write = _disk_img_write;

  out->drive_num = num;
  out->size_bytes = size;

  if (num >= 128) {
    return _geom_hard_disk(out);
  }
  else {
    return _geom_floppy_disk(out);
  }
}


struct vhd_footer_t {

  // "conectix"
  char cookie[8];

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
  uint8_t geometry[4];

  // 2  - fixed disk
  uint32_t type;

  // inverted sum of hard disk footer bytes (without checksum)
  uint32_t checksum;
  uint8_t  uuid[16];
  uint8_t  state;

  // should be 427 bytes reserved after this
};


static void _endian(void *ptr, uint32_t size) {
  uint8_t* x = (uint8_t*)ptr;
  uint8_t* y = x + (size - 1);
  for (; x < y; ++x, --y) {
    const uint8_t t = *x;
    *x = *y;
    *y = t;
  }
}

bool _disk_vhd_open(
  const uint8_t num, const char *path, struct disk_info_t *out) {

  assert(path && out);

  // open disk image file
  FILE *fd = fopen(path, "r+b");
  if (!fd) {
    log_printf(LOG_CHAN_DISK, "unable to open VHD file '%s'", path);
    return false;
  }

  // get drive size
  if (fseek(fd, -512, SEEK_END) != 0) {
    log_printf(LOG_CHAN_DISK, "unable to read VHD file footer");
    fclose(fd);
    return false;
  }

  struct vhd_footer_t footer;

  if (fread(&footer, 1, sizeof(footer), fd) != sizeof(footer)) {
    log_printf(LOG_CHAN_DISK, "unable to read VHD file footer");
    fclose(fd);
    return false;
  }

  if (memcmp(footer.cookie, "conectix", 8)) {
    log_printf(LOG_CHAN_DISK, "VHD file invalid");
    fclose(fd);
    return false;
  }

  _endian(&footer.type, 4);
  if (footer.type != 2) {
    log_printf(LOG_CHAN_DISK, "VHD file is not fixed type");
    fclose(fd);
    return false;
  }

  _endian(&footer.offset, 8);
  if (footer.offset != ~0ull) {
    log_printf(LOG_CHAN_DISK, "VHD offset non fixed");
    fclose(fd);
    return false;
  }

  _endian(&footer.size, 8);

  const uint8_t *g = footer.geometry;

  const uint16_t cyls = g[1] | (g[0] << 8);
  const uint8_t heads = g[2];
  const uint8_t sects = g[3];

  const uint32_t size = 512 * sects * cyls * heads;

  // allocate self structure
  struct disk_img_t *img =
    (struct disk_img_t*)malloc(sizeof(struct disk_img_t));
  memset(img, 0, sizeof(struct disk_img_t));

  img->fd = fd;
  img->seek_pos = 0;
  fseek(fd, 0, SEEK_SET);

  // populate disk structure
  out->self = img;
  out->eject = _disk_img_eject;
  out->seek  = _disk_img_seek;
  out->read  = _disk_img_read;
  out->write = _disk_img_write;

  out->drive_num = num;
  out->size_bytes = size;

  out->sector_size = 512;
  out->cyls = cyls;
  out->sects = sects;
  out->heads = heads;

  return true;
}
