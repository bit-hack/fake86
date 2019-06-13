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

#include "../common/common.h"
#include "../cpu/cpu.h"


static void _on_dos_write_stdout(void) {
  const uint32_t offset = (cpu_regs.ds << 4) + cpu_regs.dx;
  char *str = (char*)(RAM + offset);
  char *term = str;
  for (;*term != '$'; ++term);
  *term = '\0';
  log_printf(LOG_CHAN_DOS, "stdout: '%s'", str);
  *term = '$';
}

static void _on_dos_load_exec(void) {
  const uint32_t offset = (cpu_regs.ds << 4) + cpu_regs.dx;
  const char *name = (const char*)(RAM + offset);
  log_printf(LOG_CHAN_DOS, "load  and execute '%s'", name);
}

static void _on_dos_create_file(void) {
  const uint32_t offset = (cpu_regs.ds << 4) + cpu_regs.dx;
  const char *name = (const char*)(RAM + offset);
  log_printf(LOG_CHAN_DOS, "create file '%s'", name);
}

static void _on_dos_open_file(void) {
  const uint32_t offset = (cpu_regs.ds << 4) + cpu_regs.dx;
  const char *name = (const char*)(RAM + offset);
  log_printf(LOG_CHAN_DOS, "open file '%s'", name);
}

static void _on_dos_close_file(void) {
#if 0
  log_printf(LOG_CHAN_DOS, "close file '%d'", cpu_regs.bx);
#endif
}

bool on_dos_int(void) {
  switch (cpu_regs.ah) {
  case 0x09:  // Write stdout
    _on_dos_write_stdout();
    break;
  case 0x3C:  // Create File
    _on_dos_create_file();
    break;
  case 0x3D:  // Open File
    _on_dos_open_file();
    break;
  case 0x3E:  // Close File
    _on_dos_close_file();
    break;
  case 0x4B:  // EXEC
    if (cpu_regs.al == 0) {
      _on_dos_load_exec();
    }
    break;
  }
  // interupt not handled
  return false;
}
