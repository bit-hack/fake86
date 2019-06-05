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

#include "cpu.h"
#include "../common/common.h"
#include "../external/udis86/udis86.h"


enum ir_type_t {
  // end interpretation
  opc_finish,

  // no operation
  opc_nop,

  // reg[ op0 ] = reg[ op1 ]
  opc_mov,

  // reg[ op0 ] = (op1 << 8) | op2
  opc_mov_imm,

  // reg[ op0 ] = memory[ (reg[op1] << 4) + reg[op2] ]
  opc_load_b,

  // memory[ (reg[op0] << 4) + re[op1] ] = reg[ op2 ]
  opc_store_b,

  // reg[ op0 ] = memory[ (reg[op1] << 4) + reg[op2] ]
  // op2 += 1
  opc_pop_b,

  // memory[ (reg[op0] << 4) + reg[op1] ] = reg[ op2 ]
  // op2 -= 1
  opc_push_b,

  // reg[ op0 ] = memory[ (reg[op1] << 4) + reg[op2] ]
  // op2 += 2
  opc_pop_w,

  // memory[ (reg[op0] << 4) + reg[op1] ] = reg[ op2 ]
  // op2 -= 2
  opc_push_w,

  // reg[ op0 ] += (op1 << 8) | op2
  opc_add_imm,
};

enum ir_reg_t {
  // 8bit low byte
  reg_al,  reg_dl,  reg_bl,  reg_cl,
  // 8bit high byte
  reg_ah,  reg_dh,  reg_bh,  reg_ch,
  // 16bit word
  reg_ax,  reg_dx,  reg_bx,  reg_cx,
  // 16bit instruction pointer
  reg_ip,
  // stack pointers
  reg_sp,  reg_bp,
  // index registers
  reg_si,  reg_di,
  // segment registers
  reg_ss,  reg_es,  reg_cs,  reg_ds,
  // 8bit temporaries
  reg_b0,  reg_b1,  reg_b2,  reg_b3,
  // 16bit temporaries
  reg_s0,  reg_s1,  reg_s2,  reg_s3,
  // 32bit temporaries
  reg_d0,  reg_d1,  reg_d2,  reg_d3,
  // flag registers
  reg_fl
};

struct ir_inst_t {
  uint8_t type;
  uint8_t opr[3];
};

static struct ir_inst_t _ir_buf[1028];
static uint32_t _ir_head;


static void _exec_ir(void) {
  for (int i=0; ;++i) {
    switch (_ir_buf[i].type) {
    case opc_finish:
      return;
    case opc_nop:
      continue;
    }
  }
}

static enum ir_reg_t map_reg(enum ud_type reg) {
  switch (reg) {
  case UD_R_AL: return reg_al;
  case UD_R_CL: return reg_cl;
  case UD_R_DL: return reg_dl;
  case UD_R_BL: return reg_bl;
  case UD_R_AH: return reg_ah;
  case UD_R_CH: return reg_ch;
  case UD_R_DH: return reg_dh;
  case UD_R_BH: return reg_bh;
  case UD_R_AX: return reg_ax;
  case UD_R_CX: return reg_cx;
  case UD_R_DX: return reg_dx;
  case UD_R_BX: return reg_bx;
  case UD_R_SP: return reg_sp;
  case UD_R_BP: return reg_bp;
  case UD_R_SI: return reg_si;
  case UD_R_DI: return reg_di;
  case UD_R_ES: return reg_es;
  case UD_R_CS: return reg_cs;
  case UD_R_SS: return reg_ss;
  case UD_R_DS: return reg_ds;
  default:
    UNREACHABLE();
  };
}

static _inst_push(uint8_t type, uint8_t op0, uint8_t op1, uint8_t op2) {
  _ir_buf[_ir_head].type = type;
  _ir_buf[_ir_head].opr[0] = op0;
  _ir_buf[_ir_head].opr[1] = op1;
  _ir_buf[_ir_head].opr[2] = op2;
  ++_ir_head;
}

static bool _on_inst_mov(const ud_t *ud) {
  // https://c9x.me/x86/html/file_module_x86_id_176.html
  switch (ud->primary_opcode) {
  case 0x88:
  case 0x89:
  case 0x8A:
  case 0x8B:
  case 0x8C:
  case 0x8E:
  case 0xA0:
  case 0xA1:
  case 0xA2:
  case 0xA3:
  case 0xB0:
  case 0xB8:
  case 0xC6:
  case 0xC7:
    printf("\t%s\n", ud_insn_asm(ud));
    return false;
  default:
    UNREACHABLE();
  }

  // step the IP
  _inst_push(opc_add_imm, reg_ip, 0, ud->pc);
  return true;
}

bool cpu_udis_exec(const uint8_t *stream) {

  ud_t ud_obj;
  ud_init(&ud_obj);
  ud_set_input_buffer(&ud_obj, stream, 16);
  ud_set_mode(&ud_obj, 16);
  ud_set_syntax(&ud_obj, UD_SYN_INTEL);

  if (!ud_disassemble(&ud_obj)) {
    printf("bad disassembly!");
    UNREACHABLE();
  }

  switch (ud_obj.mnemonic) {
  case UD_Imov:
    _on_inst_mov(&ud_obj);
    break;

  default:
    return false;
  }

  //TODO: excute the code buffer
  _exec_ir();

  return true;
}
