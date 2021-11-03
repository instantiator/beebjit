#include "jit_optimizer.h"

#include "defs_6502.h"
#include "jit_opcode.h"
#include "asm/asm_jit.h"
#include "asm/asm_opcodes.h"

#include <assert.h>
#include <string.h>

/* TODO: replace direct references to defs_6502_get_6502_optype_map(). */

static const int32_t k_value_unknown = -1;

static void
jit_optimizer_eliminate(struct jit_opcode_details** pp_elim_opcode,
                        struct asm_uop* p_elim_uop,
                        struct jit_opcode_details* p_curr_opcode) {
  struct jit_opcode_details* p_elim_opcode = *pp_elim_opcode;

  *pp_elim_opcode = NULL;

  assert(!p_elim_uop->is_eliminated);
  p_elim_uop->is_eliminated = 1;

  p_elim_opcode += p_elim_opcode->num_bytes_6502;
  while (p_elim_opcode <= p_curr_opcode) {
    uint32_t num_fixup_uops = p_elim_opcode->num_fixup_uops;
    assert(p_elim_opcode->addr_6502 != -1);
    assert(num_fixup_uops < k_max_uops_per_opcode);
    /* Prepend the elimination so that fixups are applied in order of last
     * fixup first. This is important because some fixups trash each other,
     * such as FLAGX trashing any pending SAVE_CARRY.
     */
    (void) memmove(&p_elim_opcode->fixup_uops[1],
                   &p_elim_opcode->fixup_uops[0],
                   (sizeof(struct asm_uop*) * num_fixup_uops));
    p_elim_opcode->fixup_uops[0] = p_elim_uop;
    p_elim_opcode->num_fixup_uops++;
    p_elim_opcode += p_elim_opcode->num_bytes_6502;
  }
}

static int
jit_optimizer_uopcode_can_jump(int32_t uopcode) {
  int ret = 0;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    uint8_t opbranch = g_opbranch[optype];
    if (opbranch != k_bra_n) {
      ret = 1;
    }
  } else {
    switch (uopcode) {
    case k_opcode_JMP_SCRATCH_n:
      ret = 1;
      break;
    default:
      break;
    }
  }
  return ret;
}

static int
jit_optimizer_uop_could_write(struct asm_uop* p_uop, uint16_t addr) {
  int32_t write_addr_start = -1;
  int32_t write_addr_end = -1;

  int32_t uopcode = p_uop->uopcode;

  if (uopcode <= 0xFF) {
    uint8_t opmode = defs_6502_get_6502_opmode_map()[uopcode];
    uint8_t opmem = defs_6502_get_6502_opmem_map()[uopcode];
    if (opmem & k_opmem_write_flag) {
      switch (opmode) {
      case k_zpg:
      case k_abs:
        write_addr_start = p_uop->value1;
        write_addr_end = p_uop->value1;
        break;
      case k_abx:
      case k_aby:
      case k_idx:
      case k_idy:
      case k_zpx:
      case k_zpy:
        /* NOTE: could be more refined with at least abx, aby. */
        write_addr_start = 0;
        write_addr_end = (k_6502_addr_space_size - 1);
      default:
        break;
      }
    }
  } else {
    switch (uopcode) {
    case k_opcode_STOA_IMM:
      write_addr_start = p_uop->value1;
      write_addr_end = p_uop->value1;
      break;
    case k_opcode_STA_SCRATCH_n:
      write_addr_start = 0;
      write_addr_end = (k_6502_addr_space_size - 1);
      break;
    default:
      break;
    }
  }

  if (write_addr_start == -1) {
    return 0;
  }
  if (addr >= write_addr_start && addr <= write_addr_end) {
    return 1;
  }
  return 0;
}

static int
jit_optimizer_uopcode_sets_nz_flags(int32_t uopcode) {
  int ret;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    ret = g_optype_changes_nz_flags[optype];
  } else {
    switch (uopcode) {
    case k_opcode_ADD_ABS:
    case k_opcode_ADD_ABX:
    case k_opcode_ADD_ABY:
    case k_opcode_ADD_IMM:
    case k_opcode_ADD_SCRATCH:
    case k_opcode_ADD_SCRATCH_Y:
    case k_opcode_ASL_ACC_n:
    case k_opcode_EOR_SCRATCH_n:
    case k_opcode_FLAGA:
    case k_opcode_FLAGX:
    case k_opcode_FLAGY:
    case k_opcode_FLAG_MEM:
    case k_opcode_LDA_SCRATCH_n:
    case k_opcode_LDA_Z:
    case k_opcode_LDX_Z:
    case k_opcode_LDY_Z:
    case k_opcode_LSR_ACC_n:
    case k_opcode_SUB_ABS:
    case k_opcode_SUB_IMM:
      ret = 1;
      break;
    default:
      ret = 0;
      break;
    }
  }
  return ret;
}

static int
jit_optimizer_uopcode_needs_nz_flags(int32_t uopcode) {
  if (jit_optimizer_uopcode_can_jump(uopcode)) {
    return 1;
  }
  switch (uopcode) {
  case 0x08: /* PHP */
    return 1;
  default:
    return 0;
  }
}

static int
jit_optimizer_uopcode_overwrites_a(int32_t uopcode) {
  int ret = 0;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    switch (optype) {
    case k_lda:
    case k_pla:
    case k_txa:
    case k_tya:
      ret = 1;
      break;
    default:
      break;
    }
  } else {
    switch (uopcode) {
    case k_opcode_LDA_SCRATCH_n:
    case k_opcode_LDA_Z:
      ret = 1;
      break;
    default:
      break;
    }
  }
  return ret;
}

static int
jit_optimizer_uopcode_needs_a(int32_t uopcode) {
  int ret = 0;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    uint8_t opmode = defs_6502_get_6502_opmode_map()[uopcode];
    switch (optype) {
    case k_ora:
    case k_and:
    case k_bit:
    case k_eor:
    case k_pha:
    case k_adc:
    case k_sta:
    case k_tay:
    case k_tax:
    case k_cmp:
    case k_sbc:
    case k_sax:
    case k_alr:
    case k_slo:
      ret = 1;
      break;
    case k_asl:
    case k_rol:
    case k_lsr:
    case k_ror:
      if (opmode == k_acc) {
        ret = 1;
      }
      break;
    default:
      break;
    }
  } else {
    switch (uopcode) {
    case k_opcode_ADD_ABS:
    case k_opcode_ADD_ABX:
    case k_opcode_ADD_ABY:
    case k_opcode_ADD_IMM:
    case k_opcode_ADD_SCRATCH:
    case k_opcode_ADD_SCRATCH_Y:
    case k_opcode_ASL_ACC_n:
    case k_opcode_FLAGA:
    case k_opcode_LSR_ACC_n:
    case k_opcode_ROL_ACC_n:
    case k_opcode_ROR_ACC_n:
    case k_opcode_STA_SCRATCH_n:
    case k_opcode_SUB_ABS:
    case k_opcode_SUB_IMM:
      ret = 1;
      break;
    default:
      break;
    }
  }
  return ret;
}

static int
jit_optimizer_uopcode_overwrites_x(int32_t uopcode) {
  int ret = 0;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    switch (optype) {
    case k_ldx:
    case k_tax:
    case k_tsx:
      ret = 1;
      break;
    default:
      break;
    }
  } else {
    switch (uopcode) {
    case k_opcode_LDX_Z:
      ret = 1;
      break;
    default:
      break;
    }
  }
  return ret;
}

static int
jit_optimizer_uopcode_needs_x(int32_t uopcode) {
  int ret = 0;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    uint8_t opmode = defs_6502_get_6502_opmode_map()[uopcode];
    switch (optype) {
    case k_stx:
    case k_txa:
    case k_txs:
    case k_cpx:
    case k_dex:
    case k_inx:
    case k_sax:
      ret = 1;
      break;
    default:
      break;
    }
    switch (opmode) {
    case k_zpx:
    case k_abx:
    case k_idx:
      ret = 1;
      break;
    default:
      break;
    }
  } else {
    switch (uopcode) {
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_X:
    case k_opcode_CHECK_PAGE_CROSSING_X_n:
    case k_opcode_ADD_ABX:
    case k_opcode_FLAGX:
    case k_opcode_MODE_ABX:
    case k_opcode_MODE_ZPX:
      ret = 1;
      break;
    default:
      break;
    }
  }
  return ret;
}

static int
jit_optimizer_uopcode_overwrites_y(int32_t uopcode) {
  int ret = 0;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    switch (optype) {
    case k_ldy:
    case k_tay:
      ret = 1;
      break;
    default:
      break;
    }
  } else {
    switch (uopcode) {
    case k_opcode_LDY_Z:
      ret = 1;
      break;
    default:
      break;
    }
  }
  return ret;
}

static int
jit_optimizer_uopcode_needs_y(int32_t uopcode) {
  int ret = 0;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    uint8_t opmode = defs_6502_get_6502_opmode_map()[uopcode];
    switch (optype) {
    case k_sty:
    case k_dey:
    case k_tya:
    case k_cpy:
    case k_iny:
    case k_shy:
      ret = 1;
      break;
    default:
      break;
    }
    switch (opmode) {
    case k_zpy:
    case k_aby:
    case k_idy:
      ret = 1;
      break;
    default:
      break;
    }
  } else {
    switch (uopcode) {
    case k_opcode_ADD_ABY:
    case k_opcode_ADD_SCRATCH_Y:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_Y:
    case k_opcode_CHECK_PAGE_CROSSING_Y_n:
    case k_opcode_FLAGY:
    case k_opcode_MODE_ABY:
    case k_opcode_MODE_ZPY:
    case k_opcode_WRITE_INV_SCRATCH_Y:
      ret = 1;
      break;
    default:
      break;
    }
  }
  return ret;
}

static int
jit_optimizer_uop_idy_match(struct asm_uop* p_uop, struct asm_uop* p_idy_uop) {
  if ((p_uop->uopcode == k_opcode_MODE_IND_8) &&
      (p_uop->value1 == p_idy_uop->value1)) {
    return 1;
  }
  return 0;
}

static int
jit_optimizer_uop_invalidates_idy(struct asm_uop* p_uop,
                                  struct asm_uop* p_idy_uop) {
  int ret = 1;
  int32_t uopcode = p_uop->uopcode;

  /* If this opcode could write to the idy indirect address, we must
   * invalidate.
   */
  if (jit_optimizer_uop_could_write(p_uop, p_idy_uop->value1) ||
      jit_optimizer_uop_could_write(p_uop, (uint8_t) (p_idy_uop->value1 + 1))) {
    return 1;
  }

  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    switch (optype) {
    case k_nop:
    case k_adc:
    case k_and:
    case k_cmp:
    case k_eor:
    case k_ora:
    case k_lda:
    case k_ldx:
    case k_ldy:
    case k_sbc:
    case k_sta:
    case k_stx:
    case k_sty:
    case k_inx:
    case k_dex:
    case k_iny:
    case k_dey:
    case k_tax:
    case k_txa:
    case k_tay:
    case k_tya:
      ret = 0;
      break;
    default:
      break;
    }
  } else {
    switch (uopcode) {
    case k_opcode_debug:
    case k_opcode_ADD_ABS:
    case k_opcode_ADD_ABX:
    case k_opcode_ADD_ABY:
    case k_opcode_ADD_IMM:
    case k_opcode_ADD_SCRATCH:
    case k_opcode_ADD_SCRATCH_Y:
    case k_opcode_CHECK_BCD:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_n:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_X:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_Y:
    case k_opcode_CHECK_PAGE_CROSSING_X_n:
    case k_opcode_CHECK_PAGE_CROSSING_Y_n:
    case k_opcode_EOR_SCRATCH_n:
    case k_opcode_FLAGA:
    case k_opcode_FLAGX:
    case k_opcode_FLAGY:
    case k_opcode_FLAG_MEM:
    case k_opcode_LDA_SCRATCH_n:
    case k_opcode_LDA_Z:
    case k_opcode_LDX_Z:
    case k_opcode_LDY_Z:
    case k_opcode_LOAD_CARRY_FOR_BRANCH:
    case k_opcode_LOAD_CARRY_FOR_CALC:
    case k_opcode_LOAD_CARRY_INV_FOR_CALC:
    case k_opcode_LOAD_OVERFLOW:
    case k_opcode_SAVE_CARRY:
    case k_opcode_SAVE_CARRY_INV:
    case k_opcode_SAVE_OVERFLOW:
    case k_opcode_STOA_IMM:
    case k_opcode_SUB_ABS:
    case k_opcode_SUB_IMM:
      ret = 0;
      break;
    default:
      break;
    }
  }
  return ret;
}

/* TODO: these lists are duplicative and awful. Improve. */
static int
jit_optimizer_uopcode_needs_or_trashes_overflow(int32_t uopcode) {
  /* Many things need or trash overflow so we'll just enumerate what's safe. */
  int ret = 1;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    switch (optype) {
    case k_nop:
    case k_adc:
    case k_sbc:
    case k_bit:
    case k_clv:
    case k_lda:
    case k_ldx:
    case k_ldy:
    case k_sta:
    case k_stx:
    case k_sty:
    case k_sec:
    case k_clc:
    case k_pla:
    case k_pha:
    case k_tax:
    case k_tay:
    case k_txa:
    case k_tya:
      ret = 0;
      break;
    default:
      break;
    }
  } else {
    switch (uopcode) {
    case k_opcode_debug:
    case k_opcode_ADD_ABS:
    case k_opcode_ADD_ABX:
    case k_opcode_ADD_ABY:
    case k_opcode_ADD_IMM:
    case k_opcode_ADD_SCRATCH:
    case k_opcode_ADD_SCRATCH_Y:
    case k_opcode_CHECK_BCD:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_n:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_X:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_Y:
    case k_opcode_CHECK_PAGE_CROSSING_X_n:
    case k_opcode_CHECK_PAGE_CROSSING_Y_n:
    case k_opcode_CHECK_PENDING_IRQ:
    case k_opcode_LDA_SCRATCH_n:
    case k_opcode_LOAD_CARRY_FOR_BRANCH:
    case k_opcode_LOAD_CARRY_FOR_CALC:
    case k_opcode_LOAD_CARRY_INV_FOR_CALC:
    case k_opcode_LOAD_SCRATCH_8:
    case k_opcode_LOAD_SCRATCH_16:
    case k_opcode_MODE_ABX:
    case k_opcode_MODE_ABY:
    case k_opcode_MODE_IND_8:
    case k_opcode_MODE_IND_16:
    case k_opcode_MODE_IND_SCRATCH_8:
    case k_opcode_MODE_IND_SCRATCH_16:
    case k_opcode_MODE_ZPX:
    case k_opcode_MODE_ZPY:
    case k_opcode_SAVE_CARRY:
    case k_opcode_SAVE_CARRY_INV:
    case k_opcode_SAVE_OVERFLOW:
    case k_opcode_STA_SCRATCH_n:
    case k_opcode_STOA_IMM:
    case k_opcode_SUB_ABS:
    case k_opcode_SUB_IMM:
    case k_opcode_WRITE_INV_ABS:
    case k_opcode_WRITE_INV_SCRATCH:
    case k_opcode_WRITE_INV_SCRATCH_n:
    case k_opcode_WRITE_INV_SCRATCH_Y:
      ret = 0;
      break;
    default:
      break;
    }
  }
  return ret;
}

static int
jit_optimizer_uopcode_needs_or_trashes_carry(int32_t uopcode) {
  /* Many things need or trash carry so we'll just enumerate what's safe. */
  int ret = 1;
  if (uopcode <= 0xFF) {
    uint8_t optype = defs_6502_get_6502_optype_map()[uopcode];
    switch (optype) {
    case k_nop:
    case k_cmp:
    case k_cpx:
    case k_cpy:
    case k_inx:
    case k_dex:
    case k_iny:
    case k_dey:
    case k_asl:
    case k_lsr:
    case k_lda:
    case k_ldx:
    case k_ldy:
    case k_sta:
    case k_stx:
    case k_sty:
    case k_sec:
    case k_clc:
    case k_pla:
    case k_pha:
    case k_tax:
    case k_tay:
    case k_txa:
    case k_tya:
      ret = 0;
      break;
    default:
      break;
    }
  } else {
    switch (uopcode) {
    case k_opcode_debug:
    case k_opcode_ADD_ABS:
    case k_opcode_ADD_ABX:
    case k_opcode_ADD_ABY:
    case k_opcode_ADD_IMM:
    case k_opcode_ADD_SCRATCH:
    case k_opcode_ADD_SCRATCH_Y:
    case k_opcode_ASL_ACC_n:
    case k_opcode_CHECK_BCD:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_n:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_X:
    case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_Y:
    case k_opcode_CHECK_PAGE_CROSSING_X_n:
    case k_opcode_CHECK_PAGE_CROSSING_Y_n:
    case k_opcode_CHECK_PENDING_IRQ:
    case k_opcode_LDA_SCRATCH_n:
    case k_opcode_LOAD_CARRY_FOR_BRANCH:
    case k_opcode_LOAD_CARRY_FOR_CALC:
    case k_opcode_LOAD_CARRY_INV_FOR_CALC:
    case k_opcode_LOAD_SCRATCH_8:
    case k_opcode_LOAD_SCRATCH_16:
    case k_opcode_LSR_ACC_n:
    case k_opcode_MODE_ABX:
    case k_opcode_MODE_ABY:
    case k_opcode_MODE_IND_8:
    case k_opcode_MODE_IND_16:
    case k_opcode_MODE_IND_SCRATCH_8:
    case k_opcode_MODE_IND_SCRATCH_16:
    case k_opcode_MODE_ZPX:
    case k_opcode_MODE_ZPY:
    case k_opcode_SAVE_CARRY:
    case k_opcode_SAVE_CARRY_INV:
    case k_opcode_SAVE_OVERFLOW:
    case k_opcode_STA_SCRATCH_n:
    case k_opcode_STOA_IMM:
    case k_opcode_SUB_ABS:
    case k_opcode_SUB_IMM:
    case k_opcode_WRITE_INV_ABS:
    case k_opcode_WRITE_INV_SCRATCH:
    case k_opcode_WRITE_INV_SCRATCH_n:
    case k_opcode_WRITE_INV_SCRATCH_Y:
      ret = 0;
      break;
    default:
      break;
    }
  }
  return ret;
}

static void
jit_optimizer_append_uop(struct jit_opcode_details* p_opcode,
                         int32_t uopcode) {
  uint8_t num_uops = p_opcode->num_uops;
  struct asm_uop* p_uop = &p_opcode->uops[num_uops];
  assert(num_uops < k_max_uops_per_opcode);
  if (p_opcode->has_postfix_uop) {
    (void) memcpy(p_uop, (p_uop - 1), sizeof(struct asm_uop));
    p_uop--;
  }
  p_uop->uopcode = uopcode;
  p_uop->value1 = 0;
  p_uop->value2 = 0;
  p_uop->is_eliminated = 0;

  p_opcode->num_uops++;
}

struct jit_opcode_details*
jit_optimizer_optimize(struct jit_opcode_details* p_opcodes) {
  int32_t reg_a;
  int32_t reg_x;
  int32_t reg_y;
  int32_t flag_carry;
  int32_t flag_decimal;
  int had_bcd_check;

  struct jit_opcode_details* p_opcode;
  struct jit_opcode_details* p_prev_opcode;

  struct jit_opcode_details* p_nz_flags_opcode;
  struct asm_uop* p_nz_flags_uop;
  struct jit_opcode_details* p_lda_opcode;
  struct asm_uop* p_lda_uop;
  struct jit_opcode_details* p_ldx_opcode;
  struct asm_uop* p_ldx_uop;
  struct jit_opcode_details* p_ldy_opcode;
  struct asm_uop* p_ldy_uop;
  struct jit_opcode_details* p_idy_opcode;
  struct asm_uop* p_idy_uop;
  struct jit_opcode_details* p_overflow_opcode;
  struct asm_uop* p_overflow_uop;
  struct jit_opcode_details* p_carry_write_opcode;
  struct asm_uop* p_carry_write_uop;
  int carry_flipped_for_branch;

  int has_STOA = asm_jit_supports_uopcode(k_opcode_STOA_IMM);

  /* Pass 1: tag opcodes with any known register and flag values. */
  /* TODO: this pass operates on 6502 opcodes but it should probably work on
   * uopcodes, because previous passes may change the characteristic of
   * 6502 opcodes.
   * One example is LDY imm -> dynamic operand conversion, which no longer
   * results in "known Y".
   */
  reg_a = k_value_unknown;
  reg_x = k_value_unknown;
  reg_y = k_value_unknown;
  flag_carry = k_value_unknown;
  flag_decimal = k_value_unknown;

  for (p_opcode = p_opcodes;
       p_opcode->addr_6502 != -1;
       p_opcode += p_opcode->num_bytes_6502) {
    uint8_t opcode_6502 = p_opcode->opcode_6502;
    uint16_t operand_6502 = p_opcode->operand_6502;
    uint8_t optype = defs_6502_get_6502_optype_map()[opcode_6502];
    uint8_t opreg = g_optype_sets_register[optype];
    uint8_t opmode = defs_6502_get_6502_opmode_map()[opcode_6502];
    int changes_carry = g_optype_changes_carry[optype];

    /* TODO: seems hacky, should g_optype_sets_register just be per-opcode? */
    if (opmode == k_acc) {
      opreg = k_a;
    }

    p_opcode->reg_a = reg_a;
    p_opcode->reg_x = reg_x;
    p_opcode->reg_y = reg_y;
    p_opcode->flag_carry = flag_carry;
    p_opcode->flag_decimal = flag_decimal;

    switch (opcode_6502) {
    case 0x18: /* CLC */
    case 0xB0: /* BCS */
      flag_carry = 0;
      break;
    case 0x38: /* SEC */
    case 0x90: /* BCC */
      flag_carry = 1;
      break;
    case 0x88: /* DEY */
      if (reg_y != k_value_unknown) {
        reg_y = (uint8_t) (reg_y - 1);
      }
      break;
    case 0x8A: /* TXA */
      reg_a = reg_x;
      break;
    case 0x98: /* TYA */
      reg_a = reg_y;
      break;
    case 0xA0: /* LDY imm */
      if (!p_opcode->is_dynamic_operand) {
        reg_y = operand_6502;
      } else {
        reg_y = k_value_unknown;
      }
      break;
    case 0xA2: /* LDX imm */
      if (!p_opcode->is_dynamic_operand) {
        reg_x = operand_6502;
      } else {
        reg_x = k_value_unknown;
      }
      break;
    case 0xA8: /* TAY */
      reg_y = reg_a;
      break;
    case 0xA9: /* LDA imm */
      if (!p_opcode->is_dynamic_operand) {
        reg_a = operand_6502;
      } else {
        reg_a = k_value_unknown;
      }
      break;
    case 0xAA: /* TAX */
      reg_x = reg_a;
      break;
    case 0xC8: /* INY */
      if (reg_y != k_value_unknown) {
        reg_y = (uint8_t) (reg_y + 1);
      }
      break;
    case 0xCA: /* DEX */
      if (reg_x != k_value_unknown) {
        reg_x = (uint8_t) (reg_x - 1);
      }
      break;
    case 0xD8: /* CLD */
      flag_decimal = 0;
      break;
    case 0xE8: /* INX */
      if (reg_x != k_value_unknown) {
        reg_x = (uint8_t) (reg_x + 1);
      }
      break;
    case 0xF8: /* SED */
      flag_decimal = 1;
      break;
    default:
      switch (opreg) {
      case k_a:
        reg_a = k_value_unknown;
        break;
      case k_x:
        reg_x = k_value_unknown;
        break;
      case k_y:
        reg_y = k_value_unknown;
        break;
      default:
        break;
      }
      if (changes_carry) {
        flag_carry = k_value_unknown;
      }
      break;
    }
  }

  /* Pass 2: replace single uops with replacements if known state offers
   * better alternatives.
   * Classic example is CLC; ADC. At the ADC instruction, it is known that
   * CF==0 so the ADC can become just an ADD.
   */
  had_bcd_check = 0;
  for (p_opcode = p_opcodes;
       p_opcode->addr_6502 != -1;
       p_opcode += p_opcode->num_bytes_6502) {
    uint32_t num_uops;
    uint32_t i_uops;

    uint16_t addr_6502 = p_opcode->addr_6502;
    int32_t interp_replace = -1;

    reg_a = p_opcode->reg_a;
    reg_x = p_opcode->reg_x;
    reg_y = p_opcode->reg_y;
    flag_carry = p_opcode->flag_carry;
    flag_decimal = p_opcode->flag_decimal;

    num_uops = p_opcode->num_uops;
    for (i_uops = 0; i_uops < num_uops; ++i_uops) {
      struct asm_uop* p_uop = &p_opcode->uops[i_uops];
      int32_t uopcode = p_uop->uopcode;
      int32_t new_add_uopcode = -1;
      int32_t new_sub_uopcode = -1;

      switch (uopcode) {
      case 0x61: /* ADC idx */
      case 0x75: /* ADC zpx */
        new_add_uopcode = k_opcode_ADD_SCRATCH;
        break;
      case 0x65: /* ADC zpg */
      case 0x6D: /* ADC abs */
        new_add_uopcode = k_opcode_ADD_ABS;
        break;
      case 0x69: /* ADC imm */
        new_add_uopcode = k_opcode_ADD_IMM;
        break;
      case 0x71: /* ADC idy */
        new_add_uopcode = k_opcode_ADD_SCRATCH_Y;
        break;
      case 0x79: /* ADC aby */
        new_add_uopcode = k_opcode_ADD_ABY;
        break;
      case 0x7D: /* ADC abx */
        new_add_uopcode = k_opcode_ADD_ABX;
        break;
      case 0x84: /* STY zpg */
      case 0x8C: /* STY abs */
        if (has_STOA && (reg_y != k_value_unknown)) {
          uopcode = k_opcode_STOA_IMM;
          p_uop->value2 = reg_y;
        }
        break;
      case 0x85: /* STA zpg */
      case 0x8D: /* STA abs */
        if (has_STOA && (reg_a != k_value_unknown)) {
          uopcode = k_opcode_STOA_IMM;
          p_uop->value2 = reg_a;
        }
        break;
      case 0x86: /* STX zpg */
      case 0x8E: /* STX abs */
        if (has_STOA && (reg_x != k_value_unknown)) {
          uopcode = k_opcode_STOA_IMM;
          p_uop->value2 = reg_x;
        }
        break;
      case 0x88: /* DEY */
        if (reg_y != k_value_unknown) {
          uopcode = 0xA0; /* LDY imm */
          p_uop->value1 = (uint8_t) (reg_y - 1);
          jit_optimizer_append_uop(p_opcode, k_opcode_FLAGY);
        }
        break;
      case 0x8A: /* TXA */
        if (reg_x != k_value_unknown) {
          uopcode = 0xA9; /* LDA imm */
          p_uop->value1 = reg_x;
        }
        break;
      case 0x98: /* TYA */
        if (reg_y != k_value_unknown) {
          uopcode = 0xA9; /* LDA imm */
          p_uop->value1 = reg_y;
        }
        break;
      case 0xA8: /* TAY */
        if (reg_a != k_value_unknown) {
          uopcode = 0xA0; /* LDY imm */
          p_uop->value1 = reg_a;
        }
        break;
      case 0xAA: /* TAX */
        if (reg_a != k_value_unknown) {
          uopcode = 0xA2; /* LDX imm */
          p_uop->value1 = reg_a;
        }
        break;
      case 0xC8: /* INY */
        if (reg_y != k_value_unknown) {
          uopcode = 0xA0; /* LDY imm */
          p_uop->value1 = (uint8_t) (reg_y + 1);
          jit_optimizer_append_uop(p_opcode, k_opcode_FLAGY);
        }
        break;
      case 0xCA: /* DEX */
        if (reg_x != k_value_unknown) {
          uopcode = 0xA2; /* LDX imm */
          p_uop->value1 = (uint8_t) (reg_x - 1);
          jit_optimizer_append_uop(p_opcode, k_opcode_FLAGX);
        }
        break;
      case 0xE5: /* SBC zpg */
      case 0xED: /* SBC abs */
        new_sub_uopcode = k_opcode_SUB_ABS;
        break;
      case 0xE8: /* INX */
        if (reg_x != k_value_unknown) {
          uopcode = 0xA2; /* LDX imm */
          p_uop->value1 = (uint8_t) (reg_x + 1);
          jit_optimizer_append_uop(p_opcode, k_opcode_FLAGX);
        }
        break;
      case 0xE9: /* SBC imm */
        new_sub_uopcode = k_opcode_SUB_IMM;
        break;
      case k_opcode_CHECK_BCD:
        if (flag_decimal == k_value_unknown) {
          if (!had_bcd_check) {
            /* Leave the first one intact and then elimanate any others. */
            had_bcd_check = 1;
          } else {
            p_uop->is_eliminated = 1;
          }
        } else if (flag_decimal == 0) {
          p_uop->is_eliminated = 1;
        } else {
          interp_replace = k_opcode_CHECK_BCD;
        }
        break;
      default:
        break;
      }

      if ((new_add_uopcode != -1) && (flag_carry != k_value_unknown)) {
        if ((flag_carry == 0) ||
            ((new_add_uopcode == k_opcode_ADD_IMM) &&
             (p_uop->value1 != 0xFF) &&
             (p_uop->value1 != 0x7F))) {
          /* Eliminate LOAD_CARRY_FOR_CALC, flip ADC to ADD. */
          struct asm_uop* p_elim_uop;
          uopcode = new_add_uopcode;
          if (flag_carry == 1) {
            p_uop->value1++;
          }
          p_elim_uop = jit_opcode_find_uop(p_opcode,
                                           k_opcode_LOAD_CARRY_FOR_CALC);
          assert(p_elim_uop != NULL);
          p_elim_uop->is_eliminated = 1;
        }
      }
      if ((new_sub_uopcode != -1) && (flag_carry != k_value_unknown)) {
        if ((flag_carry == 1) ||
            ((new_sub_uopcode == k_opcode_SUB_IMM) &&
             (p_uop->value1 != 0xFF) &&
             (p_uop->value1 != 0x7F))) {
          /* Eliminate LOAD_CARRY_INV_FOR_CALC, flip SBC to SUB. */
          struct asm_uop* p_elim_uop;
          uopcode = new_sub_uopcode;
          if (flag_carry == 0) {
            p_uop->value1++;
          }
          p_elim_uop = jit_opcode_find_uop(p_opcode,
                                           k_opcode_LOAD_CARRY_INV_FOR_CALC);
          assert(p_elim_uop != NULL);
          p_elim_uop->is_eliminated = 1;
        }
      }

      if (reg_y != k_value_unknown) {
        int replaced = 0;
        switch (uopcode) {
        case 0x51: /* EOR idy */
          uopcode = k_opcode_EOR_SCRATCH_n;
          replaced = 1;
          break;
        case 0x91: /* STA idy */
          uopcode = k_opcode_STA_SCRATCH_n;
          replaced = 1;
          break;
        case 0xB1: /* LDA idy */
          uopcode = k_opcode_LDA_SCRATCH_n;
          replaced = 1;
          break;
        default:
          break;
        }

        if (replaced) {
          struct asm_uop* p_crossing_uop =
              jit_opcode_find_uop(p_opcode,
                                  k_opcode_CHECK_PAGE_CROSSING_SCRATCH_Y);
          struct asm_uop* p_write_inv_uop =
              jit_opcode_find_uop(p_opcode, k_opcode_WRITE_INV_SCRATCH_Y);

          p_uop->value1 = reg_y;
          if (p_crossing_uop != NULL) {
            p_crossing_uop->uopcode = k_opcode_CHECK_PAGE_CROSSING_SCRATCH_n;
            p_crossing_uop->value1 = reg_y;
          }
          if (p_write_inv_uop != NULL) {
            p_write_inv_uop->uopcode = k_opcode_WRITE_INV_SCRATCH_n;
            p_write_inv_uop->value1 = reg_y;
          }
        }
      }

      p_uop->uopcode = uopcode;
    }

    if (interp_replace != -1) {
      jit_opcode_find_replace1(p_opcode,
                               interp_replace,
                               k_opcode_interp,
                               addr_6502);
      p_opcode->ends_block = 1;
      (p_opcode + p_opcode->num_bytes_6502)->addr_6502 = -1;
      break;
    }
  }

  /* Pass 3: merge macro opcodes as we can. */
  p_prev_opcode = NULL;
  for (p_opcode = p_opcodes;
       p_opcode->addr_6502 != -1;
       p_opcode += p_opcode->num_bytes_6502) {
    uint8_t opcode_6502 = p_opcode->opcode_6502;

    if (p_prev_opcode == NULL) {
      p_prev_opcode = p_opcode;
      continue;
    }

    /* Merge runs of the same opcode into just one, if supported. */
    if (opcode_6502 == p_prev_opcode->opcode_6502) {
      int32_t old_uopcode = -1;
      int32_t new_uopcode = -1;
      switch (opcode_6502) {
      case 0x0A: /* ASL A */
        old_uopcode = 0x0A;
        new_uopcode = k_opcode_ASL_ACC_n;
        break;
      case 0x2A: /* ROL A */
        old_uopcode = 0x2A;
        new_uopcode = k_opcode_ROL_ACC_n;
        break;
      case 0x4A: /* LSR A */
        old_uopcode = 0x4A;
        new_uopcode = k_opcode_LSR_ACC_n;
        break;
      case 0x6A: /* ROR A */
        old_uopcode = 0x6A;
        new_uopcode = k_opcode_ROR_ACC_n;
        break;
      default:
        break;
      }

      if (old_uopcode != -1) {
        struct asm_uop* p_modify_uop = jit_opcode_find_uop(p_prev_opcode,
                                                           old_uopcode);
        if (p_modify_uop != NULL) {
          p_modify_uop->uopcode = new_uopcode;
          p_modify_uop->value1 = 1;
        } else {
          p_modify_uop = jit_opcode_find_uop(p_prev_opcode, new_uopcode);
        }
        assert(p_modify_uop != NULL);
        jit_opcode_eliminate(p_opcode);
        p_modify_uop->value1++;

        continue;
      }
    }

    /* Merge a "branch not taken" cycles fixup with a countdown check. */
    if (p_opcode->uops[0].uopcode == k_opcode_countdown) {
      struct asm_uop* p_prev_last_uop =
          &p_prev_opcode->uops[p_prev_opcode->num_uops - 1];
      if (p_prev_last_uop->uopcode == k_opcode_ADD_CYCLES) {
        p_opcode->uops[0].value2 -= p_prev_last_uop->value1;
        p_prev_last_uop->is_eliminated = 1;
      }
    }

    p_prev_opcode = p_opcode;
  }

  /* Pass 4: first uopcode elimination pass, particularly FLAGn. */
  p_nz_flags_opcode = NULL;
  p_nz_flags_uop = NULL;
  p_idy_opcode = NULL;
  p_idy_uop = NULL;
  for (p_opcode = p_opcodes;
       p_opcode->addr_6502 != -1;
       p_opcode += p_opcode->num_bytes_6502) {
    uint32_t i_uops;
    uint32_t num_uops;

    num_uops = p_opcode->num_uops;
    for (i_uops = 0; i_uops < num_uops; ++i_uops) {
      int32_t uopcode;
      struct asm_uop* p_uop = &p_opcode->uops[i_uops];
      if (p_uop->is_eliminated) {
        continue;
      }
      uopcode = p_uop->uopcode;

      /* Finalize eliminations. */
      /* NZ flag save. */
      if ((p_nz_flags_opcode != NULL) &&
          jit_optimizer_uopcode_sets_nz_flags(uopcode)) {
        jit_optimizer_eliminate(&p_nz_flags_opcode, p_nz_flags_uop, p_opcode);
        p_nz_flags_opcode = NULL;
        p_nz_flags_uop = NULL;
      }
      /* idy indirect load. */
      if ((p_idy_opcode != NULL) &&
          jit_optimizer_uop_idy_match(p_uop, p_idy_uop)) {
        struct jit_opcode_details* p_eliminate_opcode = p_opcode;
        jit_optimizer_eliminate(&p_eliminate_opcode, p_uop, NULL);
      }

      /* Cancel eliminations. */
      /* NZ flag load. */
      if (p_nz_flags_opcode != NULL) {
        int32_t nz_flags_uopcode = p_nz_flags_uop->uopcode;
        if (jit_optimizer_uopcode_needs_nz_flags(uopcode) ||
            ((nz_flags_uopcode == k_opcode_FLAG_MEM) &&
             jit_optimizer_uop_could_write(p_uop, p_nz_flags_uop->value1))) {
          /* If we can't eliminate a flag load, there's a special case of
           * loading 0 into a register where we can collapse the register load
           * and flag load.
           */
          int32_t find_uopcode = -1;
          int32_t replace_uopcode = -1;
          struct asm_uop* p_find_uop;
          switch (nz_flags_uopcode) {
          case k_opcode_FLAGA:
            find_uopcode = 0xA9; /* LDA imm */
            replace_uopcode = k_opcode_LDA_Z;
            break;
          case k_opcode_FLAGX:
            find_uopcode = 0xA2; /* LDX imm */
            replace_uopcode = k_opcode_LDX_Z;
            break;
          case k_opcode_FLAGY:
            find_uopcode = 0xA0; /* LDY imm */
            replace_uopcode = k_opcode_LDY_Z;
            break;
          case k_opcode_FLAG_MEM:
            break;
          default:
            assert(0);
            break;
          }
          p_find_uop = jit_opcode_find_uop(p_nz_flags_opcode, find_uopcode);
          if ((p_find_uop != NULL) && (p_find_uop->value1 == 0x00)) {
            p_find_uop->uopcode = replace_uopcode;
            p_nz_flags_uop->is_eliminated = 1;
          }
          p_nz_flags_opcode = NULL;
        }
      }
      /* idy indirect load. */
      if ((p_idy_opcode != NULL) &&
          jit_optimizer_uop_invalidates_idy(p_uop, p_idy_uop)) {
        p_idy_opcode = NULL;
      }

      /* Keep track of uops we may be able to eliminate. */
      switch (uopcode) {
      case k_opcode_FLAGA:
      case k_opcode_FLAGX:
      case k_opcode_FLAGY:
      case k_opcode_FLAG_MEM:
        p_nz_flags_opcode = p_opcode;
        p_nz_flags_uop = p_uop;
        break;
      case k_opcode_MODE_IND_8:
        p_idy_opcode = p_opcode;
        p_idy_uop = p_uop;
        break;
      default:
        break;
      }
    }
  }

  /* Pass 5: second uopcode elimination pass, particularly those eliminations
   * that only occur well after FLAGn has been eliminated.
   */
  p_prev_opcode = NULL;
  p_lda_opcode = NULL;
  p_lda_uop = NULL;
  p_ldx_opcode = NULL;
  p_ldx_uop = NULL;
  p_ldy_opcode = NULL;
  p_ldy_uop = NULL;
  p_overflow_opcode = NULL;
  p_overflow_uop = NULL;
  p_carry_write_opcode = NULL;
  p_carry_write_uop = NULL;
  carry_flipped_for_branch = 0;
  for (p_opcode = p_opcodes;
       p_opcode->addr_6502 != -1;
       p_opcode += p_opcode->num_bytes_6502) {
    uint32_t i_uops;
    uint32_t num_uops;

    p_prev_opcode = p_opcode;
    num_uops = p_opcode->num_uops;
    for (i_uops = 0; i_uops < num_uops; ++i_uops) {
      int32_t uopcode;
      struct asm_uop* p_uop = &p_opcode->uops[i_uops];
      if (p_uop->is_eliminated) {
        continue;
      }
      uopcode = p_uop->uopcode;

      /* Finalize eliminations. */
      /* LDA A load. */
      if ((p_lda_opcode != NULL) &&
          jit_optimizer_uopcode_overwrites_a(uopcode)) {
        jit_optimizer_eliminate(&p_lda_opcode, p_lda_uop, p_opcode);
      }
      /* LDX X load. */
      if ((p_ldx_opcode != NULL) &&
          jit_optimizer_uopcode_overwrites_x(uopcode)) {
        jit_optimizer_eliminate(&p_ldx_opcode, p_ldx_uop, p_opcode);
      }
      /* LDY Y load. */
      if ((p_ldy_opcode != NULL) &&
          jit_optimizer_uopcode_overwrites_y(uopcode)) {
        jit_optimizer_eliminate(&p_ldy_opcode, p_ldy_uop, p_opcode);
      }
      /* Overflow flag save elimination. */
      if ((p_overflow_opcode != NULL) && (uopcode == k_opcode_SAVE_OVERFLOW)) {
        jit_optimizer_eliminate(&p_overflow_opcode, p_overflow_uop, p_opcode);
      }
      /* Carry flag save elimination. */
      if (p_carry_write_opcode != NULL) {
        struct jit_opcode_details* p_eliminate_opcode = p_opcode;
        int32_t carry_write_uopcode = p_carry_write_uop->uopcode;
        int inversion = 0;
        switch (uopcode) {
        case k_opcode_SAVE_CARRY:
        case k_opcode_SAVE_CARRY_INV:
        case 0x18: /* CLC */
        case 0x38: /* SEC */
          jit_optimizer_eliminate(&p_carry_write_opcode,
                                  p_carry_write_uop,
                                  p_opcode);
          break;
        case k_opcode_LOAD_CARRY_FOR_BRANCH:
        case k_opcode_LOAD_CARRY_FOR_CALC:
        case k_opcode_LOAD_CARRY_INV_FOR_CALC:
          if (uopcode != k_opcode_LOAD_CARRY_FOR_BRANCH) {
            /* Eliminate unfinalized write. */
            jit_optimizer_eliminate(&p_carry_write_opcode,
                                    p_carry_write_uop,
                                    p_opcode);
          }
          /* Eliminate load or replace with direct host carry flag change. */
          inversion ^= (uopcode == k_opcode_LOAD_CARRY_INV_FOR_CALC);
          switch (carry_write_uopcode) {
          case k_opcode_SAVE_CARRY:
          case k_opcode_SAVE_CARRY_INV:
            inversion ^= (carry_write_uopcode == k_opcode_SAVE_CARRY_INV);
            if (inversion) {
              if (uopcode == k_opcode_LOAD_CARRY_FOR_BRANCH) {
                carry_flipped_for_branch = 1;
                jit_optimizer_eliminate(&p_eliminate_opcode, p_uop, NULL);
              } else {
                p_uop->uopcode = k_opcode_INVERT_CARRY;
              }
            } else {
              jit_optimizer_eliminate(&p_eliminate_opcode, p_uop, NULL);
            }
            break;
          case 0x18: /* CLC */
          case 0x38: /* SEC */
            inversion ^= (carry_write_uopcode == 0x38); /* SEC */
            if (inversion) {
              p_uop->uopcode = k_opcode_SET_CARRY;
            } else {
              p_uop->uopcode = k_opcode_CLEAR_CARRY;
            }
            break;
          default:
            assert(0);
            break;
          }
          break;
        default:
          break;
        }
      }
      /* Carry flip vs. carry branch elimination. */
      if (carry_flipped_for_branch) {
        switch (uopcode) {
        case 0x90: /* BCC */
          p_uop->uopcode = 0xB0; /* BCS */
          break;
        case 0xB0: /* BCS */
          p_uop->uopcode = 0x90; /* BCC */
          break;
        default:
          break;
        }
      }

      /* Cancel eliminations. */
      /* LDA A load. */
      if ((p_lda_opcode != NULL) && jit_optimizer_uopcode_needs_a(uopcode)) {
        p_lda_opcode = NULL;
      }
      /* LDX X load. */
      if ((p_ldx_opcode != NULL) && jit_optimizer_uopcode_needs_x(uopcode)) {
        p_ldx_opcode = NULL;
      }
      /* LDX Y load. */
      if ((p_ldy_opcode != NULL) && jit_optimizer_uopcode_needs_y(uopcode)) {
        p_ldy_opcode = NULL;
      }
      /* Overflow flag save elimination. */
      if ((p_overflow_opcode != NULL) &&
          jit_optimizer_uopcode_needs_or_trashes_overflow(uopcode)) {
        p_overflow_opcode = NULL;
      }
      /* Carry flag save elimination. */
      if ((p_carry_write_opcode != NULL) &&
          jit_optimizer_uopcode_needs_or_trashes_carry(uopcode)) {
        p_carry_write_opcode = NULL;
      }
      /* Many eliminations can't cross branches, or need modifications. */
      if (jit_optimizer_uopcode_can_jump(uopcode)) {
        p_lda_opcode = NULL;
        p_ldx_opcode = NULL;
        p_ldy_opcode = NULL;
        p_overflow_opcode = NULL;
        p_carry_write_opcode = NULL;
        carry_flipped_for_branch = 0;
      }

      /* Keep track of uops we may be able to eliminate. */
      switch (uopcode) {
      case 0xA9: /* LDA imm */
        p_lda_opcode = p_opcode;
        p_lda_uop = p_uop;
        break;
      case 0xA2: /* LDX imm */
        p_ldx_opcode = p_opcode;
        p_ldx_uop = p_uop;
        break;
      case 0xA0: /* LDY imm */
        p_ldy_opcode = p_opcode;
        p_ldy_uop = p_uop;
        break;
      case k_opcode_SAVE_OVERFLOW:
        p_overflow_opcode = p_opcode;
        p_overflow_uop = p_uop;
        break;
      case k_opcode_SAVE_CARRY:
      case k_opcode_SAVE_CARRY_INV:
      case 0x18: /* CLC */
      case 0x38: /* SEC */
        p_carry_write_opcode = p_opcode;
        p_carry_write_uop = p_uop;
        break;
      default:
        break;
      }
    }
  }

  return p_prev_opcode;
}
