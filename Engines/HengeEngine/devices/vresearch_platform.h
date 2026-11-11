#ifndef HENGEVM_VRESEARCH_PLATFORM_H
#define HENGEVM_VRESEARCH_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

/* The platform register block of the vresearch1 board, as OpenAVB reads it (src/platform/platform.c).
   Pure C, no machine dependency, so it can be tested anywhere.

   On the real board Virtualization.framework fills this block. Henge's cfg.c already fills the same page
   for vmapple, and the two layouts agree on every register the ROM reads except the values, which is why
   this only has to overwrite values and not move anything:

     +0x10 board ID          cfg.c writes 0x20 (vmapple); the ROM wants 0x90
     +0x14 chip revision     0 in both
     +0x18 ECID (64 bits)    same
     +0x2c security domain   1 in both
     +0x30 boot configuration 1 in both; the ROM only boots a platform source for 1 or 2
     +0x34 boot options      bit 0 forces DFU; cfg.c's run_installer2 is the same register
     +0x38 entropy           a live random-number register in cfg.c; left alone here
     +0x5c8 variant string   not used by cfg.c; empty makes the ROM keep its own DFU product string
     +0x6f0 chip ID          0 makes the ROM report 0xfe01 itself, as Virtualization.framework leaves it */

#define VRESEARCH_PLAT_BOARD_ID      0x10
#define VRESEARCH_PLAT_CHIP_REV      0x14
#define VRESEARCH_PLAT_ECID_LO       0x18
#define VRESEARCH_PLAT_ECID_HI       0x1c
#define VRESEARCH_PLAT_SECURITY_DOM  0x2c
#define VRESEARCH_PLAT_BOOT_CONFIG   0x30
#define VRESEARCH_PLAT_BOOT_OPTIONS  0x34
#define VRESEARCH_PLAT_ENTROPY       0x38
#define VRESEARCH_PLAT_VARIANT       0x5c8
#define VRESEARCH_PLAT_VARIANT_SIZE  64
#define VRESEARCH_PLAT_CHIP_ID       0x6f0

#define VRESEARCH_BOARD_ID_VPHONE    0x90 /* vresearch101ap, from the DFU identity vphone documents (BDID 0x90) */
#define VRESEARCH_BOOT_CONFIG_DEFAULT 1
#define VRESEARCH_BOOT_OPTION_FORCE_DFU 1u

typedef struct HvVresearchIdentity {
    uint8_t board_id;
    uint8_t chip_rev;
    uint64_t ecid;
    uint8_t security_domain;
    uint32_t boot_config;
    int force_dfu;           /* boot options bit 0 */
    uint32_t chip_id;        /* 0: leave the register at 0 and let the ROM name the chip (0xfe01) */
    const char *variant;     /* NULL or "": empty */
} HvVresearchIdentity;

/* vphone's identity (BDID 0x90, CPRV 0, SDOM 1) for the given ECID. */
HvVresearchIdentity hv_vresearch_identity(uint64_t ecid, int force_dfu);

/* Writes the registers above into block (the first `size` bytes of the region at 0x400000, at least 0x700).
   Touches nothing else, so the entropy register and anything the ROM wrote stay as they were.
   Returns 0, or -1 if size is too small or the variant string does not fit. */
int hv_vresearch_fill_platform(uint8_t *block, size_t size, const HvVresearchIdentity *id);

#endif
