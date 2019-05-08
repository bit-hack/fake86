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

#include "common.h"

#include "../80x86/cpu.h"


uint8_t bootdrive = 0, hdcount = 0;

struct struct_drive disk[256];

static uint8_t sectorbuffer[512];

uint8_t disk_insert(uint8_t drivenum, char *filename) {
  if (disk[drivenum].inserted) {
    fclose(disk[drivenum].diskfile);
  }
  else {
    disk[drivenum].inserted = 1;
  }
  disk[drivenum].diskfile = fopen(filename, "r+b");
  if (disk[drivenum].diskfile == NULL) {
    disk[drivenum].inserted = 0;
    return 1;
  }
  fseek(disk[drivenum].diskfile, 0L, SEEK_END);
  disk[drivenum].filesize = ftell(disk[drivenum].diskfile);
  fseek(disk[drivenum].diskfile, 0L, SEEK_SET);
  // it's a hard disk image
  if (drivenum >= 0x80) {
    disk[drivenum].sects = 63;
    disk[drivenum].heads = 16;
    disk[drivenum].cyls = disk[drivenum].filesize /
                          (disk[drivenum].sects * disk[drivenum].heads * 512);
    hdcount++;
  // it's a floppy image
  } else {
    disk[drivenum].cyls = 80;
    disk[drivenum].sects = 18;
    disk[drivenum].heads = 2;
    if (disk[drivenum].filesize <= 1228800) {
      disk[drivenum].sects = 15;
    }
    if (disk[drivenum].filesize <= 737280) {
      disk[drivenum].sects = 9;
    }
    if (disk[drivenum].filesize <= 368640) {
      disk[drivenum].cyls = 40;
      disk[drivenum].sects = 9;
    }
    if (disk[drivenum].filesize <= 163840) {
      disk[drivenum].cyls = 40;
      disk[drivenum].sects = 8;
      disk[drivenum].heads = 1;
    }
  }
  return 0;
}

void disk_eject(uint8_t drivenum) {
  disk[drivenum].inserted = 0;
  if (disk[drivenum].diskfile != NULL) {
    assert(disk[drivenum].diskfile);
    fclose(disk[drivenum].diskfile);
  }
#if 1
  if (drivenum >= 0x80) {
    --hdcount;
  }
#endif
}

void disk_read(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl,
              uint16_t sect, uint16_t head, uint16_t sectcount) {
  if (!sect || !disk[drivenum].inserted) {
    return;
  }
  const uint32_t lba = ((uint32_t)cyl * (uint32_t)disk[drivenum].heads + (uint32_t)head) *
            (uint32_t)disk[drivenum].sects +
        (uint32_t)sect - 1;
  const uint32_t fileoffset = lba * 512;
  if (fileoffset > disk[drivenum].filesize) {
    return;
  }
  fseek(disk[drivenum].diskfile, fileoffset, SEEK_SET);
  uint32_t memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
  // for the readdisk function, we need to use write86 instead of directly
  // fread'ing into
  // the RAM array, so that read-only flags are honored. otherwise, a program
  // could load
  // data from a disk over BIOS or other ROM code that it shouldn't be able to.
  uint32_t cursect = 0;
  for (; cursect < sectcount; cursect++) {
    if (fread(sectorbuffer, 1, 512, disk[drivenum].diskfile) < 512)
      break;
    for (uint32_t sectoffset = 0; sectoffset < 512; sectoffset++) {
      write86(memdest++, sectorbuffer[sectoffset]);
    }
  }
  regs.byteregs[regal] = cursect;
  cf = 0;
  regs.byteregs[regah] = 0;
}

void disk_write(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl,
               uint16_t sect, uint16_t head, uint16_t sectcount) {
  uint32_t memdest, lba, fileoffset, cursect, sectoffset;
  if (!sect || !disk[drivenum].inserted)
    return;
  lba = ((uint32_t)cyl * (uint32_t)disk[drivenum].heads + (uint32_t)head) *
            (uint32_t)disk[drivenum].sects +
        (uint32_t)sect - 1;
  fileoffset = lba * 512;
  if (fileoffset > disk[drivenum].filesize)
    return;
  fseek(disk[drivenum].diskfile, fileoffset, SEEK_SET);
  memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
  for (cursect = 0; cursect < sectcount; cursect++) {
    for (sectoffset = 0; sectoffset < 512; sectoffset++) {
      sectorbuffer[sectoffset] = read86(memdest++);
    }
    fwrite(sectorbuffer, 1, 512, disk[drivenum].diskfile);
  }
  regs.byteregs[regal] = (uint8_t)sectcount;
  cf = 0;
  regs.byteregs[regah] = 0;
}

void disk_int_handler(int intnum) {
  static uint8_t lastdiskah[256], lastdiskcf[256];
  switch (regs.byteregs[regah]) {
  case 0: // reset disk system
    regs.byteregs[regah] = 0;
    cf = 0; // useless function in an emulator. say success and return.
    break;
  case 1: // return last status
    regs.byteregs[regah] = lastdiskah[regs.byteregs[regdl]];
    cf = lastdiskcf[regs.byteregs[regdl]];
    return;
  case 2: // read sector(s) into memory
    if (disk[regs.byteregs[regdl]].inserted) {
      disk_read(regs.byteregs[regdl], segregs[reges], getreg16(regbx),
                regs.byteregs[regch] + (regs.byteregs[regcl] / 64) * 256,
                regs.byteregs[regcl] & 63, regs.byteregs[regdh],
                regs.byteregs[regal]);
      cf = 0;
      regs.byteregs[regah] = 0;
    } else {
      cf = 1;
      regs.byteregs[regah] = 1;
    }
    break;
  case 3: // write sector(s) from memory
    if (disk[regs.byteregs[regdl]].inserted) {
      disk_write(regs.byteregs[regdl], segregs[reges], getreg16(regbx),
                 regs.byteregs[regch] + (regs.byteregs[regcl] / 64) * 256,
                 regs.byteregs[regcl] & 63, regs.byteregs[regdh],
                 regs.byteregs[regal]);
      cf = 0;
      regs.byteregs[regah] = 0;
    } else {
      cf = 1;
      regs.byteregs[regah] = 1;
    }
    break;
  case 4:
  case 5: // format track
    cf = 0;
    regs.byteregs[regah] = 0;
    break;
  case 8: // get drive parameters
    if (disk[regs.byteregs[regdl]].inserted) {
      cf = 0;
      regs.byteregs[regah] = 0;
      regs.byteregs[regch] = disk[regs.byteregs[regdl]].cyls - 1;
      regs.byteregs[regcl] = disk[regs.byteregs[regdl]].sects & 63;
      regs.byteregs[regcl] =
          regs.byteregs[regcl] + (disk[regs.byteregs[regdl]].cyls / 256) * 64;
      regs.byteregs[regdh] = disk[regs.byteregs[regdl]].heads - 1;
      if (regs.byteregs[regdl] < 0x80) {
        regs.byteregs[regbl] = 4; // else regs.byteregs[regbl] = 0;
        regs.byteregs[regdl] = 2;
      } else
        regs.byteregs[regdl] = hdcount;
    } else {
      cf = 1;
      regs.byteregs[regah] = 0xAA;
    }
    break;
  default:
    cf = 1;
  }
  lastdiskah[regs.byteregs[regdl]] = regs.byteregs[regah];
  lastdiskcf[regs.byteregs[regdl]] = cf;
  if (regs.byteregs[regdl] & 0x80) {
    //    RAM[0x474] = regs.byteregs[regah];
    write86(0x474, regs.byteregs[regah]);
  }
}

void disk_bootstrap(int intnum) {
  didbootstrap = 1;
#ifdef BENCHMARK_BIOS
  running = 0;
#endif
  // read first sector of boot drive into 07C0:0000 and execute it
  if (bootdrive < 255) { 
    regs.byteregs[regdl] = bootdrive;
    disk_read(regs.byteregs[regdl], 0x07C0, 0x0000, 0, 1, 0, 1);
    segregs[regcs] = 0x0000;
    ip = 0x7C00;
  } else {
    segregs[regcs] = 0xF600; // start ROM BASIC at bootstrap if requested
    ip = 0x0000;
  }
}