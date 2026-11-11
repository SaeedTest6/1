// Guest programs for the vresearch board test (JIT only, EL1, MMU off, guest PA = host address).
#include "harness.h"

    .arch armv8.4-a
    .text
    .p2align 14
    .globl _vresearch_programs_begin
    .globl _vresearch_programs_finish
_vresearch_programs_begin:

// What OpenAVB does with the SEP mailbox: ldp pops a message, stp sends one.
// In: x0 = mailbox, x7/x8 = message to send.
PROGRAM p_sep_pair
    ldr w6, [x0, #0x1c]             // RX_STATUS before the pop
    ldp x4, x5, [x0, #0x100]        // RX slot: one 16-byte access
    ldr w10, [x0, #0x1c]            // RX_STATUS after
    stp x7, x8, [x0, #0x200]        // TX slot: one 16-byte access
    ldr w9, [x0, #0x20]             // TX_STATUS
END p_sep_pair

// The identity registers OpenAVB reads. In: x1 = platform block.
PROGRAM p_platform
    ldr w10, [x1, #0x10]            // board ID
    ldr w11, [x1, #0x18]            // ECID low
    ldr w12, [x1, #0x1c]            // ECID high
    ldr w13, [x1, #0x2c]            // security domain
    ldr w14, [x1, #0x30]            // boot configuration
END p_platform

    .p2align 14
_vresearch_programs_finish:
