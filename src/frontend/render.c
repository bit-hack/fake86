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

/* render.c: functions for SDL initialization, as well as video
   scaling/rendering.
   it is a bit messy. i plan to rework much of this in the future. i am also
   going to add hardware accelerated scaling soon. */

#include "../fake86/common.h"


SDL_Surface *screen = NULL;

uint32_t usefullscreen = 0;
uint32_t usegrabmode = SDL_GRAB_OFF;

extern uint8_t RAM[0x100000], portram[0x10000];
extern uint8_t VRAM[262144], vidmode, cgabg, blankattr, vidgfxmode, vidcolor;
extern uint8_t running;
extern uint16_t cursx, cursy, cols, rows, vgapage, cursorposition;
extern bool cursorvisible;
extern uint8_t updatedscreen;
extern uint16_t VGA_SC[0x100], VGA_CRTC[0x100], VGA_ATTR[0x100], VGA_GC[0x100];
extern uint32_t videobase, textbase;
extern uint8_t fontcga[32768];
extern uint32_t palettecga[16], palettevga[256];
extern uint16_t vtotal;
extern uint16_t oldw, oldh, constantw, constanth;

static uint32_t prestretch[1024][1024];

// native width and height, pre-stretching (i.e. 320x200 for mode 13h)
uint32_t nw, nh;

// time since last screen update
static uint32_t cursorprevtick = 0;

uint64_t totalframes = 0;
uint32_t framedelay = 20;

// XXX: make this atomic
volatile bool scrmodechange = false;

uint8_t noscale = 0, nosmooth = 1, renderbenchmark = 0, doaudio = 1;
char windowtitle[128];

void initcga();

const uint32_t window_flags = SDL_HWSURFACE;

void set_window_title(uint8_t *extra) {
  char temptext[128];
  sprintf(temptext, "%s%s", windowtitle, extra);
  SDL_WM_SetCaption((const char *)temptext, NULL);
}

bool render_init(void) {

  uint32_t init_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
  if (doaudio) {
    init_flags |= SDL_INIT_AUDIO;
  }

  if (SDL_Init(init_flags)) {
    return false;
  }
  log_printf(LOG_CHAN_SDL, "initalized SDL");

  screen = SDL_SetVideoMode(640, 400, 32, window_flags);
  if (!screen) {
    return false;
  }

  sprintf(windowtitle, "%s ", BUILD_STRING);
  set_window_title("");

  initcga();
  return true;
}

static void render_redraw(void);

void render_update(void) {
  if (screen == NULL) {
    return;
  }
  if (updatedscreen) {
    render_redraw();
    totalframes++;
    updatedscreen = 0;
  }
}

void render_check_for_mode_change(void) {
  if (!scrmodechange) {
    return;
  }

  // free old video surface
  if (screen != NULL) {
    log_printf(LOG_CHAN_SDL, "released old surface");
    SDL_FreeSurface(screen);
  }

  const uint32_t flags = window_flags | usefullscreen;
  if (constantw && constanth) {
    screen = SDL_SetVideoMode(constantw, constanth, 32, flags);
  } else if (noscale) {
    screen = SDL_SetVideoMode(nw, nh, 32, flags);
  } else {
    if (nw >= 640 || nh >= 400) {
      screen = SDL_SetVideoMode(nw, nh, 32, flags);
    } else {
      screen = SDL_SetVideoMode(640, 400, 32, flags);
    }
  }

  if (screen) {
    log_printf(LOG_CHAN_SDL, "new video mode [%d, %d]", screen->w, screen->h);
  }

#if 0
  if (usefullscreen) {
    SDL_WM_GrabInput(
      SDL_GRAB_ON); // always have mouse grab turned on for full screen mode
  } else {
    SDL_WM_GrabInput(usegrabmode);
  }
  SDL_ShowCursor(SDL_DISABLE);
  if (!usefullscreen) {
    if (usegrabmode == SDL_GRAB_ON)
      setwindowtitle(" (press Ctrl + Alt to release mouse)");
    else
      setwindowtitle("");
  }
#endif
  scrmodechange = false;
}

static void blit_1x(SDL_Surface *target) {

  const uint32_t tw = (uint32_t)target->w, th = (uint32_t)target->h;

  if (SDL_MUSTLOCK(target)) {
    if (SDL_LockSurface(target)) {
      return;
    }
  }
  uint32_t *dst = (uint32_t *)target->pixels;
  const uint32_t sz_x = (nw < tw) ? nw : tw;
  const uint32_t sz_y = (nh < th) ? nh : th;
  for (uint32_t y = 0; y < sz_y; ++y) {
    const uint32_t *src = prestretch[y];
    for (uint32_t x = 0; x < sz_x; ++x) {
#if 0
      const uint8_t *srcx = (const uint8_t*)(src + x);
      dst[x] = SDL_MapRGB(target->format, srcx[0], srcx[1], srcx[2]);
#else
      const uint32_t rgb =
        (0x0000ff & (src[x] >> 16)) |
        (0x00ff00 & (src[x] >>  0)) |
        (0xff0000 & (src[x] << 16));
      dst[x] = rgb;
#endif
    }
    dst += target->w;
  }
  if (SDL_MUSTLOCK(target)) {
    SDL_UnlockSurface(target);
  }
#if !BENCHMARKING
  SDL_UpdateRect(target, 0, 0, tw, th);
#endif
}

static void blit_2x(SDL_Surface *target) {
  const uint32_t tw = (uint32_t)target->w, th = (uint32_t)target->h;
  if (SDL_MUSTLOCK(target)) {
    if (SDL_LockSurface(target)) {
      return;
    }
  }
  uint32_t *dst = (uint32_t *)target->pixels;
  const uint32_t sz_x = (nw < tw) ? nw : tw;
  const uint32_t sz_y = (nh < th) ? nh : th;
  const uint32_t pitch = tw;
  for (uint32_t y = 0; y < sz_y; ++y) {
    const uint32_t *src = prestretch[y];
    uint32_t *dstx = dst;
    for (uint32_t x = 0; x < sz_x; ++x) {
#if 0
      const uint8_t *srcx = (const uint8_t*)(src + x);
      const uint32_t rgb = SDL_MapRGB(target->format, srcx[0], srcx[1], srcx[2]);
#else
      const uint32_t rgb =
        (0x0000ff & (src[x] >> 16)) |
        (0x00ff00 & (src[x] >>  0)) |
        (0xff0000 & (src[x] << 16));
#endif
      dstx[      + 0] = rgb;
      dstx[      + 1] = rgb;
      dstx[pitch + 0] = rgb;
      dstx[pitch + 1] = rgb;
      dstx += 2;
    }
    dst += pitch * 2;
  }
  if (SDL_MUSTLOCK(target)) {
    SDL_UnlockSurface(target);
  }
#if !BENCHMARKING
  SDL_UpdateRect(target, 0, 0, tw, th);
#endif
}

static void draw_mode_text() {
  uint32_t chary, charx, divx, color, vidptr, curchar;
  nw = 640;
  nh = 400;
  vgapage = ((uint32_t)VGA_CRTC[0xC] << 8) + (uint32_t)VGA_CRTC[0xD];
  for (uint32_t y = 0; y < 400; y++) {
    for (uint32_t x = 0; x < 640; x++) {
      if (cols == 80) {
        charx = x / 8;
        divx = 1;
      } else {
        charx = x / 16;
        divx = 2;
      }
      if ((portram[0x3D8] == 9) && (portram[0x3D4] == 9)) {
        chary = y / 4;
        vidptr = vgapage + videobase + chary * cols * 2 + charx * 2;
        curchar = RAM[vidptr];
        color = fontcga[curchar * 128 + (y % 4) * 8 + ((x / divx) % 8)];
      } else {
        chary = y / 16;
        vidptr = videobase + chary * cols * 2 + charx * 2;
        curchar = RAM[vidptr];
        color = fontcga[curchar * 128 + (y % 16) * 8 + ((x / divx) % 8)];
      }
      if (vidcolor) {
        // high intensity background
        if (!color)
          color = palettecga[RAM[vidptr + 1] / 16];
        else
          color = palettecga[RAM[vidptr + 1] & 15];
      } else {
        if ((RAM[vidptr + 1] & 0x70)) {
          color = (!color) ? palettecga[7] : palettecga[0];
        } else {
          color = (!color) ? palettecga[0] : palettecga[7];
        }
      }
      prestretch[y][x] = color;
    }
  }
}

static void draw_mode_4_5(void) {
  uint32_t color, chary, charx, vidptr, curpixel, usepal, intensity;
  nw = 320;
  nh = 200;
  usepal = (portram[0x3D9] >> 5) & 1;
  intensity = ((portram[0x3D9] >> 4) & 1) << 3;
  for (uint32_t y = 0; y < 200; y++) {
    for (uint32_t x = 0; x < 320; x++) {
      charx = x;
      chary = y;
      vidptr = videobase + ((chary >> 1) * 80) + ((chary & 1) * 8192) +
                (charx >> 2);
      curpixel = RAM[vidptr];
      switch (charx & 3) {
      case 3:
        curpixel = curpixel & 3;
        break;
      case 2:
        curpixel = (curpixel >> 2) & 3;
        break;
      case 1:
        curpixel = (curpixel >> 4) & 3;
        break;
      case 0:
        curpixel = (curpixel >> 6) & 3;
        break;
      }
      if (vidmode == 4) {
        curpixel = curpixel * 2 + usepal + intensity;
        if (curpixel == (usepal + intensity))
          curpixel = cgabg;
        color = palettecga[curpixel];
        prestretch[y][x] = color;
      } else {
        curpixel = curpixel * 63;
        color = palettecga[curpixel];
        prestretch[y][x] = color;
      }
    }
  }
}

static void draw_mode_6(void) {
  uint32_t color, chary, charx, vidptr, curpixel;
  nw = 640;
  nh = 200;
  for (uint32_t y = 0; y < 400; y += 2) {
    for (uint32_t x = 0; x < 640; x++) {
      charx = x;
      chary = y >> 1;
      vidptr = videobase + ((chary >> 1) * 80) + ((chary & 1) * 8192) +
                (charx >> 3);
      curpixel = (RAM[vidptr] >> (7 - (charx & 7))) & 1;
      color = palettecga[curpixel * 15];
      prestretch[y + 0][x] = color;
      prestretch[y + 1][x] = color;
    }
  }
}

static void draw_mode_127(void) {
  uint32_t color, chary, charx, vidptr, curpixel;
  nw = 720;
  nh = 348;
  for (uint32_t y = 0; y < 348; y++) {
    for (uint32_t x = 0; x < 720; x++) {
      charx = x;
      chary = y >> 1;
      vidptr = videobase + ((y & 3) << 13) + (y >> 2) * 90 + (x >> 3);
      curpixel = (RAM[vidptr] >> (7 - (charx & 7))) & 1;
#ifdef __BIG_ENDIAN__
      if (curpixel)
        color = 0xFFFFFF00;
#else
      if (curpixel)
        color = 0x00FFFFFF;
#endif
      else
        color = 0x00000000;
      prestretch[y][x] = color;
    }
  }
}

static void draw_mode_8(void) {
  uint32_t color, vidptr;
  // 160x200 16-color (PCjr)
  nw = 640; // fix this
  nh = 400; // part later
  for (uint32_t y = 0; y < 400; y++) {
    for (uint32_t x = 0; x < 640; x++) {
      vidptr = 0xB8000 + (y >> 2) * 80 + (x >> 3) + ((y >> 1) & 1) * 8192;
      if (((x >> 1) & 1) == 0)
        color = palettecga[RAM[vidptr] >> 4];
      else
        color = palettecga[RAM[vidptr] & 15];
      prestretch[y][x] = color;
    }
  }
}

static void draw_mode_9(void) {
  uint32_t color, vidptr;
  nw = 640; // fix this
  nh = 400; // part later
  for (uint32_t y = 0; y < 400; y++) {
    for (uint32_t x = 0; x < 640; x++) {
      vidptr = 0xB8000 + (y >> 3) * 160 + (x >> 2) + ((y >> 1) & 3) * 8192;
      if (((x >> 1) & 1) == 0)
        color = palettecga[RAM[vidptr] >> 4];
      else
        color = palettecga[RAM[vidptr] & 15];
      prestretch[y][x] = color;
    }
  }
}

static void draw_mode_d_e(void) {
  uint32_t color, vidptr, divx, divy, x1;
  nw = 640; // fix this
  nh = 400; // part later
  for (uint32_t y = 0; y < 400; y++) {
    for (uint32_t x = 0; x < 640; x++) {
      divx = x >> 1;
      divy = y >> 1;
      vidptr = divy * 40 + (divx >> 3);
      x1 = 7 - (divx & 7);
      color = (VRAM[vidptr] >> x1) & 1;
      color += (((VRAM[0x10000 + vidptr] >> x1) & 1) << 1);
      color += (((VRAM[0x20000 + vidptr] >> x1) & 1) << 2);
      color += (((VRAM[0x30000 + vidptr] >> x1) & 1) << 3);
      color = palettevga[color];
      prestretch[y][x] = color;
    }
  }
}

static void render_redraw(void) {
  uint32_t planemode, vgapage, color, vidptr, blockw, curheight, x1, y1;
  switch (vidmode) {
  case 0:
  case 1:
  case 2: // text modes
  case 3:
  case 7:
  case 0x82:
    draw_mode_text();
    break;
  case 4:
  case 5:
    draw_mode_4_5();
    break;
  case 6:
    draw_mode_6();
    break;
  case 0x7f:
    draw_mode_127();
    break;
  case 0x8: // 160x200 16-color (PCjr)
    draw_mode_8();
    break;
  case 0x9: // 320x200 16-color (Tandy/PCjr)
    draw_mode_9();
    break;
  case 0xD:
  case 0xE:
    draw_mode_d_e();
    break;
  case 0x10:
    nw = 640;
    nh = 350;
    for (uint32_t y = 0; y < 350; y++)
      for (uint32_t x = 0; x < 640; x++) {
        vidptr = y * 80 + (x >> 3);
        x1 = 7 - (x & 7);
        color = (VRAM[vidptr] >> x1) & 1;
        color |= (((VRAM[0x10000 + vidptr] >> x1) & 1) << 1);
        color |= (((VRAM[0x20000 + vidptr] >> x1) & 1) << 2);
        color |= (((VRAM[0x30000 + vidptr] >> x1) & 1) << 3);
        color = palettevga[color];
        prestretch[y][x] = color;
      }
    break;
  case 0x12:
    nw = 640;
    nh = 480;
    vgapage = ((uint32_t)VGA_CRTC[0xC] << 8) + (uint32_t)VGA_CRTC[0xD];
    for (uint32_t y = 0; y < nh; y++)
      for (uint32_t x = 0; x < nw; x++) {
        vidptr = y * 80 + (x / 8);
        color = (VRAM[vidptr] >> (~x & 7)) & 1;
        color |= ((VRAM[vidptr + 0x10000] >> (~x & 7)) & 1) << 1;
        color |= ((VRAM[vidptr + 0x20000] >> (~x & 7)) & 1) << 2;
        color |= ((VRAM[vidptr + 0x30000] >> (~x & 7)) & 1) << 3;
        prestretch[y][x] = palettevga[color];
      }
    break;
  case 0x13:
    if (vtotal == 11) { // ugly hack to show Flashback at the proper resolution
      nw = 256;
      nh = 224;
    } else {
      nw = 320;
      nh = 200;
    }
    if (VGA_SC[4] & 6)
      planemode = 1;
    else
      planemode = 0;
    // vgapage = ( (uint32_t) VGA_CRTC[0xC]<<8) + (uint32_t) VGA_CRTC[0xD];
    vgapage = (((uint32_t)VGA_CRTC[0xC] << 8) + (uint32_t)VGA_CRTC[0xD]) << 2;
    for (uint32_t y = 0; y < nh; y++)
      for (uint32_t x = 0; x < nw; x++) {
        if (!planemode)
          color =
              palettevga[RAM[videobase + ((vgapage + y * nw + x) & 0xFFFF)]];
        // if (!planemode) color = palettevga[RAM[videobase + y*nw + x]];
        else {
          vidptr = y * nw + x;
          vidptr = vidptr / 4 + (x & 3) * 0x10000;
          vidptr = vidptr + vgapage - (VGA_ATTR[0x13] & 15);
          color = palettevga[VRAM[vidptr]];
        }
        prestretch[y][x] = color;
      }
  }

  if (vidgfxmode == 0) {
    if (cursorvisible) {
      curheight = 2;
      if (cols == 80)
        blockw = 8;
      else
        blockw = 16;
      x1 = cursx * blockw;
      y1 = cursy * 8 + 8 - curheight;
      for (uint32_t y = y1 * 2; y <= y1 * 2 + curheight - 1; y++)
        for (uint32_t x = x1; x <= x1 + blockw - 1; x++) {
          color = palettecga[RAM[videobase + cursy * cols * 2 + cursx * 2 + 1] &
                             15];
          prestretch[y & 1023][x & 1023] = color;
        }
    }
  }

  // blit display to the screen
  if (((nw << 1) <= (uint32_t)screen->w) &&
      ((nh << 1) <= (uint32_t)screen->h)) {
    blit_2x(screen);
  } else {
    blit_1x(screen);
  }
}
