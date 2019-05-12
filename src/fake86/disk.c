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

/* disk.c: disk emulation routines for Fake86. works at the BIOS interrupt 13h
 * level. */

// Use hard disk pass through as follows:
//   -fd0 dos-boot.img -hd0 \\.\PhysicalDrive2 -boot 0
//   -fd0 dos-boot.img -hd0 \\.\X: -boot 0

#include "common.h"

#if DISK_PASS_THROUGH
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winioctl.h>
#endif

#include "../80x86/cpu.h"


struct struct_drive {

  // one or the other
#if DISK_PASS_THROUGH
  HANDLE *handle;
#endif
  FILE *diskfile;

  uint32_t filesize;
  uint32_t cyls;
  uint32_t sects;
  uint32_t heads;
  uint8_t inserted;
};

uint8_t bootdrive = 0, hdcount = 0;

static struct struct_drive disk[256];


bool disk_is_inserted(int num) {
  return disk[num].inserted != 0;
}

static bool disk_insert_image(uint8_t drivenum, const char *filename) {
  struct struct_drive* d = &disk[drivenum];
  if (d->inserted) {
    fclose(d->diskfile);
  }
  else {
    d->inserted = 1;
  }
  d->diskfile = fopen(filename, "r+b");
  if (d->diskfile == NULL) {
    log_printf(LOG_CHAN_DISK, "fopen failed");
    d->inserted = 0;
    return false;
  }
  fseek(d->diskfile, 0L, SEEK_END);
  d->filesize = ftell(d->diskfile);
  fseek(d->diskfile, 0L, SEEK_SET);
  // it's a hard disk image
  if (drivenum >= 0x80) {
    d->sects = 63;
    d->heads = 16;
    d->cyls = d->filesize / (d->sects * d->heads * 512);
    hdcount++;
  // it's a floppy image
  } else {
    d->cyls = 80;
    d->sects = 18;
    d->heads = 2;
    if (d->filesize <= 1228800) {
      d->sects = 15;
    }
    if (d->filesize <= 737280) {
      d->sects = 9;
    }
    if (d->filesize <= 368640) {
      d->cyls = 40;
      d->sects = 9;
    }
    if (d->filesize <= 163840) {
      d->cyls = 40;
      d->sects = 8;
      d->heads = 1;
    }
  }
  return true;
}

static bool disk_insert_raw(uint8_t drivenum, const char *filename) {
#if DISK_PASS_THROUGH
  struct struct_drive* d = &disk[drivenum];

  if (d->handle) {
    CloseHandle(d->handle);
    d->inserted = 0;
  }

  const DWORD attribs =
    FILE_FLAG_WRITE_THROUGH |
    FILE_FLAG_NO_BUFFERING |
    FILE_ATTRIBUTE_NORMAL |
    FILE_FLAG_RANDOM_ACCESS;

  const bool read_only = true;
  const DWORD share_mode = read_only ?
    FILE_SHARE_READ : FILE_SHARE_READ | FILE_SHARE_WRITE;
  const DWORD access_mode = read_only ?
    GENERIC_READ : GENERIC_READ | GENERIC_WRITE;

  d->handle = CreateFileA(filename,
                          access_mode,
                          share_mode,
                          NULL,
                          OPEN_EXISTING,
                          attribs,
                          NULL);
  if (INVALID_HANDLE_VALUE == d->handle) {
    DWORD error = GetLastError();
    log_printf(LOG_CHAN_DISK, "CreateFileA failed (%d)", error);
    return false;
  }

  DWORD dwRet = 0;

  if (!read_only) {
    if (FALSE == DeviceIoControl(d->handle, FSCTL_ALLOW_EXTENDED_DASD_IO,
                                 NULL, 0, NULL, 0, &dwRet, NULL)) {
      log_printf(LOG_CHAN_DISK, "set FSCTL_ALLOW_EXTENDED_DASD_IO failed");
    }
  }

  DISK_GEOMETRY geo;
  memset(&geo, 0, sizeof(geo));
  if (FALSE == DeviceIoControl(d->handle, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                               NULL, 0, &geo, sizeof(geo), &dwRet, NULL)) {
    log_printf(LOG_CHAN_DISK, "IOCTL_DISK_GET_DRIVE_GEOMETRY failed");
    return false;
  }

  if (geo.BytesPerSector != 512) {
    log_printf(LOG_CHAN_DISK, "sector size of %d is unsuitable", geo.BytesPerSector);
    return false;
  }

  d->cyls = geo.Cylinders.LowPart;
  d->heads = geo.TracksPerCylinder;
  d->sects = geo.SectorsPerTrack;

  if (geo.Cylinders.HighPart) {
    log_printf(LOG_CHAN_DISK, "disk too large!");
    return false;
  }

  // Calculate drive size
  d->filesize = geo.BytesPerSector * geo.SectorsPerTrack * geo.TracksPerCylinder * geo.Cylinders.LowPart;

  d->inserted = 1;

  if (drivenum >= 0x80) {
    ++hdcount;
  }

  log_printf(LOG_CHAN_DISK, "disk mapped");

#endif // DISK_PASS_THROUGH
  return true;
}

bool disk_insert(uint8_t drivenum, const char *filename) {
#if DISK_PASS_THROUGH
  if (filename[0] == '\\' && filename[1] == '\\') {
#else
  if (true) {
#endif
    log_printf(LOG_CHAN_DISK, "mapping raw disk '%s' (%d)", filename, (int)drivenum);
    return disk_insert_raw(drivenum, filename);
  }
  else {
    log_printf(LOG_CHAN_DISK, "inserting disk '%s' (%d)", filename, (int)drivenum);
    return disk_insert_image(drivenum, filename);
  }
}

void disk_eject(uint8_t drivenum) {
  struct struct_drive* d = &disk[drivenum];
  d->inserted = 0;
  // standard disk image
  if (d->diskfile) {
    assert(d->diskfile);
    fclose(d->diskfile);
    d->diskfile = NULL;
  }
#if DISK_PASS_THROUGH
  // raw disk access
  if (d->handle) {
    CloseHandle(d->handle);
    d->handle = NULL;
  }
#endif
#if 1
  if (drivenum >= 0x80) {
    --hdcount;
  }
#endif
}

static bool _disk_seek(struct struct_drive* d, uint32_t offset) {
  if (d->diskfile) {
    if (fseek(d->diskfile, offset, SEEK_SET)) {
      return false;
    }
    return true;
  }
#if DISK_PASS_THROUGH
  if (d->handle) {
    return INVALID_SET_FILE_POINTER != SetFilePointer(d->handle, offset, NULL, FILE_BEGIN);
  }
#endif
  return false;
}

static bool _disk_read(struct struct_drive* d, uint8_t *dst, uint32_t size) {
  if (d->diskfile) {
    if (fread(dst, 1, 512, d->diskfile) < 512) {
      return false;
    }
    return true;
  }
#if DISK_PASS_THROUGH
  if (d->handle) {
    DWORD read = 0;
    if (FALSE == ReadFile(d->handle, dst, size, &read, NULL)) {
      return false;
    }
    return read == size;
  }
#endif
  return false;
}

static bool _disk_write(struct struct_drive* d, const uint8_t *src, uint32_t size) {
  if (d->diskfile) {
    if (fwrite(src, 1, size, d->diskfile) < size) {
      return true;
    }
  }
#if DISK_PASS_THROUGH
  if (d->handle) {
    DWORD written = 0;
    if (FALSE == WriteFile(d->handle, src, size, &written, NULL)) {
      return false;
    }
    if (!FlushFileBuffers(d->handle)) {
      // error flushing buffers
    }
    return written == size;
  }
#endif
  return false;
}

void disk_read(uint8_t drivenum,
               uint16_t dstseg,
               uint16_t dstoff,
               uint16_t cyl,
               uint16_t sect,
               uint16_t head,
               uint16_t sectcount) {

  struct struct_drive* d = &disk[drivenum];
  uint8_t sectorbuffer[512];

  if (!sect || !d->inserted) {
    return;
  }
  const uint32_t lba = ((uint32_t)cyl * (uint32_t)d->heads +
                       (uint32_t)head) * (uint32_t)d->sects +
                       (uint32_t)sect - 1;
  const uint32_t fileoffset = lba * 512;
  if (fileoffset > d->filesize) {
    return;
  }
  if (!_disk_seek(d, fileoffset)) {
    // ERROR
  }
  uint32_t memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
  // for the readdisk function, we need to use write86 instead of directly
  // fread'ing into
  // the RAM array, so that read-only flags are honored. otherwise, a program
  // could load
  // data from a disk over BIOS or other ROM code that it shouldn't be able to.
  uint32_t cursect = 0;
  for (; cursect < sectcount; cursect++) {
    if (!_disk_read(d, sectorbuffer, 512)) {
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

void disk_write(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl,
                uint16_t sect, uint16_t head, uint16_t sectcount) {

  struct struct_drive* d = &disk[drivenum];
  uint8_t sectorbuffer[512];

  uint32_t memdest, lba, fileoffset, cursect, sectoffset;
  if (!sect || !d->inserted)
    return;
  lba = ((uint32_t)cyl * (uint32_t)d->heads + (uint32_t)head) *
            (uint32_t)d->sects +
        (uint32_t)sect - 1;
  fileoffset = lba * 512;
  if (fileoffset > d->filesize) {
    // ERROR
    return;
  }
  if (!_disk_seek(d, fileoffset)) {
    // ERROR
    return;
  }
  memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
  for (cursect = 0; cursect < sectcount; cursect++) {
    for (sectoffset = 0; sectoffset < 512; sectoffset++) {
      sectorbuffer[sectoffset] = read86(memdest++);
    }
    if (!_disk_write(d, sectorbuffer, 512)) {
      // ERROR
      return;
    }
  }
  cpu_regs.al = (uint8_t)sectcount;
  cpu_flags.cf = 0;
  cpu_regs.ah = 0;
}

void disk_int_handler(int intnum) {
  static uint8_t lastdiskah[256], lastdiskcf[256];
  switch (cpu_regs.ah) {
  case 0: // reset disk system
    cpu_regs.ah = 0;
    cpu_flags.cf = 0; // useless function in an emulator. say success and return.
    break;
  case 1: // return last status
    cpu_regs.ah = lastdiskah[cpu_regs.dl];
    cpu_flags.cf = 1 & lastdiskcf[cpu_regs.dl];
    return;
  case 2: // read sector(s) into memory
    if (disk[cpu_regs.dl].inserted) {
      disk_read(cpu_regs.dl,
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
    break;
  case 3: // write sector(s) from memory
    if (disk[cpu_regs.dl].inserted) {
      disk_write(cpu_regs.dl,
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
    break;
  case 4:
  case 5: // format track
    cpu_flags.cf = 0;
    cpu_regs.ah = 0;
    break;
  case 8: // get drive parameters
    if (disk[cpu_regs.dl].inserted) {
      cpu_flags.cf = 0;
      cpu_regs.ah = 0;
      cpu_regs.ch = disk[cpu_regs.dl].cyls - 1;
      cpu_regs.cl = disk[cpu_regs.dl].sects & 63;
      cpu_regs.cl =
          cpu_regs.cl + (disk[cpu_regs.dl].cyls / 256) * 64;
      cpu_regs.dh = disk[cpu_regs.dl].heads - 1;
      if (cpu_regs.dl < 0x80) {
        cpu_regs.bl = 4; // else cpu_regs.bl] = 0;
        cpu_regs.dl = 2;
      } else
        cpu_regs.dl = hdcount;
    } else {
      cpu_flags.cf = 1;
      cpu_regs.ah = 0xAA;
    }
    break;
  default:
    cpu_flags.cf = 1;
  }
  lastdiskah[cpu_regs.dl] = cpu_regs.ah;
  lastdiskcf[cpu_regs.dl] = cpu_flags.cf;
  if (cpu_regs.dl & 0x80) {
    //    RAM[0x474] = cpu_regs.ah];
    write86(0x474, cpu_regs.ah);
  }
}

void disk_bootstrap(int intnum) {
  didbootstrap = 1;
#ifdef BENCHMARK_BIOS
  running = 0;
#endif
  // read first sector of boot drive into 07C0:0000 and execute it
  if (bootdrive < 255) {
    log_printf(LOG_CHAN_DISK, "booting from disk %d", bootdrive);
    cpu_regs.dl = bootdrive;
    disk_read(cpu_regs.dl, 0x07C0, 0x0000, 0, 1, 0, 1);
    cpu_regs.cs = 0x0000;
    ip = 0x7C00;
  } else {
    log_printf(LOG_CHAN_DISK, "booting into ROM BASIC");
    cpu_regs.cs = 0xF600; // start ROM BASIC at bootstrap if requested
    ip = 0x0000;
  }
}