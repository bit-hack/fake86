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


// cga mode 4 palette
const uint32_t palette_cga_4_rgb[] = {
  0x000000, 0x00AAAA, 0xAA00AA, 0xAAAAAA,
};

// cga 16 shade palette (text mode)
const uint32_t palette_cga_2_rgb[16] = {
  0x000000,
  0x393939,
  0x818181,
  0x8d8d8d,
  0x5c5c5c,
  0x6c6c6c,
  0x717171,
  0xa9a9a9,
  0x545454,
  0x757575,
  0xcacaca,
  0xd9d9d9,
  0x9b9b9b,
  0xafafaf,
  0xf0f0f0,
  0xfefefe,
};

// cga 16 colour palette (text mode)
const uint32_t palette_cga_3_rgb[16] = {
  0x000000,
  0x0000aa,
  0x00aa00,
  0x00aaaa,
  0xaa0000,
  0xaa00aa,
  0xaa5500,
  0xaaaaaa,
  0x555555,
  0x5555ff,
  0x55ff55,
  0x55ffff,
  0xff5555,
  0xff55ff,
  0xffff55,
  0xffffff
};

// TODO: flip this to native ARGB format, (currently in BGA format)
uint32_t palettecga[] = {
  0x00000000,
  0x00aa0000,
  0x0000aa00,
  0x00aaaa00,
  0x000000aa,
  0x00aa00aa,
  0x000055aa,
  0x00aaaaaa,
  0x00555555,
  0x00ff5555,
  0x0055ff55,
  0x00ffff55,
  0x005555ff,
  0x00ff55ff,
  0x0055ffff,
  0x00ffffff
};

const uint32_t palette_vga_16_rgb[] = {
  0x000000,
  0x0000a9,
  0x00a900,
  0x00a9a9,
  0xa90000,
  0xa900a9,
  0xa9a900,
  0xa9a9a9,
  0x000054,
  0x0000ff,
  0x00a954,
  0x00a9ff,
  0xa90054,
  0xa900ff,
  0xa9a954,
  0xa9a9ff,
};

// TODO: flip this to native ARGB format, (currently in BGA format)
uint32_t palettevga[] = {
  0x00000000,
  0x00a90000,
  0x0000a900,
  0x00a9a900,
  0x000000a9,
  0x00a900a9,
  0x0000a9a9,
  0x00a9a9a9,
  0x00540000,
  0x00ff0000,
  0x0054a900,
  0x00ffa900,
  0x005400a9,
  0x00ff00a9,
  0x0054a9a9,
  0x00ffa9a9,
  0x00005400,
  0x00a95400,
  0x0000ff00,
  0x00a9ff00,
  0x000054a9,
  0x00a954a9,
  0x0000ffa9,
  0x00a9ffa9,
  0x00545400,
  0x00ff5400,
  0x0054ff00,
  0x00ffff00,
  0x005454a9,
  0x00ff54a9,
  0x0054ffa9,
  0x00ffffa9,
  0x00000054,
  0x00a90054,
  0x0000a954,
  0x00a9a954,
  0x000000ff,
  0x00a900ff,
  0x0000a9ff,
  0x00a9a9ff,
  0x00540054,
  0x00ff0054,
  0x0054a954,
  0x00ffa954,
  0x005400ff,
  0x00ff00ff,
  0x0054a9ff,
  0x00ffa9ff,
  0x00005454,
  0x00a95454,
  0x0000ff54,
  0x00a9ff54,
  0x000054ff,
  0x00a954ff,
  0x0000ffff,
  0x00a9ffff,
  0x00545454,
  0x00ff5454,
  0x0054ff54,
  0x00ffff54,
  0x005454ff,
  0x00ff54ff,
  0x0054ffff,
  0x00ffffff,
  0x007d7dff,
  0x007d9dff,
  0x007dbeff,
  0x007ddeff,
  0x007dffff,
  0x007dffde,
  0x007dffbe,
  0x007dff9d,
  0x007dff7d,
  0x009dff7d,
  0x00beff7d,
  0x00deff7d,
  0x00ffff7d,
  0x00ffde7d,
  0x00ffbe7d,
  0x00ff9d7d,
  0x00ffb6b6,
  0x00ffb6c6,
  0x00ffb6da,
  0x00ffb6ea,
  0x00ffb6ff,
  0x00eab6ff,
  0x00dab6ff,
  0x00c6b6ff,
  0x00b6b6ff,
  0x00b6c6ff,
  0x00b6daff,
  0x00b6eaff,
  0x00b6ffff,
  0x00b6ffea,
  0x00b6ffda,
  0x00b6ffc6,
  0x00b6ffb6,
  0x00c6ffb6,
  0x00daffb6,
  0x00eaffb6,
  0x00ffffb6,
  0x00ffeab6,
  0x00ffdab6,
  0x00ffc6b6,
  0x00710000,
  0x0071001c,
  0x00710038,
  0x00710054,
  0x00710071,
  0x00540071,
  0x00380071,
  0x001c0071,
  0x00000071,
  0x00001c71,
  0x00003871,
  0x00005471,
  0x00007171,
  0x00007154,
  0x00007138,
  0x0000711c,
  0x00007100,
  0x001c7100,
  0x00387100,
  0x00547100,
  0x00717100,
  0x00715400,
  0x00713800,
  0x00711c00,
  0x00713838,
  0x00713844,
  0x00713854,
  0x00713861,
  0x00713871,
  0x00613871,
  0x00543871,
  0x00443871,
  0x00383871,
  0x00384471,
  0x00385471,
  0x00386171,
  0x00387171,
  0x00387161,
  0x00387154,
  0x00387144,
  0x00387138,
  0x00447138,
  0x00547138,
  0x00617138,
  0x00717138,
  0x00716138,
  0x00715438,
  0x00714438,
  0x00715050,
  0x00715059,
  0x00715061,
  0x00715069,
  0x00715071,
  0x00695071,
  0x00615071,
  0x00595071,
  0x00505071,
  0x00505971,
  0x00506171,
  0x00506971,
  0x00507171,
  0x00507169,
  0x00507161,
  0x00507159,
  0x00507150,
  0x00597150,
  0x00617150,
  0x00697150,
  0x00717150,
  0x00716950,
  0x00716150,
  0x00715950,
  0x00400000,
  0x00400010,
  0x00400020,
  0x00400030,
  0x00400040,
  0x00300040,
  0x00200040,
  0x00100040,
  0x00000040,
  0x00001040,
  0x00002040,
  0x00003040,
  0x00004040,
  0x00004030,
  0x00004020,
  0x00004010,
  0x00004000,
  0x00104000,
  0x00204000,
  0x00304000,
  0x00404000,
  0x00403000,
  0x00402000,
  0x00401000,
  0x00402020,
  0x00402028,
  0x00402030,
  0x00402038,
  0x00402040,
  0x00382040,
  0x00302040,
  0x00282040,
  0x00202040,
  0x00202840,
  0x00203040,
  0x00203840,
  0x00204040,
  0x00204038,
  0x00204030,
  0x00204028,
  0x00204020,
  0x00284020,
  0x00304020,
  0x00384020,
  0x00404020,
  0x00403820,
  0x00403020,
  0x00402820,
  0x00402c2c,
  0x00402c30,
  0x00402c34,
  0x00402c3c,
  0x00402c40,
  0x003c2c40,
  0x00342c40,
  0x00302c40,
  0x002c2c40,
  0x002c3040,
  0x002c3440,
  0x002c3c40,
  0x002c4040,
  0x002c403c,
  0x002c4034,
  0x002c4030,
  0x002c402c,
  0x0030402c,
  0x0034402c,
  0x003c402c,
  0x0040402c,
  0x00403c2c,
  0x0040342c,
  0x0040302c,
  0x00000000,
  0x00000000,
  0x00000000,
  0x00000000,
  0x00000000,
  0x00000000,
  0x00000000,
  0x00000000
};