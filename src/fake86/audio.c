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
#include "../80x86/cpu.h"


// audio enabled
bool audio_enable;

// audio sample rate
static uint32_t _sample_rate;
// cpu cycles per sample
static uint32_t _cycles_per_sample;

// event buffer mutex
static SDL_mutex *_audio_mux;

// pc speaker oscillator
static uint32_t _spk_accum;
static uint32_t _spk_delta;
static bool     _spk_enable;
static uint32_t _spk_freq;


// type of audio event
enum audio_event_type_t {
  // none, but must render cycle_delta cycles
  event_none,
  // pc speaker has updated
  event_speaker,
};

// pc speaker update
struct audio_event_speaker_t {
  bool enable;
  uint16_t freq;
};

// audio event ring buffer
struct audio_event_t {
  // cycles since last delta
  uint32_t cycle_delta;
  //
  uint8_t type;
  //
  union {
    struct audio_event_speaker_t spk;
  };
};

// last tick delta sent
static uint64_t _last_update;


static void _push_event(const struct audio_event_t *event) {
}

void audio_init(uint32_t rate) {
  // we shouldnt initalize otherwise
  assert(audio_enable);

  // create audio event stream mutex
  _audio_mux = SDL_CreateMutex();
  assert(_audio_mux);

  _sample_rate = rate;
  _cycles_per_sample = CYCLES_PER_SECOND / rate;
}

void audio_close(void) {
  if (_audio_mux) {
    SDL_DestroyMutex(_audio_mux);
  }
}

uint32_t audio_callback(int16_t *samples, uint32_t num_samples) {

  // two channels interleaved
  memset(samples, 0, num_samples * 2);

  if (_sample_rate == 0) {
    return num_samples;
  }

  // avoid aliasing and rumble
  if (_spk_freq <= 10 || _spk_freq >= 18000) {
    return num_samples;
  }

  if (_spk_enable) {
    for (uint32_t i = 0; i < num_samples; i += 2) {

      const int16_t out = (_spk_accum & 0x80000000) ? -0x7000 : 0x7000;
      _spk_accum += _spk_delta;

      samples[i + 0] = out;
      samples[i + 1] = out;
    }
  }

  return num_samples;
}

void _push_event_spk(void) {
  struct audio_event_t event;
  const uint64_t new_update = cpu_slice_ticks();
  event.cycle_delta = new_update - _last_update;
  _last_update = new_update;
  event.type = event_speaker;
  event.spk.freq = _spk_freq;
  event.spk.enable = _spk_enable;
}

void audio_pc_speaker_freq(const uint32_t freq) {
  _spk_freq = freq;
  _push_event_spk();

  if (_sample_rate == 0) {
    return;
  }

  // set per sample oscillator delta
  _spk_delta = ((uint64_t)freq * (uint64_t)0xffffffff / (uint64_t)_sample_rate);
}

void audio_pc_speaker_enable(bool enable) {
  _spk_enable = enable;
  _push_event_spk();
}

void audio_tick(const uint64_t cycles) {
  if (!audio_enable) {
    return;
  }

  assert(_last_update <= cycles);

  struct audio_event_t event;
  event.cycle_delta = cycles - _last_update;
  event.type = event_none;

  // check if its worth sending one out
  if (event.cycle_delta) {
    _push_event(&event);
  }

  _last_update = 0;
}
