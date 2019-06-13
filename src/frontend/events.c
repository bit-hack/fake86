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
#include "frontend.h"


static uint32_t is_grabbed;

extern void set_window_title(uint8_t *extra);

uint8_t translate_scancode(uint16_t keyval);

void neo_render_fs_toggle(void);


static void toggle_mouse_grab() {
  if (!is_grabbed) {
    SDL_WM_GrabInput(SDL_GRAB_OFF);
    SDL_ShowCursor(SDL_ENABLE);
    SDL_WM_SetCaption("", NULL);
  } else {
    SDL_WM_GrabInput(SDL_GRAB_ON);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_WM_SetCaption(" (press Ctrl + Alt to release mouse)", NULL);
  }
  is_grabbed = !is_grabbed;
}

static void on_key_down(const SDL_Event *event) {

  // convert scancode
  const uint8_t scancode = translate_scancode(event->key.keysym.sym);
  const uint8_t *keys = SDL_GetKeyState(NULL);

  // ctrl + alt to release mouse
  if (keys[SDLK_LCTRL] && keys[SDLK_LALT]) {
    toggle_mouse_grab();
    return;
  }

  // alt + F4 quit
  if (keys[SDLK_LALT] && keys[SDLK_F4]) {
    cpu_running = false;
    return;
  }

  // alt + enter to toggle full screen
  if (keys[SDLK_LALT] && keys[SDLK_RETURN]) {
    win_fs_toggle();
    return;
  }

  if (keys[SDLK_F11]) {
    mem_dump("mem_dump.bin");
    return;
  }

  if (keys[SDLK_F12]) {
    if (!osd_is_active()) {
      osd_open();
      return;
    }
  }

  if (!osd_is_active()) {
    i8255_key_push(scancode);
    i8259_doirq(1);
  }
  else {
    osd_on_event(event);
  }
}

static void on_key_up(const SDL_Event *event) {
  // convert scancode
  const uint8_t scancode = translate_scancode(event->key.keysym.sym) | 0x80;
  // push to keyboard controller
  i8255.port_in[0] = scancode;
  i8259_doirq(1);
}

static void on_mouse_button_down(const SDL_Event *event) {
  if (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_OFF) {
    toggle_mouse_grab();
    return;
  }
  const uint8_t buttons = SDL_GetMouseState(NULL, NULL);
  mouse_post_event(buttons & SDL_BUTTON(SDL_BUTTON_LEFT),
                   buttons & SDL_BUTTON(SDL_BUTTON_RIGHT),
                   0, 0);
}

static void on_mouse_button_up(const SDL_Event *event) {
  if (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_OFF) {
    return;
  }
  const uint8_t buttons = SDL_GetMouseState(NULL, NULL);
  mouse_post_event(buttons & SDL_BUTTON(SDL_BUTTON_LEFT),
                   buttons & SDL_BUTTON(SDL_BUTTON_RIGHT),
                   0, 0);
}

static void on_mouse_motion(const SDL_Event *event) {
  if (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_OFF)
    return;

  uint32_t midx, midy;
  win_size(&midx, &midy);
  midx /= 2;
  midy /= 2;

  int mx = 0, my = 0;
  const uint8_t buttons = SDL_GetMouseState(&mx, &my);
  const int dx = mx - midx;
  const int dy = my - midy;
  if (dx != 0 || dy != 0) {
    mouse_post_event(buttons & SDL_BUTTON(SDL_BUTTON_LEFT),
                     buttons & SDL_BUTTON(SDL_BUTTON_RIGHT),
                     dx, dy);
    // can generate a motion event
    SDL_WarpMouse(midx, midy);
  }
}

static void on_active_event(const SDL_Event *event) {
  // lost focus
  if (event->active.gain == 0) {
    if (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON) {
      toggle_mouse_grab();
    }
  }
}

void tick_events(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_ACTIVEEVENT:
      on_active_event(&event);
      break;
    case SDL_KEYDOWN:
      on_key_down(&event);
      break;
    case SDL_KEYUP:
      on_key_up(&event);
      break;
    case SDL_MOUSEBUTTONDOWN:
      on_mouse_button_down(&event);
      break;
    case SDL_MOUSEBUTTONUP:
      on_mouse_button_up(&event);
      break;
    case SDL_MOUSEMOTION:
      on_mouse_motion(&event);
      break;
    case SDL_QUIT:
      log_printf(LOG_CHAN_SDL, "received SDL_QUIT");
      cpu_running = false;
      break;
    default:
      break;
    }
  }
}
