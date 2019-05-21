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

// disk emulation routines, working at the BIOS interrupt 13h level

// Use hard disk pass through as follows:
//   -fd0 dos-boot.img -hd0 \\.\PhysicalDrive2 -boot 0
//   -fd0 dos-boot.img -hd0 \\.\X: -boot 0

// Use memory backed floppy disk as follows:
//   -fd0 *disk.img
//   -fd0 *

#include "common.h"

#if DISK_PASS_THROUGH
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winioctl.h>
#endif

#include "../80x86/cpu.h"


enum disk_type_t {
  // disk not inserted
  disk_type_none = 0,
  // floppy disk from file
  disk_type_fdd_file,
  // floppy disk in memory
  disk_type_fdd_mem,
  // hard disk from file
  disk_type_hdd_file,
  // hard disk pass through
  disk_type_hdd_direct,
};

struct struct_drive {
  // disk drive type
  enum disk_type_t type;
  // disk is inserted
  bool inserted;
#if DISK_PASS_THROUGH
  // direct disk access handle
  HANDLE *handle;
#endif
  // file handle
  FILE *diskfile;
  // in memory data
  uint8_t *memory;
  uint32_t index;
  // file size of on disk backing
  uint32_t filesize;
  // disk size
  uint32_t cyls;
  uint32_t sects;
  uint32_t heads;
};

uint8_t bootdrive = 128;
uint8_t hdcount = 0;
uint8_t fdcount = 0;

static struct struct_drive disk[256];


bool disk_is_inserted(int num) {
#if 0
  return disk[num].inserted;
#else
  return disk[num].type != disk_type_none;
#endif
}

bool disk_insert_mem(uint8_t drivenum, const char *filename) {
  struct struct_drive *d = &disk[drivenum];
  disk_eject(drivenum);
  // set disk layout
  d->cyls = 80;
  d->sects = 18;
  d->heads = 2;
  // calculate disk size
  d->filesize = 512 * d->sects * d->cyls * d->heads;
  // allocate disk
  d->memory = malloc(d->filesize);
  if (!d->memory) {
    return false;
  }
  // reset the index pointer
  d->index = 0;
  // blank the disk
  memset(d->memory, 0x41, d->filesize);
  // add boot signature
  d->memory[510] = 0x55;
  d->memory[511] = 0xAA;
  // add HLT instruction
  d->memory[0] = 0xF4;
  // load disk data
  if (filename && filename[0] != '\0') {
    FILE *fd = fopen(filename, "rb");
    if (fd) {
      int32_t num_bytes = (int32_t)fread(d->memory, 1, d->filesize, fd);
      log_printf(LOG_CHAN_DISK, "loaded %d bytes to disk", num_bytes);
      fclose(fd);
    }
  }
  // success
  d->type = disk_type_fdd_mem;
  d->inserted = true;
  return true;
}

static bool _insert_floppy_disk(struct struct_drive *d) {

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

      {0, 0, 0}};

  for (int i = 0; types[i].cyls; ++i) {
    const struct disk_type_t *t = types + i;
    const uint32_t size = 512 * t->sects * t->cyls * t->heads;

    if (size == d->filesize) {
      d->cyls = t->cyls;
      d->sects = t->sects;
      d->heads = t->heads;

      fdcount++;
      return true;
    }
  }

  return false;
}

static bool _insert_hard_disk(struct struct_drive *d) {
  assert(d);
  d->type = disk_type_hdd_file;
  d->sects = 63;
  d->heads = 16;
  d->cyls = d->filesize / (d->sects * d->heads * 512);
  hdcount++;
  return true;
}

static bool disk_insert_file(uint8_t drivenum, const char *filename) {
  struct struct_drive *d = &disk[drivenum];

  disk_eject(drivenum);

  d->diskfile = fopen(filename, "r+b");
  if (d->diskfile == NULL) {
    log_printf(LOG_CHAN_DISK, "fopen failed");
    d->inserted = false;
    return false;
  }
  // get the disk file size
  fseek(d->diskfile, 0L, SEEK_END);
  d->filesize = ftell(d->diskfile);
  fseek(d->diskfile, 0L, SEEK_SET);
  // it's a hard disk image
  if (drivenum >= 0x80) {
    if (!_insert_hard_disk(d)) {
      return false;
    }
    d->type = disk_type_hdd_file;
  // it's a floppy image
  } else {
    if (!_insert_floppy_disk(d)) {
      return false;
    }
    d->type = disk_type_fdd_file;
  }
  d->inserted = true;
  return true;
}

static bool disk_insert_raw(uint8_t drivenum, const char *filename) {
#if DISK_PASS_THROUGH
  struct struct_drive* d = &disk[drivenum];

  disk_eject(drivenum);

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
    d->handle = NULL;
    DWORD error = GetLastError();
    log_printf(LOG_CHAN_DISK, "CreateFileA failed (%d)", error);
    return false;
  }

  DWORD dwRet = 0;

  if (!read_only) {
    if (FALSE == DeviceIoControl(d->handle, FSCTL_ALLOW_EXTENDED_DASD_IO, NULL,
                                 0, NULL, 0, &dwRet, NULL)) {
      log_printf(LOG_CHAN_DISK, "set FSCTL_ALLOW_EXTENDED_DASD_IO failed");
    }
  }

  DISK_GEOMETRY geo;
  memset(&geo, 0, sizeof(geo));
  if (FALSE == DeviceIoControl(d->handle, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL,
                               0, &geo, sizeof(geo), &dwRet, NULL)) {
    log_printf(LOG_CHAN_DISK, "IOCTL_DISK_GET_DRIVE_GEOMETRY failed");
    return false;
  }

  if (geo.BytesPerSector != 512) {
    log_printf(LOG_CHAN_DISK, "sector size of %d is unsuitable",
               geo.BytesPerSector);
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
  d->filesize = geo.BytesPerSector * geo.SectorsPerTrack *
                geo.TracksPerCylinder * geo.Cylinders.LowPart;

  d->inserted = true;
  d->type = disk_type_hdd_direct;

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
    log_printf(LOG_CHAN_DISK, "mapping raw disk '%s' (%d)", filename,
               (int)drivenum);
    return disk_insert_raw(drivenum, filename);
  } else
#endif // DISK_PASS_THROUGH
      if (filename[0] == '*') {
    log_printf(LOG_CHAN_DISK, "Inserting binary '%s' (%d)", filename + 1,
               (int)drivenum);
    return disk_insert_mem(drivenum, filename + 1);
  } else {
    log_printf(LOG_CHAN_DISK, "inserting disk '%s' (%d)", filename,
               (int)drivenum);
    return disk_insert_file(drivenum, filename);
  }
}

void disk_eject(uint8_t drivenum) {
  struct struct_drive *d = &disk[drivenum];
  switch (d->type) {
  case disk_type_none:
    // disk is already closed nothing to do here
    return;
  case disk_type_fdd_file:
  case disk_type_hdd_file:
    // decrement hard drive count
    hdcount -= (drivenum >= 0x80);
    // standard disk image
    assert(d->diskfile);
    fclose(d->diskfile);
    d->diskfile = NULL;
    break;
  case disk_type_fdd_mem:
    assert(d->memory && d->filesize);
    free(d->memory);
    d->memory = NULL;
    d->index = 0;
    d->filesize = 0;
    break;
#if DISK_PASS_THROUGH
  case disk_type_hdd_direct:
    // raw disk access
    assert(d->handle);
    CloseHandle(d->handle);
    d->handle = NULL;
    break;
#endif
  default:
    UNREACHABLE();
  }
  // mark as ejected
  d->inserted = false;
  d->type = disk_type_none;
  d->filesize = 0;
}

static bool _disk_seek(struct struct_drive *d, uint32_t offset) {
  switch (d->type) {
  case disk_type_fdd_file:
  case disk_type_hdd_file:
    assert(d->diskfile);
    if (fseek(d->diskfile, offset, SEEK_SET))
      return false;
    break;
#if DISK_PASS_THROUGH
  case disk_type_hdd_direct:
    assert(d->handle);
    if (INVALID_SET_FILE_POINTER ==
        SetFilePointer(d->handle, offset, NULL, FILE_BEGIN))
      return false;
    break;
#endif
  case disk_type_fdd_mem:
    assert(d->memory && d->filesize);
    if (offset >= d->filesize)
      return false;
    d->index = offset;
    break;
  default:
    UNREACHABLE();
  }
  return true;
}

static bool _disk_read(struct struct_drive *d, uint8_t *dst, uint32_t size) {
  switch (d->type) {
  case disk_type_fdd_file:
  case disk_type_hdd_file:
    assert(d->diskfile);
    if (fread(dst, 1, 512, d->diskfile) < 512)
      return false;
    break;
#if DISK_PASS_THROUGH
  case disk_type_hdd_direct:
    if (d->handle) {
      DWORD read = 0;
      if (FALSE == ReadFile(d->handle, dst, size, &read, NULL)) {
        return false;
      }
      return read == size;
    }
    return false;
#endif
  case disk_type_fdd_mem:
    assert(d->memory && d->filesize);
    if (d->index + size > d->filesize)
      return false;
    memcpy(dst, d->memory + d->index, size);
    d->index += size;
    break;
  default:
    UNREACHABLE();
  }
  return true;
}

static bool _disk_write(struct struct_drive *d, const uint8_t *src,
                        uint32_t size) {
  switch (d->type) {
  case disk_type_fdd_file:
  case disk_type_hdd_file:
    assert(d->diskfile);
    if (fwrite(src, 1, size, d->diskfile) < size)
      return true;
    break;
#if DISK_PASS_THROUGH
  case disk_type_hdd_direct:
    assert(d->handle);
    {
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
  case disk_type_fdd_mem:
    assert(d->memory && d->filesize);
    if (d->index + size > d->filesize)
      return false;
    memcpy(d->memory + d->index, src, size);
    d->index += size;
    break;
  default:
    UNREACHABLE();
  }
  return true;
}

void disk_read(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl,
               uint16_t sect, uint16_t head, uint16_t sectcount) {

  struct struct_drive *d = &disk[drivenum];
  uint8_t sectorbuffer[512];

  if (!sect || !disk_is_inserted(drivenum)) {
    return;
  }
  const uint32_t lba = ((uint32_t)cyl * (uint32_t)d->heads + (uint32_t)head) *
                           (uint32_t)d->sects +
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

void disk_write(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff,
                uint16_t cyl, uint16_t sect, uint16_t head,
                uint16_t sectcount) {

  struct struct_drive *d = &disk[drivenum];
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

// BIOS disk services
void disk_int_handler(int intnum) {
  static uint8_t lastdiskah[256], lastdiskcf[256];
  switch (cpu_regs.ah) {
  case 0: // reset disk system
    cpu_regs.ah = 0;
    cpu_flags.cf =
        0; // useless function in an emulator. say success and return.
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
      cpu_regs.cl = cpu_regs.cl + (disk[cpu_regs.dl].cyls / 256) * 64;
      cpu_regs.dh = disk[cpu_regs.dl].heads - 1;
      if (cpu_regs.dl < 0x80) {
        cpu_regs.bl = 4;
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
    // todo: raw write?
    write86(0x474, cpu_regs.ah);
  }
}

void disk_bootstrap(int intnum) {

  // auto detect boot drive
  if (!disk[bootdrive].inserted) {
    if (disk[0].inserted) {
      bootdrive = 0;
    }
    if (disk[128].inserted) {
      bootdrive = 128;
    }
  }

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