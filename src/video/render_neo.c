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

// see:
// http://www.minuszerodegrees.net/video/bios_video_modes.htm


#include "../common/common.h"
#include "../video/video.h"
#include "../frontend/frontend.h"


// offscreen render target
static uint32_t _temp[320 * 240];


// render a grey/black dither pattern
static void _neo_render_mode_unknown(const struct render_target_t *target) {
  uint32_t *dsty = target->dst;
  for (uint32_t y = 0; y < target->h; ++y) {
    uint32_t *dstx = dsty;
    for (uint32_t x = 0; x < target->w; ++x) {
      dstx[x] = (1 & (x ^ y)) ? 0x000000 : 0x808080;
    }
    dsty += target->pitch;
  }
}

static void _neo_draw_cursor(const struct render_target_t *target,
                             const uint8_t chw, const uint8_t chh,
                             const uint8_t w, const uint8_t h,
                             const uint32_t yoffset) {

  // blink
  if ((SDL_GetTicks() % 1000) > 500) {
    return;
  }
  // convert address to location
  const uint32_t cursor = neo_crt_cursor_addr();
  const uint32_t x = cursor % w;
  const uint32_t y = cursor / w;
  // bail out if offscreen
  if (x >= w || y >= h) {
    return;
  }
  // draw target
  const uint32_t pitch = target->pitch;
  uint32_t *dst = target->dst;
  dst += yoffset * pitch;
  dst += chw * x + chh * y * pitch;
  // scanline locations
  const uint32_t start = neo_crt_cursor_start();
  const uint32_t end   = neo_crt_cursor_end();
  // draw it
  for (uint32_t y = 0; y < chh; ++y) {
    if (y >= start && y <= end) {
      for (uint32_t x = 0; x < chw; ++x) {
        dst[x] = 0xffffff;
      }
    }
    dst += pitch;
  }
}

// 80x25 greyscale text mode
static void _neo_render_mode_02(const struct render_target_t *target) {
  // text mode buffer address
  uint32_t src = 0xB8000;
  // cga/PCjr = 8x8  char px
  // EGA      = 8x14 char px
  // MCGA     = 8x16 char px
  // VGA      = 9x16 char px
  const int chw = 8, chh = 16;
  // step through VGA text-mode buffer
  const int rows = 25, cols = 80;
  // screen buffer position
  const uint32_t pitch = target->pitch;
  uint32_t *dsty = target->dst;
  const uint32_t yoffset = (target->h - (chh * rows)) / 2;
  dsty += pitch * yoffset;
  // blit loop
  for (int y = 0; y < rows; ++y) {
    uint32_t *dstx = dsty;
    for (int x = 0; x < cols; ++x) {
      // grab character and attribute
      const uint8_t ch = RAM[src + 0];
      const uint8_t at = RAM[src + 1];
      // decode colour from attribute
      const uint32_t rgba = palette_cga_2_rgb[at & 0xf];
      const uint32_t rgbb = palette_cga_2_rgb[at >> 4];
      // draw the glyph
      font_draw_glyph_8x16(dstx, pitch, ch + 0x100, rgba, rgbb);
      // step over to next glyph
      dstx += chw;
      // step over character and attribute
      src += 2;
    }
    // step over glyph line
    dsty += pitch * chh;
  }
  // this is text mode so draw the cursor if needed
  _neo_draw_cursor(target, chw, chh, cols, rows, yoffset);
}

// 80x25 16-colour text mode
static void _neo_render_mode_03(const struct render_target_t *target) {
  const uint32_t base = 0xB8000;
  // text mode buffer address
  uint32_t src = 0xB8000;
  // cga/PCjr = 8x8  char px
  // EGA      = 8x14 char px
  // MCGA     = 8x16 char px
  // VGA      = 9x16 char px
  const int chw = 8, chh = 16;
  // step through VGA text-mode buffer
  const int rows = 25, cols = 80;
  // screen buffer position
  const uint32_t pitch = target->pitch;
  uint32_t *dsty = target->dst;
  const uint32_t yoffset = (target->h - (chh * rows)) / 2;
  dsty += pitch * yoffset;
  // blit loop
  for (int y=0; y<rows; ++y) {
    uint32_t *dstx = dsty;
    for (int x=0; x<cols; ++x) {
      // grab character and attribute
      const uint8_t ch = RAM[src + 0];
      const uint8_t at = RAM[src + 1];
      // decode colour from attribute
      const uint32_t rgba = palette_cga_3_rgb[at & 0xf];
      const uint32_t rgbb = palette_cga_3_rgb[at >> 4];
      // draw the glyph
      font_draw_glyph_8x16(dstx, pitch, ch + 0x100, rgba, rgbb);
      // step over to next glyph
      dstx += chw;
      // step over character and attribute
      src += 2;
    }
    // step over glyph line
    dsty += pitch * chh;
  }
  // this is text mode so draw the cursor if needed
  _neo_draw_cursor(target, chw, chh, cols, rows, yoffset);
}

// 320x200 4-colour graphics mode interleaved
static void _neo_render_mode_04(void) {
  // buffer address
  uint32_t src = 0xB8000;
  uint32_t span = 1024 * 8;
  // screen buffer position
  const uint32_t pitch = 320;
  uint32_t *dsty = _temp;
  // blit loop
  for (int y=0; y<200; ++y) {
    uint32_t *dstx = dsty;
    uint32_t srcx = src + ((y & 1) ? span : 0);
    for (int x=0; x<320; x += 4, ++srcx) {
      const uint8_t ch = RAM[srcx];
      dstx[x + 3] = palette_cga_4_rgb[0x3 & (ch >> 0)];
      dstx[x + 2] = palette_cga_4_rgb[0x3 & (ch >> 2)];
      dstx[x + 1] = palette_cga_4_rgb[0x3 & (ch >> 4)];
      dstx[x + 0] = palette_cga_4_rgb[0x3 & (ch >> 6)];
    }
    dsty += pitch;
    src += (y & 1) ? (320 / 4) : 0;
  }
}

// 320x200 greyscale graphics mode interleaved
static void _neo_render_mode_05(void) {

  static const uint32_t ramp[] = {
    0x000000, 0x444444, 0x888888, 0xcccccc
  };

  // buffer address
  uint32_t src = 0xB8000;
  uint32_t span = 1024 * 8;
  // screen buffer position
  const uint32_t pitch = 320;
  uint32_t *dsty = _temp;
  // blit loop
  for (int y=0; y<200; ++y) {
    uint32_t *dstx = dsty;
    uint32_t srcx = src + ((y & 1) ? span : 0);
    for (int x=0; x<320; x += 4, ++srcx) {
      const uint8_t ch = RAM[srcx];
      dstx[x + 3] = ramp[0x3 & (ch >> 0)];
      dstx[x + 2] = ramp[0x3 & (ch >> 2)];
      dstx[x + 1] = ramp[0x3 & (ch >> 4)];
      dstx[x + 0] = ramp[0x3 & (ch >> 6)];
    }
    dsty += pitch;
    src += (y & 1) ? (320 / 4) : 0;
  }
}

// 80x25 greyscale text mode
// XXX: untested
static void _neo_render_mode_07(const struct render_target_t *target) {
  // text mode buffer address
  uint32_t src = 0xB8000;
  // cga/PCjr = 9x14  char px
  const int chw = 8, chh = 16;
  // step through VGA text-mode buffer
  const int rows = 25, cols = 80;
  // screen buffer position
  const uint32_t pitch = target->pitch;
  uint32_t *dsty = target->dst;
  dsty += pitch * ((target->h - (chh * rows)) / 2);
  // blit loop
  for (int y=0; y<rows; ++y) {
    uint32_t *dstx = dsty;
    for (int x=0; x<cols; ++x) {
      // grab character and attribute
      const uint8_t ch = RAM[src + 0];
      const uint8_t at = RAM[src + 1];
      // draw the glyph
      font_draw_glyph_8x16(dstx, pitch, ch + 0x100, 0xaaaaaa, 0x0);
      // step over to next glyph
      dstx += chw;
      // step over character and attribute
      src += 2;
    }
    // step over glyph line
    dsty += pitch * chh;
  }
}

static void _neo_render_mode_0e(const struct render_target_t *target) {
  //
  static const uint32_t width = 640;
  static const uint32_t height = 200;
  // vga dac palette
  const uint32_t *dac = neo_ega_dac();
  // video ram at 0xA0000
  const uint8_t *plane0 = vga_ram() + 0x10000 * 0;
  const uint8_t *plane1 = vga_ram() + 0x10000 * 1;
  const uint8_t *plane2 = vga_ram() + 0x10000 * 2;
  const uint8_t *plane3 = vga_ram() + 0x10000 * 3;
  // target memory
  uint32_t *dsty = target->dst;
  const uint32_t pitch = target->pitch;
  // center
  dsty += ((target->h - (height * 2)) / 2) * pitch;
  // blit loop
  for (uint32_t y = 0; y < height; ++y) {
    uint32_t *dstx = dsty;
    for (uint32_t x = 0; x < (width / 8); ++x) {
      // get the next colour bytes from each plane
      const uint8_t b0 = plane0[x];
      const uint8_t b1 = plane1[x];
      const uint8_t b2 = plane2[x];
      const uint8_t b3 = plane3[x];
      // write 8 pixels at a time
      for (int i=0; i<8; ++i) {
        const uint8_t mask = 0x80 >> i;
        // combine bits for colour index
        const uint8_t index = ((b0 & mask) ? 1 : 0) |
                              ((b1 & mask) ? 2 : 0) |
                              ((b2 & mask) ? 4 : 0) |
                              ((b3 & mask) ? 8 : 0);
        const uint32_t rgb = dac[index];
        // XXX: this palette index is not right
        *dstx           = rgb;
        *(dstx + pitch) = rgb;
        // next dest
        ++dstx;
      }
    }
    // step over the destination
    dsty += pitch * 2;
    // step the planes
    plane0 += (width / 8);
    plane1 += (width / 8);
    plane2 += (width / 8);
    plane3 += (width / 8);
  }
}

static void _neo_render_mode_0d(void) {
  const uint32_t *dac = neo_ega_dac();
  // clear temp buffer
  memset(_temp, 0, 320 * 240 * 4);
  // video ram at 0xA0000
  const uint8_t *plane0 = vga_ram() + 0x10000 * 0;
  const uint8_t *plane1 = vga_ram() + 0x10000 * 1;
  const uint8_t *plane2 = vga_ram() + 0x10000 * 2;
  const uint8_t *plane3 = vga_ram() + 0x10000 * 3;
  // writing to temporary buffer
  uint32_t *dsty = _temp;
  // blit loop
  for (int y = 0; y < 200; ++y) {
    uint32_t *dstx = dsty;
    for (int x = 0; x < (320 / 8); ++x) {
      // get the next colour bytes from each plane
      const uint8_t b0 = plane0[x];
      const uint8_t b1 = plane1[x];
      const uint8_t b2 = plane2[x];
      const uint8_t b3 = plane3[x];
      // write 8 pixels at a time
      for (int i=0; i<8; ++i) {
        const uint8_t mask = 0x80 >> i;
        // combine bits for colour index
        const uint32_t index = ((b0 & mask) ? 1 : 0) |
                               ((b1 & mask) ? 2 : 0) |
                               ((b2 & mask) ? 4 : 0) |
                               ((b3 & mask) ? 8 : 0);

        // XXX: this palette index is not right!
        *dstx = dac[index];
        // next dest
        ++dstx;
      }
    }
    // step over the destination
    dsty += 320;
    // step the planes
    plane0 += (320 / 8);
    plane1 += (320 / 8);
    plane2 += (320 / 8);
    plane3 += (320 / 8);
  }
}

static void _neo_render_mode_10(const struct render_target_t *target) {
  //
  static const uint32_t width = 640;
  static const uint32_t height = 350;
  // vga dac palette
  const uint32_t *dac = neo_vga_dac();
  // video ram at 0xA0000
  const uint8_t *plane0 = vga_ram() + 0x10000 * 0;
  const uint8_t *plane1 = vga_ram() + 0x10000 * 1;
  const uint8_t *plane2 = vga_ram() + 0x10000 * 2;
  const uint8_t *plane3 = vga_ram() + 0x10000 * 3;
  // destination
  uint32_t *dsty = target->dst;
  const uint32_t pitch = target->pitch;
  dsty += pitch * ((target->h - height) / 2);
  // blit loop
  for (uint32_t y = 0; y < height; ++y) {
    uint32_t *dstx = dsty;
    for (uint32_t x = 0; x < (width / 8); ++x) {
      // get the next colour bytes from each plane
      const uint8_t b0 = plane0[x];
      const uint8_t b1 = plane1[x];
      const uint8_t b2 = plane2[x];
      const uint8_t b3 = plane3[x];
      // write 8 pixels at a time
      for (int i=0; i<8; ++i) {
        const uint8_t mask = 0x80 >> i;
        // combine bits for colour index
        const uint32_t index = ((b0 & mask) ? 1 : 0) |
                               ((b1 & mask) ? 2 : 0) |
                               ((b2 & mask) ? 4 : 0) |
                               ((b3 & mask) ? 8 : 0);
        *dstx = dac[index];
        // next dest
        ++dstx;
      }
    }
    // step over the destination
    dsty += pitch;
    // step the planes
    plane0 += (width / 8);
    plane1 += (width / 8);
    plane2 += (width / 8);
    plane3 += (width / 8);
  }
}

static void _neo_render_mode_13(void) {
  const uint32_t *dac = neo_vga_dac();
  // clear temp buffer
  memset(_temp, 0, 320 * 240 * 4);
  // source now is our video ram at 0xA0000
  const uint8_t *srcy = vga_ram();
  // writing to temporary buffer
  uint32_t *dst = _temp;
  // blit loop
  for (int y = 0; y < 200; ++y) {
    const uint8_t *srcx = srcy;
    for (int x = 0; x < 320; ++x) {
      dst[x] = dac[srcx[x]];
    }
    dst += 320;
    srcy += 320;
  }
}

static void _neo_render_mode_12(const struct render_target_t *target) {
  //
  static const uint32_t width = 640;
  static const uint32_t height = 480;
  // vga dac palette
  const uint32_t *dac = neo_vga_dac();
  // video ram at 0xA0000
  const uint8_t *plane0 = vga_ram() + 0x10000 * 0;
  const uint8_t *plane1 = vga_ram() + 0x10000 * 1;
  const uint8_t *plane2 = vga_ram() + 0x10000 * 2;
  const uint8_t *plane3 = vga_ram() + 0x10000 * 3;
  // destination
  uint32_t *dsty = target->dst;
  const uint32_t pitch = target->pitch;
  dsty += pitch * ((target->h - height) / 2);
  // blit loop
  for (uint32_t y = 0; y < height; ++y) {
    uint32_t *dstx = dsty;
    for (uint32_t x = 0; x < (width / 8); ++x) {
      // get the next colour bytes from each plane
      const uint8_t b0 = plane0[x];
      const uint8_t b1 = plane1[x];
      const uint8_t b2 = plane2[x];
      const uint8_t b3 = plane3[x];
      // write 8 pixels at a time
      for (int i=0; i<8; ++i) {
        const uint8_t mask = 0x80 >> i;
        // combine bits for colour index
        const uint32_t index = ((b0 & mask) ? 1 : 0) |
                               ((b1 & mask) ? 2 : 0) |
                               ((b2 & mask) ? 4 : 0) |
                               ((b3 & mask) ? 8 : 0);
        *dstx = dac[index];
        // next dest
        ++dstx;
      }
    }
    // step over the destination
    dsty += pitch;
    // step the planes
    plane0 += (width / 8);
    plane1 += (width / 8);
    plane2 += (width / 8);
    plane3 += (width / 8);
  }
}

// blit offscreen render target to screen
static void blit_2x(uint32_t w, uint32_t h, const struct render_target_t *target) {

  // TODO: check bounds

  const uint32_t pitch = target->pitch;
  uint32_t *dst = target->dst;
  // offset to centre
  if (target->h > (int32_t)(2 * h)) {
    dst += pitch * ((target->h - h * 2) / 2);
  }
  // blit loop
  const uint32_t *src = _temp;
  for (uint32_t y = 0; y < h; ++y) {
    uint32_t *dstx = dst;
    for (uint32_t x = 0; x < w; ++x) {
      const uint32_t rgb = src[x];
      dstx[      + 0] = rgb;
      dstx[      + 1] = rgb;
      dstx[pitch + 0] = rgb;
      dstx[pitch + 1] = rgb;
      dstx += 2;
    }
    dst += pitch * 2;
    src += w;
  }
}

#if 0
static void _draw_disk(const struct render_target_t *target) {
  const uint8_t *src = asset_disk_pic;
  const uint32_t pitch = target->pitch;
  uint32_t *dst = target->dst;
  dst += 8 + pitch * 8;
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const uint32_t rgb = palette_cga_3_rgb[*src];
      dst[x] = ((dst[x] >> 1) & 0x7f7f7f) | ((rgb >> 1) & 0x7f7f7f);
      ++src;
    }
    dst += pitch;
  }
}
#endif

void neo_render_tick(const struct render_target_t *target) {

  switch (neo_get_video_mode()) {
  case 0x02: _neo_render_mode_02(target); break;
  case 0x03: _neo_render_mode_03(target); break;
  case 0x04: _neo_render_mode_04(); blit_2x(320, 200, target); break;
  case 0x05: _neo_render_mode_05(); blit_2x(320, 200, target); break;
  case 0x07: _neo_render_mode_07(target); break;
  case 0x0d: _neo_render_mode_0d(); blit_2x(320, 200, target); break;
  case 0x0e: _neo_render_mode_0e(target); break;
  case 0x10: _neo_render_mode_10(target); break;
  case 0x12: _neo_render_mode_12(target); break;
  case 0x13: _neo_render_mode_13(); blit_2x(320, 200, target); break;
  default:
    _neo_render_mode_unknown(target);
    break;
  }

#if 0
  // indicate disk activity
  if (osd_should_draw_disk()) {
    _draw_disk(target);
  }
#endif
}
