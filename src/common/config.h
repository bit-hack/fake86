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

#ifndef PATH_DATAFILES
#define PATH_DATAFILES
#endif

#define BUILD_STRING "Fake86 Redux v0.14.0.0"

// be sure to only define ONE of the CPU_* options at any given time, or
// you will likely get some unexpected/bad results!
#define CPU_8086 1
#define CPU_V20  2
#define CPU_186  3
#define CPU_286  4
#define CPU_386  5

#define CPU CPU_286

#if (CPU == CPU_8086)
#define CPU_CLEAR_ZF_ON_MUL
#define CPU_ALLOW_POP_CS
#else
#define CPU_ALLOW_ILLEGAL_OP_EXCEPTION
#define CPU_LIMIT_SHIFT_COUNT
#define CPU_USE_286_STYLE_PUSH_SP
#endif
#if (CPU == CPU_V20)
#define CPU_NO_SALC
#endif
#if (CPU == CPU_286) || (CPU == CPU_386)
#define CPU_286_STYLE_PUSH_SP
#else
#define CPU_SET_HIGH_FLAGS
#endif

#ifdef _MSC_VER
#define DISK_PASS_THROUGH 1
#else
#define DISK_PASS_THROUGH 0
#endif

// Log filename
#define LOG_FNAME "log.txt"

// disable all OS delays for benchmarking purposes
#define BENCHMARKING 0

// cpu speed selection
// 2Mhz cps
//#define CYCLES_PER_SECOND (2000000)
// ~4Mhz
#define CYCLES_PER_SECOND (3000000)
#define TICK_SLICES (100)
#define CYCLES_PER_SLICE (CYCLES_PER_SECOND / TICK_SLICES)

// buffer keyboard strokes
#define USE_KEY_BUFFER 0

// do adlib emulation
#define USE_AUDIO_ADLIB   1
#define USE_AUDIO_SPEAKER 1
#define USE_AUDIO_FLOPPY  0

// emulate disk delay
#define USE_DISK_DELAY    1

#define USE_CPU_REDUX     1

#define VERBOSE 0
