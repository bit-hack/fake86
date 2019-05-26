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

// event ringbuffer
#define RING_SIZE 1024
static struct audio_event_t _ring_data[RING_SIZE];
static uint32_t _ring_head, _ring_tail;

#define RING_GET(INDEX) _ring_data[(INDEX) & (RING_SIZE-1)]

static bool _pop_event(struct audio_event_t *out) {
  assert(_audio_mux && out);
  SDL_mutexP(_audio_mux);
  // if ring is empty
  if (&RING_GET(_ring_tail) == &RING_GET(_ring_head)) {
    SDL_mutexV(_audio_mux);
    return false;
  }
  // recv event
  const struct audio_event_t *e = &RING_GET(_ring_tail++);
  memcpy(out, e, sizeof(struct audio_event_t));
  // success
  SDL_mutexV(_audio_mux);
  return true;
}

static bool _push_event(const struct audio_event_t *event) {
  assert(_audio_mux && event);
  SDL_mutexP(_audio_mux);
  // if ring is full
  if (&RING_GET(_ring_head + 1) == &RING_GET(_ring_tail)) {
    SDL_mutexV(_audio_mux);
    return false;
  }
  // send event
  struct audio_event_t *e = &RING_GET(_ring_head++);
  memcpy(e, event, sizeof(struct audio_event_t));
  // success
  SDL_mutexV(_audio_mux);
  return true;
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


#if 1
static int32_t _pending_samples;

static bool _at_spk_enable;
static uint32_t _at_spk_freq;
static uint32_t _at_spk_delta;
static uint32_t _at_spk_accum;

static uint32_t _at_adjust = 1000;

uint32_t cycles_to_samples(uint32_t cycles) {
  const uint32_t todo = (cycles * _sample_rate) / CYCLES_PER_SECOND;
  return (todo * _at_adjust) / 1000;
}

static void _next_event(void) {
  struct audio_event_t event;
  if (!_pop_event(&event)) {
    return;
  }

  _pending_samples += cycles_to_samples(event.cycle_delta);

  switch (event.type) {
  case event_speaker:
    _at_spk_enable = event.spk.enable;
    _at_spk_freq = event.spk.freq;
    _at_spk_delta = ((uint64_t)_at_spk_freq * (uint64_t)0xffffffff / _sample_rate);
    break;
  }
}

static uint32_t last_eval = 0;

void adjust_rate() {
  SDL_mutexP(_audio_mux);

  uint64_t accum = 0;
  uint32_t i = _ring_tail;
  for (;&RING_GET(i) != &RING_GET(_ring_head); ++i) {
    accum += RING_GET(i).cycle_delta;
  }

  uint32_t samples = cycles_to_samples(accum);
  if (samples >= _sample_rate / 100) {
    _at_adjust--;
  }
  if (samples <= _sample_rate / 100) {
    _at_adjust++;
  }

  if (_at_adjust > 1100) {
    _at_adjust = 1100;
  }
  if (_at_adjust < 900) {
    _at_adjust = 900;
  }

  SDL_mutexV(_audio_mux);
}

uint32_t audio_callback(int16_t *samples, uint32_t num_samples) {

  const uint32_t next_eval = SDL_GetTicks();
  if ((next_eval - last_eval) > 10) {
    adjust_rate();
    last_eval = next_eval;
  }

  // if there are no more samples to render
  if (_pending_samples == 0) {
    // get the next event
    _next_event();
  }
  // number of samples we should render
  const uint32_t to_do = SDL_min(_pending_samples * 2, num_samples);
  _pending_samples -= to_do / 2;

  if (_at_spk_enable) {
    for (uint32_t i = 0; i < to_do; i += 2) {
      const int16_t out = (_at_spk_accum & 0x80000000) ? -0x7000 : 0x7000;
      _at_spk_accum += _at_spk_delta;
      // fill left and right
      samples[i + 0] = out;
      samples[i + 1] = out;
    }
  }
  else {
    for (uint32_t i = 0; i < to_do; i += 2) {
      samples[i + 0] = 0;
      samples[i + 1] = 0;
    }
  }

  return to_do;
}
#else
uint32_t audio_callback(int16_t *samples, uint32_t num_samples) {
  // two channels interleaved
  memset(samples, 0, num_samples * 2);
  // sample rate may not be set
  if (_sample_rate == 0) {
    return num_samples;
  }
  // avoid aliasing and rumble
  if (_spk_freq <= 10 || _spk_freq >= 18000) {
    return num_samples;
  }
  // render the oscillator
  if (_spk_enable) {
    for (uint32_t i = 0; i < num_samples; i += 2) {
      const int16_t out = (_spk_accum & 0x80000000) ? -0x7000 : 0x7000;
      _spk_accum += _spk_delta;
      // fill left and right
      samples[i + 0] = out;
      samples[i + 1] = out;
    }
  }
  return num_samples;
}
#endif

void _push_event_spk(void) {
  struct audio_event_t event;
  const uint64_t new_update = cpu_slice_ticks();
  event.cycle_delta = new_update - _last_update;
  _last_update = new_update;
  event.type = event_speaker;
  event.spk.freq = _spk_freq;
  event.spk.enable = _spk_enable;
  if (!_push_event(&event)) {
    __debugbreak();
  }
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
