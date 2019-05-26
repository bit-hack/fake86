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

#include "../fake86/common.h"


extern uint32_t usegrabmode;
extern uint8_t running, portram[0x10000];
extern SDL_Surface *screen;
extern uint8_t scrmodechange;
extern uint32_t usefullscreen;

extern void set_window_title(uint8_t *extra);

uint8_t translate_scancode(uint16_t keyval);

static void toggle_mouse_grab() {
  if (usegrabmode == SDL_GRAB_ON) {
    usegrabmode = SDL_GRAB_OFF;
    SDL_WM_GrabInput(SDL_GRAB_OFF);
    SDL_ShowCursor(SDL_ENABLE);
    set_window_title("");
  } else {
    usegrabmode = SDL_GRAB_ON;
    SDL_WM_GrabInput(SDL_GRAB_ON);
    SDL_ShowCursor(SDL_DISABLE);
    set_window_title(" (press Ctrl + Alt to release mouse)");
  }
}

static void on_key_down(const SDL_Event *event) {
  // convert scancode
  const uint8_t scancode = translate_scancode(event->key.keysym.sym);

  i8255_key_push(scancode);
  i8259_doirq(1);

  const uint8_t *keys = SDL_GetKeyState(NULL);
  // ctrl + alt to release mouse
  if (keys[SDLK_LCTRL] && keys[SDLK_LALT]) {
    toggle_mouse_grab();
    return;
  }

  // alt + F4 quit
  if (keys[SDLK_LALT] && keys[SDLK_F4]) {
    running = 0;
  }

  // alt + enter to toggle full screen
  if (keys[SDLK_LALT] && keys[SDLK_RETURN]) {

#if USE_VIDEO_NEO
    SDL_WM_ToggleFullScreen(screen);
#endif

    if (usefullscreen) {
      usefullscreen = 0;
    } else {
      usefullscreen = SDL_FULLSCREEN;
    }
    scrmodechange = 1;
    return;
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
  const int midx = screen->w / 2;
  const int midy = screen->h / 2;
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

void tick_events() {
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
      running = 0;
      break;
    default:
      break;
    }
  }
}
