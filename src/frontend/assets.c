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

#include "frontend.h"

// disk image CGA palette (16x16)
const uint8_t asset_disk_pic[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09, 0x09, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09, 0x01,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x01, 0x09, 0x00,
    0x00, 0x01, 0x09, 0x09, 0x0B, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x0B, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09, 0x09, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09, 0x09,
    0x0B, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x0B, 0x09, 0x09, 0x00,
    0x00, 0x01, 0x09, 0x09, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0B, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09, 0x09, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09, 0x09,
    0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x00,
    0x00, 0x01, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x09, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09, 0x09, 0x01, 0x01, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09, 0x09,
    0x01, 0x01, 0x03, 0x03, 0x03, 0x03, 0x01, 0x01, 0x03, 0x09, 0x09, 0x00,
    0x00, 0x01, 0x09, 0x09, 0x01, 0x01, 0x03, 0x03, 0x03, 0x03, 0x01, 0x01,
    0x03, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09, 0x09, 0x01, 0x01, 0x03, 0x03,
    0x03, 0x03, 0x01, 0x01, 0x03, 0x09, 0x09, 0x00, 0x00, 0x01, 0x09, 0x09,
    0x01, 0x01, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x09, 0x09, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};
