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


static uint32_t _last_disk_tick;
static bool _is_active;


void osd_disk_fdd_used(void) {
  _last_disk_tick = SDL_GetTicks();
}

void osd_disk_hdd_used(void) {
  _last_disk_tick = SDL_GetTicks();
}

bool osd_should_draw_disk(void) {
  return (SDL_GetTicks() - _last_disk_tick) < 250;
}

bool osd_is_active(void) {
  return _is_active;
}

void osd_open(void) {
}

void osd_close(void) {
}

void osd_on_event(const SDL_Event *t) {
}
