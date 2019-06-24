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

#include <stdarg.h>

#include "../common/common.h"
#include "../frontend/frontend.h"


static FILE *log_fd = NULL;

static const char *channel_name[] = {
  "[     ]  ",
  "[DISK ]  ",
  "[FRONT]  ",
  "[SDL  ]  ",
  "[CPU  ]  ",
  "[MEM  ]  ",
  "[VIDEO]  ",
  "[AUDIO]  ",
  "[PORT ]  ",
  "[DOS  ]  "
};

static bool _quiet;

void log_mute(bool enable) {
  _quiet = enable;
}

void log_init(void) {
  log_fd = fopen(LOG_FNAME, "w");
  if (log_fd) {
    fprintf(log_fd, "(c)2019      Aidan Dodds\n");
    fprintf(log_fd, "(c)2010-2013 Mike Chambers\n");
    fprintf(log_fd, "[A portable, open-source 8086 PC emulator]\n");
    fprintf(log_fd, "build: %s\n\n", BUILD_STRING);
  }
}

void log_close(void) {
  if (log_fd) {
    fclose(log_fd);
    log_fd = NULL;
  }
}

void log_printf(int channel, const char *fmt, ...) {
  if (!log_fd) {
    log_init();
  }
#if 0
  {
    va_list vargs;
    va_start(vargs, fmt);
    osd_vprintf(fmt, vargs);
    va_end(vargs);
  }
#endif
  if (log_fd) {
    va_list vargs;
    va_start(vargs, fmt);
    fprintf(log_fd, "%s", channel_name[channel]);
    vfprintf(log_fd, fmt, vargs);
    fputc('\n', log_fd);
    va_end(vargs);
  }
  // mirror to stdout
  if (!_quiet) {
    va_list vargs;
    va_start(vargs, fmt);
    fprintf(stdout, "%s", channel_name[channel]);
    vfprintf(stdout, fmt, vargs);
    fputc('\n', stdout);
    va_end(vargs);
  }
}
