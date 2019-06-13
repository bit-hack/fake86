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

#include "../common/common.h"
#include "../cpu/cpu.h"


struct vga_timing_t {
  uint32_t hlines, vlines, hz;
  uint64_t px_rate;
  double px_per_cycle, px_per_frame;
  double px_accum;
};

static struct vga_timing_t _vga_timing;

static bool _should_flip;

void vga_timing_init(void) {

  // see: http://tinyvga.com/vga-timing/640x480@60Hz

  // valid for 640x400@70 and 640x350@70
  _vga_timing.hlines = 800;
  _vga_timing.vlines = 449;
  _vga_timing.hz = 70;

  // pixels per second
  _vga_timing.px_rate =
    _vga_timing.hlines * _vga_timing.vlines * _vga_timing.hz;
  // pixels per cpu cycle
  _vga_timing.px_per_cycle =
    (double)_vga_timing.px_rate / (double)CYCLES_PER_SECOND;
  // pixels per video frame
  _vga_timing.px_per_frame =
    (double)(_vga_timing.hlines * _vga_timing.vlines);
}

// this is a bit of a fudge without this, wolf3d gets stuck in its
// @@waitdisplay loop inside of ID_VL_A.asm.
//
// TODO: fix this in the long term... cpu speed?
double speed_scale = 0.9f;

void vga_timing_advance(const uint64_t cycles) {
  // accumulate and wrap
  const double num_pixels = _vga_timing.px_per_cycle * (double)cycles * speed_scale;
  // accumulate
  _vga_timing.px_accum += num_pixels;
  // wrap back into range
  while (_vga_timing.px_accum > _vga_timing.px_per_frame) {
    _should_flip = true;
    // wrap back into range
    _vga_timing.px_accum -= _vga_timing.px_per_frame;
  }
}

uint8_t vga_timing_get_3da(void) {
  // find our cycles part way through the slice
  double acc = _vga_timing.px_accum;
  acc += _vga_timing.px_per_cycle * (double)cpu_slice_ticks() * speed_scale;
  while (acc > _vga_timing.px_per_frame) {
    acc -= _vga_timing.px_per_frame;
  }
  // current pixel in frame
  const uint64_t px_number = (uint64_t)acc;
  // horz and vert progression
  const uint64_t hpos = px_number % _vga_timing.hlines;
  const uint64_t vpos = px_number / _vga_timing.hlines;
  // set vblank and hblank bits accordingly
  const bool in_hblank = (hpos >= 640);
  const bool in_vblank = (vpos >= 400);
  // vblank is 8 + 1
  return (in_vblank ? 9 : 0) |
         (in_hblank ? 1 : 0);
}

bool vga_timing_should_flip(void) {
  return _should_flip;
}

void vga_timing_did_flip(void) {
  _should_flip = false;
}

void vga_timing_state_save(FILE *fd) {
  fwrite(&_vga_timing, 1, sizeof(_vga_timing), fd);
  fwrite(&_should_flip, 1, sizeof(_should_flip), fd);
}

void vga_timing_state_load(FILE *fd) {
  fread(&_vga_timing, 1, sizeof(_vga_timing), fd);
  fread(&_should_flip, 1, sizeof(_should_flip), fd);
}
