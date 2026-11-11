#ifndef HENGEVM_SEP_CORE_H
#define HENGEVM_SEP_CORE_H

#include "machine.h"
#include "sep_mailbox.h"
#include "vresearch.h"

/* The SEP core of the vresearch1 board: the address space OpenSEP's boot ROM runs in, on its own HvMachine.

   From OpenSEP docs/external-contract.md ("Image and memory", "Mailbox"):
     0x00000000..0x02000000  device block, 32 MiB. The ROM maps it at SEP VA 0x30000000 and only touches a
                             platform page (+0x400000) and a configuration sink (+0x600000); both may read as zero.
     0x02000000              OpenSEP.bin, flat; the ROM maps 0x50000 bytes of it executable.
     0x04020000..0x04050000  SEP RAM, zero at cold boot.
     0x06000000..            runtime region: the SEP firmware is written here and entered at 0x06000000 with the
                             handoff pointer in x0. Its size is chosen by the AP (opcode 0x11, in KiB), so the
                             caller passes it in.
     0x30250000              the mailbox (devices/sep_mailbox_core.h), the SEP side.
     0x70000000..            the AP's RAM. The SEP reads the firmware the AP publishes (opcode 0x06) from an
                             address the AP gives it, which "the vresearch1 build uses as it is": the same
                             host memory as the AP machine's RAM, mapped into this machine too.

   Scope: this is the boot ROM only. The firmware that runs after it (Apple's SEPOS, sep-firmware.vphone600) needs
   whatever hardware Virtualization.framework's closed SEP process gives it, SEPStorage included, and no source
   describes those devices. Nothing here models them. See MERGE.md. */

#define SEPCORE_DEVICE_BLOCK_SIZE 0x02000000ull
#define SEPCORE_ROM_WINDOW        0x00050000ull
#define SEPCORE_RAM_SIZE          0x00030000ull

typedef struct HvSepCore {
    HvMachine *m;
    HvCpu *cpu;
    uint8_t *rom, *ram, *runtime;
    uint64_t runtime_size;
    HvSepMailboxView *view;
    HvMachine *ap;        /* woken when the SEP sends; set by hv_sepcore_connect */
} HvSepCore;

/* ap_ram/ap_ram_size: the AP machine's RAM, shared (not copied). runtime_bytes: backing for the runtime region
   at 0x06000000, a multiple of 4 KiB. Returns 0, or -1 with nothing left allocated. */
int hv_sepcore_create(HvSepCore *s, HvMachine *m, HvCpu *cpu, HvSepMailbox *mb, uint8_t *ap_ram, uint64_t ap_ram_size,
                      uint64_t runtime_bytes);
/* The ROM must fit the window the ROM maps executable. */
int hv_sepcore_load_rom(HvSepCore *s, const char *path);
/* EL1, MMU off, at the start of the ROM. */
void hv_sepcore_reset_cpu(HvSepCore *s);
/* Mailbox changes wake the machine on the side that has something to read (the ROM waits with wfe).
   Replaces the mailbox's notify callback; call it once, before either machine runs. */
void hv_sepcore_connect(HvSepCore *s, HvSepMailbox *mb, HvMachine *ap);
/* Nothing may be running s. Frees what create allocated, not the machine. */
void hv_sepcore_destroy(HvSepCore *s);

#endif
