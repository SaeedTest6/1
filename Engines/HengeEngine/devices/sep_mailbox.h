#ifndef HENGEVM_SEP_MAILBOX_H
#define HENGEVM_SEP_MAILBOX_H

#include "machine.h"
#include "sep_mailbox_core.h"

/* Puts one side's view of the mailbox into a machine's physical address space
   (0x30250000 on both the AP's and the SEP core's, see sep_mailbox_core.h).
   The returned view is owned by the caller (free it after the machine stops),
   or NULL if the region could not be added. */
typedef struct HvSepMailboxView {
    HvSepMailbox *mb;
    int side;
} HvSepMailboxView;

HvSepMailboxView *hv_sepmbx_attach(HvMachine *m, uint64_t pa, HvSepMailbox *mb, int side, const char *name);

#endif
