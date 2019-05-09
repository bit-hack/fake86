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

static uint8_t _key_buf[256];
static uint8_t _tail, _head;

SDL_mutex *_mutex;

void i8042_key_push(uint8_t key) {
  assert(_mutex);
  SDL_LockMutex(_mutex);
#if 1
  _key_buf[0] = key;
  printf("push %d\n", (int)key);
  i8259_doirq(1);
#else
  {
    printf("h%02d t%02d  <-  %d\n", (int)_head, (int)_tail, (int)key);
    _key_buf[_head++] = key;
    // if we are at the end of the list nudge the tail
    if (_head == _tail) {
      ++_tail;
    }

    // keyboard interrupt
    i8259_doirq(1);
  }
#endif
  SDL_UnlockMutex(_mutex);
}

uint8_t i8042_key_pop() {
  assert(_mutex);
  SDL_LockMutex(_mutex);
#if 1
  const uint8_t out = _key_buf[0];
  printf("pop  %d\n", (int)out);
#else
  const uint8_t out = _key_buf[_tail];
  {
    // if we are not empty then advance
    if (_tail != _head) {
      ++_tail;
    }
    printf("h%02d t%02d  ->  %d\n", (int)_head, (int)_tail, (int)out);
  }
#endif
  SDL_UnlockMutex(_mutex);
  return out;
}

uint8_t i8042_port_read(uint16_t port) {
  // data port (scancode)
  if (port == 0x60) {
    const uint8_t data = i8042_key_pop();
    return data;
  }
  // status
  if (port == 0x64) {

    // input buffer is sent from host to keyboard
    // output buffer is sent from keyboard to host

    uint8_t status;
    // output buffer full
    status |= (_head != _tail) ? 0x01 : 0x00;
    // system flag
    status |= 0x40;
    // ?? why should this always be set
    status |= 2;

    __debugbreak();

    return status;
  }
  return 0;
}

void i8042_port_write(uint16_t port, uint8_t value) {
  // data port (scancode)
  if (port == 0x60) {
    _key_buf[_tail] = value;
    __debugbreak();
  }
  // command
  if (port == 0x64) {
    __debugbreak();
  }
}

void i8042_reset() {
#if 0
  assert(_mutex);
  SDL_LockMutex(_mutex);
  {
    _head = 0;
    _tail = 0;
  }
  SDL_UnlockMutex(_mutex);
#endif
}

void i8042_tick() {
  if (_head != _tail) {
//    i8259_doirq(1);
  }
}

void i8042_init() {
  _mutex = SDL_CreateMutex();
  assert(_mutex);
  i8042_reset();
}
