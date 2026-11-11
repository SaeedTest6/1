#ifndef HENGEVM_VRESEARCH_BOARD_H
#define HENGEVM_VRESEARCH_BOARD_H

#include "sep_mailbox.h"
#include "vmapple.h"
#include "vresearch.h"
#include "vresearch_platform.h"

/* The vresearch1 board as a layer on the vmapple one.

   hv_vmapple_create builds everything the two boards share (ROM region at 0x100000, the 1 MB platform
   window at 0x400000, PL011 at 0x20010000, pvpanic at 0x20070000, bdif at 0x30000000 with its USB queue at
   +0x100000, RAM at 0x70000000, plus the PCIe devices the guest may use later). This adds what is only on
   vresearch1: the platform identity values the open boot ROM reads, and the AP side of the SEP mailbox.

   It does not run the SEP. Until a second machine answers on the other side of `mailbox`, OpenAVB cannot
   pass sep_bootstrap() (src/protocol/sep/sep.c): the BOOTSTRAP request times out and the ROM calls
   pvpanic_trigger(). So a board built here panics at that point by design; see MERGE.md. */
typedef struct HvVresearch {
    HvVmapple base;
    HvSepMailbox mailbox;          /* the two queues; the SEP machine will attach HV_SEPMBX_SEP */
    HvSepMailboxView *ap_view;     /* the AP's window at VRESEARCH_SEP_MAILBOX */
    HvVresearchIdentity identity;
} HvVresearch;

/* `id` is copied. cfg is as for hv_vmapple_create; its ECID and force-DFU fields are replaced from id.
   Returns 0, or -1 on failure (the board is then unusable, but hv_vresearch_destroy is still safe). */
int hv_vresearch_create(HvVresearch *v, HvMachine *m, HvCpu *cpus, uint64_t ram_size, HvDisk *root, HvDisk *aux,
                        const HvCfgParams *cfg, const HvVresearchIdentity *id, const uint8_t mac[6]);
int hv_vresearch_load_rom(HvVresearch *v, const char *path);
void hv_vresearch_reset_cpu(HvVresearch *v);
/* Same rule as hv_vmapple_destroy: nothing may be using the board. */
void hv_vresearch_destroy(HvVresearch *v);

#endif
