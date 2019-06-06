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

#include "../common/common.h"


// 512 characters from codepage 437
//   0-255  normal
// 256-511  bold

// characters are 8x8 pixels in size

// left most pixel is MSB
// right most pixel is LSB

// 8 byte run is ordered y top to y low

// image reference comes from https://www.seasip.info/VintagePC/cga.html


static const uint8_t cga_font_8x8[512 * 8] = {  
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0
  0x7e, 0x81, 0xa5, 0x81, 0xbd, 0x99, 0x81, 0x7e,  // 1
  0x7e, 0xff, 0xdb, 0xff, 0xc3, 0xe7, 0xff, 0x7e,  // 2
  0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00,  // 3
  0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00,  // 4
  0x38, 0x7c, 0x38, 0xfe, 0xfe, 0xd6, 0x10, 0x38,  // 5
  0x10, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x10, 0x38,  // 6
  0x00, 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00, 0x00,  // 7
  0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff,  // 8
  0x00, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0x00,  // 9
  0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff,  // 10
  0x0f, 0x03, 0x05, 0x7d, 0x84, 0x84, 0x84, 0x78,  // 11
  0x3c, 0x42, 0x42, 0x42, 0x3c, 0x18, 0x7e, 0x18,  // 12
  0x3f, 0x21, 0x3f, 0x20, 0x20, 0x60, 0xe0, 0xc0,  // 13
  0x3f, 0x21, 0x3f, 0x21, 0x23, 0x67, 0xe6, 0xc0,  // 14
  0x18, 0xdb, 0x3c, 0xe7, 0xe7, 0x3c, 0xdb, 0x18,  // 15
  0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00,  // 16
  0x02, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x02, 0x00,  // 17
  0x18, 0x3c, 0x7e, 0x18, 0x18, 0x7e, 0x3c, 0x18,  // 18
  0x24, 0x24, 0x24, 0x24, 0x24, 0x00, 0x24, 0x00,  // 19
  0x7f, 0x92, 0x92, 0x72, 0x12, 0x12, 0x12, 0x00,  // 20
  0x3e, 0x63, 0x38, 0x44, 0x44, 0x38, 0xcc, 0x78,  // 21
  0x00, 0x00, 0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x00,  // 22
  0x18, 0x3c, 0x7e, 0x18, 0x7e, 0x3c, 0x18, 0xff,  // 23
  0x10, 0x38, 0x7c, 0x54, 0x10, 0x10, 0x10, 0x00,  // 24
  0x10, 0x10, 0x10, 0x54, 0x7c, 0x38, 0x10, 0x00,  // 25
  0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00,  // 26
  0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00,  // 27
  0x00, 0x00, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00,  // 28
  0x00, 0x24, 0x66, 0xff, 0x66, 0x24, 0x00, 0x00,  // 29
  0x00, 0x10, 0x38, 0x7c, 0xfe, 0xfe, 0x00, 0x00,  // 30
  0x00, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00,  // 31
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 32
  0x10, 0x38, 0x38, 0x10, 0x10, 0x00, 0x10, 0x00,  // 33
  0x24, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00,  // 34
  0x24, 0x24, 0x7e, 0x24, 0x7e, 0x24, 0x24, 0x00,  // 35
  0x18, 0x3e, 0x40, 0x3c, 0x02, 0x7c, 0x18, 0x00,  // 36
  0x00, 0x62, 0x64, 0x08, 0x10, 0x26, 0x46, 0x00,  // 37
  0x30, 0x48, 0x30, 0x56, 0x88, 0x88, 0x76, 0x00,  // 38
  0x10, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,  // 39
  0x10, 0x20, 0x40, 0x40, 0x40, 0x20, 0x10, 0x00,  // 40
  0x20, 0x10, 0x08, 0x08, 0x08, 0x10, 0x20, 0x00,  // 41
  0x00, 0x44, 0x38, 0xfe, 0x38, 0x44, 0x00, 0x00,  // 42
  0x00, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x00, 0x00,  // 43
  0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x20,  // 44
  0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,  // 45
  0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x00,  // 46
  0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00,  // 47
  0x3c, 0x42, 0x46, 0x4a, 0x52, 0x62, 0x3c, 0x00,  // 48
  0x10, 0x30, 0x50, 0x10, 0x10, 0x10, 0x7c, 0x00,  // 49
  0x3c, 0x42, 0x02, 0x0c, 0x30, 0x42, 0x7e, 0x00,  // 50
  0x3c, 0x42, 0x02, 0x1c, 0x02, 0x42, 0x3c, 0x00,  // 51
  0x08, 0x18, 0x28, 0x48, 0xfe, 0x08, 0x1c, 0x00,  // 52
  0x7e, 0x40, 0x7c, 0x02, 0x02, 0x42, 0x3c, 0x00,  // 53
  0x1c, 0x20, 0x40, 0x7c, 0x42, 0x42, 0x3c, 0x00,  // 54
  0x7e, 0x42, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00,  // 55
  0x3c, 0x42, 0x42, 0x3c, 0x42, 0x42, 0x3c, 0x00,  // 56
  0x3c, 0x42, 0x42, 0x3e, 0x02, 0x04, 0x38, 0x00,  // 57
  0x00, 0x10, 0x10, 0x00, 0x00, 0x10, 0x10, 0x00,  // 58
  0x00, 0x10, 0x10, 0x00, 0x00, 0x10, 0x10, 0x20,  // 59
  0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0x00,  // 60
  0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00,  // 61
  0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00,  // 62
  0x3c, 0x42, 0x02, 0x04, 0x08, 0x00, 0x08, 0x00,  // 63
  0x3c, 0x42, 0x5e, 0x52, 0x5e, 0x40, 0x3c, 0x00,  // 64
  0x18, 0x24, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x00,  // 65
  0x7c, 0x22, 0x22, 0x3c, 0x22, 0x22, 0x7c, 0x00,  // 66
  0x1c, 0x22, 0x40, 0x40, 0x40, 0x22, 0x1c, 0x00,  // 67
  0x78, 0x24, 0x22, 0x22, 0x22, 0x24, 0x78, 0x00,  // 68
  0x7e, 0x22, 0x28, 0x38, 0x28, 0x22, 0x7e, 0x00,  // 69
  0x7e, 0x22, 0x28, 0x38, 0x28, 0x20, 0x70, 0x00,  // 70
  0x1c, 0x22, 0x40, 0x40, 0x4e, 0x22, 0x1e, 0x00,  // 71
  0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x00,  // 72
  0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00,  // 73
  0x0e, 0x04, 0x04, 0x04, 0x44, 0x44, 0x38, 0x00,  // 74
  0x62, 0x24, 0x28, 0x30, 0x28, 0x24, 0x63, 0x00,  // 75
  0x70, 0x20, 0x20, 0x20, 0x20, 0x22, 0x7e, 0x00,  // 76
  0x63, 0x55, 0x49, 0x41, 0x41, 0x41, 0x41, 0x00,  // 77
  0x62, 0x52, 0x4a, 0x46, 0x42, 0x42, 0x42, 0x00,  // 78
  0x18, 0x24, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00,  // 79
  0x7c, 0x22, 0x22, 0x3c, 0x20, 0x20, 0x70, 0x00,  // 80
  0x3c, 0x42, 0x42, 0x42, 0x4a, 0x3c, 0x03, 0x00,  // 81
  0x7c, 0x22, 0x22, 0x3c, 0x28, 0x24, 0x72, 0x00,  // 82
  0x3c, 0x42, 0x40, 0x3c, 0x02, 0x42, 0x3c, 0x00,  // 83
  0x7f, 0x49, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00,  // 84
  0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00,  // 85
  0x41, 0x41, 0x41, 0x41, 0x22, 0x14, 0x08, 0x00,  // 86
  0x41, 0x41, 0x41, 0x49, 0x49, 0x49, 0x36, 0x00,  // 87
  0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x00,  // 88
  0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x1c, 0x00,  // 89
  0x7f, 0x42, 0x04, 0x08, 0x10, 0x21, 0x7f, 0x00,  // 90
  0x78, 0x40, 0x40, 0x40, 0x40, 0x40, 0x78, 0x00,  // 91
  0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00,  // 92
  0x78, 0x08, 0x08, 0x08, 0x08, 0x08, 0x78, 0x00,  // 93
  0x10, 0x28, 0x44, 0x82, 0x00, 0x00, 0x00, 0x00,  // 94
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,  // 95
  0x10, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,  // 96
  0x00, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x3f, 0x00,  // 97
  0x60, 0x20, 0x20, 0x2e, 0x31, 0x31, 0x2e, 0x00,  // 98
  0x00, 0x00, 0x3c, 0x42, 0x40, 0x42, 0x3c, 0x00,  // 99
  0x06, 0x02, 0x02, 0x3a, 0x46, 0x46, 0x3b, 0x00,  // 100
  0x00, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00,  // 101
  0x0c, 0x12, 0x10, 0x38, 0x10, 0x10, 0x38, 0x00,  // 102
  0x00, 0x00, 0x3d, 0x42, 0x42, 0x3e, 0x02, 0x7c,  // 103
  0x60, 0x20, 0x2c, 0x32, 0x22, 0x22, 0x62, 0x00,  // 104
  0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00,  // 105
  0x02, 0x00, 0x06, 0x02, 0x02, 0x42, 0x42, 0x3c,  // 106
  0x60, 0x20, 0x24, 0x28, 0x30, 0x28, 0x26, 0x00,  // 107
  0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00,  // 108
  0x00, 0x00, 0x76, 0x49, 0x49, 0x49, 0x49, 0x00,  // 109
  0x00, 0x00, 0x5c, 0x62, 0x42, 0x42, 0x42, 0x00,  // 110
  0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x3c, 0x00,  // 111
  0x00, 0x00, 0x6c, 0x32, 0x32, 0x2c, 0x20, 0x70,  // 112
  0x00, 0x00, 0x36, 0x4c, 0x4c, 0x34, 0x04, 0x0e,  // 113
  0x00, 0x00, 0x6c, 0x32, 0x22, 0x20, 0x70, 0x00,  // 114
  0x00, 0x00, 0x3e, 0x40, 0x3c, 0x02, 0x7c, 0x00,  // 115
  0x10, 0x10, 0x7c, 0x10, 0x10, 0x12, 0x0c, 0x00,  // 116
  0x00, 0x00, 0x42, 0x42, 0x42, 0x46, 0x3a, 0x00,  // 117
  0x00, 0x00, 0x41, 0x41, 0x22, 0x14, 0x08, 0x00,  // 118
  0x00, 0x00, 0x41, 0x49, 0x49, 0x49, 0x36, 0x00,  // 119
  0x00, 0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00,  // 120
  0x00, 0x00, 0x42, 0x42, 0x42, 0x3e, 0x02, 0x7c,  // 121
  0x00, 0x00, 0x7c, 0x08, 0x10, 0x20, 0x7c, 0x00,  // 122
  0x0c, 0x10, 0x10, 0x60, 0x10, 0x10, 0x0c, 0x00,  // 123
  0x10, 0x10, 0x10, 0x00, 0x10, 0x10, 0x10, 0x00,  // 124
  0x30, 0x08, 0x08, 0x06, 0x08, 0x08, 0x30, 0x00,  // 125
  0x32, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 126
  0x00, 0x08, 0x14, 0x22, 0x41, 0x41, 0x7f, 0x00,  // 127
  0x3c, 0x42, 0x40, 0x42, 0x3c, 0x0c, 0x02, 0x3c,  // 128
  0x00, 0x44, 0x00, 0x44, 0x44, 0x44, 0x3e, 0x00,  // 129
  0x0c, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00,  // 130
  0x3c, 0x42, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00,  // 131
  0x42, 0x00, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00,  // 132
  0x30, 0x00, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00,  // 133
  0x10, 0x00, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00,  // 134
  0x00, 0x00, 0x3c, 0x40, 0x40, 0x3c, 0x06, 0x1c,  // 135
  0x3c, 0x42, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00,  // 136
  0x42, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00,  // 137
  0x30, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00,  // 138
  0x24, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1c, 0x00,  // 139
  0x7c, 0x82, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00,  // 140
  0x30, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1c, 0x00,  // 141
  0x42, 0x18, 0x24, 0x42, 0x7e, 0x42, 0x42, 0x00,  // 142
  0x18, 0x18, 0x00, 0x3c, 0x42, 0x7e, 0x42, 0x00,  // 143
  0x0c, 0x00, 0x7c, 0x20, 0x38, 0x20, 0x7c, 0x00,  // 144
  0x00, 0x00, 0x33, 0x0c, 0x3f, 0x44, 0x3b, 0x00,  // 145
  0x1f, 0x24, 0x44, 0x7f, 0x44, 0x44, 0x47, 0x00,  // 146
  0x18, 0x24, 0x00, 0x3c, 0x42, 0x42, 0x3c, 0x00,  // 147
  0x00, 0x42, 0x00, 0x3c, 0x42, 0x42, 0x3c, 0x00,  // 148
  0x20, 0x10, 0x00, 0x3c, 0x42, 0x42, 0x3c, 0x00,  // 149
  0x18, 0x24, 0x00, 0x42, 0x42, 0x42, 0x3c, 0x00,  // 150
  0x20, 0x10, 0x00, 0x42, 0x42, 0x42, 0x3c, 0x00,  // 151
  0x00, 0x42, 0x00, 0x42, 0x42, 0x3e, 0x02, 0x3c,  // 152
  0x42, 0x18, 0x24, 0x42, 0x42, 0x24, 0x18, 0x00,  // 153
  0x42, 0x00, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00,  // 154
  0x08, 0x08, 0x3e, 0x40, 0x40, 0x3e, 0x08, 0x08,  // 155
  0x18, 0x24, 0x20, 0x70, 0x20, 0x42, 0x7c, 0x00,  // 156
  0x44, 0x28, 0x7c, 0x10, 0x7c, 0x10, 0x10, 0x00,  // 157
  0xf8, 0x4c, 0x78, 0x44, 0x4f, 0x44, 0x45, 0xe6,  // 158
  0x1c, 0x12, 0x10, 0x7c, 0x10, 0x10, 0x90, 0x60,  // 159
  0x0c, 0x00, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00,  // 160
  0x0c, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1c, 0x00,  // 161
  0x04, 0x08, 0x00, 0x3c, 0x42, 0x42, 0x3c, 0x00,  // 162
  0x00, 0x04, 0x08, 0x42, 0x42, 0x42, 0x3c, 0x00,  // 163
  0x32, 0x4c, 0x00, 0x7c, 0x42, 0x42, 0x42, 0x00,  // 164
  0x34, 0x4c, 0x00, 0x62, 0x52, 0x4a, 0x46, 0x00,  // 165
  0x3c, 0x44, 0x44, 0x3e, 0x00, 0x7e, 0x00, 0x00,  // 166
  0x38, 0x44, 0x44, 0x38, 0x00, 0x7c, 0x00, 0x00,  // 167
  0x10, 0x00, 0x10, 0x20, 0x40, 0x42, 0x3c, 0x00,  // 168
  0x00, 0x00, 0x00, 0x7e, 0x40, 0x40, 0x00, 0x00,  // 169
  0x00, 0x00, 0x00, 0x7e, 0x02, 0x02, 0x00, 0x00,  // 170
  0x42, 0xc4, 0x48, 0xf6, 0x29, 0x43, 0x8c, 0x1f,  // 171
  0x42, 0xc4, 0x4a, 0xf6, 0x2a, 0x5f, 0x82, 0x02,  // 172
  0x00, 0x10, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00,  // 173
  0x00, 0x12, 0x24, 0x48, 0x24, 0x12, 0x00, 0x00,  // 174
  0x00, 0x48, 0x24, 0x12, 0x24, 0x48, 0x00, 0x00,  // 175
  0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88,  // 176
  0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,  // 177
  0xdb, 0x77, 0xdb, 0xee, 0xdb, 0x77, 0xdb, 0xee,  // 178
  0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,  // 179
  0x10, 0x10, 0x10, 0x10, 0xf0, 0x10, 0x10, 0x10,  // 180
  0x10, 0x10, 0xf0, 0x10, 0xf0, 0x10, 0x10, 0x10,  // 181
  0x14, 0x14, 0x14, 0x14, 0xf4, 0x14, 0x14, 0x14,  // 182
  0x00, 0x00, 0x00, 0x00, 0xfc, 0x14, 0x14, 0x14,  // 183
  0x00, 0x00, 0xf0, 0x10, 0xf0, 0x10, 0x10, 0x10,  // 184
  0x14, 0x14, 0xf4, 0x04, 0xf4, 0x14, 0x14, 0x14,  // 185
  0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,  // 186
  0x00, 0x00, 0xfc, 0x04, 0xf4, 0x14, 0x14, 0x14,  // 187
  0x14, 0x14, 0xf4, 0x04, 0xfc, 0x00, 0x00, 0x00,  // 188
  0x14, 0x14, 0x14, 0x14, 0xfc, 0x00, 0x00, 0x00,  // 189
  0x10, 0x10, 0xf0, 0x10, 0xf0, 0x00, 0x00, 0x00,  // 190
  0x00, 0x00, 0x00, 0x00, 0xf0, 0x10, 0x10, 0x10,  // 191
  0x10, 0x10, 0x10, 0x10, 0x1f, 0x00, 0x00, 0x00,  // 192
  0x10, 0x10, 0x10, 0x10, 0xff, 0x00, 0x00, 0x00,  // 193
  0x00, 0x00, 0x00, 0x00, 0xff, 0x10, 0x10, 0x10,  // 194
  0x10, 0x10, 0x10, 0x10, 0x1f, 0x10, 0x10, 0x10,  // 195
  0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,  // 196
  0x10, 0x10, 0x10, 0x10, 0xff, 0x10, 0x10, 0x10,  // 197
  0x10, 0x10, 0x1f, 0x10, 0x1f, 0x10, 0x10, 0x10,  // 198
  0x14, 0x14, 0x14, 0x14, 0x17, 0x14, 0x14, 0x14,  // 199
  0x14, 0x14, 0x17, 0x10, 0x1f, 0x00, 0x00, 0x00,  // 200
  0x00, 0x00, 0x1f, 0x10, 0x17, 0x14, 0x14, 0x14,  // 201
  0x14, 0x14, 0xf7, 0x00, 0xff, 0x00, 0x00, 0x00,  // 202
  0x00, 0x00, 0xff, 0x00, 0xf7, 0x14, 0x14, 0x14,  // 203
  0x14, 0x14, 0x17, 0x10, 0x17, 0x14, 0x14, 0x14,  // 204
  0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,  // 205
  0x14, 0x14, 0xf7, 0x00, 0xf7, 0x14, 0x14, 0x14,  // 206
  0x10, 0x10, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,  // 207
  0x14, 0x14, 0x14, 0x14, 0xff, 0x00, 0x00, 0x00,  // 208
  0x00, 0x00, 0xff, 0x00, 0xff, 0x10, 0x10, 0x10,  // 209
  0x00, 0x00, 0x00, 0x00, 0xff, 0x14, 0x14, 0x14,  // 210
  0x14, 0x14, 0x14, 0x14, 0x1f, 0x00, 0x00, 0x00,  // 211
  0x10, 0x10, 0x1f, 0x10, 0x1f, 0x00, 0x00, 0x00,  // 212
  0x00, 0x00, 0x1f, 0x10, 0x1f, 0x10, 0x10, 0x10,  // 213
  0x00, 0x00, 0x00, 0x00, 0x1f, 0x14, 0x14, 0x14,  // 214
  0x14, 0x14, 0x14, 0x14, 0xff, 0x14, 0x14, 0x14,  // 215
  0x10, 0x10, 0xff, 0x10, 0xff, 0x10, 0x10, 0x10,  // 216
  0x10, 0x10, 0x10, 0x10, 0xf0, 0x00, 0x00, 0x00,  // 217
  0x00, 0x00, 0x00, 0x00, 0x1f, 0x10, 0x10, 0x10,  // 218
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 219
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,  // 220
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,  // 221
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,  // 222
  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,  // 223
  0x00, 0x00, 0x31, 0x4a, 0x44, 0x4a, 0x31, 0x00,  // 224
  0x00, 0x3c, 0x42, 0x7c, 0x42, 0x7c, 0x40, 0x40,  // 225
  0x00, 0x7e, 0x42, 0x40, 0x40, 0x40, 0x40, 0x00,  // 226
  0x00, 0x3f, 0x54, 0x14, 0x14, 0x14, 0x14, 0x00,  // 227
  0x7e, 0x42, 0x20, 0x18, 0x20, 0x42, 0x7e, 0x00,  // 228
  0x00, 0x00, 0x3e, 0x48, 0x48, 0x48, 0x30, 0x00,  // 229
  0x00, 0x44, 0x44, 0x44, 0x7a, 0x40, 0x40, 0x80,  // 230
  0x00, 0x33, 0x4c, 0x08, 0x08, 0x08, 0x08, 0x00,  // 231
  0x7c, 0x10, 0x38, 0x44, 0x44, 0x38, 0x10, 0x7c,  // 232
  0x18, 0x24, 0x42, 0x7e, 0x42, 0x24, 0x18, 0x00,  // 233
  0x18, 0x24, 0x42, 0x42, 0x24, 0x24, 0x66, 0x00,  // 234
  0x1c, 0x20, 0x18, 0x3c, 0x42, 0x42, 0x3c, 0x00,  // 235
  0x00, 0x62, 0x95, 0x89, 0x95, 0x62, 0x00, 0x00,  // 236
  0x02, 0x04, 0x3c, 0x4a, 0x52, 0x3c, 0x40, 0x80,  // 237
  0x0c, 0x10, 0x20, 0x3c, 0x20, 0x10, 0x0c, 0x00,  // 238
  0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00,  // 239
  0x00, 0x7e, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00,  // 240
  0x10, 0x10, 0x7c, 0x10, 0x10, 0x00, 0x7c, 0x00,  // 241
  0x10, 0x08, 0x04, 0x08, 0x10, 0x00, 0x7e, 0x00,  // 242
  0x08, 0x10, 0x20, 0x10, 0x08, 0x00, 0x7e, 0x00,  // 243
  0x0c, 0x12, 0x12, 0x10, 0x10, 0x10, 0x10, 0x10,  // 244
  0x10, 0x10, 0x10, 0x10, 0x10, 0x90, 0x90, 0x60,  // 245
  0x18, 0x18, 0x00, 0x7e, 0x00, 0x18, 0x18, 0x00,  // 246
  0x00, 0x32, 0x4c, 0x00, 0x32, 0x4c, 0x00, 0x00,  // 247
  0x30, 0x48, 0x48, 0x30, 0x00, 0x00, 0x00, 0x00,  // 248
  0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,  // 249
  0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,  // 250
  0x0f, 0x08, 0x08, 0x08, 0x08, 0xc8, 0x28, 0x18,  // 251
  0x78, 0x44, 0x44, 0x44, 0x44, 0x00, 0x00, 0x00,  // 252
  0x30, 0x48, 0x10, 0x20, 0x78, 0x00, 0x00, 0x00,  // 253
  0x00, 0x00, 0x3c, 0x3c, 0x3c, 0x3c, 0x00, 0x00,  // 254
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 255
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 256
  0x7e, 0x81, 0xa5, 0x81, 0xbd, 0x99, 0x81, 0x7e,  // 257
  0x7e, 0xff, 0xdb, 0xff, 0xc3, 0xe7, 0xff, 0x7e,  // 258
  0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00,  // 259
  0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00,  // 260
  0x38, 0x7c, 0x38, 0xfe, 0xfe, 0xd6, 0x10, 0x38,  // 261
  0x10, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x10, 0x38,  // 262
  0x00, 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00, 0x00,  // 263
  0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff,  // 264
  0x00, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0x00,  // 265
  0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff,  // 266
  0x0f, 0x07, 0x0f, 0x7d, 0xcc, 0xcc, 0xcc, 0x78,  // 267
  0x3c, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18,  // 268
  0x3f, 0x33, 0x3f, 0x30, 0x30, 0x70, 0xf0, 0xe0,  // 269
  0x7f, 0x63, 0x7f, 0x63, 0x63, 0x67, 0xe6, 0xc0,  // 270
  0x18, 0xdb, 0x3c, 0xe7, 0xe7, 0x3c, 0xdb, 0x18,  // 271
  0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00,  // 272
  0x02, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x02, 0x00,  // 273
  0x18, 0x3c, 0x7e, 0x18, 0x18, 0x7e, 0x3c, 0x18,  // 274
  0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00,  // 275
  0x7f, 0xdb, 0xdb, 0x7b, 0x1b, 0x1b, 0x1b, 0x00,  // 276
  0x3e, 0x63, 0x38, 0x6c, 0x6c, 0x38, 0xcc, 0x78,  // 277
  0x00, 0x00, 0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x00,  // 278
  0x18, 0x3c, 0x7e, 0x18, 0x7e, 0x3c, 0x18, 0xff,  // 279
  0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x00,  // 280
  0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00,  // 281
  0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00,  // 282
  0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00,  // 283
  0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00,  // 284
  0x00, 0x24, 0x66, 0xff, 0x66, 0x24, 0x00, 0x00,  // 285
  0x00, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0x00,  // 286
  0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00, 0x00,  // 287
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 288
  0x30, 0x78, 0x78, 0x30, 0x30, 0x00, 0x30, 0x00,  // 289
  0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00,  // 290
  0x6c, 0x6c, 0xfe, 0x6c, 0xfe, 0x6c, 0x6c, 0x00,  // 291
  0x30, 0x7c, 0xc0, 0x78, 0x0c, 0xf8, 0x30, 0x00,  // 292
  0x00, 0xc6, 0xcc, 0x18, 0x30, 0x66, 0xc6, 0x00,  // 293
  0x38, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0x76, 0x00,  // 294
  0x60, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,  // 295
  0x18, 0x30, 0x60, 0x60, 0x60, 0x30, 0x18, 0x00,  // 296
  0x60, 0x30, 0x18, 0x18, 0x18, 0x30, 0x60, 0x00,  // 297
  0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00,  // 298
  0x00, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x00, 0x00,  // 299
  0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x60,  // 300
  0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00,  // 301
  0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00,  // 302
  0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x00,  // 303
  0x7c, 0xc6, 0xce, 0xde, 0xf6, 0xe6, 0x7c, 0x00,  // 304
  0x30, 0x70, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x00,  // 305
  0x78, 0xcc, 0x0c, 0x38, 0x60, 0xcc, 0xfc, 0x00,  // 306
  0x78, 0xcc, 0x0c, 0x38, 0x0c, 0xcc, 0x78, 0x00,  // 307
  0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x1e, 0x00,  // 308
  0xfc, 0xc0, 0xf8, 0x0c, 0x0c, 0xcc, 0x78, 0x00,  // 309
  0x38, 0x60, 0xc0, 0xf8, 0xcc, 0xcc, 0x78, 0x00,  // 310
  0xfc, 0xcc, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x00,  // 311
  0x78, 0xcc, 0xcc, 0x78, 0xcc, 0xcc, 0x78, 0x00,  // 312
  0x78, 0xcc, 0xcc, 0x7c, 0x0c, 0x18, 0x70, 0x00,  // 313
  0x00, 0x30, 0x30, 0x00, 0x00, 0x30, 0x30, 0x00,  // 314
  0x00, 0x30, 0x30, 0x00, 0x00, 0x30, 0x30, 0x60,  // 315
  0x18, 0x30, 0x60, 0xc0, 0x60, 0x30, 0x18, 0x00,  // 316
  0x00, 0x00, 0xfc, 0x00, 0x00, 0xfc, 0x00, 0x00,  // 317
  0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00,  // 318
  0x78, 0xcc, 0x0c, 0x18, 0x30, 0x00, 0x30, 0x00,  // 319
  0x7c, 0xc6, 0xde, 0xde, 0xde, 0xc0, 0x78, 0x00,  // 320
  0x30, 0x78, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0x00,  // 321
  0xfc, 0x66, 0x66, 0x7c, 0x66, 0x66, 0xfc, 0x00,  // 322
  0x3c, 0x66, 0xc0, 0xc0, 0xc0, 0x66, 0x3c, 0x00,  // 323
  0xf8, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00,  // 324
  0xfe, 0x62, 0x68, 0x78, 0x68, 0x62, 0xfe, 0x00,  // 325
  0xfe, 0x62, 0x68, 0x78, 0x68, 0x60, 0xf0, 0x00,  // 326
  0x3c, 0x66, 0xc0, 0xc0, 0xce, 0x66, 0x3e, 0x00,  // 327
  0xcc, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0xcc, 0x00,  // 328
  0x78, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,  // 329
  0x1e, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78, 0x00,  // 330
  0xe6, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0xe6, 0x00,  // 331
  0xf0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00,  // 332
  0xc6, 0xee, 0xfe, 0xfe, 0xd6, 0xc6, 0xc6, 0x00,  // 333
  0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0x00,  // 334
  0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00,  // 335
  0xfc, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0x00,  // 336
  0x78, 0xcc, 0xcc, 0xcc, 0xdc, 0x78, 0x1c, 0x00,  // 337
  0xfc, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0xe6, 0x00,  // 338
  0x78, 0xcc, 0x60, 0x30, 0x18, 0xcc, 0x78, 0x00,  // 339
  0xfc, 0xb4, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,  // 340
  0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xfc, 0x00,  // 341
  0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x00,  // 342
  0xc6, 0xc6, 0xc6, 0xd6, 0xfe, 0xee, 0xc6, 0x00,  // 343
  0xc6, 0xc6, 0x6c, 0x38, 0x38, 0x6c, 0xc6, 0x00,  // 344
  0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x30, 0x78, 0x00,  // 345
  0xfe, 0xc6, 0x8c, 0x18, 0x32, 0x66, 0xfe, 0x00,  // 346
  0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x78, 0x00,  // 347
  0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x02, 0x00,  // 348
  0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x00,  // 349
  0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00,  // 350
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,  // 351
  0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,  // 352
  0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0x00,  // 353
  0xe0, 0x60, 0x60, 0x7c, 0x66, 0x66, 0xdc, 0x00,  // 354
  0x00, 0x00, 0x78, 0xcc, 0xc0, 0xcc, 0x78, 0x00,  // 355
  0x1c, 0x0c, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,  // 356
  0x00, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00,  // 357
  0x38, 0x6c, 0x60, 0xf0, 0x60, 0x60, 0xf0, 0x00,  // 358
  0x00, 0x00, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8,  // 359
  0xe0, 0x60, 0x6c, 0x76, 0x66, 0x66, 0xe6, 0x00,  // 360
  0x30, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00,  // 361
  0x0c, 0x00, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78,  // 362
  0xe0, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0xe6, 0x00,  // 363
  0x70, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,  // 364
  0x00, 0x00, 0xcc, 0xfe, 0xfe, 0xd6, 0xc6, 0x00,  // 365
  0x00, 0x00, 0xf8, 0xcc, 0xcc, 0xcc, 0xcc, 0x00,  // 366
  0x00, 0x00, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0x00,  // 367
  0x00, 0x00, 0xdc, 0x66, 0x66, 0x7c, 0x60, 0xf0,  // 368
  0x00, 0x00, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0x1e,  // 369
  0x00, 0x00, 0xdc, 0x76, 0x66, 0x60, 0xf0, 0x00,  // 370
  0x00, 0x00, 0x7c, 0xc0, 0x78, 0x0c, 0xf8, 0x00,  // 371
  0x10, 0x30, 0x7c, 0x30, 0x30, 0x34, 0x18, 0x00,  // 372
  0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,  // 373
  0x00, 0x00, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x00,  // 374
  0x00, 0x00, 0xc6, 0xd6, 0xfe, 0xfe, 0x6c, 0x00,  // 375
  0x00, 0x00, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0x00,  // 376
  0x00, 0x00, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8,  // 377
  0x00, 0x00, 0xfc, 0x98, 0x30, 0x64, 0xfc, 0x00,  // 378
  0x1c, 0x30, 0x30, 0xe0, 0x30, 0x30, 0x1c, 0x00,  // 379
  0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00,  // 380
  0xe0, 0x30, 0x30, 0x1c, 0x30, 0x30, 0xe0, 0x00,  // 381
  0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 382
  0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0x00,  // 383
  0x78, 0xcc, 0xc0, 0xcc, 0x78, 0x18, 0x0c, 0x78,  // 384
  0x00, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00,  // 385
  0x1c, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00,  // 386
  0x7e, 0xc3, 0x3c, 0x06, 0x3e, 0x66, 0x3f, 0x00,  // 387
  0xcc, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00,  // 388
  0xe0, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00,  // 389
  0x30, 0x30, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00,  // 390
  0x00, 0x00, 0x78, 0xc0, 0xc0, 0x78, 0x0c, 0x38,  // 391
  0x7e, 0xc3, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00,  // 392
  0xcc, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00,  // 393
  0xe0, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00,  // 394
  0xcc, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00,  // 395
  0x7c, 0xc6, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,  // 396
  0xe0, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00,  // 397
  0xc6, 0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0xc6, 0x00,  // 398
  0x30, 0x30, 0x00, 0x78, 0xcc, 0xfc, 0xcc, 0x00,  // 399
  0x1c, 0x00, 0xfc, 0x60, 0x78, 0x60, 0xfc, 0x00,  // 400
  0x00, 0x00, 0x7f, 0x0c, 0x7f, 0xcc, 0x7f, 0x00,  // 401
  0x3e, 0x6c, 0xcc, 0xfe, 0xcc, 0xcc, 0xce, 0x00,  // 402
  0x78, 0xcc, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00,  // 403
  0x00, 0xcc, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00,  // 404
  0x00, 0xe0, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00,  // 405
  0x78, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00,  // 406
  0x00, 0xe0, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00,  // 407
  0x00, 0xcc, 0x00, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8,  // 408
  0xc3, 0x18, 0x3c, 0x66, 0x66, 0x3c, 0x18, 0x00,  // 409
  0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x78, 0x00,  // 410
  0x18, 0x18, 0x7e, 0xc0, 0xc0, 0x7e, 0x18, 0x18,  // 411
  0x38, 0x6c, 0x64, 0xf0, 0x60, 0xe6, 0xfc, 0x00,  // 412
  0xcc, 0xcc, 0x78, 0xfc, 0x30, 0xfc, 0x30, 0x30,  // 413
  0xf8, 0xcc, 0xcc, 0xfa, 0xc6, 0xcf, 0xc6, 0xc7,  // 414
  0x0e, 0x1b, 0x18, 0x3c, 0x18, 0x18, 0xd8, 0x70,  // 415
  0x1c, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00,  // 416
  0x38, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00,  // 417
  0x00, 0x1c, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00,  // 418
  0x00, 0x1c, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00,  // 419
  0x00, 0xf8, 0x00, 0xf8, 0xcc, 0xcc, 0xcc, 0x00,  // 420
  0xfc, 0x00, 0xcc, 0xec, 0xfc, 0xdc, 0xcc, 0x00,  // 421
  0x3c, 0x6c, 0x6c, 0x3e, 0x00, 0x7e, 0x00, 0x00,  // 422
  0x38, 0x6c, 0x6c, 0x38, 0x00, 0x7c, 0x00, 0x00,  // 423
  0x30, 0x00, 0x30, 0x60, 0xc0, 0xcc, 0x78, 0x00,  // 424
  0x00, 0x00, 0x00, 0xfc, 0xc0, 0xc0, 0x00, 0x00,  // 425
  0x00, 0x00, 0x00, 0xfc, 0x0c, 0x0c, 0x00, 0x00,  // 426
  0xc3, 0xc6, 0xcc, 0xde, 0x33, 0x66, 0xcc, 0x0f,  // 427
  0xc3, 0xc6, 0xcc, 0xdb, 0x37, 0x6f, 0xcf, 0x03,  // 428
  0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00,  // 429
  0x00, 0x33, 0x66, 0xcc, 0x66, 0x33, 0x00, 0x00,  // 430
  0x00, 0xcc, 0x66, 0x33, 0x66, 0xcc, 0x00, 0x00,  // 431
  0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88,  // 432
  0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,  // 433
  0xdb, 0x77, 0xdb, 0xee, 0xdb, 0x77, 0xdb, 0xee,  // 434
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,  // 435
  0x18, 0x18, 0x18, 0x18, 0xf8, 0x18, 0x18, 0x18,  // 436
  0x18, 0x18, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18,  // 437
  0x36, 0x36, 0x36, 0x36, 0xf6, 0x36, 0x36, 0x36,  // 438
  0x00, 0x00, 0x00, 0x00, 0xfe, 0x36, 0x36, 0x36,  // 439
  0x00, 0x00, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18,  // 440
  0x36, 0x36, 0xf6, 0x06, 0xf6, 0x36, 0x36, 0x36,  // 441
  0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,  // 442
  0x00, 0x00, 0xfe, 0x06, 0xf6, 0x36, 0x36, 0x36,  // 443
  0x36, 0x36, 0xf6, 0x06, 0xfe, 0x00, 0x00, 0x00,  // 444
  0x36, 0x36, 0x36, 0x36, 0xfe, 0x00, 0x00, 0x00,  // 445
  0x18, 0x18, 0xf8, 0x18, 0xf8, 0x00, 0x00, 0x00,  // 446
  0x00, 0x00, 0x00, 0x00, 0xf8, 0x18, 0x18, 0x18,  // 447
  0x18, 0x18, 0x18, 0x18, 0x1f, 0x00, 0x00, 0x00,  // 448
  0x18, 0x18, 0x18, 0x18, 0xff, 0x00, 0x00, 0x00,  // 449
  0x00, 0x00, 0x00, 0x00, 0xff, 0x18, 0x18, 0x18,  // 450
  0x18, 0x18, 0x18, 0x18, 0x1f, 0x18, 0x18, 0x18,  // 451
  0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,  // 452
  0x18, 0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18,  // 453
  0x18, 0x18, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18,  // 454
  0x36, 0x36, 0x36, 0x36, 0x37, 0x36, 0x36, 0x36,  // 455
  0x36, 0x36, 0x37, 0x30, 0x3f, 0x00, 0x00, 0x00,  // 456
  0x00, 0x00, 0x3f, 0x30, 0x37, 0x36, 0x36, 0x36,  // 457
  0x36, 0x36, 0xf7, 0x00, 0xff, 0x00, 0x00, 0x00,  // 458
  0x00, 0x00, 0xff, 0x00, 0xf7, 0x36, 0x36, 0x36,  // 459
  0x36, 0x36, 0x37, 0x30, 0x37, 0x36, 0x36, 0x36,  // 460
  0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,  // 461
  0x36, 0x36, 0xf7, 0x00, 0xf7, 0x36, 0x36, 0x36,  // 462
  0x18, 0x18, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,  // 463
  0x36, 0x36, 0x36, 0x36, 0xff, 0x00, 0x00, 0x00,  // 464
  0x00, 0x00, 0xff, 0x00, 0xff, 0x18, 0x18, 0x18,  // 465
  0x00, 0x00, 0x00, 0x00, 0xff, 0x36, 0x36, 0x36,  // 466
  0x36, 0x36, 0x36, 0x36, 0x3f, 0x00, 0x00, 0x00,  // 467
  0x18, 0x18, 0x1f, 0x18, 0x1f, 0x00, 0x00, 0x00,  // 468
  0x00, 0x00, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18,  // 469
  0x00, 0x00, 0x00, 0x00, 0x3f, 0x36, 0x36, 0x36,  // 470
  0x36, 0x36, 0x36, 0x36, 0xff, 0x36, 0x36, 0x36,  // 471
  0x18, 0x18, 0xff, 0x18, 0xff, 0x18, 0x18, 0x18,  // 472
  0x18, 0x18, 0x18, 0x18, 0xf8, 0x00, 0x00, 0x00,  // 473
  0x00, 0x00, 0x00, 0x00, 0x1f, 0x18, 0x18, 0x18,  // 474
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 475
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,  // 476
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,  // 477
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,  // 478
  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,  // 479
  0x00, 0x00, 0x76, 0xdc, 0xc8, 0xdc, 0x76, 0x00,  // 480
  0x00, 0x78, 0xcc, 0xf8, 0xcc, 0xf8, 0xc0, 0xc0,  // 481
  0x00, 0xfc, 0xcc, 0xc0, 0xc0, 0xc0, 0xc0, 0x00,  // 482
  0x00, 0xfe, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x00,  // 483
  0xfc, 0xcc, 0x60, 0x30, 0x60, 0xcc, 0xfc, 0x00,  // 484
  0x00, 0x00, 0x7e, 0xd8, 0xd8, 0xd8, 0x70, 0x00,  // 485
  0x00, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0xc0,  // 486
  0x00, 0x76, 0xdc, 0x18, 0x18, 0x18, 0x18, 0x00,  // 487
  0xfc, 0x30, 0x78, 0xcc, 0xcc, 0x78, 0x30, 0xfc,  // 488
  0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0x6c, 0x38, 0x00,  // 489
  0x38, 0x6c, 0xc6, 0xc6, 0x6c, 0x6c, 0xee, 0x00,  // 490
  0x1c, 0x30, 0x18, 0x7c, 0xcc, 0xcc, 0x78, 0x00,  // 491
  0x00, 0x00, 0x7e, 0xdb, 0xdb, 0x7e, 0x00, 0x00,  // 492
  0x06, 0x0c, 0x7e, 0xdb, 0xdb, 0x7e, 0x60, 0xc0,  // 493
  0x38, 0x60, 0xc0, 0xf8, 0xc0, 0x60, 0x38, 0x00,  // 494
  0x78, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x00,  // 495
  0x00, 0xfc, 0x00, 0xfc, 0x00, 0xfc, 0x00, 0x00,  // 496
  0x30, 0x30, 0xfc, 0x30, 0x30, 0x00, 0xfc, 0x00,  // 497
  0x60, 0x30, 0x18, 0x30, 0x60, 0x00, 0xfc, 0x00,  // 498
  0x18, 0x30, 0x60, 0x30, 0x18, 0x00, 0xfc, 0x00,  // 499
  0x0e, 0x1b, 0x1b, 0x18, 0x18, 0x18, 0x18, 0x18,  // 500
  0x18, 0x18, 0x18, 0x18, 0x18, 0xd8, 0xd8, 0x70,  // 501
  0x30, 0x30, 0x00, 0xfc, 0x00, 0x30, 0x30, 0x00,  // 502
  0x00, 0x76, 0xdc, 0x00, 0x76, 0xdc, 0x00, 0x00,  // 503
  0x38, 0x6c, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00,  // 504
  0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,  // 505
  0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,  // 506
  0x0f, 0x0c, 0x0c, 0x0c, 0xec, 0x6c, 0x3c, 0x1c,  // 507
  0x78, 0x6c, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00,  // 508
  0x70, 0x18, 0x30, 0x60, 0x78, 0x00, 0x00, 0x00,  // 509
  0x00, 0x00, 0x3c, 0x3c, 0x3c, 0x3c, 0x00, 0x00,  // 510
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 511
};


void font_draw_glyph_8x8(
  uint32_t *dst, const uint32_t pitch, uint16_t ch,
  const uint32_t rgb_a, const uint32_t rgb_b)
{
  const uint8_t *src = &cga_font_8x8[(ch & 0x1ff) * 8];
  for (int y=0; y<8; ++y) {
    uint8_t mask = *src;
    for (int x=0; x<8; ++x) {
      dst[x] = (mask & 0x80) ? rgb_a : rgb_b;
      mask <<= 1;
    }
    ++src;
    dst += pitch;
  }
}

void font_draw_glyph_8x16(
  uint32_t *dst, const uint32_t pitch, uint16_t ch,
  const uint32_t rgb_a, const uint32_t rgb_b)
{
  //note: there is a font in the bios at 0xffa6e

  const uint8_t *src = &cga_font_8x8[(ch & 0x1ff) * 8];

  for (int y=0; y<16; ++y) {
    uint8_t mask = *src;
    for (int x=0; x<8; ++x) {
      dst[x] = (mask & 0x80) ? rgb_a : rgb_b;
      mask <<= 1;
    }
    src += (y & 1);
    dst += pitch;
  }
}

void font_draw_glyph_8x16_gliss(
  uint32_t *dst, const uint32_t pitch, uint16_t ch, uint32_t rgb)
{
  const uint8_t *src = &cga_font_8x8[(ch & 0x1ff) * 8];

  rgb = (rgb >> 1) & 0x7f7f7f;

  for (int y=0; y<16; ++y) {
    uint8_t mask = *src;
    for (int x=0; x<8; ++x) {
      const uint32_t clr = (mask & 0x80) ? rgb : 0;
      dst[x] = ((dst[x] >> 2) & 0x3f3f3f) + clr;
      mask <<= 1;
    }
    src += (y & 1);
    dst += pitch;
  }
}
