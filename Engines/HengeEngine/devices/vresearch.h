#ifndef HENGEVM_VRESEARCH_H
#define HENGEVM_VRESEARCH_H

/* The AP's physical memory map on the vresearch1 board, the board Virtualization.framework
   gives vphone-cli's iPhone, iPad, Apple Watch and Apple TV guests.

   Source: OpenAVB docs/external-contract.md ("Memory map and devices"), the host contract of the
   open boot ROM vphone boots with. Every constant below is from that table unless it says otherwise.

   Most of it is the vmapple map devices/vmapple.h already has, which is why the board is a variant of
   that one and not a new machine:

     address        vresearch1 (this file)             vmapple (devices/vmapple.h)
     0x00100000     ROM image                          VMAPPLE_FIRMWARE
     0x00400000     platform register block            VMAPPLE_CONFIG (devices/cfg.c: same page, same
                                                register offsets; see vresearch_platform.h)
     0x20010000     early console data register        VMAPPLE_UART (PL011)
     0x20070000     pvpanic                            VMAPPLE_PVPANIC
     0x30000000     bdif channel 0: root block device  VMAPPLE_BDOOR (devices/bdif.c)
     0x30010000     bdif channel 1: utility block      (same bdif)
     0x30100000     bdif channel 0x100000: USB device  (same bdif, devices/virtio_usb.c)
     0x70000000     work RAM                           VMAPPLE_MEM

   What vresearch1 has that vmapple does not: the SEP mailbox (devices/sep_mailbox_core.h), the boot
   ROM's patch store and upload buffer in work RAM, and a SEP core with its own address space. */

#define VRESEARCH_ROM          0x00100000ull /* ..0x00180000 */
#define VRESEARCH_PLATFORM     0x00400000ull /* board ID +0x10, chip rev +0x14, ECID +0x18/+0x1c, domain +0x2c,
                                                boot config +0x30, options +0x34, entropy +0x38, progress +0x64,
                                                stage marker +0x3ac, variant string +0x5c8, chip ID +0x6f0 */
#define VRESEARCH_BOOT_CONTEXT 0x00400300ull /* {status_flags, boot_config, board_id, security_epoch} */
#define VRESEARCH_HANDOFF_SCRATCH 0x00160000ull
#define VRESEARCH_BOOT_ARGS_EXTRA 0x00161000ull /* "OMVBARG", a mode byte, then the string */
#define VRESEARCH_CONSOLE      0x20010000ull
#define VRESEARCH_PVPANIC      0x20070000ull
#define VRESEARCH_BDIF         0x30000000ull
#define VRESEARCH_VIRTIO_UTIL  0x30010000ull
#define VRESEARCH_VIRTIO_USB   0x30100000ull
#define VRESEARCH_SEP_MAILBOX  0x30250000ull
#define VRESEARCH_RAM          0x70000000ull
#define VRESEARCH_UPLOAD       0x71000000ull /* personalized IMG4 upload buffer (DFU) */
#define VRESEARCH_PATCH_STORE  0x71200000ull /* one 0x40000 slot per stage: iBSS/LLB, iBEC, SPTM, kernel */

/* Not zero under Virtualization.framework: it leaves the chip ID register at +0x6f0 reading 0, and
   OpenAVB then falls back to this value, "as Apple's ROM reports" (vphone openavb_boot_rom.md, change 2). */
#define VRESEARCH_CHIP_ID_FALLBACK 0xfe01

/* The SEP core's own address space (OpenSEP docs/external-contract.md). */
#define VRESEARCH_SEP_ROM      0x02000000ull /* OpenSEP.bin, flat, entered at EL1 */
#define VRESEARCH_SEP_RAM      0x04020000ull /* ..0x04050000, zero at cold boot */
#define VRESEARCH_SEP_RUNTIME  0x06000000ull /* the SEPOS runtime; entered here with the handoff pointer in x0 */
#define VRESEARCH_SEP_MAILBOX_ON_SEP 0x30250000ull /* the SEP maps it at VA 0x05000000 */

#endif
