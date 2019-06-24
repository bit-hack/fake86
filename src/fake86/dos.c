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
#if 1
  log_printf(LOG_CHAN_DOS, "close file '%d'", cpu_regs.bx);
#endif
}

static const char *_get_int_desc(int num) {
  switch (num) {
  case 0x0:   return "Program terminate";
  case 0x1:   return "Keyboard input with echo";
  case 0x2:   return "Display output";
  case 0x3:   return "Wait for auxiliary device input";
  case 0x4:   return "Auxiliary output";
  case 0x5:   return "Printer output";
  case 0x6:   return "Direct console I/O";
  case 0x7:   return "Wait for direct console input without echo";
  case 0x8:   return "Wait for console input without echo";
  case 0x9:   return "Print string";
  case 0xA:   return "Buffered keyboard input";
  case 0xB:   return "Check standard input status";
  case 0xC:   return "Clear keyboard buffer, invoke keyboard function";
  case 0xD:   return "Disk reset";
  case 0xE:   return "Select disk";
  case 0xF:   return "Open file using FCB";
  case 0x10:  return "Close file using FCB";
  case 0x11:  return "Search for first entry using FCB";
  case 0x12:  return "Search for next entry using FCB";
  case 0x13:  return "Delete file using FCB";
  case 0x14:  return "Sequential read using FCB";
  case 0x15:  return "Sequential write using FCB";
  case 0x16:  return "Create a file using FCB";
  case 0x17:  return "Rename file using FCB";
  case 0x18:  return "DOS dummy function (CP/M) (not used/listed)";
  case 0x19:  return "Get current default drive";
  case 0x1A:  return "Set disk transfer address";
  case 0x1B:  return "Get allocation table information";
  case 0x1C:  return "Get allocation table info for specific device";
  case 0x1D:  return "DOS dummy function (CP/M) (not used/listed)";
  case 0x1E:  return "DOS dummy function (CP/M) (not used/listed)";
  case 0x1F:  return "Get pointer to default drive parameter table (undocumented)";
  case 0x20:  return "DOS dummy function (CP/M) (not used/listed)";
  case 0x21:  return "Random read using FCB";
  case 0x22:  return "Random write using FCB";
  case 0x23:  return "Get file size using FCB";
  case 0x24:  return "Set relative record field for FCB";
  case 0x25:  return "Set interrupt vector";
  case 0x26:  return "Create new program segment";
  case 0x27:  return "Random block read using FCB";
  case 0x28:  return "Random block write using FCB";
  case 0x29:  return "Parse filename for FCB";
  case 0x2A:  return "Get date";
  case 0x2B:  return "Set date";
  case 0x2C:  return "Get time";
  case 0x2D:  return "Set time";
  case 0x2E:  return "Set/reset verify switch";
  case 0x2F:  return "Get disk transfer address";
  case 0x30:  return "Get DOS version number";
  case 0x31:  return "Terminate process and remain resident";
  case 0x32:  return "Get pointer to drive parameter table (undocumented)";
  case 0x33:  return "Get/set Ctrl-Break check state & get boot drive";
  case 0x34:  return "Get address to DOS critical flag (undocumented)";
  case 0x35:  return "Get vector";
  case 0x36:  return "Get disk free space";
  case 0x37:  return "Get/set switch character (undocumented)";
  case 0x38:  return "Get/set country dependent information";
  case 0x39:  return "Create subdirectory (mkdir)";
  case 0x3A:  return "Remove subdirectory (rmdir)";
  case 0x3B:  return "Change current subdirectory (chdir)";
  case 0x3C:  return "Create file using handle";
  case 0x3D:  return "Open file using handle";
  case 0x3E:  return "Close file using handle";
  case 0x3F:  return "Read file or device using handle";
  case 0x40:  return "Write file or device using handle";
  case 0x41:  return "Delete file";
  case 0x42:  return "Move file pointer using handle";
  case 0x43:  return "Change file mode";
  case 0x44:  return "I/O control for devices (IOCTL)";
  case 0x45:  return "Duplicate file handle";
  case 0x46:  return "Force duplicate file handle";
  case 0x47:  return "Get current directory";
  case 0x48:  return "Allocate memory blocks";
  case 0x49:  return "Free allocated memory blocks";
  case 0x4A:  return "Modify allocated memory blocks";
  case 0x4B:  return "EXEC load and execute program (func 1 undocumented)";
  case 0x4C:  return "Terminate process with return code";
  case 0x4D:  return "Get return code of a sub-process";
  case 0x4E:  return "Find first matching file";
  case 0x4F:  return "Find next matching file";
  case 0x50:  return "Set current process id (undocumented)";
  case 0x51:  return "Get current process id (undocumented)";
  case 0x52:  return "Get pointer to DOS 'INVARS' (undocumented)";
  case 0x53:  return "Generate drive parameter table (undocumented)";
  case 0x54:  return "Get verify setting";
  case 0x55:  return "Create PSP (undocumented)";
  case 0x56:  return "Rename file";
  case 0x57:  return "Get/set file date and time using handle";
  case 0x58:  return "Get/set memory allocation strategy (3.x+, undocumented)";
  case 0x59:  return "Get extended error information (3.x+)";
  case 0x5A:  return "Create temporary file (3.x+)";
  case 0x5B:  return "Create new file (3.x+)";
  case 0x5C:  return "Lock/unlock file access (3.x+)";
  case 0x5D:  return "Critical error information (undocumented 3.x+)";
  case 0x5E:  return "Network services (3.1+)";
  case 0x5F:  return "Network redirection (3.1+)";
  case 0x60:  return "Get fully qualified file name (undocumented 3.x+)";
  case 0x62:  return "Get address of program segment prefix (3.x+)";
  case 0x63:  return "Get system lead byte table (MSDOS 2.25 only)";
  case 0x64:  return "Set device driver look ahead  (undocumented 3.3+)";
  case 0x65:  return "Get extended country information (3.3+)";
  case 0x66:  return "Get/set global code page (3.3+)";
  case 0x67:  return "Set handle count (3.3+)";
  case 0x68:  return "Flush buffer (3.3+)";
  case 0x69:  return "Get/set disk serial number (undocumented DOS 4.0+)";
  case 0x6A:  return "DOS reserved (DOS 4.0+)";
  case 0x6B:  return "DOS reserved";
  case 0x6C:  return "Extended open/create (4.x+)";
  case 0xF8:  return "Set OEM INT 21 handler (functions F9-FF) (undocumented)";
  default:    return "Unknown";
  }
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
    if (!VERBOSE) {
      _on_dos_close_file();
    }
    break;
  case 0x4B:  // EXEC
    if (cpu_regs.al == 0) {
      _on_dos_load_exec();
    }
    break;
  default:
    if (!VERBOSE) {
      log_printf(LOG_CHAN_DOS, "%s", _get_int_desc(cpu_regs.ah));
    }
  }
  // interupt not handled
  return false;
}
