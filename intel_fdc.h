#ifndef BEEBJIT_INTEL_FDC_H
#define BEEBJIT_INTEL_FDC_H

#include <stdint.h>

struct intel_fdc_struct;

struct disc_struct;
struct state_6502;

struct intel_fdc_struct* intel_fdc_create(struct state_6502* p_state_6502);
void intel_fdc_destroy(struct intel_fdc_struct* p_fdc);

/* Setup. */
void intel_fdc_set_drives(struct intel_fdc_struct* p_fdc,
                          struct disc_struct* p_disc_0,
                          struct disc_struct* p_disc_1);

/* Host hardware register I/O. */
uint8_t intel_fdc_read(struct intel_fdc_struct* p_fdc, uint16_t addr);
void intel_fdc_write(struct intel_fdc_struct* p_fdc,
                     uint16_t addr,
                     uint8_t val);

void intel_fdc_byte_callback(void* p,
                             uint8_t data_byte,
                             uint8_t clocks_byte,
                             int is_index);
void intel_fdc_timer_tick(struct intel_fdc_struct* p_fdc);

#endif /* BEEBJIT_INTEL_FDC_H */
