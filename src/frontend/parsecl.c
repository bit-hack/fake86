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

/* parsecl.c: Fake86 command line parsing for runtime options. */

#include "../fake86/common.h"


extern const char *biosfile;

extern uint8_t bootdrive, verbose, cgaonly;
extern uint32_t framedelay, textbase, usefullscreen, speed;
extern int32_t usesamplerate, latency;
uint16_t constantw = 0, constanth = 0;
uint8_t slowsystem = 0;

extern bool do_fullscreen;
extern uint32_t frame_skip;

typedef bool(*cl_callback_t)(const char *opt, const char *arg[]);

struct cl_entry_t {
  const char *name;
  const int num_args;
  cl_callback_t callback;
  const char *desc;
  const char *example;
};

static bool _cl_do_help(const char *opt, const char *arg[]);

static bool _cl_do_fd(const char *opt, const char *arg[]) {
  const int num = opt[3] - '0';
  if (num < 0 || num > 2) {
    return false;
  }
  return disk_insert((uint8_t)num, *arg);
}

static bool _cl_do_hd(const char *opt, const char *arg[]) {
  const int num = opt[3] - '0';
  if (num < 0 || num > 3) {
    return false;
  }
  return disk_insert((uint8_t)(0x80 + num), *arg);
}

static bool _cl_do_boot(const char *opt, const char *arg[]) {
  bootdrive = 0;
  if (strcmp(*arg, "rom") == 0)
    bootdrive = 255;
  else
    bootdrive = atoi(*arg);
  return true;
}

static bool _cl_do_nosound(const char *opt, const char *arg[]) {
  log_printf(LOG_CHAN_AUDIO, "sound disabled");
  audio_enable = false;
  return true;
}

static bool _cl_do_bios(const char *opt, const char *arg[]) {
  biosfile = *arg;
  return true;
}

static bool _cl_do_fullscreen(const char *opt, const char *arg[]) {
  do_fullscreen = true;
  return true;
}

static bool _cl_do_frameskip(const char *opt, const char *arg[]) {
  frame_skip = atoi(*arg);
  log_printf(LOG_CHAN_FRONTEND, "skipping %d frames", frame_skip);
  return true;
}

static const struct cl_entry_t _cl_list[] = {
  {"-help", 0, _cl_do_help, "Display this help page"
  },
  {"-fd*", 1, _cl_do_fd, "Specify a floppy disk image (* = 0..3)",
    "   -fd0 [image file path]\n"
    "   -fd0 disk_image.img\n"
    "   (prepend * for memory backed binary loading)"
    "   -fd0 *[image file path]\n"
    "   (to insert blank disk)\n"
    "   -fd0 *\n"
  },
  {"-hd*", 1, _cl_do_hd, "Specify a hard disk image (* = 0..3)",
    "   -hd0 [image file path]\n"
    "   -hd0 hard_drive.img\n"
    "   -hd1 \\\\.\\F:\n"
    "   -hd2 \\\\.\\PhysicalDrive2:\n"
  },
  {"-boot", 1, _cl_do_boot, "Specify BIOS drive ID to boot from",
    "   -boot [BIOSDiskId]\n"
    "   -boot 0            (floppy disk 0)\n"
    "   -boot 128          (hard disk 0)\n"
  },
  {"-nosound", 0, _cl_do_nosound, "Disable sound output"
  },
  {"-bios", 1, _cl_do_bios, "Specify bios image to load",
    "   -bios pcxtbios.bin\n"
    "   -bios landmarktest.bin\n"
  },
  {"-fullscreen", 0, _cl_do_fullscreen, "Enable fullscreen mode"
  },
  {"-frameskip", 1, _cl_do_frameskip, "Number of frames to skip",
    "   -frameskip 1\n"
  },
  {NULL, 0, NULL, NULL}
};

static bool _cl_do_help(const char *opt, const char *arg[]) {
  printf("usage: fake86 [argument] [...]\n");
  printf("available arguments:\n");
  const struct cl_entry_t *entry = _cl_list;
  for (; entry->name; ++entry) {
    const char *example = entry->example ? entry->example : "";
    printf("> %-12s %s\n%s\n", entry->name, entry->desc, example);
  }
  return false;
}

static inline bool strpcmp(const char *a, const char *b) {
  for (;; ++a, ++b) {
    if (*a == '\0') {
      return true;
    }
    if (*b == '\0') {
      return false;
    }
    // wildcard
    if (*a == '*') {
      continue;
    }
    // mismatch
    if (*a != *b) {
      return false;
    }
  }
}

static void _cl_defaults() {
  biosfile = "pcxtbios.bin";
  textbase = 0xB8000;
  usefullscreen = 0;
  audio_enable = true;
  frame_skip = 0;
  bootdrive = 0;
}

bool cl_parse(const int argc, const char **args) {
  // load command line defaults
  _cl_defaults();
  // show help at no arguments
  if (argc <= 1) {
    return _cl_do_help(NULL, NULL);
  }
  // for incomming arguments
  for (int i = 1; i < argc;) {
    bool unknown = true;
    // for all possible entries
    const struct cl_entry_t *entry = _cl_list;
    for (; entry->name; ++entry) {
      // check for command line match
      if (!strpcmp(entry->name, args[i])) {
        continue;
      }
      // execute option line handler
      unknown = false;
      if (!entry->callback(args[i], &args[i + 1])) {
        return false;
      }
      // step over arguments
      i += 1 + entry->num_args;
      break;
    }
    if (unknown) {
      printf("Unknown command line option '%s'\n", args[i]);
      return false;
    }
  }
  return true;
}
