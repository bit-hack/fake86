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

#pragma once

#include "../fake86/common.h"


extern SDL_Surface *_surface;

extern bool do_fullscreen;
extern bool cpu_running;
extern const char *biosfile;
extern uint8_t bootdrive;
extern bool do_fullscreen;
extern uint32_t frame_skip;

// on screen display
void osd_disk_fdd_used(void);
void osd_disk_hdd_used(void);

bool osd_should_draw_disk(void);

// assets.c
extern const uint8_t asset_disk_pic[256];
