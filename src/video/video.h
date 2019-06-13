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


// crt driver access
uint32_t neo_crt_cursor_addr(void);
uint8_t neo_crt_register(uint32_t index);
uint8_t neo_crt_cursor_start(void);
uint8_t neo_crt_cursor_end(void);

const uint8_t *vga_ram(void);

// return video DAC data
const uint32_t *neo_vga_dac(void);
const uint32_t *neo_ega_dac(void);

// font glyph blitters
void font_draw_glyph_8x8(
  uint32_t *dst, const uint32_t pitch, uint16_t ch,
  const uint32_t rgb_a, const uint32_t rgb_b);

void font_draw_glyph_8x16(
  uint32_t *dst, const uint32_t pitch, uint16_t ch,
  const uint32_t rgb_a, const uint32_t rgb_b);

void font_draw_glyph_8x16_gliss(
  uint32_t *dst, const uint32_t pitch, uint16_t ch, uint32_t rgb);

// palette.c
extern const uint32_t palette_cga_2_rgb[];
extern const uint32_t palette_cga_3_rgb[];
extern const uint32_t palette_cga_4_rgb[];
extern const uint32_t palette_vga_16_rgb[];

// video mode is &0x80, non blanking
bool is_non_blanking(void);
