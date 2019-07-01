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

#include "cpu.h"

uint16_t useseg;
bool segoverride;

extern struct cpu_io_t _cpu_io;

bool cpu_redux_exec(void);

enum {
  CF = (1 << 0),
  PF = (1 << 2),
  AF = (1 << 4),
  ZF = (1 << 6),
  SF = (1 << 7),
  TF = (1 << 8),
  IF = (1 << 9),
  DF = (1 << 10),
  OF = (1 << 11)
};
