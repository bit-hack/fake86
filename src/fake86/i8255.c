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

// i8255 Peripheral Interface Adapter

#include "../common/common.h"


// PORTA
//
//   Keyboard scancode
// or
//   0 IPL 5.25 Diskette drive
//   1 reserved
//   2 Sys. Brd. R/W memory size*
//   3 Sys. Brd. R/W memory size*
//   4 +Display type**
//   5 +Display type**
//   6 No. of 5.25 drives
//   7 No. of 5.25 drives
//
//  * 00 16k    01 32k    10 48k    11 64k
// ** 00 Reserved
// ** 01 40x25 cga
// ** 10 80x25 cga
// ** 11 MDA

// PORTB
//
//   0 Speaker Gate
//   1 Speaker Data
//   2 Read R/W memory size or SW2-5
//   3 Cassette motor Off
//   4 Enable R/W memory
//   5 Enable I/O CH CK
//   6 Hold KBD CLK Low
//   7 Enable KBD or Enable Sense SW (PortA)

// PORTC
//
//   0 SW2-1 or SW2-5 (PORTB-2)
//   1 SW2-2
//   2 SW2-3
//   3 SW2-4
//   4 Cassette Data In
//   5 Timer Channel 2 Out
//   6 I/O Channel Check
//   7 R/W Memory Pck

// Motherboard switch 2
//   bit 7 = 1  RAM parity check
//   bit 6 = 1  I/O channel check
//   bit 5 = 1  timer 2 channel out
//   bit 4      reserved
//   bit 3 = 1  system board RAM size type 1
//   bit 2 = 1  system board RAM size type 2
//   bit 1 = 1  coprocessor installed
//   bit 0 = 1  loop in POST

struct i8255_t i8255;

static uint8_t _SW1 = (3 << 2) | (3 << 4);
static uint8_t _SW2 = 0x0C;


static uint8_t _keys[256];
static uint8_t _key_index = 0;

void i8255_key_push(uint8_t key) {
#if 0
  printf("pushing key %d\n", key);
#endif
  if (USE_KEY_BUFFER) {
    if (_key_index >= sizeof(_keys)) {
      // key buffer overflow
      __debugbreak();
    }
    // push key into the buffer
    _keys[_key_index] = key;
    // advance _key_index
    _key_index += (_key_index < sizeof(_keys));
  }
  else {
    i8255.port_in[0] = key;
  }
}

static void write_ctrl_word(uint8_t value) {

  const int mode_set    = 1 & (value >> 7);
  const int mode        = 3 & (value >> 5);
  const int io_porta    = 1 & (value >> 4);
  const int io_portc_hi = 1 & (value >> 3);
  const int mode_portb  = 1 & (value >> 2);
  const int io_portb    = 1 & (value >> 1);
  const int io_portc_lo = 1 & (value >> 0);

  i8255.ctrl_word = value;
}

static void i8255_port_write(uint16_t port, uint8_t value) {
  switch (port) {
  // PORTA
  case 0x60:
    if ((i8255.port_out[1] & 0x80) == 0) {
      // write to keyboard (may only be possible on AT)
      // i8255.port_out[port & 0x3] = value;
    } else {
      // write to switches?
    }
  // PORTB
  case 0x61:
    i8255.port_out[port & 0x3] = value;
    // update audio
    if ((port & 0x3) == 1) {
      audio_pc_speaker_enable(value & 1);
    }
    break;
  // PORTC
  case 0x62:
    if (value & 0x80) {
      const uint8_t bit = 1 << ((value >> 1) & 0x0f);
      if (value & 0x01) {
        i8255.port_out[2] &= ~bit;
      } else {
        i8255.port_out[2] |= bit;
      }
    } else {
      i8255.port_out[2] &= 0xf0;
      i8255.port_out[2] |= value & 0x0f;
    }
  case 0x63:
    i8255.ctrl_word = value;
    break;
  }
}

static uint8_t i8255_port_read(uint16_t port) {
  switch (port) {
  case 0x60:
    if ((i8255.port_out[1] & 0x80) == 0) {
      // return keyboard scan code
      return i8255.ctrl_word & 0x10 ? i8255.port_in[0] : i8255.port_out[0];
    } else {
      // return switch 1
      return _SW1;
    }
  case 0x61:
    return i8255.ctrl_word & 0x02 ? i8255.port_in[1] : i8255.port_out[1];
#if 0
  case 0x62:
  {
    uint8_t out = 0;
    out |= (i8255.ctrl_word & 0x01 ?
        i8255.port_in[3]:
        i8255.port_out[3]);
    out |= (i8255.ctrl_word & 0x08 ?
        i8255.port_in[3]:
        i8255.port_out[3]) & 0xf0;
    return out;
  }
  case 0x63:
    return i8255.ctrl_word;
#else
  case 0x62:
    if (i8255.port_out[1] & 0x2) {
      uint8_t out = 0;
      // return SW2 bits 1-4
      out |= _SW2 & 0x0f;
      // Timer2 channel out
      out |= i8253_channel2_out() << 5;
    } else {
      // return SW2 bit 5
#if 1
      return (_SW2 >> 5) & 1;
#endif
    }
  case 0x63:
    // cmd/mode register
    return 0x99;
#endif
  }
  return 0;
}

bool i8255_init(void) {
  i8255_reset();
  set_port_write_redirector(0x60, 0x63, i8255_port_write);
  set_port_read_redirector(0x60, 0x63, i8255_port_read);
  return true;
}

void i8255_reset(void) {
  memset(&i8255, 0, sizeof(i8255));
  // PORTA is input
  i8255.ctrl_word |= 0x10;
}

void i8255_tick(uint64_t cycles) {
  (void)cycles;
}

bool i8255_speaker_on(void) {
  return i8255.port_out[1] & 0x1;
}

// called just prior to servicing an int9
void i8255_key_required(void) {
  if (USE_KEY_BUFFER) {
    // pull out the next item from the key buffer
    i8255.port_in[0] = _keys[0];
    // shift the key buffer down
    for (int i = 0; i < _key_index; ++i) {
      _keys[i] = _keys[i + 1];
    }
    // decrement key index
    _key_index -= (_key_index > 0);
  }
#if 0
  printf("popping key %d\n", i8255.port_in[0]);
#endif
}

void i8255_state_save(FILE *fd) {
  fwrite(&i8255, 1, sizeof(i8255), fd);
  fwrite(&_SW1, 1, sizeof(_SW1), fd);
  fwrite(&_SW2, 1, sizeof(_SW2), fd);
  fwrite(_keys, 1, sizeof(_keys), fd);
  fwrite(&_key_index, 1, sizeof(_key_index), fd);
}

void i8255_state_load(FILE *fd) {
  fread(&i8255, 1, sizeof(i8255), fd);
  fread(&_SW1, 1, sizeof(_SW1), fd);
  fread(&_SW2, 1, sizeof(_SW2), fd);
  fread(_keys, 1, sizeof(_keys), fd);
  fread(&_key_index, 1, sizeof(_key_index), fd);
}
