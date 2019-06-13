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


// cga mode 4 palette
const uint32_t palette_cga_4_rgb[] = {
  0x000000, 0x00AAAA, 0xAA00AA, 0xAAAAAA,
};

// cga 16 shade palette (text mode)
const uint32_t palette_cga_2_rgb[16] = {
  0x000000,
  0x393939,
  0x818181,
  0x8d8d8d,
  0x5c5c5c,
  0x6c6c6c,
  0x717171,
  0xa9a9a9,
  0x545454,
  0x757575,
  0xcacaca,
  0xd9d9d9,
  0x9b9b9b,
  0xafafaf,
  0xf0f0f0,
  0xfefefe,
};

// cga 16 colour palette (text mode)
const uint32_t palette_cga_3_rgb[16] = {
  0x000000,
  0x0000aa,
  0x00aa00,
  0x00aaaa,
  0xaa0000,
  0xaa00aa,
  0xaa5500,
  0xaaaaaa,
  0x555555,
  0x5555ff,
  0x55ff55,
  0x55ffff,
  0xff5555,
  0xff55ff,
  0xffff55,
  0xffffff
};

const uint32_t palette_vga_16_rgb[] = {
  0x000000,
  0x0000a9,
  0x00a900,
  0x00a9a9,
  0xa90000,
  0xa900a9,
  0xa9a900,
  0xa9a9a9,
  0x000054,
  0x0000ff,
  0x00a954,
  0x00a9ff,
  0xa90054,
  0xa900ff,
  0xa9a954,
  0xa9a9ff,
};
