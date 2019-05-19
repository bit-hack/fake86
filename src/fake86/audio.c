/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2012 Mike Chambers

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

#include "common.h"


static uint32_t sample_rate = 0;
static uint32_t accum = 0;
static uint32_t delta = 0;

void audio_init(uint32_t rate) {
  sample_rate = rate;
}

void audio_close(void) {
}

uint32_t audio_callback(int16_t *samples, uint32_t num_samples) {

  // two channels interleaved
  memset(samples, 0, num_samples * 2);

  if (sample_rate == 0) {
    return num_samples;
  }

  if (!i8255_speaker_on()) {
    return num_samples;
  }

  const uint32_t freq = i8253_frequency(2);

  uint32_t delta = ((uint64_t)freq * (uint64_t)0xffffffff / (uint64_t)sample_rate);

  for (uint32_t i = 0; i < num_samples; i += 2) {

    const int16_t out = (accum & 0x80000000) ? -0x7000 : 0x7000;
    accum += delta;

    samples[i + 0] = out;
    samples[i + 1] = out;
  }

  return num_samples;
}
