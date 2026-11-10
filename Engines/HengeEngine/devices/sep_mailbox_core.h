#ifndef HENGEVM_SEP_MAILBOX_CORE_H
#define HENGEVM_SEP_MAILBOX_CORE_H

#include <stdint.h>

/* The mailbox between the AP and the SEP core on the vresearch1 board
   (the board Virtualization.framework gives vphone-cli's guests).

   One device, two views. Each side sees the same register layout at its own
   physical address 0x30250000 (the AP's address space and the SEP core's are
   separate); what one side sends, the other receives.

   Sources, for the registers and the message layout:
     OpenSEP docs/external-contract.md ("Mailbox", the SEP's view) and
     OpenAVB src/protocol/sep/sep.c (the AP's view).
   Only bits those two name are modelled. Anything else reads as zero and is
   ignored on write. This has not been compared with Apple's own model, so it
   is a reading of the documents, not a verified copy of the hardware.

   This file has no machine dependency, so it can be tested anywhere. The
   glue that puts a view into a machine's address space is sep_mailbox.c. */

#define HV_SEPMBX_DEPTH 8 /* entries a side can have waiting (OpenAVB: "FULL eight-entry receive queue") */

enum { HV_SEPMBX_AP = 0, HV_SEPMBX_SEP = 1 };

/* Register offsets, the same on both sides. */
#define HV_SEPMBX_CONTROL   0x10  /* bit 0 set during activation */
#define HV_SEPMBX_STATUS    0x14  /* bit 31 active, bit 4 RX event (write 1 to clear) */
#define HV_SEPMBX_IRQ_EN    0x18  /* bit 4 RX not empty, bit 5 TX empty */
#define HV_SEPMBX_RX_STATUS 0x1c  /* bit 17 RX empty */
#define HV_SEPMBX_TX_STATUS 0x20  /* bit 17 TX empty, bit 16 TX full */
#define HV_SEPMBX_RX_SLOT   0x100 /* 16 bytes, read with one ldp; reading the low half pops */
#define HV_SEPMBX_TX_SLOT   0x200 /* 16 bytes, written with one stp; writing the high half sends */

#define HV_SEPMBX_STATUS_ACTIVE   (1u << 31)
#define HV_SEPMBX_STATUS_RX_EVENT (1u << 4)
#define HV_SEPMBX_FIFO_EMPTY      (1u << 17)
#define HV_SEPMBX_TX_FULL         (1u << 16)
#define HV_SEPMBX_IRQ_RX_NOT_EMPTY (1u << 4)
#define HV_SEPMBX_IRQ_TX_EMPTY     (1u << 5)

typedef struct HvSepMsg {
    uint64_t lo, hi; /* in practice hi is zero: the message is the low word */
} HvSepMsg;

typedef struct HvSepFifo {
    HvSepMsg m[HV_SEPMBX_DEPTH];
    unsigned head, count;
} HvSepFifo;

typedef struct HvSepSide {
    uint32_t control, status, irq_enable;
    uint64_t rx_hi;    /* the high half of the entry the last RX slot low read popped */
    uint64_t tx_lo;    /* the low half the last TX slot low write latched */
} HvSepSide;

typedef struct HvSepMailbox {
    HvSepFifo to[2];   /* to[side]: messages waiting for that side to receive */
    HvSepSide side[2];
    /* A message arrived for `side`: wake it (an event for wfe, or its interrupt line). May be NULL. */
    void (*notify)(void *opaque, int side);
    void *notify_opaque;
} HvSepMailbox;

void hv_sepmbx_init(HvSepMailbox *mb, void (*notify)(void *, int), void *notify_opaque);

/* A register access from `side`. size is 4 or 8; a 16-byte pair access reaches
   these as two 8-byte ones, low half first (core/machine.c splits it). */
uint64_t hv_sepmbx_read(HvSepMailbox *mb, int side, uint64_t off, unsigned size);
void hv_sepmbx_write(HvSepMailbox *mb, int side, uint64_t off, uint64_t val, unsigned size);

/* Host side: queue a message for `side`. Returns 0, or -1 when its queue is full. */
int hv_sepmbx_inject(HvSepMailbox *mb, int side, uint64_t lo, uint64_t hi);

/* Whether `side`'s interrupt line should be high, from its enable bits and the queues. */
int hv_sepmbx_irq_pending(const HvSepMailbox *mb, int side);

#endif
