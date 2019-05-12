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

// i8253 Programmable Interval Timer

#include "common.h"


struct i8253_s i8253;

static void i8253_channel_write(const int channel, uint8_t value) {
  uint8_t curbyte = 0;

  const int rl_mode = i8253.rlmode[channel];

  if ((rl_mode == PIT_RLMODE_LOBYTE) ||
     ((rl_mode == PIT_RLMODE_TOGGLE) && (i8253.bytetoggle[channel] == 0))) {
    curbyte = 0;
  } else if ((rl_mode == PIT_RLMODE_HIBYTE) ||
            ((rl_mode == PIT_RLMODE_TOGGLE) && (i8253.bytetoggle[channel] == 1))) {
    curbyte = 1;
  }

  if (curbyte == 0) {
    // update low byte
    i8253.chandata[channel] &= 0xFF00;
    i8253.chandata[channel] |= value;
  } else {
    // update high byte
    i8253.chandata[channel] &= 0x00ff;
    i8253.chandata[channel] |= ((uint16_t)value) << 8;
  }

  if (i8253.chandata[channel] == 0) {
    i8253.effectivedata[channel] = 0x10000;
  } else {
    i8253.effectivedata[channel] = i8253.chandata[channel];
  }
  i8253.active[channel] = 1;

  i8253.chanfreq[channel] =
      (float)((uint32_t)(
          ((float)1193182.0 / (float)i8253.effectivedata[channel]) *
          (float)1000.0)) /
      (float)1000.0;
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

  const int select = 0x03 & (value >> 6);
  const int mode   = 0x07 & (value >> 1);
  const int rl     = 0x03 & (value >> 4);
  const int bcd    = 0x01 & (value >> 0);

  if (select == 3) {
    // invalid counter
    return;
  }

  i8253.rlmode[select] = rl;
  if (i8253.rlmode[select] == PIT_RLMODE_TOGGLE) {
    i8253.bytetoggle[select] = 0;
  }

  i8253.mode[select] = mode;
  i8253.bcd[select] = bcd;
}

static void i8253_port_write(uint16_t portnum, uint8_t value) {
  switch (portnum & 0x03) {
  case 0:
  case 1:
  case 2: // channel data
    i8253_channel_write(portnum & 0x03, value);
    break;
  case 3: // mode/command
    i8253_mode_write(value);
    break;
  }
}

static uint8_t i8253_port_read(uint16_t portnum) {
  uint8_t curbyte;
  const int channel = portnum & 0x03;

  const int rl_mode = i8253.rlmode[channel];
  if (portnum & 0x03) {
    // no operation 3 state
    return 0;
  }

  if ((rl_mode == PIT_RLMODE_LATCHCOUNT) ||
      (rl_mode == PIT_RLMODE_LOBYTE) ||
     ((rl_mode == PIT_RLMODE_TOGGLE) && (i8253.bytetoggle[channel] == 0))) {
    curbyte = 0;
  } else if ((rl_mode == PIT_RLMODE_HIBYTE) ||
            ((rl_mode == PIT_RLMODE_TOGGLE) && (i8253.bytetoggle[channel] == 1))) {
    curbyte = 1;
  }

  if ((rl_mode == 0) ||
      (rl_mode == PIT_RLMODE_TOGGLE)) {
    i8253.bytetoggle[channel] ^= 0x01;
  }

  if (curbyte == 0) {
    // low byte
    return ((uint8_t)i8253.counter[channel]);
  } else {
    // high byte
    return ((uint8_t)(i8253.counter[channel] >> 8));
  }
}

void i8253_init() {
  memset(&i8253, 0, sizeof(i8253));
  set_port_read_redirector(0x40, 0x43, i8253_port_read);
  set_port_write_redirector(0x40, 0x43, i8253_port_write);
}

// 182Hz threshold
static const int64_t irq0_thresh = CYCLES_PER_SECOND / 182;
static int64_t irq0_accum  = 0;


#if 0
uint32_t accum = 0;
uint32_t old_time = 0;
#endif

void i8253_tick(uint64_t cycles) {

  // In the IBM PC the PIT timer is fed from a 1.1931817Mhz clock
  // Convert from the CPU cycles to PIT CLK
  const int64_t pit_cycles = (cycles * 1193182) / CYCLES_PER_SECOND;

  // TODO: Remove when channel 0 is working correctly
  // Generate an interupt every 18.2Hz on IRQ0
  irq0_accum += cycles;
  if (irq0_accum >= irq0_thresh * 10) {
    irq0_accum -= irq0_thresh * 10;
    i8259_doirq(0);
#if 0
    accum += 1;
#endif
  }

#if 0
  const uint32_t new_time = SDL_GetTicks();
  if (new_time - old_time > 1000) {
    old_time = new_time;
    printf("PIT 0: %d\n", accum);
    accum = 0;
  }
#endif

  // Channel2 generates a DREQ0 for the DMA controller?

  // Channel3 drives the speaker but is gated by the 8255-PB1

  // update timer channels
  for (int i=0; i<3; ++i) {
    switch (i8253.mode[i]) {
    case 2:
      // toggle output when the counter flips
    case 3:
      // output high and low for half cycle
    default:

      // timer will wrap
      if (pit_cycles > i8253.counter[i]) {
        // do something
      }

      i8253.counter[i] -= pit_cycles;
    }
  }
}
