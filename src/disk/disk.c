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

// disk emulation routines, working at the BIOS interrupt 13h level

// Use hard disk pass through as follows:
//   -fd0 dos-boot.img -hd0 \\.\PhysicalDrive2 -boot 0
//   -fd0 dos-boot.img -hd0 \\.\X: -boot 0

// Use memory backed floppy disk as follows:
//   -fd0 *disk.img
//   -fd0 *

#include "../common/common.h"
#include "../frontend/frontend.h"
#include "../cpu/cpu.h"
#include "disk.h"


uint8_t bootdrive, hdcount, fdcount;

#define NUM_DISKS 16

static struct disk_info_t _disk[NUM_DISKS];

struct disk_info_t *_get_disk(const uint8_t num) {
  for (int i=0; i < NUM_DISKS; ++i) {
    struct disk_info_t *info = _disk + i;
    if (info->drive_num == num) {
      return info->eject ? info : NULL;
    }
  }
  return NULL;
}

bool _eject(uint8_t num) {
  struct disk_info_t *disk = _get_disk(num);
  if (!disk) {
    return false;
  }
  if (disk->eject(disk->self)) {
    memset(disk, 0, sizeof(struct disk_info_t));
  }
  return true;
}

bool _seek(const uint8_t num, const uint32_t offset) {
  struct disk_info_t *disk = _get_disk(num);
  if (!disk) {
    return false;
  }
  return disk->seek(disk->self, offset);
}

bool _read(const uint8_t num, uint8_t *dst, const uint32_t count) {
  struct disk_info_t *disk = _get_disk(num);
  if (!disk) {
    return false;
  }
  return disk->read(disk->self, dst, count);
}

bool _write(const uint8_t num, const uint8_t *src, const uint32_t count) {
  struct disk_info_t *disk = _get_disk(num);
  if (!disk) {
    return false;
  }
  return disk->write(disk->self, src, count);
}

bool _tell(const uint8_t num, uint32_t *out) {
  struct disk_info_t *disk = _get_disk(num);
  if (!disk) {
    return false;
  }
  return disk->tell(disk->self, out);
}

bool _open(const uint8_t num, const char *path) {

  _eject(num);

  const char *ext = strrchr(path, '.');
  if (ext == NULL) {
    return false;
  }

  struct disk_info_t *disk = NULL;
  for (int i=0; i<NUM_DISKS; ++i) {
    if (_disk[i].eject == NULL) {
      disk = _disk + i;
      break;
    }
  }
  if (disk == NULL) {
    return false;
  }

  bool success = true;

  if (strcmp(ext, ".img") == 0) {
    success = _disk_img_open(num, path, disk);
  }
  if (strcmp(ext, ".vhd") == 0) {
    success = _disk_vhd_open(num, path, disk);
  }
  // TODO: raw drives

  if (!success) {
    _eject(num);
    return false;
  }

  fdcount += (num < 128);
  hdcount += (num > 127);

  return true;
}

bool disk_is_inserted(int num) {
  return _get_disk(num) != NULL;
}

bool _geom_floppy_disk(struct disk_info_t *d) {

  struct disk_type_t {
    uint32_t cyls, sects, heads;
  };

  const struct disk_type_t types[] = {

    // https://en.wikipedia.org/wiki/List_of_floppy_disk_formats

    {80, 18, 2}, // 1.44Mb 3.5"
    {80, 15, 2}, // 1200Kb 5.25"
    {80, 9, 2},  //  720Kb 3.5"
    {80, 8, 2},  //  640Kb 3.5"
    {40, 9, 2},  //  360Kb 5.25"
    {40, 8, 1},  //  160Kb 5.25"

    {0, 0, 0}
  };

  for (int i = 0; types[i].cyls; ++i) {
    const struct disk_type_t *t = types + i;
    const uint32_t size = 512 * t->sects * t->cyls * t->heads;

    if (size == d->size_bytes) {
      d->sector_size = 512;
      d->cyls = t->cyls;
      d->sects = t->sects;
      d->heads = t->heads;

      return true;
    }
  }

  return false;
}

bool _geom_hard_disk(struct disk_info_t *d) {
  assert(d);
  d->sector_size = 512;
  d->sects = 63;
  d->heads = 16;
  d->cyls = d->size_bytes / (d->sects * d->heads * 512);
  return true;
}

bool disk_insert(uint8_t drivenum, const char *filename) {
  return _open(drivenum, filename);
}

void disk_eject(uint8_t drivenum) {

  hdcount -= (drivenum >= 128);
  fdcount -= (drivenum <  128);

  _eject(drivenum);
}

static void _disk_read(uint8_t drivenum,
                       uint16_t dstseg,
                       uint16_t dstoff,
                       uint16_t cyl,
                       uint16_t sect,
                       uint16_t head,
                       uint16_t sectcount) {

  struct disk_info_t *d = _get_disk(drivenum);
  uint8_t sectorbuffer[512];

  if (!sect || !d) {
    return;
  }
  const uint32_t lba = ((uint32_t)cyl * (uint32_t)d->heads + (uint32_t)head) *
                         (uint32_t)d->sects + (uint32_t)sect - 1;
  const uint32_t fileoffset = lba * 512;
  if (fileoffset > d->size_bytes) {
    return;
  }
  _seek(drivenum, fileoffset);
  uint32_t memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
  uint32_t cursect = 0;
  for (; cursect < sectcount; cursect++) {
    if (!_read(drivenum, sectorbuffer, 512)) {
      // ERROR
      break;
    }
    for (uint32_t sectoffset = 0; sectoffset < 512; sectoffset++) {
      write86(memdest++, sectorbuffer[sectoffset]);
    }
  }
  cpu_regs.al = cursect;
  cpu_flags.cf = 0;
  cpu_regs.ah = 0;
}

static void _disk_write(uint8_t drivenum,
                        uint16_t dstseg,
                        uint16_t dstoff,
                        uint16_t cyl,
                        uint16_t sect,
                        uint16_t head,
                        uint16_t sectcount) {
  
  struct disk_info_t *d = _get_disk(drivenum);
  uint8_t sectorbuffer[512];

  uint32_t memdest, lba, fileoffset, cursect, sectoffset;
  if (!sect || !d)
    return;
  lba = ((uint32_t)cyl * (uint32_t)d->heads + (uint32_t)head) *
          (uint32_t)d->sects + (uint32_t)sect - 1;
  fileoffset = lba * 512;
  if (fileoffset > d->size_bytes) {
    // ERROR
    return;
  }
  _seek(drivenum, fileoffset);
  memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
  for (cursect = 0; cursect < sectcount; cursect++) {
    for (sectoffset = 0; sectoffset < 512; sectoffset++) {
      sectorbuffer[sectoffset] = read86(memdest++);
    }
    _write(drivenum, sectorbuffer, 512);
  }
  cpu_regs.al = (uint8_t)sectcount;
  cpu_flags.cf = 0;
  cpu_regs.ah = 0;
}

static void _disk_reset(void) {
  // useless function in an emulator. say success and return.
  cpu_regs.ah = 0;
  cpu_flags.cf = 0;
}

static void _disk_return_status(void) {
  const uint8_t drive_num = cpu_regs.dl;
  struct disk_info_t *disk = _get_disk(drive_num);
  cpu_regs.ah = disk->last_ah;
  cpu_flags.cf = disk->last_cf;
}

static void _disk_read_sect(void) {

  const uint8_t drive_num = cpu_regs.dl;

  if (disk_is_inserted(drive_num)) {
    _disk_read(cpu_regs.dl,
                cpu_regs.es,
                cpu_regs.bx,
                cpu_regs.ch + (cpu_regs.cl / 64) * 256,
                cpu_regs.cl & 63,
                cpu_regs.dh,
                cpu_regs.al);
    cpu_flags.cf = 0;
    cpu_regs.ah = 0;
  } else {
    cpu_flags.cf = 1;
    cpu_regs.ah = 1;
  }
}

static void _disk_write_sect(void) {

  const uint8_t drive_num = cpu_regs.dl;

  if (disk_is_inserted(drive_num)) {
    _disk_write(cpu_regs.dl,
                cpu_regs.es,
                cpu_regs.bx,
                cpu_regs.ch + (cpu_regs.cl / 64) * 256,
                cpu_regs.cl & 63,
                cpu_regs.dh,
                cpu_regs.al);
    cpu_flags.cf = 0;
    cpu_regs.ah = 0;
  } else {
    cpu_flags.cf = 1;
    cpu_regs.ah = 1;
  }
}

static void _disk_format_track(void) {
  cpu_flags.cf = 0;
  cpu_regs.ah = 0;
}

static void _disk_get_params(void) {

  const uint8_t drive_num = cpu_regs.dl;
  struct disk_info_t *disk = _get_disk(drive_num);

  if (disk) {
    cpu_flags.cf = 0;
    cpu_regs.ah = 0;
    cpu_regs.ch = disk->cyls - 1;
    cpu_regs.cl = disk->sects & 63;
    cpu_regs.cl = cpu_regs.cl + (disk->cyls / 256) * 64;
    cpu_regs.dh = disk->heads - 1;
    if (cpu_regs.dl < 0x80) {
      cpu_regs.bl = 4;
      cpu_regs.dl = 2;
    } else {
      cpu_regs.dl = hdcount;
    }
  } else {
    cpu_flags.cf = 1;
    cpu_regs.ah = 0xAA;
  }
}

// BIOS disk services
void disk_int_handler(int intnum) {

  const uint8_t drive_num = cpu_regs.dl;
  struct disk_info_t *disk = _get_disk(drive_num);

  switch (cpu_regs.ah) {
  case 0: // reset disk system
    _disk_reset();
    break;
  case 1: // return last status
    _disk_return_status();
    return;
  case 2: // read sector(s) into memory
    _disk_read_sect();
    break;
  case 3: // write sector(s) from memory
    _disk_write_sect();
    break;
  case 4:
  case 5: // format track
    _disk_format_track();
    break;
  case 8: // get drive parameters
    _disk_get_params();
    break;
  default:
    cpu_flags.cf = 1;
  }

  // store for get status call
  if (disk) {
    disk->last_ah = cpu_regs.ah;
    disk->last_cf = cpu_flags.cf;
  }

  const bool is_hdd = (drive_num >= 0x80);
  if (is_hdd) {
    // bios data area write
    // set status of last hard disk operation
    write86(0x474, cpu_regs.ah);
  }
}

void disk_bootstrap(int intnum) {

  const uint8_t drive_num = cpu_regs.dl;
  struct disk_info_t *disk = _get_disk(drive_num);

  // auto detect boot drive
  if (disk_is_inserted(bootdrive)) {
    if (disk_is_inserted(128)) {
      bootdrive = 128;
    }
    if (disk_is_inserted(0)) {
      bootdrive = 0;
    }
  }

  bool load_basic = false;

  if (bootdrive < 255) {
    if (disk_is_inserted(bootdrive)) {
      // read first sector of boot drive into 07C0:0000 and execute it
      log_printf(LOG_CHAN_DISK, "booting from disk %d", bootdrive);
      cpu_regs.dl = bootdrive;
      _disk_read(cpu_regs.dl, 0x07C0, 0x0000, 0, 1, 0, 1);
      cpu_regs.cs = 0x0000;
      cpu_regs.ip = 0x7C00;
    }
    else {
      load_basic = true;
    }
  }

  if (load_basic) {
    // start ROM BASIC at bootstrap if requested
    log_printf(LOG_CHAN_DISK, "booting into ROM BASIC");
    cpu_regs.cs = 0xF600;
    cpu_regs.ip = 0x0000;
  }
}
