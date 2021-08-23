#include "../asm_jit.h"

#include "../../defs_6502.h"
#include "../../os_alloc.h"
#include "../../util.h"
#include "../asm_common.h"
#include "../asm_defs_host.h"
#include "../asm_jit_defs.h"
#include "../asm_opcodes.h"
/* For REG_MEM_OFFSET. */
#include "asm_defs_registers_x64.h"

#include <assert.h>

enum {
  k_opcode_x64_load_abs = 0x1000,
  k_opcode_x64_load_zpg,
  k_opcode_x64_mode_abx_and_load,
  k_opcode_x64_mode_abx_store,
  k_opcode_x64_mode_idy_load,
  k_opcode_x64_mode_zpx,
  k_opcode_x64_mode_zpy,
  k_opcode_x64_store_abs,
  k_opcode_x64_ADC_ABS,
  k_opcode_x64_ADC_ABX,
  k_opcode_x64_ADC_ABY,
  k_opcode_x64_ADC_addr,
  k_opcode_x64_ADC_addr_Y,
  k_opcode_x64_ADC_IMM,
  k_opcode_x64_ADC_ZPG,
  k_opcode_x64_ALR_IMM,
  k_opcode_x64_AND_ABS,
  k_opcode_x64_AND_ABX,
  k_opcode_x64_AND_ABY,
  k_opcode_x64_AND_addr,
  k_opcode_x64_AND_addr_Y,
  k_opcode_x64_AND_ZPG,
  k_opcode_x64_AND_IMM,
  k_opcode_x64_ASL_ABS,
  k_opcode_x64_ASL_ZPG,
  k_opcode_x64_CMP_ABS,
  k_opcode_x64_CMP_ABX,
  k_opcode_x64_CMP_ABY,
  k_opcode_x64_CMP_addr,
  k_opcode_x64_CMP_addr_Y,
  k_opcode_x64_CMP_IMM,
  k_opcode_x64_CMP_ZPG,
  k_opcode_x64_CPX_ABS,
  k_opcode_x64_CPX_IMM,
  k_opcode_x64_CPX_ZPG,
  k_opcode_x64_CPY_ABS,
  k_opcode_x64_CPY_IMM,
  k_opcode_x64_CPY_ZPG,
  k_opcode_x64_DEC_ABS,
  k_opcode_x64_DEC_ZPG,
  k_opcode_x64_EOR_ABS,
  k_opcode_x64_EOR_ABX,
  k_opcode_x64_EOR_ABY,
  k_opcode_x64_EOR_addr,
  k_opcode_x64_EOR_addr_Y,
  k_opcode_x64_EOR_IMM,
  k_opcode_x64_EOR_ZPG,
  k_opcode_x64_INC_ABS,
  k_opcode_x64_INC_ZPG,
  k_opcode_x64_LDA_addr,
  k_opcode_x64_LDA_addr_Y,
  k_opcode_x64_LDA_ABS,
  k_opcode_x64_LDA_ABX,
  k_opcode_x64_LDA_ABY,
  k_opcode_x64_LDA_IMM,
  k_opcode_x64_LDA_ZPG,
  k_opcode_x64_LDX_addr,
  k_opcode_x64_LDX_ABS,
  k_opcode_x64_LDX_ABY,
  k_opcode_x64_LDX_IMM,
  k_opcode_x64_LDX_ZPG,
  k_opcode_x64_LDY_addr,
  k_opcode_x64_LDY_ABS,
  k_opcode_x64_LDY_ABX,
  k_opcode_x64_LDY_IMM,
  k_opcode_x64_LDY_ZPG,
  k_opcode_x64_LSR_ABS,
  k_opcode_x64_LSR_ZPG,
  k_opcode_x64_ORA_ABS,
  k_opcode_x64_ORA_ABX,
  k_opcode_x64_ORA_ABY,
  k_opcode_x64_ORA_addr,
  k_opcode_x64_ORA_addr_Y,
  k_opcode_x64_ORA_IMM,
  k_opcode_x64_ORA_ZPG,
  k_opcode_x64_SAX_ABS,
  k_opcode_x64_SBC_ABS,
  k_opcode_x64_SBC_ABX,
  k_opcode_x64_SBC_ABY,
  k_opcode_x64_SBC_addr,
  k_opcode_x64_SBC_addr_Y,
  k_opcode_x64_SBC_IMM,
  k_opcode_x64_SBC_ZPG,
  k_opcode_x64_SLO_ABS,
  k_opcode_x64_STA_addr,
  k_opcode_x64_STA_addr_Y,
  k_opcode_x64_STA_ABS,
  k_opcode_x64_STA_ABX,
  k_opcode_x64_STA_ABY,
  k_opcode_x64_STA_ZPG,
  k_opcode_x64_STX_addr,
  k_opcode_x64_STX_ABS,
  k_opcode_x64_STX_ZPG,
  k_opcode_x64_STY_addr,
  k_opcode_x64_STY_ABS,
  k_opcode_x64_STY_ZPG,
};

#define ASM(x)                                                                 \
  void asm_jit_ ## x(void);                                                    \
  void asm_jit_ ## x ## _END(void);                                            \
  asm_copy(p_dest_buf, asm_jit_ ## x, asm_jit_ ## x ## _END);

#define ASM_U8(x)                                                              \
  void asm_jit_ ## x(void);                                                    \
  void asm_jit_ ## x ## _END(void);                                            \
  asm_copy_patch_byte(p_dest_buf, asm_jit_ ## x, asm_jit_ ## x ## _END, value1);

#define ASM_ADDR_U8(x)                                                         \
  void asm_jit_ ## x(void);                                                    \
  void asm_jit_ ## x ## _END(void);                                            \
  delta = (value2 - K_BBC_MEM_READ_IND_ADDR);                                  \
  asm_copy_patch_byte(p_dest_buf,                                              \
                      asm_jit_ ## x,                                           \
                      asm_jit_ ## x ## _END,                                   \
                      (value1 - REG_MEM_OFFSET + delta))

#define ASM_U32(x)                                                             \
  void asm_jit_ ## x(void);                                                    \
  void asm_jit_ ## x ## _END(void);                                            \
  asm_copy_patch_u32(p_dest_buf, asm_jit_ ## x, asm_jit_ ## x ## _END, value1);

#define ASM_ADDR_U32(x)                                                        \
  void asm_jit_ ## x(void);                                                    \
  void asm_jit_ ## x ## _END(void);                                            \
  delta = (value2 - K_BBC_MEM_READ_IND_ADDR);                                  \
  asm_copy_patch_u32(p_dest_buf,                                               \
                     asm_jit_ ## x,                                            \
                     asm_jit_ ## x ## _END,                                    \
                     (value1 - REG_MEM_OFFSET + delta));

#define ASM_ADDR_U32_RAW(x)                                                    \
  void asm_jit_ ## x(void);                                                    \
  void asm_jit_ ## x ## _END(void);                                            \
  asm_copy_patch_u32(p_dest_buf,                                               \
                     asm_jit_ ## x,                                            \
                     asm_jit_ ## x ## _END,                                    \
                     (value1 + value2));

#define ASM_Bxx(x)                                                             \
  void asm_jit_ ## x(void);                                                    \
  void asm_jit_ ## x ## _END(void);                                            \
  void asm_jit_ ## x ## _8bit(void);                                           \
  void asm_jit_ ## x ## _8bit_END(void);                                       \
  asm_emit_jit_jump(p_dest_buf,                                                \
                    (void*) (uintptr_t) value1,                                \
                    asm_jit_ ## x,                                             \
                    asm_jit_ ## x ## _END,                                     \
                    asm_jit_ ## x ## _8bit,                                    \
                    asm_jit_ ## x ## _8bit_END);

static struct os_alloc_mapping* s_p_mapping_trampolines;

struct asm_jit_struct {
  int (*is_memory_always_ram)(void* p, uint16_t addr);
  void* p_memory_object;
};

static void
asm_emit_jit_jump(struct util_buffer* p_buf,
                  void* p_target,
                  void* p_jmp_32bit,
                  void* p_jmp_end_32bit,
                  void* p_jmp_8bit,
                  void* p_jmp_end_8bit) {
  int32_t len_x64;
  int64_t delta;

  size_t offset = util_buffer_get_pos(p_buf);
  void* p_source = (util_buffer_get_base_address(p_buf) + offset);

  len_x64 = (p_jmp_end_8bit - p_jmp_8bit);
  delta = (p_target - (p_source + len_x64));

  if (delta <= INT8_MAX && delta >= INT8_MIN) {
    asm_copy(p_buf, p_jmp_8bit, p_jmp_end_8bit);
    asm_patch_byte(p_buf, offset, p_jmp_8bit, p_jmp_end_8bit, delta);
  } else {
    len_x64 = (p_jmp_end_32bit - p_jmp_32bit);
    delta = (p_target - (p_source + len_x64));
    assert(delta <= INT32_MAX && delta >= INT32_MIN);
    asm_copy(p_buf, p_jmp_32bit, p_jmp_end_32bit);
    asm_patch_int(p_buf, offset, p_jmp_32bit, p_jmp_end_32bit, delta);
  }
}

void
asm_emit_jit_jump_interp_trampoline(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_jump_interp_trampoline(void);
  void asm_jit_jump_interp_trampoline_pc_patch(void);
  void asm_jit_jump_interp_trampoline_jump_patch(void);
  void asm_jit_jump_interp_trampoline_END(void);
  void asm_jit_interp(void);
  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf,
           asm_jit_jump_interp_trampoline,
           asm_jit_jump_interp_trampoline_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_jump_interp_trampoline,
                asm_jit_jump_interp_trampoline_pc_patch,
                addr);
  asm_patch_jump(p_buf,
                 offset,
                 asm_jit_jump_interp_trampoline,
                 asm_jit_jump_interp_trampoline_jump_patch,
                 asm_jit_interp);
}

int
asm_jit_is_enabled(void) {
  return 1;
}

void
asm_jit_test_preconditions(void) {
  void asm_jit_BEQ_8bit(void);
  void asm_jit_BEQ_8bit_END(void);
  if ((asm_jit_BEQ_8bit_END - asm_jit_BEQ_8bit) != 2) {
    util_bail("JIT assembly miscompiled -- clang issue? try opt build.");
  }
}

int
asm_jit_supports_optimizer(void) {
  return 0;
}

int
asm_jit_supports_uopcode(int32_t uopcode) {
  (void) uopcode;
  return 1;
}

struct asm_jit_struct*
asm_jit_create(void* p_jit_base,
               int (*is_memory_always_ram)(void* p, uint16_t addr),
               void* p_memory_object) {
  struct asm_jit_struct* p_asm;
  size_t mapping_size;
  uint32_t i;
  void* p_trampolines;
  struct util_buffer* p_temp_buf = util_buffer_create();

  (void) p_jit_base;

  assert(s_p_mapping_trampolines == NULL);

  p_asm = util_mallocz(sizeof(struct asm_jit_struct));
  p_asm->is_memory_always_ram = is_memory_always_ram;
  p_asm->p_memory_object = p_memory_object;

  /* This is the mapping that holds trampolines to jump out of JIT. These
   * one-per-6502-address trampolines enable the core JIT code to be simpler
   * and smaller, at the expense of more complicated bridging between JIT and
   * interp.
   */
  mapping_size = (k_6502_addr_space_size * K_BBC_JIT_TRAMPOLINE_BYTES);
  s_p_mapping_trampolines =
      os_alloc_get_mapping((void*) K_BBC_JIT_TRAMPOLINES_ADDR, mapping_size);
  p_trampolines = os_alloc_get_mapping_addr(s_p_mapping_trampolines);
  os_alloc_make_mapping_read_write_exec(p_trampolines, mapping_size);
  util_buffer_setup(p_temp_buf, p_trampolines, mapping_size);
  asm_fill_with_trap(p_temp_buf);

  for (i = 0; i < k_6502_addr_space_size; ++i) {
    /* Initialize JIT trampoline. */
    util_buffer_setup(
        p_temp_buf,
        (p_trampolines + (i * K_BBC_JIT_TRAMPOLINE_BYTES)),
        K_BBC_JIT_TRAMPOLINE_BYTES);
    asm_emit_jit_jump_interp_trampoline(p_temp_buf, i);
  }

  util_buffer_destroy(p_temp_buf);

  return p_asm;
}

void
asm_jit_destroy(struct asm_jit_struct* p_asm) {
  assert(s_p_mapping_trampolines != NULL);
  os_alloc_free_mapping(s_p_mapping_trampolines);

  util_free(p_asm);
}

void*
asm_jit_get_private(struct asm_jit_struct* p_asm) {
  void asm_jit_compile_trampoline(void);
  (void) p_asm;
  return asm_jit_compile_trampoline;
}

void
asm_jit_start_code_updates(struct asm_jit_struct* p_asm) {
  (void) p_asm;
}

void
asm_jit_finish_code_updates(struct asm_jit_struct* p_asm) {
  (void) p_asm;
}

int
asm_jit_handle_fault(struct asm_jit_struct* p_asm,
                     uintptr_t* p_pc,
                     uint16_t addr_6502,
                     void* p_fault_addr,
                     int is_write) {
  int inaccessible_indirect_page;
  int ff_fault_fixup;
  int bcd_fault_fixup;
  int stack_wrap_fault_fixup;
  int wrap_indirect_read;
  int wrap_indirect_write;

  (void) p_asm;

  /* The indirect page fault occurs when an indirect addressing mode is used
   * to access 0xF000 - 0xFFFF, primarily of interest due to the hardware
   * registers. Using a fault + fixup here is a good performance boost for the
   * common case.
   * This fault is also encountered in the Windows port, which needs to use it
   * for ROM writes.
   */
  inaccessible_indirect_page = 0;
  /* The 0xFF page wrap fault occurs when a word fetch is performed at the end
   * of a page, where that page wraps. e.g. idx mode fetching the address from
   * 0xFF. Using a fault + fixup here makes the code footprint for idx mode
   * addressing smaller.
   */
  ff_fault_fixup = 0;
  /* The BCD fault occurs when the BCD flag is unknown and set at the start of
   * a block with ADC / SBC instructions.
   */
  bcd_fault_fixup = 0;
  /* The stack wrap fault occurs if a 16-bit stack access wraps the S
   * register.
   */
  stack_wrap_fault_fixup = 0;
  /* The address space indirect wrap faults occurs if an indirect 16-bit access
   * crosses the 0xFFFF address space boundary.
   */
  wrap_indirect_read = 0;
  wrap_indirect_write = 0;

  /* TODO: more checks, etc. */
  if ((p_fault_addr >=
          ((void*) K_BBC_MEM_WRITE_IND_ADDR + K_BBC_MEM_OS_ROM_OFFSET)) &&
      (p_fault_addr <
          ((void*) K_BBC_MEM_WRITE_IND_ADDR + K_6502_ADDR_SPACE_SIZE))) {
    if (is_write) {
      inaccessible_indirect_page = 1;
    }
  }
  if ((p_fault_addr >=
          ((void*) K_BBC_MEM_WRITE_IND_ADDR + K_6502_ADDR_SPACE_SIZE)) &&
      (p_fault_addr <=
          ((void*) K_BBC_MEM_WRITE_IND_ADDR + K_6502_ADDR_SPACE_SIZE + 0xFE))) {
    if (is_write) {
      wrap_indirect_write = 1;
    }
  }

  /* From this point on, nothing else is a write fault. */
  if (!inaccessible_indirect_page && !wrap_indirect_write && is_write) {
    return 0;
  }

  if ((p_fault_addr >=
          ((void*) K_BBC_MEM_READ_IND_ADDR + K_BBC_MEM_INACCESSIBLE_OFFSET)) &&
      (p_fault_addr <
          ((void*) K_BBC_MEM_READ_IND_ADDR + K_6502_ADDR_SPACE_SIZE))) {
    inaccessible_indirect_page = 1;
  }
  if ((p_fault_addr >=
          ((void*) K_BBC_MEM_READ_IND_ADDR + K_6502_ADDR_SPACE_SIZE)) &&
      (p_fault_addr <=
          ((void*) K_BBC_MEM_READ_IND_ADDR + K_6502_ADDR_SPACE_SIZE + 0xFE))) {
    wrap_indirect_read = 1;
  }
  if (p_fault_addr ==
          ((void*) K_BBC_MEM_READ_FULL_ADDR + K_6502_ADDR_SPACE_SIZE)) {
    ff_fault_fixup = 1;
  }
  if (p_fault_addr ==
          ((void*) K_BBC_MEM_READ_FULL_ADDR + K_6502_ADDR_SPACE_SIZE + 2)) {
    /* D flag alone. */
    bcd_fault_fixup = 1;
  }
  if (p_fault_addr ==
          ((void*) K_BBC_MEM_READ_FULL_ADDR + K_6502_ADDR_SPACE_SIZE + 6)) {
    /* D flag and I flag. */
    bcd_fault_fixup = 1;
  }
  if ((p_fault_addr == ((void*) K_BBC_MEM_READ_FULL_ADDR - 1)) ||
      (p_fault_addr == ((void*) K_BBC_MEM_READ_FULL_ADDR - 2))) {
    /* Wrap via pushing (decrementing). */
    stack_wrap_fault_fixup = 1;
  }
  if ((p_fault_addr ==
          ((void*) K_BBC_MEM_READ_FULL_ADDR + K_6502_ADDR_SPACE_SIZE)) ||
      (p_fault_addr ==
          ((void*) K_BBC_MEM_READ_FULL_ADDR + K_6502_ADDR_SPACE_SIZE + 1))) {
    /* Wrap via pulling (incrementing). */
    stack_wrap_fault_fixup = 1;
  }

  if (!inaccessible_indirect_page &&
      !ff_fault_fixup &&
      !bcd_fault_fixup &&
      !stack_wrap_fault_fixup &&
      !wrap_indirect_read &&
      !wrap_indirect_write) {
    return 0;
  }

  /* Fault is recognized.
   * Bounce into the interpreter via the trampolines.
   */
  *p_pc =
      (K_BBC_JIT_TRAMPOLINES_ADDR + (addr_6502 * K_BBC_JIT_TRAMPOLINE_BYTES));
  return 1;
}

void
asm_jit_invalidate_code_at(void* p) {
  uint16_t* p_dst = (uint16_t*) p;
  /* call [rdi] */
  *p_dst = 0x17ff;
}

int
asm_jit_is_invalidated_code_at(void* p) {
  uint16_t code = *(uint16_t*) p;
  return (code == 0x17ff);
}

void
asm_emit_jit_invalidated(struct util_buffer* p_buf) {
  /* call [rdi] */
  util_buffer_add_2b(p_buf, 0xff, 0x17);
}

void
asm_emit_jit_check_countdown(struct util_buffer* p_buf,
                             struct util_buffer* p_buf_epilog,
                             uint32_t count,
                             uint16_t addr,
                             void* p_trampoline) {
  void asm_jit_check_countdown_8bit(void);
  void asm_jit_check_countdown_8bit_count_patch(void);
  void asm_jit_check_countdown_8bit_jump_patch(void);
  void asm_jit_check_countdown_8bit_END(void);
  void asm_jit_check_countdown(void);
  void asm_jit_check_countdown_count_patch(void);
  void asm_jit_check_countdown_jump_patch(void);
  void asm_jit_check_countdown_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  (void) p_buf_epilog;
  (void) addr;

  if (count <= 128) {
    asm_copy(p_buf,
             asm_jit_check_countdown_8bit,
             asm_jit_check_countdown_8bit_END);
    asm_patch_byte(p_buf,
                   offset,
                   asm_jit_check_countdown_8bit,
                   asm_jit_check_countdown_8bit_count_patch,
                   -count);
    asm_patch_jump(p_buf,
                   offset,
                   asm_jit_check_countdown_8bit,
                   asm_jit_check_countdown_8bit_jump_patch,
                   p_trampoline);
  } else {
    asm_copy(p_buf,
             asm_jit_check_countdown,
             asm_jit_check_countdown_END);
    asm_patch_int(p_buf,
                  offset,
                  asm_jit_check_countdown,
                  asm_jit_check_countdown_count_patch,
                  -count);
    asm_patch_jump(p_buf,
                   offset,
                   asm_jit_check_countdown,
                   asm_jit_check_countdown_jump_patch,
                   p_trampoline);
  }
}

void
asm_emit_jit_call_debug(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_call_debug(void);
  void asm_jit_call_debug_pc_patch(void);
  void asm_jit_call_debug_call_patch(void);
  void asm_jit_call_debug_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf, asm_jit_call_debug, asm_jit_call_debug_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_call_debug,
                asm_jit_call_debug_pc_patch,
                addr);
  asm_patch_jump(p_buf,
                 offset,
                 asm_jit_call_debug,
                 asm_jit_call_debug_call_patch,
                 asm_debug);
}

void
asm_emit_jit_call_inturbo(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_call_inturbo(void);
  void asm_jit_call_inturbo_pc_patch(void);
  void asm_jit_call_inturbo_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf, asm_jit_call_inturbo, asm_jit_call_inturbo_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_call_inturbo,
                asm_jit_call_inturbo_pc_patch,
                (addr + K_BBC_MEM_READ_FULL_ADDR));
}

void
asm_emit_jit_jump_interp(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_jump_interp(void);
  void asm_jit_jump_interp_pc_patch(void);
  void asm_jit_jump_interp_jump_patch(void);
  void asm_jit_jump_interp_END(void);
  void asm_jit_interp(void);
  size_t offset = util_buffer_get_pos(p_buf);

  /* Require the trampolines to be hosted above the main JIT code in virtual
   * memory. This ensures that the (uncommon) jumps out of JIT are forward
   * jumps, which may help on some CPUs.
   */
  assert(K_BBC_JIT_TRAMPOLINES_ADDR > K_BBC_JIT_ADDR);

  asm_copy(p_buf, asm_jit_jump_interp, asm_jit_jump_interp_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_jump_interp,
                asm_jit_jump_interp_pc_patch,
                addr);
  asm_patch_jump(p_buf,
                 offset,
                 asm_jit_jump_interp,
                 asm_jit_jump_interp_jump_patch,
                 asm_jit_interp);
}

void
asm_emit_jit_for_testing(struct util_buffer* p_buf) {
  void asm_jit_for_testing(void);
  void asm_jit_for_testing_END(void);
  asm_copy(p_buf, asm_jit_for_testing, asm_jit_for_testing_END);
}

void
asm_emit_jit_ADD_CYCLES(struct util_buffer* p_buf, uint8_t value) {
  void asm_jit_ADD_CYCLES(void);
  void asm_jit_ADD_CYCLES_END(void);
  asm_copy_patch_byte(p_buf, asm_jit_ADD_CYCLES, asm_jit_ADD_CYCLES_END, value);
}

void
asm_emit_jit_ADD_ABS(struct util_buffer* p_buf,
                     uint16_t addr,
                     uint32_t segment) {
  void asm_jit_ADD_ZPG(void);
  void asm_jit_ADD_ZPG_END(void);
  void asm_jit_ADD_ABS(void);
  void asm_jit_ADD_ABS_END(void);
  uint32_t delta = (segment - K_BBC_MEM_READ_IND_ADDR);
  if (addr < 0x100) {
    asm_copy_patch_byte(p_buf,
                        asm_jit_ADD_ZPG,
                        asm_jit_ADD_ZPG_END,
                        (addr - REG_MEM_OFFSET + delta));
  } else {
    asm_copy_patch_u32(p_buf,
                       asm_jit_ADD_ABS,
                       asm_jit_ADD_ABS_END,
                       (addr - REG_MEM_OFFSET + delta));
  }
}

void
asm_emit_jit_ADD_ABX(struct util_buffer* p_buf,
                     uint16_t addr,
                     uint32_t segment) {
  void asm_jit_ADD_ABX(void);
  void asm_jit_ADD_ABX_END(void);
  asm_copy_patch_u32(p_buf,
                     asm_jit_ADD_ABX,
                     asm_jit_ADD_ABX_END,
                     (addr + segment));
}

void
asm_emit_jit_ADD_ABY(struct util_buffer* p_buf,
                     uint16_t addr,
                     uint32_t segment) {
  void asm_jit_ADD_ABY(void);
  void asm_jit_ADD_ABY_END(void);
  asm_copy_patch_u32(p_buf,
                     asm_jit_ADD_ABY,
                     asm_jit_ADD_ABY_END,
                     (addr + segment));
}

void
asm_emit_jit_ADD_IMM(struct util_buffer* p_buf, uint8_t value) {
  void asm_jit_ADD_IMM(void);
  void asm_jit_ADD_IMM_END(void);
  asm_copy_patch_byte(p_buf, asm_jit_ADD_IMM, asm_jit_ADD_IMM_END, value);
}

void
asm_emit_jit_ADD_SCRATCH(struct util_buffer* p_buf, uint8_t offset) {
  void asm_jit_ADD_SCRATCH(void);
  void asm_jit_ADD_SCRATCH_END(void);
  asm_copy_patch_u32(p_buf,
                     asm_jit_ADD_SCRATCH,
                     asm_jit_ADD_SCRATCH_END,
                     (K_BBC_MEM_READ_IND_ADDR + offset));
}

void
asm_emit_jit_ADD_SCRATCH_Y(struct util_buffer* p_buf) {
  void asm_jit_ADD_SCRATCH_Y(void);
  void asm_jit_ADD_SCRATCH_Y_END(void);
  asm_copy(p_buf, asm_jit_ADD_SCRATCH_Y, asm_jit_ADD_SCRATCH_Y_END);
}

void
asm_emit_jit_CHECK_BCD(struct util_buffer* p_buf,
                       struct util_buffer* p_epilog_buf,
                       uint16_t addr) {
  void asm_jit_CHECK_BCD(void);
  void asm_jit_CHECK_BCD_END(void);
  (void) p_epilog_buf;
  (void) addr;
  asm_copy(p_buf, asm_jit_CHECK_BCD, asm_jit_CHECK_BCD_END);
}

void
asm_emit_jit_CHECK_PAGE_CROSSING_SCRATCH_n(struct util_buffer* p_buf,
                                           uint8_t n) {
  void asm_jit_CHECK_PAGE_CROSSING_SCRATCH_n(void);
  void asm_jit_CHECK_PAGE_CROSSING_SCRATCH_n_mov_patch(void);
  void asm_jit_CHECK_PAGE_CROSSING_SCRATCH_n_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf,
           asm_jit_CHECK_PAGE_CROSSING_SCRATCH_n,
           asm_jit_CHECK_PAGE_CROSSING_SCRATCH_n_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_CHECK_PAGE_CROSSING_SCRATCH_n,
                asm_jit_CHECK_PAGE_CROSSING_SCRATCH_n_mov_patch,
                (K_ASM_TABLE_PAGE_WRAP_CYCLE_INV + n));
}

void
asm_emit_jit_CHECK_PAGE_CROSSING_SCRATCH_X(struct util_buffer* p_buf) {
  void asm_jit_CHECK_PAGE_CROSSING_SCRATCH_X(void);
  void asm_jit_CHECK_PAGE_CROSSING_SCRATCH_X_END(void);
  asm_copy(p_buf,
           asm_jit_CHECK_PAGE_CROSSING_SCRATCH_X,
           asm_jit_CHECK_PAGE_CROSSING_SCRATCH_X_END);
}

void
asm_emit_jit_CHECK_PAGE_CROSSING_SCRATCH_Y(struct util_buffer* p_buf) {
  void asm_jit_CHECK_PAGE_CROSSING_SCRATCH_Y(void);
  void asm_jit_CHECK_PAGE_CROSSING_SCRATCH_Y_END(void);
  asm_copy(p_buf,
           asm_jit_CHECK_PAGE_CROSSING_SCRATCH_Y,
           asm_jit_CHECK_PAGE_CROSSING_SCRATCH_Y_END);
}

void
asm_emit_jit_CHECK_PAGE_CROSSING_X_n(struct util_buffer* p_buf,
                                     uint16_t addr) {
  void asm_jit_CHECK_PAGE_CROSSING_X_n(void);
  void asm_jit_CHECK_PAGE_CROSSING_X_n_mov_patch(void);
  void asm_jit_CHECK_PAGE_CROSSING_X_n_END(void);
  uint32_t value;

  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf,
           asm_jit_CHECK_PAGE_CROSSING_X_n,
           asm_jit_CHECK_PAGE_CROSSING_X_n_END);
  value = K_ASM_TABLE_PAGE_WRAP_CYCLE_INV;
  value += (addr & 0xFF);
  asm_patch_int(p_buf,
                offset,
                asm_jit_CHECK_PAGE_CROSSING_X_n,
                asm_jit_CHECK_PAGE_CROSSING_X_n_mov_patch,
                value);
}

void
asm_emit_jit_CHECK_PAGE_CROSSING_Y_n(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_CHECK_PAGE_CROSSING_Y_n(void);
  void asm_jit_CHECK_PAGE_CROSSING_Y_n_mov_patch(void);
  void asm_jit_CHECK_PAGE_CROSSING_Y_n_END(void);
  uint32_t value;

  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf,
           asm_jit_CHECK_PAGE_CROSSING_Y_n,
           asm_jit_CHECK_PAGE_CROSSING_Y_n_END);
  value = K_ASM_TABLE_PAGE_WRAP_CYCLE_INV;
  value += (addr & 0xFF);
  asm_patch_int(p_buf,
                offset,
                asm_jit_CHECK_PAGE_CROSSING_Y_n,
                asm_jit_CHECK_PAGE_CROSSING_Y_n_mov_patch,
                value);
}

void
asm_emit_jit_CHECK_PENDING_IRQ(struct util_buffer* p_buf,
                               struct util_buffer* p_buf_epilog,
                               uint16_t addr,
                               void* p_trampoline) {
  void asm_jit_CHECK_PENDING_IRQ(void);
  void asm_jit_CHECK_PENDING_IRQ_jump_patch(void);
  void asm_jit_CHECK_PENDING_IRQ_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  (void) p_buf_epilog;
  (void) addr;

  asm_copy(p_buf, asm_jit_CHECK_PENDING_IRQ, asm_jit_CHECK_PENDING_IRQ_END);
  asm_patch_jump(p_buf,
                 offset,
                 asm_jit_CHECK_PENDING_IRQ,
                 asm_jit_CHECK_PENDING_IRQ_jump_patch,
                 p_trampoline);
}

void
asm_emit_jit_CLEAR_CARRY(struct util_buffer* p_buf) {
  void asm_jit_CLEAR_CARRY(void);
  void asm_jit_CLEAR_CARRY_END(void);
  asm_copy(p_buf, asm_jit_CLEAR_CARRY, asm_jit_CLEAR_CARRY_END);
}

void
asm_emit_jit_INVERT_CARRY(struct util_buffer* p_buf) {
  void asm_jit_INVERT_CARRY(void);
  void asm_jit_INVERT_CARRY_END(void);
  asm_copy(p_buf, asm_jit_INVERT_CARRY, asm_jit_INVERT_CARRY_END);
}

void
asm_emit_jit_JMP_SCRATCH_n(struct util_buffer* p_buf, uint16_t n) {
  void asm_jit_JMP_SCRATCH_n(void);
  void asm_jit_JMP_SCRATCH_n_lea_patch(void);
  void asm_jit_JMP_SCRATCH_n_END(void);
  size_t offset = util_buffer_get_pos(p_buf);
  asm_copy(p_buf, asm_jit_JMP_SCRATCH_n, asm_jit_JMP_SCRATCH_n_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_JMP_SCRATCH_n,
                asm_jit_JMP_SCRATCH_n_lea_patch,
                ((K_BBC_JIT_ADDR >> K_BBC_JIT_BYTES_SHIFT) + n));
}

void
asm_emit_jit_LDA_Z(struct util_buffer* p_buf) {
  void asm_jit_LDA_Z(void);
  void asm_jit_LDA_Z_END(void);
  asm_copy(p_buf, asm_jit_LDA_Z, asm_jit_LDA_Z_END);
}

void
asm_emit_jit_LDX_Z(struct util_buffer* p_buf) {
  void asm_jit_LDX_Z(void);
  void asm_jit_LDX_Z_END(void);
  asm_copy(p_buf, asm_jit_LDX_Z, asm_jit_LDX_Z_END);
}

void
asm_emit_jit_LDY_Z(struct util_buffer* p_buf) {
  void asm_jit_LDY_Z(void);
  void asm_jit_LDY_Z_END(void);
  asm_copy(p_buf, asm_jit_LDY_Z, asm_jit_LDY_Z_END);
}

void
asm_emit_jit_LOAD_CARRY_FOR_BRANCH(struct util_buffer* p_buf) {
  void asm_jit_LOAD_CARRY_FOR_BRANCH(void);
  void asm_jit_LOAD_CARRY_FOR_BRANCH_END(void);
  asm_copy(p_buf,
           asm_jit_LOAD_CARRY_FOR_BRANCH,
           asm_jit_LOAD_CARRY_FOR_BRANCH_END);
}

void
asm_emit_jit_LOAD_CARRY_FOR_CALC(struct util_buffer* p_buf) {
  void asm_jit_LOAD_CARRY_FOR_CALC(void);
  void asm_jit_LOAD_CARRY_FOR_CALC_END(void);
  asm_copy(p_buf,
           asm_jit_LOAD_CARRY_FOR_CALC,
           asm_jit_LOAD_CARRY_FOR_CALC_END);
}

void
asm_emit_jit_LOAD_CARRY_INV_FOR_CALC(struct util_buffer* p_buf) {
  void asm_jit_LOAD_CARRY_INV_FOR_CALC(void);
  void asm_jit_LOAD_CARRY_INV_FOR_CALC_END(void);
  asm_copy(p_buf,
           asm_jit_LOAD_CARRY_INV_FOR_CALC,
           asm_jit_LOAD_CARRY_INV_FOR_CALC_END);
}

void
asm_emit_jit_LOAD_OVERFLOW(struct util_buffer* p_buf) {
  void asm_jit_LOAD_OVERFLOW(void);
  void asm_jit_LOAD_OVERFLOW_END(void);
  asm_copy(p_buf, asm_jit_LOAD_OVERFLOW, asm_jit_LOAD_OVERFLOW_END);
}

void
asm_emit_jit_LOAD_SCRATCH_8(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_LOAD_SCRATCH_8(void);
  void asm_jit_LOAD_SCRATCH_8_END(void);
  asm_copy_patch_u32(p_buf,
                     asm_jit_LOAD_SCRATCH_8,
                     asm_jit_LOAD_SCRATCH_8_END,
                     (addr - REG_MEM_OFFSET));
}

void
asm_emit_jit_LOAD_SCRATCH_16(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_MODE_IND_16(void);
  void asm_jit_MODE_IND_16_mov1_patch(void);
  void asm_jit_MODE_IND_16_mov2_patch(void);
  void asm_jit_MODE_IND_16_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf, asm_jit_MODE_IND_16, asm_jit_MODE_IND_16_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_MODE_IND_16,
                asm_jit_MODE_IND_16_mov1_patch,
                (addr - REG_MEM_OFFSET));
  asm_patch_int(p_buf,
                offset,
                asm_jit_MODE_IND_16,
                asm_jit_MODE_IND_16_mov2_patch,
                ((addr + 1) - REG_MEM_OFFSET));
}

void
asm_emit_jit_MODE_ABX(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_MODE_ABX(void);
  void asm_jit_MODE_ABX_END(void);
  asm_copy_patch_u32(p_buf, asm_jit_MODE_ABX, asm_jit_MODE_ABX_END, addr);
}

void
asm_emit_jit_MODE_ABY(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_MODE_ABY(void);
  void asm_jit_MODE_ABY_END(void);
  asm_copy_patch_u32(p_buf, asm_jit_MODE_ABY, asm_jit_MODE_ABY_END, addr);
}

void
asm_emit_jit_MODE_IND_8(struct util_buffer* p_buf, uint8_t addr) {
  void asm_jit_MODE_IND_8(void);
  void asm_jit_MODE_IND_8_mov1_patch(void);
  void asm_jit_MODE_IND_8_mov2_patch(void);
  void asm_jit_MODE_IND_8_END(void);
  uint16_t next_addr;

  size_t offset = util_buffer_get_pos(p_buf);

  if (addr == 0xFF) {
    next_addr = 0;
  } else {
    next_addr = (addr + 1);
  }

  asm_copy(p_buf, asm_jit_MODE_IND_8, asm_jit_MODE_IND_8_END);
  asm_patch_byte(p_buf,
                 offset,
                 asm_jit_MODE_IND_8,
                 asm_jit_MODE_IND_8_mov1_patch,
                 (addr - REG_MEM_OFFSET));
  asm_patch_byte(p_buf,
                 offset,
                 asm_jit_MODE_IND_8,
                 asm_jit_MODE_IND_8_mov2_patch,
                 (next_addr - REG_MEM_OFFSET));
}

void
asm_emit_jit_MODE_IND_16(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_MODE_IND_16(void);
  void asm_jit_MODE_IND_16_mov1_patch(void);
  void asm_jit_MODE_IND_16_mov2_patch(void);
  void asm_jit_MODE_IND_16_END(void);
  uint16_t next_addr;

  size_t offset = util_buffer_get_pos(p_buf);
  uint32_t segment = K_BBC_MEM_READ_FULL_ADDR;
  uint32_t delta = (segment - K_BBC_MEM_READ_IND_ADDR);

  /* On the 6502, (e.g.) JMP (&10FF) does not fetch across the page boundary. */
  if ((addr & 0xFF) == 0xFF) {
    next_addr = (addr & 0xFF00);
  } else {
    next_addr = (addr + 1);
  }

  asm_copy(p_buf, asm_jit_MODE_IND_16, asm_jit_MODE_IND_16_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_MODE_IND_16,
                asm_jit_MODE_IND_16_mov1_patch,
                (addr - REG_MEM_OFFSET + delta));
  asm_patch_int(p_buf,
                offset,
                asm_jit_MODE_IND_16,
                asm_jit_MODE_IND_16_mov2_patch,
                (next_addr - REG_MEM_OFFSET + delta));
}

void
asm_emit_jit_MODE_IND_SCRATCH_16(struct util_buffer* p_buf) {
  void asm_jit_MODE_IND_SCRATCH_16(void);
  void asm_jit_MODE_IND_SCRATCH_16_END(void);
  asm_copy(p_buf, asm_jit_MODE_IND_SCRATCH_16, asm_jit_MODE_IND_SCRATCH_16_END);
}

void
asm_emit_jit_MODE_ZPX(struct util_buffer* p_buf, uint8_t value) {
  void asm_jit_MODE_ZPX_8bit(void);
  void asm_jit_MODE_ZPX_8bit_lea_patch(void);
  void asm_jit_MODE_ZPX_8bit_END(void);
  void asm_jit_MODE_ZPX(void);
  void asm_jit_MODE_ZPX_lea_patch(void);
  void asm_jit_MODE_ZPX_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  if (value <= 0x7F) {
    asm_copy(p_buf, asm_jit_MODE_ZPX_8bit, asm_jit_MODE_ZPX_8bit_END);
    asm_patch_byte(p_buf,
                   offset,
                   asm_jit_MODE_ZPX_8bit,
                   asm_jit_MODE_ZPX_8bit_lea_patch,
                   value);
  } else {
    asm_copy(p_buf, asm_jit_MODE_ZPX, asm_jit_MODE_ZPX_END);
    asm_patch_int(p_buf,
                  offset,
                  asm_jit_MODE_ZPX,
                  asm_jit_MODE_ZPX_lea_patch,
                  value);
  }
}

void
asm_emit_jit_MODE_ZPY(struct util_buffer* p_buf, uint8_t value) {
  void asm_jit_MODE_ZPY_8bit(void);
  void asm_jit_MODE_ZPY_8bit_lea_patch(void);
  void asm_jit_MODE_ZPY_8bit_END(void);
  void asm_jit_MODE_ZPY(void);
  void asm_jit_MODE_ZPY_lea_patch(void);
  void asm_jit_MODE_ZPY_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  if (value <= 0x7F) {
    asm_copy(p_buf, asm_jit_MODE_ZPY_8bit, asm_jit_MODE_ZPY_8bit_END);
    asm_patch_byte(p_buf,
                   offset,
                   asm_jit_MODE_ZPY_8bit,
                   asm_jit_MODE_ZPY_8bit_lea_patch,
                   value);
  } else {
    asm_copy(p_buf, asm_jit_MODE_ZPY, asm_jit_MODE_ZPY_END);
    asm_patch_int(p_buf,
                  offset,
                  asm_jit_MODE_ZPY,
                  asm_jit_MODE_ZPY_lea_patch,
                  value);
  }
}

void
asm_emit_jit_PULL_16(struct util_buffer* p_buf) {
  void asm_jit_PULL_16(void);
  void asm_jit_PULL_16_END(void);
  asm_copy(p_buf, asm_jit_PULL_16, asm_jit_PULL_16_END);
}

void
asm_emit_jit_PUSH_16(struct util_buffer* p_buf, uint16_t value) {
  void asm_jit_PUSH_16(void);
  void asm_jit_PUSH_16_word_patch(void);
  void asm_jit_PUSH_16_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf, asm_jit_PUSH_16, asm_jit_PUSH_16_END);
  asm_patch_u16(p_buf,
                offset,
                asm_jit_PUSH_16,
                asm_jit_PUSH_16_word_patch,
                value);
}

void
asm_emit_jit_SAVE_CARRY(struct util_buffer* p_buf) {
  void asm_jit_SAVE_CARRY(void);
  void asm_jit_SAVE_CARRY_END(void);
  asm_copy(p_buf, asm_jit_SAVE_CARRY, asm_jit_SAVE_CARRY_END);
}

void
asm_emit_jit_SAVE_CARRY_INV(struct util_buffer* p_buf) {
  void asm_jit_SAVE_CARRY_INV(void);
  void asm_jit_SAVE_CARRY_INV_END(void);
  asm_copy(p_buf, asm_jit_SAVE_CARRY_INV, asm_jit_SAVE_CARRY_INV_END);
}

void
asm_emit_jit_SAVE_OVERFLOW(struct util_buffer* p_buf) {
  void asm_jit_SAVE_OVERFLOW(void);
  void asm_jit_SAVE_OVERFLOW_END(void);
  asm_copy(p_buf, asm_jit_SAVE_OVERFLOW, asm_jit_SAVE_OVERFLOW_END);
}

void
asm_emit_jit_SET_CARRY(struct util_buffer* p_buf) {
  void asm_jit_SET_CARRY(void);
  void asm_jit_SET_CARRY_END(void);
  asm_copy(p_buf, asm_jit_SET_CARRY, asm_jit_SET_CARRY_END);
}

void
asm_emit_jit_STOA_IMM(struct util_buffer* p_buf, uint16_t addr, uint8_t value) {
  void asm_jit_STOA_IMM(void);
  void asm_jit_STOA_IMM_END(void);
  void asm_jit_STOA_IMM_8bit(void);
  void asm_jit_STOA_IMM_8bit_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  if (addr < 0x100) {
    asm_copy(p_buf, asm_jit_STOA_IMM_8bit, asm_jit_STOA_IMM_8bit_END);
    asm_patch_byte(p_buf,
                   offset,
                   asm_jit_STOA_IMM_8bit,
                   asm_jit_STOA_IMM_8bit_END,
                   value);
    asm_patch_byte(p_buf,
                   (offset - 1),
                   asm_jit_STOA_IMM_8bit,
                   asm_jit_STOA_IMM_8bit_END,
                   (addr - REG_MEM_OFFSET));
  } else {
    asm_copy(p_buf, asm_jit_STOA_IMM, asm_jit_STOA_IMM_END);
    asm_patch_byte(p_buf,
                   offset,
                   asm_jit_STOA_IMM,
                   asm_jit_STOA_IMM_END,
                   value);
    asm_patch_int(p_buf,
                  (offset - 1),
                  asm_jit_STOA_IMM,
                  asm_jit_STOA_IMM_END,
                  (addr - REG_MEM_OFFSET + K_BBC_MEM_OFFSET_TO_WRITE_FULL));
  }
}

void
asm_emit_jit_SUB_ABS(struct util_buffer* p_buf,
                     uint16_t addr,
                     uint32_t segment) {
  void asm_jit_SUB_ZPG(void);
  void asm_jit_SUB_ZPG_END(void);
  void asm_jit_SUB_ABS(void);
  void asm_jit_SUB_ABS_END(void);
  uint32_t delta = (segment - K_BBC_MEM_READ_IND_ADDR);
  if (addr < 0x100) {
    asm_copy_patch_byte(p_buf,
                        asm_jit_SUB_ZPG,
                        asm_jit_SUB_ZPG_END,
                        (addr - REG_MEM_OFFSET + delta));
  } else {
    asm_copy_patch_u32(p_buf,
                       asm_jit_SUB_ABS,
                       asm_jit_SUB_ABS_END,
                       (addr - REG_MEM_OFFSET + delta));
  }
}

void
asm_emit_jit_SUB_IMM(struct util_buffer* p_buf, uint8_t value) {
  void asm_jit_SUB_IMM(void);
  void asm_jit_SUB_IMM_END(void);
  asm_copy_patch_byte(p_buf, asm_jit_SUB_IMM, asm_jit_SUB_IMM_END, value);
}

void
asm_emit_jit_WRITE_INV_ABS(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_WRITE_INV_ABS(void);
  void asm_jit_WRITE_INV_ABS_offset_patch(void);
  void asm_jit_WRITE_INV_ABS_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf, asm_jit_WRITE_INV_ABS, asm_jit_WRITE_INV_ABS_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_WRITE_INV_ABS,
                asm_jit_WRITE_INV_ABS_offset_patch,
                (K_JIT_CONTEXT_OFFSET_JIT_PTRS + (addr * sizeof(uint32_t))));
}

void
asm_emit_jit_WRITE_INV_SCRATCH(struct util_buffer* p_buf) {
  void asm_jit_WRITE_INV_SCRATCH(void);
  void asm_jit_WRITE_INV_SCRATCH_END(void);
  asm_copy(p_buf, asm_jit_WRITE_INV_SCRATCH, asm_jit_WRITE_INV_SCRATCH_END);
}

void
asm_emit_jit_WRITE_INV_SCRATCH_n(struct util_buffer* p_buf, uint8_t value) {
  void asm_jit_WRITE_INV_SCRATCH_n_8bit(void);
  void asm_jit_WRITE_INV_SCRATCH_n_8bit_lea_patch(void);
  void asm_jit_WRITE_INV_SCRATCH_n_8bit_END(void);
  void asm_jit_WRITE_INV_SCRATCH_n_32bit(void);
  void asm_jit_WRITE_INV_SCRATCH_n_32bit_lea_patch(void);
  void asm_jit_WRITE_INV_SCRATCH_n_32bit_END(void);

  size_t offset = util_buffer_get_pos(p_buf);

  if (value < 0x80) {
    asm_copy(p_buf,
             asm_jit_WRITE_INV_SCRATCH_n_8bit,
             asm_jit_WRITE_INV_SCRATCH_n_8bit_END);
    asm_patch_byte(p_buf,
                   offset,
                   asm_jit_WRITE_INV_SCRATCH_n_8bit,
                   asm_jit_WRITE_INV_SCRATCH_n_8bit_lea_patch,
                   value);
  } else {
    asm_copy(p_buf,
             asm_jit_WRITE_INV_SCRATCH_n_32bit,
             asm_jit_WRITE_INV_SCRATCH_n_32bit_END);
    asm_patch_int(p_buf,
                  offset,
                  asm_jit_WRITE_INV_SCRATCH_n_32bit,
                  asm_jit_WRITE_INV_SCRATCH_n_32bit_lea_patch,
                  value);
  }
}

void
asm_emit_jit_WRITE_INV_SCRATCH_Y(struct util_buffer* p_buf) {
  void asm_jit_WRITE_INV_SCRATCH_Y(void);
  void asm_jit_WRITE_INV_SCRATCH_Y_END(void);
  asm_copy(p_buf, asm_jit_WRITE_INV_SCRATCH_Y, asm_jit_WRITE_INV_SCRATCH_Y_END);
}

void
asm_emit_jit_ALR_IMM(struct util_buffer* p_buf, uint8_t value) {
  void asm_jit_ALR_IMM(void);
  void asm_jit_ALR_IMM_patch_byte(void);
  void asm_jit_ALR_IMM_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf, asm_jit_ALR_IMM, asm_jit_ALR_IMM_END);
  asm_patch_byte(p_buf,
                 offset,
                 asm_jit_ALR_IMM,
                 asm_jit_ALR_IMM_patch_byte,
                 value);
}

void
asm_emit_jit_ASL_ACC_n(struct util_buffer* p_buf, uint8_t n) {
  void asm_jit_ASL_ACC_n(void);
  void asm_jit_ASL_ACC_n_END(void);
  asm_copy_patch_byte(p_buf, asm_jit_ASL_ACC_n, asm_jit_ASL_ACC_n_END, n);
}

void
asm_emit_jit_LDA_SCRATCH_X(struct util_buffer* p_buf) {
  void asm_jit_LDA_SCRATCH_X(void);
  void asm_jit_LDA_SCRATCH_X_END(void);
  asm_copy(p_buf, asm_jit_LDA_SCRATCH_X, asm_jit_LDA_SCRATCH_X_END);
}

void
asm_emit_jit_LSR_ACC_n(struct util_buffer* p_buf, uint8_t n) {
  void asm_jit_LSR_ACC_n(void);
  void asm_jit_LSR_ACC_n_END(void);
  asm_copy_patch_byte(p_buf, asm_jit_LSR_ACC_n, asm_jit_LSR_ACC_n_END, n);
}

void
asm_emit_jit_ROL_ABS(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_ROL_ZPG(void);
  void asm_jit_ROL_ZPG_END(void);
  void asm_jit_ROL_ABS(void);
  void asm_jit_ROL_ABS_END(void);
  if (addr < 0x100) {
    asm_copy_patch_byte(p_buf,
                        asm_jit_ROL_ZPG,
                        asm_jit_ROL_ZPG_END,
                        (addr - REG_MEM_OFFSET));
  } else {
    asm_copy_patch_u32(p_buf,
                       asm_jit_ROL_ABS,
                       asm_jit_ROL_ABS_END,
                       (addr - REG_MEM_OFFSET));
  }
}

void
asm_emit_jit_ROL_ACC_n(struct util_buffer* p_buf, uint8_t n) {
  void asm_jit_ROL_ACC_n(void);
  void asm_jit_ROL_ACC_n_END(void);
  asm_copy_patch_byte(p_buf,
                          asm_jit_ROL_ACC_n,
                          asm_jit_ROL_ACC_n_END,
                          n);
}

void
asm_emit_jit_ROR_ABS(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_ROR_ZPG(void);
  void asm_jit_ROR_ZPG_END(void);
  void asm_jit_ROR_ABS(void);
  void asm_jit_ROR_ABS_END(void);
  if (addr < 0x100) {
    asm_copy_patch_byte(p_buf,
                        asm_jit_ROR_ZPG,
                        asm_jit_ROR_ZPG_END,
                        (addr - REG_MEM_OFFSET));
  } else {
    asm_copy_patch_u32(p_buf,
                       asm_jit_ROR_ABS,
                       asm_jit_ROR_ABS_END,
                       (addr - REG_MEM_OFFSET));
  }
}

void
asm_emit_jit_ROR_ACC_n(struct util_buffer* p_buf, uint8_t n) {
  void asm_jit_ROR_ACC_n(void);
  void asm_jit_ROR_ACC_n_END(void);
  asm_copy_patch_byte(p_buf, asm_jit_ROR_ACC_n, asm_jit_ROR_ACC_n_END, n);
}

void
asm_emit_jit_SLO_ABS(struct util_buffer* p_buf, uint16_t addr) {
  void asm_jit_SLO_ABS(void);
  void asm_jit_SLO_ABS_mov1_patch(void);
  void asm_jit_SLO_ABS_mov2_patch(void);
  void asm_jit_SLO_ABS_END(void);
  size_t offset = util_buffer_get_pos(p_buf);

  asm_copy(p_buf, asm_jit_SLO_ABS, asm_jit_SLO_ABS_END);
  asm_patch_int(p_buf,
                offset,
                asm_jit_SLO_ABS,
                asm_jit_SLO_ABS_mov1_patch,
                (addr - REG_MEM_OFFSET));
  asm_patch_int(p_buf,
                offset,
                asm_jit_SLO_ABS,
                asm_jit_SLO_ABS_mov2_patch,
                (addr - REG_MEM_OFFSET + K_BBC_MEM_OFFSET_TO_WRITE_FULL));
}

static int
asm_jit_uop_match(struct asm_uop** p_out,
                  struct asm_uop* p_start_uop,
                  uint32_t num_left,
                  int32_t match1,
                  int32_t match2,
                  int32_t match3) {
  struct asm_uop* p_uop = p_start_uop;
  assert(match1 != -1);
  if (num_left < 2) return 0;
  if (p_uop->is_eliminated) return 0;
  if (p_uop->uopcode != match1) return 0;
  p_uop++;
  if (match2 != -1) {
    if (num_left < 3) return 0;
    if (p_uop->is_eliminated) return 0;
    if (p_uop->uopcode != match2) return 0;
    p_uop++;
  }
  if (match3 != -1) {
    if (num_left < 4) return 0;
    if (p_uop->is_eliminated) return 0;
    if (p_uop->uopcode != match3) return 0;
    p_uop++;
  }

  *p_out = p_uop;

  while (p_uop != p_start_uop) {
    p_uop->is_eliminated = 1;
    p_uop--;
  }

  return 1;
}

static int
asm_jit_uop_eliminate(struct asm_uop* p_uop,
                      uint32_t num_left,
                      int32_t uopcode) {
  while (num_left--) {
    if (p_uop->uopcode == uopcode) {
      p_uop->is_eliminated = 1;
      return 1;
    }
    p_uop++;
  }

  return 0;
}

static uint32_t
asm_jit_get_segment(struct asm_jit_struct* p_asm,
                    uint16_t addr,
                    int is_write,
                    int is_abn) {
  uint32_t segment;
  int is_always_ram = p_asm->is_memory_always_ram(p_asm->p_memory_object, addr);
  /* Assumes address space wrap and hardware register access taken care of
   * elsewhere.
   */
  if (is_abn) {
    is_always_ram &= p_asm->is_memory_always_ram(p_asm->p_memory_object,
                                                 (uint16_t) (addr + 0xFF));
  }

  if (is_always_ram) segment = K_BBC_MEM_READ_IND_ADDR;
  else if (!is_write) segment = K_BBC_MEM_READ_FULL_ADDR;
  else segment = K_BBC_MEM_WRITE_FULL_ADDR;

  return segment;
}

void
asm_jit_rewrite(struct asm_jit_struct* p_asm,
                struct asm_uop* p_uops,
                uint32_t num_uops) {
  uint32_t i;
  struct asm_uop* p_next_uop;
  uint32_t num_left = num_uops;

  (void) p_asm;

  for (i = 0; i < num_uops; ++i, num_left--) {
    struct asm_uop* p_match;
    struct asm_uop* p_uop = &p_uops[i];
    if (asm_jit_uop_match(&p_match,
                          p_uop,
                          num_left,
                          k_opcode_value_set,
                          -1,
                          -1)) {
      switch (p_match->uopcode) {
      case k_opcode_ADC: p_uop->uopcode = k_opcode_x64_ADC_IMM; break;
      case k_opcode_ALR: p_uop->uopcode = k_opcode_x64_ALR_IMM; break;
      case k_opcode_AND: p_uop->uopcode = k_opcode_x64_AND_IMM; break;
      case k_opcode_CMP: p_uop->uopcode = k_opcode_x64_CMP_IMM; break;
      case k_opcode_CPX: p_uop->uopcode = k_opcode_x64_CPX_IMM; break;
      case k_opcode_CPY: p_uop->uopcode = k_opcode_x64_CPY_IMM; break;
      case k_opcode_EOR: p_uop->uopcode = k_opcode_x64_EOR_IMM; break;
      case k_opcode_LDA: p_uop->uopcode = k_opcode_x64_LDA_IMM; break;
      case k_opcode_LDX: p_uop->uopcode = k_opcode_x64_LDX_IMM; break;
      case k_opcode_LDY: p_uop->uopcode = k_opcode_x64_LDY_IMM; break;
      case k_opcode_ORA: p_uop->uopcode = k_opcode_x64_ORA_IMM; break;
      case k_opcode_SBC: p_uop->uopcode = k_opcode_x64_SBC_IMM; break;
      default: assert(0); break;
      }
    } else if (asm_jit_uop_match(&p_match,
                                 p_uop,
                                 num_left,
                                 k_opcode_addr_add_y,
                                 k_opcode_addr_check,
                                 k_opcode_value_load)) {
      switch (p_match->uopcode) {
      case k_opcode_ADC: p_uop->uopcode = k_opcode_x64_ADC_addr_Y; break;
      case k_opcode_AND: p_uop->uopcode = k_opcode_x64_AND_addr_Y; break;
      case k_opcode_CMP: p_uop->uopcode = k_opcode_x64_CMP_addr_Y; break;
      case k_opcode_EOR: p_uop->uopcode = k_opcode_x64_EOR_addr_Y; break;
      case k_opcode_LDA: p_uop->uopcode = k_opcode_x64_LDA_addr_Y; break;
      case k_opcode_ORA: p_uop->uopcode = k_opcode_x64_ORA_addr_Y; break;
      case k_opcode_SBC: p_uop->uopcode = k_opcode_x64_SBC_addr_Y; break;
      case k_opcode_STA: p_uop->uopcode = k_opcode_x64_STA_addr_Y; break;
      default: assert(0); break;
      }
    } else if (asm_jit_uop_match(&p_match,
                                 p_uop,
                                 num_left,
                                 k_opcode_value_load,
                                 -1,
                                 -1)) {
      switch (p_match->uopcode) {
      case k_opcode_ADC: p_uop->uopcode = k_opcode_x64_ADC_addr; break;
      case k_opcode_ASL_value:
      case k_opcode_DEC_value:
      case k_opcode_INC_value:
      case k_opcode_LSR_value:
      case k_opcode_ROL_value:
      case k_opcode_ROR_value:
        /* These are the zpx mode RMW opcodes. Do a full RMW for now. */
        p_match->is_eliminated = 0;
        if ((p_match->uopcode != k_opcode_ROL_value) &&
            (p_match->uopcode != k_opcode_ROR_value)) {
          /* Eliminate the NZ flags fetch for the x64 opcodes that do it
           * themselves.
           */
          (void) asm_jit_uop_eliminate((p_match + 1),
                                       (num_left - 2),
                                       k_opcode_flags_nz_value);
        }
        break;
      case k_opcode_AND: p_uop->uopcode = k_opcode_x64_AND_addr; break;
      case k_opcode_CMP: p_uop->uopcode = k_opcode_x64_CMP_addr; break;
      case k_opcode_EOR: p_uop->uopcode = k_opcode_x64_EOR_addr; break;
      case k_opcode_LDA: p_uop->uopcode = k_opcode_x64_LDA_addr; break;
      case k_opcode_LDX: p_uop->uopcode = k_opcode_x64_LDX_addr; break;
      case k_opcode_LDY: p_uop->uopcode = k_opcode_x64_LDY_addr; break;
      case k_opcode_NOP: p_uop->uopcode = k_opcode_NOP; break;
      case k_opcode_ORA: p_uop->uopcode = k_opcode_x64_ORA_addr; break;
      case k_opcode_SBC: p_uop->uopcode = k_opcode_x64_SBC_addr; break;
      case k_opcode_STA: p_uop->uopcode = k_opcode_x64_STA_addr; break;
      case k_opcode_STX: p_uop->uopcode = k_opcode_x64_STX_addr; break;
      case k_opcode_STY: p_uop->uopcode = k_opcode_x64_STY_addr; break;
      default: assert(0); break;
      }
    } else if (asm_jit_uop_match(&p_match,
                                 p_uop,
                                 num_left,
                                 k_opcode_addr_set,
                                 k_opcode_value_load,
                                 -1)) {
      /* TODO: handle RMW with a real RMW for non-RAM addresses. */
      int is_zpg = (p_uop->value1 <= 0xFF);
      int is_write = 0;
      int is_rmw = 0;
      switch (p_match->uopcode) {
      case k_opcode_ADC:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_ADC_ZPG;
        else p_uop->uopcode = k_opcode_x64_ADC_ABS;
        break;
      case k_opcode_AND:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_AND_ZPG;
        else p_uop->uopcode = k_opcode_x64_AND_ABS;
        break;
      case k_opcode_ASL_value:
        is_rmw = 1;
        if (is_zpg) p_uop->uopcode = k_opcode_x64_ASL_ZPG;
        else p_uop->uopcode = k_opcode_x64_ASL_ABS;
        break;
      case k_opcode_BIT:
        p_match->is_eliminated = 0;
        if (is_zpg) p_uop->uopcode = k_opcode_x64_load_zpg;
        else p_uop->uopcode = k_opcode_x64_load_abs;
        break;
      case k_opcode_CMP:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_CMP_ZPG;
        else p_uop->uopcode = k_opcode_x64_CMP_ABS;
        break;
      case k_opcode_CPX:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_CPX_ZPG;
        else p_uop->uopcode = k_opcode_x64_CPX_ABS;
        break;
      case k_opcode_CPY:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_CPY_ZPG;
        else p_uop->uopcode = k_opcode_x64_CPY_ABS;
        break;
      case k_opcode_DEC_value:
        is_rmw = 1;
        if (is_zpg) p_uop->uopcode = k_opcode_x64_DEC_ZPG;
        else p_uop->uopcode = k_opcode_x64_DEC_ABS;
        break;
      case k_opcode_EOR:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_EOR_ZPG;
        else p_uop->uopcode = k_opcode_x64_EOR_ABS;
        break;
      case k_opcode_INC_value:
        is_rmw = 1;
        if (is_zpg) p_uop->uopcode = k_opcode_x64_INC_ZPG;
        else p_uop->uopcode = k_opcode_x64_INC_ABS;
        break;
      case k_opcode_LDA:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_LDA_ZPG;
        else p_uop->uopcode = k_opcode_x64_LDA_ABS;
        break;
      case k_opcode_LDX:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_LDX_ZPG;
        else p_uop->uopcode = k_opcode_x64_LDX_ABS;
        break;
      case k_opcode_LDY:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_LDY_ZPG;
        else p_uop->uopcode = k_opcode_x64_LDY_ABS;
        break;
      case k_opcode_LSR_value:
        is_rmw = 1;
        if (is_zpg) p_uop->uopcode = k_opcode_x64_LSR_ZPG;
        else p_uop->uopcode = k_opcode_x64_LSR_ABS;
        break;
      case k_opcode_NOP: p_uop->uopcode = k_opcode_NOP; break;
      case k_opcode_ORA:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_ORA_ZPG;
        else p_uop->uopcode = k_opcode_x64_ORA_ABS;
        break;
      case k_opcode_ROL_value:
      case k_opcode_ROR_value:
        p_uop->uopcode = k_opcode_x64_load_abs;
        p_match->is_eliminated = 0;
        assert((i + 4) < num_uops);
        p_next_uop = &p_uops[i + 3];
        assert(p_next_uop->uopcode == k_opcode_SAVE_CARRY);
        p_next_uop = &p_uops[i + 4];
        assert(p_next_uop->uopcode == k_opcode_value_store);
        p_next_uop->uopcode = k_opcode_x64_store_abs;
        p_next_uop->value1 = p_uop->value1;
        /* TODO. */
        p_next_uop->value2 = K_BBC_MEM_WRITE_FULL_ADDR;
        break;
      case k_opcode_SBC:
        if (is_zpg) p_uop->uopcode = k_opcode_x64_SBC_ZPG;
        else p_uop->uopcode = k_opcode_x64_SBC_ABS;
        break;
      case k_opcode_SAX:
        is_write = 1;
        p_uop->uopcode = k_opcode_x64_SAX_ABS;
        break;
      case k_opcode_SLO:
        is_rmw = 1;
        p_uop->uopcode = k_opcode_x64_SLO_ABS;
        break;
      case k_opcode_STA:
        is_write = 1;
        if (is_zpg) p_uop->uopcode = k_opcode_x64_STA_ZPG;
        else p_uop->uopcode = k_opcode_x64_STA_ABS;
        break;
      case k_opcode_STX:
        is_write = 1;
        if (is_zpg) p_uop->uopcode = k_opcode_x64_STX_ZPG;
        else p_uop->uopcode = k_opcode_x64_STX_ABS;
        break;
      case k_opcode_STY:
        is_write = 1;
        if (is_zpg) p_uop->uopcode = k_opcode_x64_STY_ZPG;
        else p_uop->uopcode = k_opcode_x64_STY_ABS;
        break;
      default: assert(0); break;
      }
      p_uop->value2 = asm_jit_get_segment(p_asm, p_uop->value1, is_write, 0);
      if (is_rmw) {
        int found;
        found = asm_jit_uop_eliminate((p_match + 1),
                                      (num_left - 3),
                                      k_opcode_value_store);
        assert(found);
        (void) asm_jit_uop_eliminate((p_match + 1),
                                      (num_left - 3),
                                      k_opcode_flags_nz_value);
      }
    } else if (asm_jit_uop_match(&p_match,
                                 p_uop,
                                 num_left,
                                 k_opcode_addr_set,
                                 k_opcode_addr_add_x,
                                 k_opcode_value_load)) {
      int is_write = 0;
      switch (p_match->uopcode) {
      case k_opcode_ADC: p_uop->uopcode = k_opcode_x64_ADC_ABX; break;
      case k_opcode_AND: p_uop->uopcode = k_opcode_x64_AND_ABX; break;
      /* TODO: optimize these into a single RMW instruction where possible. */
      case k_opcode_ASL_value:
      case k_opcode_DEC_value:
      case k_opcode_INC_value:
      case k_opcode_LSR_value:
      case k_opcode_ROL_value:
      case k_opcode_ROR_value:
        p_uop->uopcode = k_opcode_x64_mode_abx_and_load;
        p_match->is_eliminated = 0;
        assert((i + 4) < num_uops);
        p_next_uop = &p_uops[i + 4];
        if (p_next_uop->uopcode == k_opcode_SAVE_CARRY) {
          assert((i + 5) < num_uops);
          p_next_uop = &p_uops[i + 5];
        }
        assert(p_next_uop->uopcode == k_opcode_value_store);
        p_next_uop->uopcode = k_opcode_x64_mode_abx_store;
        p_next_uop->value1 = p_uop->value1;
        /* TODO. */
        p_next_uop->value2 = K_BBC_MEM_WRITE_FULL_ADDR;
        if ((p_match->uopcode != k_opcode_ROL_value) &&
            (p_match->uopcode != k_opcode_ROR_value)) {
          (void) asm_jit_uop_eliminate((p_match + 1),
                                        (num_left - 4),
                                        k_opcode_flags_nz_value);
        }
        break;
      case k_opcode_CMP: p_uop->uopcode = k_opcode_x64_CMP_ABX; break;
      case k_opcode_EOR: p_uop->uopcode = k_opcode_x64_EOR_ABX; break;
      case k_opcode_LDA: p_uop->uopcode = k_opcode_x64_LDA_ABX; break;
      case k_opcode_LDY: p_uop->uopcode = k_opcode_x64_LDY_ABX; break;
      case k_opcode_NOP: p_uop->uopcode = k_opcode_NOP; break;
      case k_opcode_ORA: p_uop->uopcode = k_opcode_x64_ORA_ABX; break;
      case k_opcode_SBC: p_uop->uopcode = k_opcode_x64_SBC_ABX; break;
      case k_opcode_STA:
        is_write = 1;
        p_uop->uopcode = k_opcode_x64_STA_ABX;
        break;
      default: assert(0); break;
      }
      p_uop->value2 = asm_jit_get_segment(p_asm, p_uop->value1, is_write, 1);
    } else if (asm_jit_uop_match(&p_match,
                                 p_uop,
                                 num_left,
                                 k_opcode_addr_set,
                                 k_opcode_addr_add_y,
                                 k_opcode_value_load)) {
      int is_write = 0;
      switch (p_match->uopcode) {
      case k_opcode_ADC: p_uop->uopcode = k_opcode_x64_ADC_ABY; break;
      case k_opcode_AND: p_uop->uopcode = k_opcode_x64_AND_ABY; break;
      case k_opcode_CMP: p_uop->uopcode = k_opcode_x64_CMP_ABY; break;
      case k_opcode_EOR: p_uop->uopcode = k_opcode_x64_EOR_ABY; break;
      case k_opcode_LDA: p_uop->uopcode = k_opcode_x64_LDA_ABY; break;
      case k_opcode_LDX: p_uop->uopcode = k_opcode_x64_LDX_ABY; break;
      case k_opcode_NOP: p_uop->uopcode = k_opcode_NOP; break;
      case k_opcode_ORA: p_uop->uopcode = k_opcode_x64_ORA_ABY; break;
      case k_opcode_SBC: p_uop->uopcode = k_opcode_x64_SBC_ABY; break;
      case k_opcode_STA:
        is_write = 1;
        p_uop->uopcode = k_opcode_x64_STA_ABY;
        break;
      default: assert(0); break;
      }
      p_uop->value2 = asm_jit_get_segment(p_asm, p_uop->value1, is_write, 1);
    }
    switch (p_uop->uopcode) {
    case k_opcode_addr_set:
      if ((i + 1) >= num_uops) break;
      p_next_uop = &p_uops[i + 1];
      switch (p_next_uop->uopcode) {
      case k_opcode_value_load_16bit:
        /* This is the indirect load sequence for JMP ($xxxx). */
        p_uop->uopcode = k_opcode_MODE_IND_16;
        p_next_uop->is_eliminated = 1;
        break;
      case k_opcode_addr_add_x_8bit:
        p_uop->uopcode = k_opcode_x64_mode_zpx;
        p_next_uop->is_eliminated = 1;
        break;
      case k_opcode_addr_add_y_8bit:
        p_uop->uopcode = k_opcode_x64_mode_zpy;
        p_next_uop->is_eliminated = 1;
        break;
      case k_opcode_addr_load_16bit_zpg:
        p_uop->uopcode = k_opcode_x64_mode_idy_load;
        p_next_uop->is_eliminated = 1;
      default:
        break;
      }
      break;
    case k_opcode_addr_check:
      /* Checking high addresses is handled via fault-fixup, not explicitly. */
      p_uop->is_eliminated = 1;
      break;
    default:
      break;
    }
  }
}

void
asm_emit_jit(struct asm_jit_struct* p_asm,
             struct util_buffer* p_dest_buf,
             struct util_buffer* p_dest_buf_epilog,
             struct asm_uop* p_uop) {
  /* The segment we need to hit for memory accesses.
   * We have different segments:
   * READ
   * WRITE
   * READ INDIRECT
   * WRITE INDIRECT
   * These segments are generally different virtual views on top of the same
   * physical 6502 memory backing buffer. The differences are that writes to
   * ROM area are silently / quickly ignored, and the indirect segments fault
   * if hardware registers are hit.
   * To minimize L1 DTLB issues, we'll generally map all accesses to READ
   * INDIRECT when we know it doesn't make a difference.
   */
  uint32_t delta;
  void* p_trampolines;
  void* p_trampoline_addr = NULL;
  int32_t uopcode = p_uop->uopcode;
  uint32_t value1 = p_uop->value1;
  uint32_t value2 = p_uop->value2;

  (void) p_asm;

  assert(uopcode >= 0x100);

  /* Resolve trampoline addresses. */
  /* Resolve any addresses to real pointers. */
  switch (uopcode) {
  case k_opcode_countdown:
  case k_opcode_check_pending_irq:
    p_trampolines = os_alloc_get_mapping_addr(s_p_mapping_trampolines);
    p_trampoline_addr = (p_trampolines + (value1 * K_BBC_JIT_TRAMPOLINE_BYTES));
    break;
  default:
    break;
  }

  /* Emit the opcode. */
  switch (uopcode) {
  case k_opcode_add_cycles:
    asm_emit_jit_ADD_CYCLES(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_check_bcd:
    asm_emit_jit_CHECK_BCD(p_dest_buf, p_dest_buf_epilog, (uint16_t) value1);
    break;
  case k_opcode_check_pending_irq:
    asm_emit_jit_CHECK_PENDING_IRQ(p_dest_buf,
                                   p_dest_buf_epilog,
                                   (uint16_t) value1,
                                   p_trampoline_addr);
    break;
  case k_opcode_countdown:
    asm_emit_jit_check_countdown(p_dest_buf,
                                 p_dest_buf_epilog,
                                 (uint32_t) value2,
                                 (uint16_t) value1,
                                 p_trampoline_addr);
    break;
  case k_opcode_debug:
    asm_emit_jit_call_debug(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_interp:
    asm_emit_jit_jump_interp(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_inturbo:
    asm_emit_jit_call_inturbo(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_for_testing:
    asm_emit_jit_for_testing(p_dest_buf);
    break;
  case k_opcode_addr_load_16bit_zpg: ASM(addr_load_16bit_zpg); break;
  case k_opcode_ADD_ABS:
    asm_emit_jit_ADD_ABS(p_dest_buf, (uint16_t) value1, value2);
    break;
  case k_opcode_ADD_ABX:
    asm_emit_jit_ADD_ABX(p_dest_buf, (uint16_t) value1, value2);
    break;
  case k_opcode_ADD_ABY:
    asm_emit_jit_ADD_ABY(p_dest_buf, (uint16_t) value1, value2);
    break;
  case k_opcode_ADD_IMM:
    asm_emit_jit_ADD_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_ADD_SCRATCH:
    asm_emit_jit_ADD_SCRATCH(p_dest_buf, 0);
    break;
  case k_opcode_ADD_SCRATCH_Y:
    asm_emit_jit_ADD_SCRATCH_Y(p_dest_buf);
    break;
  case k_opcode_ASL_ACC_n:
    asm_emit_jit_ASL_ACC_n(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_n:
    asm_emit_jit_CHECK_PAGE_CROSSING_SCRATCH_n(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_X:
    asm_emit_jit_CHECK_PAGE_CROSSING_SCRATCH_X(p_dest_buf);
    break;
  case k_opcode_CHECK_PAGE_CROSSING_SCRATCH_Y:
    asm_emit_jit_CHECK_PAGE_CROSSING_SCRATCH_Y(p_dest_buf);
    break;
  case k_opcode_CHECK_PAGE_CROSSING_X_n:
    asm_emit_jit_CHECK_PAGE_CROSSING_X_n(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_CHECK_PAGE_CROSSING_Y_n:
    asm_emit_jit_CHECK_PAGE_CROSSING_Y_n(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_CLEAR_CARRY:
    asm_emit_jit_CLEAR_CARRY(p_dest_buf);
    break;
  case k_opcode_flags_nz_a: asm_emit_instruction_A_NZ_flags(p_dest_buf); break;
  case k_opcode_flags_nz_x: asm_emit_instruction_X_NZ_flags(p_dest_buf); break;
  case k_opcode_flags_nz_y: asm_emit_instruction_Y_NZ_flags(p_dest_buf); break;
  case k_opcode_flags_nz_value: ASM(flags_nz_value); break;
  case k_opcode_INVERT_CARRY:
    asm_emit_jit_INVERT_CARRY(p_dest_buf);
    break;
  case k_opcode_JMP_SCRATCH_n:
    asm_emit_jit_JMP_SCRATCH_n(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_LDA_SCRATCH_X:
    asm_emit_jit_LDA_SCRATCH_X(p_dest_buf);
    break;
  case k_opcode_LDA_Z:
    asm_emit_jit_LDA_Z(p_dest_buf);
    break;
  case k_opcode_LDX_Z:
    asm_emit_jit_LDX_Z(p_dest_buf);
    break;
  case k_opcode_LDY_Z:
    asm_emit_jit_LDY_Z(p_dest_buf);
    break;
  case k_opcode_LOAD_CARRY_FOR_BRANCH:
    asm_emit_jit_LOAD_CARRY_FOR_BRANCH(p_dest_buf);
    break;
  case k_opcode_LOAD_CARRY_FOR_CALC:
    asm_emit_jit_LOAD_CARRY_FOR_CALC(p_dest_buf);
    break;
  case k_opcode_LOAD_CARRY_INV_FOR_CALC:
    asm_emit_jit_LOAD_CARRY_INV_FOR_CALC(p_dest_buf);
    break;
  case k_opcode_LOAD_OVERFLOW:
    asm_emit_jit_LOAD_OVERFLOW(p_dest_buf);
    break;
  case k_opcode_LOAD_SCRATCH_8:
    asm_emit_jit_LOAD_SCRATCH_8(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_LOAD_SCRATCH_16:
    asm_emit_jit_LOAD_SCRATCH_16(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_LSR_ACC_n:
    asm_emit_jit_LSR_ACC_n(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_MODE_ABX:
    asm_emit_jit_MODE_ABX(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_MODE_ABY:
    asm_emit_jit_MODE_ABY(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_MODE_IND_16:
    asm_emit_jit_MODE_IND_16(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_MODE_IND_SCRATCH_16:
    asm_emit_jit_MODE_IND_SCRATCH_16(p_dest_buf);
    break;
  case k_opcode_PULL_16:
    asm_emit_jit_PULL_16(p_dest_buf);
    break;
  case k_opcode_PUSH_16:
    asm_emit_jit_PUSH_16(p_dest_buf, (uint16_t) value1);
    break;
  case k_opcode_ROL_ACC_n:
    asm_emit_jit_ROL_ACC_n(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_ROR_ACC_n:
    asm_emit_jit_ROR_ACC_n(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_SAVE_CARRY:
    asm_emit_jit_SAVE_CARRY(p_dest_buf);
    break;
  case k_opcode_SAVE_CARRY_INV:
    asm_emit_jit_SAVE_CARRY_INV(p_dest_buf);
    break;
  case k_opcode_SAVE_OVERFLOW:
    asm_emit_jit_SAVE_OVERFLOW(p_dest_buf);
    break;
  case k_opcode_SET_CARRY:
    asm_emit_jit_SET_CARRY(p_dest_buf);
    break;
  case k_opcode_STOA_IMM:
    asm_emit_jit_STOA_IMM(p_dest_buf, (uint16_t) value1, (uint8_t) value2);
    break;
  case k_opcode_SUB_ABS:
    asm_emit_jit_SUB_ABS(p_dest_buf, (uint16_t) value1, value2);
    break;
  case k_opcode_SUB_IMM:
    asm_emit_jit_SUB_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_WRITE_INV_ABS:
    asm_emit_jit_WRITE_INV_ABS(p_dest_buf, (uint32_t) value1);
    break;
  case k_opcode_WRITE_INV_SCRATCH:
    asm_emit_jit_WRITE_INV_SCRATCH(p_dest_buf);
    break;
  case k_opcode_WRITE_INV_SCRATCH_n:
    asm_emit_jit_WRITE_INV_SCRATCH_n(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_WRITE_INV_SCRATCH_Y:
    asm_emit_jit_WRITE_INV_SCRATCH_Y(p_dest_buf);
    break;
  case k_opcode_value_load: ASM(value_load); break;
  case k_opcode_value_store: ASM(value_store); break;
  case k_opcode_ASL_acc: ASM(ASL_acc); break;
  case k_opcode_ASL_value: ASM(ASL_value); break;
  case k_opcode_BCC: ASM_Bxx(BCC); break;
  case k_opcode_BCS: ASM_Bxx(BCS); break;
  case k_opcode_BEQ: ASM_Bxx(BEQ); break;
  case k_opcode_BIT: asm_emit_instruction_BIT_value(p_dest_buf); break;
  case k_opcode_BMI: ASM_Bxx(BMI); break;
  case k_opcode_BNE: ASM_Bxx(BNE); break;
  case k_opcode_BPL: ASM_Bxx(BPL); break;
  case k_opcode_BVC: ASM_Bxx(BVC); break;
  case k_opcode_BVS: ASM_Bxx(BVS); break;
  case k_opcode_CLC: asm_emit_instruction_CLC(p_dest_buf); break;
  case k_opcode_CLD: asm_emit_instruction_CLD(p_dest_buf); break;
  case k_opcode_CLI: asm_emit_instruction_CLI(p_dest_buf); break;
  case k_opcode_CLV: asm_emit_instruction_CLV(p_dest_buf); break;
  case k_opcode_DEC_value: ASM(DEC_value); break;
  case k_opcode_DEX: asm_emit_instruction_DEX(p_dest_buf); break;
  case k_opcode_DEY: asm_emit_instruction_DEY(p_dest_buf); break;
  case k_opcode_INC_value: ASM(INC_value); break;
  case k_opcode_INX: asm_emit_instruction_INX(p_dest_buf); break;
  case k_opcode_INY: asm_emit_instruction_INY(p_dest_buf); break;
  case k_opcode_JMP: ASM_Bxx(JMP); break;
  case k_opcode_LSR_acc: ASM(LSR_acc); break;
  case k_opcode_LSR_value: ASM(LSR_value); break;
  case k_opcode_NOP:
    /* We don't really have to emit anything for a NOP, but for now and for
     * good readability, we'll emit a host NOP.
     * We actually emit two host NOPs to cover the size of the invalidation
     * sequence.
     */
    asm_emit_instruction_REAL_NOP(p_dest_buf);
    asm_emit_instruction_REAL_NOP(p_dest_buf);
    break;
  case k_opcode_PHA: asm_emit_instruction_PHA(p_dest_buf); break;
  case k_opcode_PHP: asm_emit_instruction_PHP(p_dest_buf); break;
  case k_opcode_PLA: asm_emit_instruction_PLA(p_dest_buf); break;
  case k_opcode_PLP: asm_emit_instruction_PLP(p_dest_buf); break;
  case k_opcode_ROL_acc: ASM(ROL_acc); break;
  case k_opcode_ROL_value: ASM(ROL_value); break;
  case k_opcode_ROR_acc: ASM(ROR_acc); break;
  case k_opcode_ROR_value: ASM(ROR_value); break;
  case k_opcode_SEC: asm_emit_instruction_SEC(p_dest_buf); break;
  case k_opcode_SED: asm_emit_instruction_SED(p_dest_buf); break;
  case k_opcode_SEI: asm_emit_instruction_SEI(p_dest_buf); break;
  case k_opcode_TAX: asm_emit_instruction_TAX(p_dest_buf); break;
  case k_opcode_TAY: asm_emit_instruction_TAY(p_dest_buf); break;
  case k_opcode_TSX: asm_emit_instruction_TSX(p_dest_buf); break;
  case k_opcode_TXA: asm_emit_instruction_TXA(p_dest_buf); break;
  case k_opcode_TXS: asm_emit_instruction_TXS(p_dest_buf); break;
  case k_opcode_TYA: asm_emit_instruction_TYA(p_dest_buf); break;
  case k_opcode_x64_load_abs: ASM_ADDR_U32(load_abs); break;
  case k_opcode_x64_load_zpg: ASM_ADDR_U8(load_zpg); break;
  case k_opcode_x64_mode_abx_and_load:
    ASM_ADDR_U32_RAW(mode_abx_and_load);
    break;
  case k_opcode_x64_mode_abx_store: ASM_ADDR_U32_RAW(mode_abx_store); break;
  case k_opcode_x64_mode_idy_load:
    asm_emit_jit_MODE_IND_8(p_dest_buf, value1);
    break;
  case k_opcode_x64_mode_zpx: asm_emit_jit_MODE_ZPX(p_dest_buf, value1); break;
  case k_opcode_x64_mode_zpy: asm_emit_jit_MODE_ZPY(p_dest_buf, value1); break;
  case k_opcode_x64_store_abs: ASM_ADDR_U32(store_abs); break;
  case k_opcode_x64_ADC_ABS: ASM_ADDR_U32(ADC_ABS); break;
  case k_opcode_x64_ADC_ABX: ASM_ADDR_U32_RAW(ADC_ABX); break;
  case k_opcode_x64_ADC_ABY: ASM_ADDR_U32_RAW(ADC_ABY); break;
  case k_opcode_x64_ADC_addr: ASM(ADC_addr); break;
  case k_opcode_x64_ADC_addr_Y: ASM(ADC_addr_Y); break;
  case k_opcode_x64_ADC_IMM: ASM_U8(ADC_IMM); break;
  case k_opcode_x64_ADC_ZPG: ASM_ADDR_U8(ADC_ZPG); break;
  case k_opcode_x64_ALR_IMM:
    asm_emit_jit_ALR_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_x64_AND_ABS: ASM_ADDR_U32(AND_ABS); break;
  case k_opcode_x64_AND_ABX: ASM_ADDR_U32_RAW(AND_ABX); break;
  case k_opcode_x64_AND_ABY: ASM_ADDR_U32_RAW(AND_ABY); break;
  case k_opcode_x64_AND_addr: ASM(AND_addr); break;
  case k_opcode_x64_AND_addr_Y: ASM(AND_addr_Y); break;
  case k_opcode_x64_AND_IMM: ASM_U8(AND_IMM); break;
  case k_opcode_x64_AND_ZPG: ASM_ADDR_U8(AND_ZPG); break;
  case k_opcode_x64_ASL_ABS: ASM_ADDR_U32(ASL_ABS); break;
  case k_opcode_x64_ASL_ZPG: ASM_ADDR_U8(ASL_ZPG); break;
  case k_opcode_x64_CMP_ABS: ASM_ADDR_U32(CMP_ABS); break;
  case k_opcode_x64_CMP_ABX: ASM_ADDR_U32_RAW(CMP_ABX); break;
  case k_opcode_x64_CMP_ABY: ASM_ADDR_U32_RAW(CMP_ABY); break;
  case k_opcode_x64_CMP_addr: ASM(CMP_addr); break;
  case k_opcode_x64_CMP_addr_Y: ASM(CMP_addr_Y); break;
  case k_opcode_x64_CMP_IMM: ASM_U8(CMP_IMM); break;
  case k_opcode_x64_CMP_ZPG: ASM_ADDR_U8(CMP_ZPG); break;
  case k_opcode_x64_CPX_ABS: ASM_ADDR_U32(CPX_ABS); break;
  case k_opcode_x64_CPX_IMM: ASM_U8(CPX_IMM); break;
  case k_opcode_x64_CPX_ZPG: ASM_ADDR_U8(CPX_ZPG); break;
  case k_opcode_x64_CPY_ABS: ASM_ADDR_U32(CPY_ABS); break;
  case k_opcode_x64_CPY_IMM: ASM_U8(CPY_IMM); break;
  case k_opcode_x64_CPY_ZPG: ASM_ADDR_U8(CPY_ZPG); break;
  case k_opcode_x64_DEC_ABS: ASM_ADDR_U32(DEC_ABS); break;
  case k_opcode_x64_DEC_ZPG: ASM_ADDR_U8(DEC_ZPG); break;
  case k_opcode_x64_EOR_ABS: ASM_ADDR_U32(EOR_ABS); break;
  case k_opcode_x64_EOR_ABX: ASM_ADDR_U32_RAW(EOR_ABX); break;
  case k_opcode_x64_EOR_ABY: ASM_ADDR_U32_RAW(EOR_ABY); break;
  case k_opcode_x64_EOR_addr: ASM(EOR_addr); break;
  case k_opcode_x64_EOR_addr_Y: ASM(EOR_addr_Y); break;
  case k_opcode_x64_EOR_IMM: ASM_U8(EOR_IMM); break;
  case k_opcode_x64_EOR_ZPG: ASM_ADDR_U8(EOR_ZPG); break;
  case k_opcode_x64_INC_ABS: ASM_ADDR_U32(INC_ABS); break;
  case k_opcode_x64_INC_ZPG: ASM_ADDR_U8(INC_ZPG); break;
  case k_opcode_x64_LDA_addr: ASM(LDA_addr); break;
  case k_opcode_x64_LDA_addr_Y: ASM(LDA_addr_Y); break;
  case k_opcode_x64_LDA_ABS: ASM_ADDR_U32(LDA_ABS); break;
  case k_opcode_x64_LDA_ABX: ASM_ADDR_U32_RAW(LDA_ABX); break;
  case k_opcode_x64_LDA_ABY: ASM_ADDR_U32_RAW(LDA_ABY); break;
  case k_opcode_x64_LDA_IMM: ASM_U32(LDA_IMM); break;
  case k_opcode_x64_LDA_ZPG: ASM_ADDR_U8(LDA_ZPG); break;
  case k_opcode_x64_LDX_addr: ASM(LDX_addr); break;
  case k_opcode_x64_LDX_ABS: ASM_ADDR_U32(LDX_ABS); break;
  case k_opcode_x64_LDX_ABY: ASM_ADDR_U32_RAW(LDX_ABY); break;
  case k_opcode_x64_LDX_IMM: ASM_U8(LDX_IMM); break;
  case k_opcode_x64_LDX_ZPG: ASM_ADDR_U8(LDX_ZPG); break;
  case k_opcode_x64_LDY_addr: ASM(LDY_addr); break;
  case k_opcode_x64_LDY_ABS: ASM_ADDR_U32(LDY_ABS); break;
  case k_opcode_x64_LDY_ABX: ASM_ADDR_U32_RAW(LDY_ABX); break;
  case k_opcode_x64_LDY_IMM: ASM_U8(LDY_IMM); break;
  case k_opcode_x64_LDY_ZPG: ASM_ADDR_U8(LDY_ZPG); break;
  case k_opcode_x64_LSR_ABS: ASM_ADDR_U32(LSR_ABS); break;
  case k_opcode_x64_LSR_ZPG: ASM_ADDR_U8(LSR_ZPG); break;
  case k_opcode_x64_ORA_ABS: ASM_ADDR_U32(ORA_ABS); break;
  case k_opcode_x64_ORA_ABX: ASM_ADDR_U32_RAW(ORA_ABX); break;
  case k_opcode_x64_ORA_ABY: ASM_ADDR_U32_RAW(ORA_ABY); break;
  case k_opcode_x64_ORA_addr: ASM(ORA_addr); break;
  case k_opcode_x64_ORA_addr_Y: ASM(ORA_addr_Y); break;
  case k_opcode_x64_ORA_IMM: ASM_U8(ORA_IMM); break;
  case k_opcode_x64_ORA_ZPG: ASM_ADDR_U8(ORA_ZPG); break;
  case k_opcode_x64_SAX_ABS: ASM_ADDR_U32(SAX_ABS); break;
  case k_opcode_x64_SBC_ABS: ASM_ADDR_U32(SBC_ABS); break;
  case k_opcode_x64_SBC_ABX: ASM_ADDR_U32_RAW(SBC_ABX); break;
  case k_opcode_x64_SBC_ABY: ASM_ADDR_U32_RAW(SBC_ABY); break;
  case k_opcode_x64_SBC_addr: ASM(SBC_addr); break;
  case k_opcode_x64_SBC_addr_Y: ASM(SBC_addr_Y); break;
  case k_opcode_x64_SBC_IMM: ASM_U8(SBC_IMM); break;
  case k_opcode_x64_SBC_ZPG: ASM_ADDR_U8(SBC_ZPG); break;
  case k_opcode_x64_SLO_ABS: asm_emit_jit_SLO_ABS(p_dest_buf, value1); break;
  case k_opcode_x64_STA_addr: ASM(STA_addr); break;
  case k_opcode_x64_STA_addr_Y: ASM(STA_addr_Y); break;
  case k_opcode_x64_STA_ABS: ASM_ADDR_U32(STA_ABS); break;
  case k_opcode_x64_STA_ABX: ASM_ADDR_U32_RAW(STA_ABX); break;
  case k_opcode_x64_STA_ABY: ASM_ADDR_U32_RAW(STA_ABY); break;
  case k_opcode_x64_STA_ZPG: ASM_ADDR_U8(STA_ZPG); break;
  case k_opcode_x64_STX_addr: ASM(STX_addr); break;
  case k_opcode_x64_STX_ABS: ASM_ADDR_U32(STX_ABS); break;
  case k_opcode_x64_STX_ZPG: ASM_ADDR_U8(STX_ZPG); break;
  case k_opcode_x64_STY_addr: ASM(STY_addr); break;
  case k_opcode_x64_STY_ABS: ASM_ADDR_U32(STY_ABS); break;
  case k_opcode_x64_STY_ZPG: ASM_ADDR_U8(STY_ZPG); break;
  default:
    assert(0);
    break;
  }
}
