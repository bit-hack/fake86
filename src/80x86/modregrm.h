#pragma once

#include "../fake86/config.h"

#define modregrm()                                                             \
  {                                                                            \
    addrbyte = getmem8(cpu_regs.cs, ip);                                       \
    StepIP(1);                                                                 \
    mode = addrbyte >> 6;                                                      \
    reg = (addrbyte >> 3) & 7;                                                 \
    rm = addrbyte & 7;                                                         \
    switch (mode) {                                                            \
    case 0:                                                                    \
      if (rm == 6) {                                                           \
        disp16 = getmem16(cpu_regs.cs, ip);                                    \
        StepIP(2);                                                             \
      }                                                                        \
      if (((rm == 2) || (rm == 3)) && !segoverride) {                          \
        useseg = cpu_regs.ss;                                                  \
      }                                                                        \
      break;                                                                   \
                                                                               \
    case 1:                                                                    \
      disp16 = signext(getmem8(cpu_regs.cs, ip));                              \
      StepIP(1);                                                               \
      if (((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) {             \
        useseg = cpu_regs.ss;                                                  \
      }                                                                        \
      break;                                                                   \
                                                                               \
    case 2:                                                                    \
      disp16 = getmem16(cpu_regs.cs, ip);                                      \
      StepIP(2);                                                               \
      if (((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) {             \
        useseg = cpu_regs.ss;                                                  \
      }                                                                        \
      break;                                                                   \
                                                                               \
    default:                                                                   \
      disp8 = 0;                                                               \
      disp16 = 0;                                                              \
    }                                                                          \
  }
