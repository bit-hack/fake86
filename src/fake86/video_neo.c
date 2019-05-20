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

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

uint8_t neo_mem_read_A0000(uint32_t addr32) {
  return 0;
}

void neo_mem_write_A0000(uint32_t addr, uint8_t value) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// MDA
uint8_t neo_mem_read_B0000(uint32_t addr32) {
  return 0;
}

// MDA
void neo_mem_write_B0000(uint32_t addr, uint8_t value) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// CGA
uint8_t neo_mem_read_B8000(uint32_t addr32) {
  return 0;
}

// CGA
void neo_mem_write_B8000(uint32_t addr, uint8_t value) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// EGA
uint8_t neo_mem_read_C0000(uint32_t addr32) {
  return 0;
}

// EGA
void neo_mem_write_C0000(uint32_t addr, uint8_t value) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// Professional Graphics
uint8_t neo_mem_read_C6000(uint32_t addr32) {
  return 0;
}

// Professional Graphics
void neo_mem_write_C6000(uint32_t addr, uint8_t value) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// 8bit port read
static uint8_t neo_port_read(uint16_t portnum) {
  return 0;
}

// 8bit port write
static void neo_port_write(uint16_t portnum, uint8_t value) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// initalize neo display adapter
bool neo_init(void) {
  return true;
}
// update neo display adapter
void neo_tick(uint64_t cycles) {
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// BIOS int 10h Video Services handler
void neo_int10_handler() {
}
