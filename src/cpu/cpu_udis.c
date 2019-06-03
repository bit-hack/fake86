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

#include "../common/common.h"
#include "../external/udis86/udis86.h"

void cpu_udis_exec(const uint8_t *stream) {
#if 0
  ud_t ud_obj;
  ud_init(&ud_obj);
  ud_set_input_buffer(&ud_obj, stream, 16);
  ud_set_mode(&ud_obj, 16);
  ud_set_syntax(&ud_obj, UD_SYN_INTEL);

  if (!ud_disassemble(&ud_obj)) {
    printf("bad disassembly!");
    UNREACHABLE();
  }

  printf("\t%s\n", ud_insn_asm(&ud_obj));
#endif
}
