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

#include "../common/common.h"

extern bool do_fullscreen;
extern bool cpu_running;
extern const char *biosfile;
extern uint8_t bootdrive;
extern bool do_fullscreen;
extern uint32_t frame_skip;

// TODO: push back to video.h
struct render_target_t {
  uint32_t *dst;
  uint32_t w, h, pitch;
};

void neo_render_tick(const struct render_target_t *target);

// on screen display
void osd_disk_fdd_used(void);
void osd_disk_hdd_used(void);

bool osd_should_draw_disk(void);
bool osd_is_active(void);
void osd_open(void);
void osd_close(void);
void osd_on_event(const SDL_Event *t);
void osd_render(const struct render_target_t *);

// assets.c
extern const uint8_t asset_disk_pic[256];

// window.c
void win_fs_toggle(void);
bool win_init(void);
void win_render(void);
void win_size(uint32_t *w, uint32_t *h);

// events.c
void tick_events(void);

// parsecl.c
bool cl_parse(const int argc, const char **args);
