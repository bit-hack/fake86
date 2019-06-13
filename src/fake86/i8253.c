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

// i8253 Programmable Interval Timer

// NOTE: it seems now like we should buffer the cycles and compute the timer
//       state on the fly as needed.

#include "../common/common.h"
#include "../cpu/cpu.h"


static void _i8253_tick_update(void);

#define DEVELOPER 0

enum {
  PIT_RLMODE_LATCH = 0,
  PIT_RLMODE_LOBYTE = 1,
  PIT_RLMODE_HIBYTE = 2,
  PIT_RLMODE_TOGGLE = 3,
};

struct i8253_channel_t {
  // reload value
  uint16_t rvalue;
  // counter value
  uint16_t counter;
  // number of writes needed
  uint8_t inhibit_count;
  // binary coded decimal mode
  bool bcd;
  // register read/write access mode
  uint8_t mode_access;
  //
  bool toggle_access;
  // timer operation mode
  uint8_t mode_op;
  // latched value output
  uint16_t latch_out;
  // output is currently active
  bool output_active;
  // current output state
  uint8_t output;
  // effective output frequency
  uint32_t frequency;
};

struct i8253_s {
  struct i8253_channel_t channel[3];
  uint8_t control;
  // cycle fraction since last tick
  int64_t left_over;
  // 
  uint64_t last_ticks;
};

struct i8253_s i8253;

// In the IBM PC the PIT timer is fed from a 1.1931817Mhz clock
const uint64_t pit_speed = 1193182;

uint32_t i8253_frequency(int channel) {
  switch (channel) {
  case 1:
  case 2:
  case 3:
    return i8253.channel[channel].frequency;
  default:
    return 0;
  }
}

uint8_t i8253_channel2_out(void) {
  return i8253.channel[2].output;
}

static void i8253_reload(struct i8253_channel_t *c) {
  c->counter = (c->rvalue == 0 ? 0xffff : c->rvalue);
}

static void i8253_channel_write(int channel,
                                struct i8253_channel_t *c,
                                uint8_t value) {

  if (c->inhibit_count > 0) {
    --c->inhibit_count;
  }

  switch (c->mode_access) {
  case PIT_RLMODE_LATCH:
    assert(!"This would be weird");
    break;
  case PIT_RLMODE_LOBYTE:
    c->rvalue &= 0xFF00;
    c->rvalue |= value;
    break;
  case PIT_RLMODE_HIBYTE:
    c->rvalue &= 0x00FF;
    c->rvalue |= (uint16_t)value << 8;
    break;
  case PIT_RLMODE_TOGGLE:
    UNREACHABLE();
  }

  if (c->toggle_access) {
    c->mode_access ^= 0x3;
  }

  i8253_reload(c);

  // calculate frequency in Hz
  c->frequency = 1193182 / (c->rvalue == 0 ? 0xffff : c->rvalue);

#if 0
  // update pit hardware
  if (channel == 0) {
    cpu_preempt();
  }
#endif

  // update audio
  if (channel == 2) {
    audio_pc_speaker_freq(c->frequency);
  }

#if DEVELOPER
  printf("chan %d freq %d\n", channel, c->frequency);
#endif

  // modes where the timer starts immediately
  switch (c->mode_op) {
  case 0: // interupt on terminal count
  case 2: // rate generator
  case 3: // square wave generator
    c->output_active = (c->inhibit_count == 0);
  }
}

static void i8253_mode_write(uint8_t value) {

  // Control byte
  // 7   6   5   4   3   2   1   0
  // SC1 SC0 RL1 RL0 M2  M1  M0  BCD

  // SC1  SC0
  // 0    0         Select Counter 0
  // 0    1         Select Counter 1
  // 1    0         Select counter 2
  // 1    1         Illegal

  // RL1  RL0
  // 0    0         Counter Latching operation
  // 0    1         Read/Load LSB only
  // 1    0         Read/Load MSB only
  // 1    1         Read/Load LSB first then MSB

  // M2   M1   M0
  // 0    0    0    Mode 0    (interrupt on terminal count)
  // 0    0    1    Mode 1    (one shot)
  // X    1    0    Mode 2    (rate generator)
  // X    1    1    Mode 3    (square wave rate generator)
  // 1    0    0    Mode 4    (software triggered strobe)
  // 1    0    1    Mode 5    (hardware triggered strobe)

  // BCD
  // 0              Binary Counter 16bits
  // 1              Binary Coded Decimal Counter (4 decades)

  // save the control word
  i8253.control = value;

  // decode control word
  const int select = 0x03 & (value >> 6);
  const int mode   = 0x07 & (value >> 1);
  const int rl     = 0x03 & (value >> 4);
  const int bcd    = 0x01 & (value >> 0);

#if DEVELOPER
  printf("sel:%d mode:%d, rl:%d bcd:%d\n", select, mode, rl, bcd);
#endif

  // skip invalid counter
  if (select == 3) {
    // invalid counter
    return;
  }

  // save counter data
  struct i8253_channel_t *c = &(i8253.channel[select]);

  if (mode == PIT_RLMODE_LATCH) {
    c->latch_out = c->counter;
    return;
  }

#if 0
  // if channel 0 then update pit hardware
  if (select == 0) {
    cpu_preempt();
  }
#endif

  c->output = 0;
  c->toggle_access = (rl == PIT_RLMODE_TOGGLE);
  c->bcd = bcd;
  c->mode_access = (c->toggle_access ? PIT_RLMODE_LOBYTE : rl);
  c->mode_op = mode;
  // number of writes needed before timer is active again
  c->inhibit_count = (rl == PIT_RLMODE_LATCH)  ? 0 : (
                     (rl == PIT_RLMODE_TOGGLE) ? 2 : 1);
}

// port write
static void i8253_port_write(uint16_t portnum, uint8_t value) {

  // update the timers
  _i8253_tick_update();

#if DEVELOPER
  printf("%02x <- %02x\n", portnum, value);
#endif

  struct i8253_channel_t *c = &i8253.channel[portnum & 3];
  switch (portnum & 0x03) {
  case 0:
  case 1:
  case 2: // channel data
    i8253_channel_write(portnum & 3, c, value);
    break;
  case 3: // mode/command
    i8253_mode_write(value);
    break;
  }
}

// port read
static uint8_t i8253_port_read(uint16_t portnum) {

  // update the timers
  _i8253_tick_update();

#if DEVELOPER
  printf("%02x -> \n", portnum);
#endif

  const int channel = portnum & 0x03;
  if (channel == 3) {
    // no operation 3 state
    return 0;
  }

#if 0
  // tick the PIT timer hardware
  if (channel == 0) {
    cpu_preempt();
  }
#endif

  struct i8253_channel_t *c = &i8253.channel[channel];

  uint8_t out = 0;

  switch (c->mode_access) {
  case PIT_RLMODE_LATCH:
    // get low byte
    out = c->latch_out & 0x00ff;
    // shift down
    c->latch_out >>= 8;
    break;
  case PIT_RLMODE_LOBYTE:
    out = c->counter & 0x00ff;
    break;
  case PIT_RLMODE_HIBYTE:
    out = c->counter >> 8;
    break;
  case PIT_RLMODE_TOGGLE:
    assert(false);
    break;
  }

  if (c->toggle_access) {
    c->mode_access ^= 0x3;
  }

  return out;
}

void i8253_init() {
  memset(&i8253, 0, sizeof(i8253));
  set_port_read_redirector(0x40, 0x43, i8253_port_read);
  set_port_write_redirector(0x40, 0x43, i8253_port_write);
}

static void update_mode_0(struct i8253_channel_t *c, uint32_t cycles) {
  if (c->inhibit_count > 0) {
    return;
  }
  const bool is_chan_0 = (c == &i8253.channel[0]);
  // if counter will wrap
  if (cycles >= c->counter) {
    // if channel 0
    if (is_chan_0 && c->output == 0) {
      c->output = 1;
      // move to the 0 counter state
      cycles -= c->counter;
      c->counter = 0;
      i8259_doirq(0);
    }
  }
  c->counter -= cycles;
}

static void update_mode_1(struct i8253_channel_t *c, uint32_t cycles) {
  // TODO: Wait for posedge of gate signal
  assert(false);
}

// rate generator
static void update_mode_2(struct i8253_channel_t *c, uint32_t cycles) {
  if (c->inhibit_count > 0) {
    return;
  }
  const bool is_chan_0 = (c == &i8253.channel[0]);
  while (cycles) {
    // if counter will wrap
    if (cycles >= c->counter) {
      // reset counter
      cycles -= c->counter;
      // if channel 0
      if (is_chan_0 && c->output_active) {
        c->counter = 2;
        i8259_doirq(0);
      }
      i8253_reload(c);
    } else {
      c->counter -= cycles;
      cycles = 0;
    }
  }
  // on for the last two values
  c->output = (c->counter <= 2);
}

// square wave generator
static void update_mode_3(struct i8253_channel_t *c, uint32_t cycles) {
  if (c->inhibit_count > 0) {
    return;
  }
  // cycles twice as fast
  cycles *= 2;
  const bool is_chan_0 = (c == &i8253.channel[0]);
  while (cycles) {
    // if counter will wrap
    if (cycles >= c->counter) {
      // reset counter
      cycles -= c->counter;
      i8253_reload(c);
      // if channel 0
      if (is_chan_0 && c->output_active && !c->output) {
        i8259_doirq(0);
      }
      c->output ^= 1;
    } else {
      c->counter -= cycles;
      cycles = 0;
    }
  }
}

static void update_mode_4(struct i8253_channel_t *c, uint32_t cycles) {
  assert(false);
}

static void update_mode_5(struct i8253_channel_t *c, uint32_t cycles) {
  assert(false);
}

static void _i8253_tick_partial(uint64_t cycles) {

  // Channel2 generates a DREQ0 for the DMA controller?

  // Channel3 drives the speaker but is gated by the 8255-PB1

  // Convert from the CPU cycles to PIT CLK
  // note: we keep track of the error for the next cycles this process
  const int64_t in_cycles = cycles + i8253.left_over;
  const int64_t pit_cycles = (in_cycles * pit_speed) / CYCLES_PER_SECOND;
  // convert back and track the error
  const int64_t will_do = (pit_cycles * CYCLES_PER_SECOND) / pit_speed;
  i8253.left_over = in_cycles - will_do;

  // update timer channels
  for (int i = 0; i < 3; ++i) {

    struct i8253_channel_t *c = &i8253.channel[i];

    switch (c->mode_op) {
    case 0: update_mode_0(c, (uint32_t)pit_cycles); break;
    case 1: update_mode_1(c, (uint32_t)pit_cycles); break;
    case 2: update_mode_2(c, (uint32_t)pit_cycles); break;
    case 3: update_mode_3(c, (uint32_t)pit_cycles); break;
    case 4: update_mode_4(c, (uint32_t)pit_cycles); break;
    case 5: update_mode_5(c, (uint32_t)pit_cycles); break;
    case 6: update_mode_2(c, (uint32_t)pit_cycles); break;
    case 7: update_mode_3(c, (uint32_t)pit_cycles); break;
    }
  }
}

// update the timer between slices
static void _i8253_tick_update(void) {
  const uint64_t slice = cpu_slice_ticks();
  if (slice == 0x7531) {
    __debugbreak();
  }
  assert(slice >= i8253.last_ticks);
  const uint64_t diff = slice - i8253.last_ticks;
  if (diff > 0) {
    _i8253_tick_partial(diff);
  }
  i8253.last_ticks = slice;
}

// new slice, update timer
void i8253_tick(uint64_t cycles) {
  assert(i8253.last_ticks <= cycles);
  const uint64_t remain = cycles - i8253.last_ticks;
  if (remain > 0) {
    _i8253_tick_partial(remain);
  }
  i8253.last_ticks = 0;
}

int64_t i8253_cycles_before_irq(void) {

  _i8253_tick_update();

  const struct i8253_channel_t *c0 = &i8253.channel[0];
  if (c0->rvalue == 0) {
    return 0xffffff;
  }
  if (!c0->output_active) {
    return 0xffffff;
  }
  // In the IBM PC the PIT timer is fed from a 1.1931817Mhz clock
  const int64_t cycles =
    (((uint64_t)c0->counter * CYCLES_PER_SECOND) / pit_speed);
  return cycles - i8253.left_over;
}

void i8253_state_save(FILE *fd) {
  fwrite(&i8253, 1, sizeof(i8253), fd);
}

void i8253_state_load(FILE *fd) {
  fread(&i8253, 1, sizeof(i8253), fd);
}
