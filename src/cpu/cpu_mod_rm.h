/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms cpu_flags.of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  cpu_flags.of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty cpu_flags.of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy cpu_flags.of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
  USA.
*/

#pragma once

#include <stdint.h>


struct cpu_mod_rm_t {
  uint8_t mod;
  uint8_t reg;
  uint8_t rm;

  // effective address
  uint32_t ea;
};

static void _decode_mod_rm(const uint8_t *code, struct cpu_mod_rm_t *m) {
  m->mod = (code[0] & 0xC0) >> 6;
  m->reg = (code[0] & 0x38) >> 3;
  m->rm  = (code[0] & 0x07) >> 0;

  switch (m->mod) {
  case 0:
  case 1:
  case 2:
  case 3:
    break;
  }
}
