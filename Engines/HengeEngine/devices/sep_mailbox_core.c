#include "sep_mailbox_core.h"

#include <string.h>

void hv_sepmbx_init(HvSepMailbox *mb, void (*notify)(void *, int), void *notify_opaque) {
    memset(mb, 0, sizeof *mb);
    mb->notify = notify;
    mb->notify_opaque = notify_opaque;
}

static int fifo_push(HvSepFifo *f, uint64_t lo, uint64_t hi) {
    if (f->count == HV_SEPMBX_DEPTH) return -1;
    HvSepMsg *e = &f->m[(f->head + f->count) % HV_SEPMBX_DEPTH];
    e->lo = lo;
    e->hi = hi;
    f->count++;
    return 0;
}

static int fifo_pop(HvSepFifo *f, HvSepMsg *out) {
    if (!f->count) return -1;
    *out = f->m[f->head];
    f->head = (f->head + 1) % HV_SEPMBX_DEPTH;
    f->count--;
    return 0;
}

static int deliver(HvSepMailbox *mb, int to_side, uint64_t lo, uint64_t hi) {
    if (fifo_push(&mb->to[to_side], lo, hi)) return -1;
    mb->side[to_side].status |= HV_SEPMBX_STATUS_RX_EVENT;
    if (mb->notify) mb->notify(mb->notify_opaque, to_side);
    return 0;
}

int hv_sepmbx_inject(HvSepMailbox *mb, int side, uint64_t lo, uint64_t hi) {
    return deliver(mb, side, lo, hi);
}

int hv_sepmbx_irq_pending(const HvSepMailbox *mb, int side) {
    const HvSepSide *s = &mb->side[side];
    int rx_not_empty = mb->to[side].count != 0;
    int tx_empty = mb->to[1 - side].count == 0;
    return ((s->irq_enable & HV_SEPMBX_IRQ_RX_NOT_EMPTY) && rx_not_empty) ||
           ((s->irq_enable & HV_SEPMBX_IRQ_TX_EMPTY) && tx_empty);
}

uint64_t hv_sepmbx_read(HvSepMailbox *mb, int side, uint64_t off, unsigned size) {
    HvSepSide *s = &mb->side[side];
    (void)size;
    switch (off) {
    case HV_SEPMBX_CONTROL: return s->control;
    case HV_SEPMBX_STATUS: return s->status;
    case HV_SEPMBX_IRQ_EN: return s->irq_enable;
    case HV_SEPMBX_RX_STATUS: return mb->to[side].count ? 0 : HV_SEPMBX_FIFO_EMPTY;
    case HV_SEPMBX_TX_STATUS: {
        unsigned n = mb->to[1 - side].count;
        return (n == 0 ? HV_SEPMBX_FIFO_EMPTY : 0) | (n == HV_SEPMBX_DEPTH ? HV_SEPMBX_TX_FULL : 0);
    }
    case HV_SEPMBX_RX_SLOT: { /* the low half of an ldp: pop, keep the high half for the next read */
        HvSepMsg e;
        if (fifo_pop(&mb->to[side], &e)) {
            s->rx_hi = 0;
            return 0;
        }
        s->rx_hi = e.hi;
        return e.lo;
    }
    case HV_SEPMBX_RX_SLOT + 8: return s->rx_hi;
    default: return 0;
    }
}

void hv_sepmbx_write(HvSepMailbox *mb, int side, uint64_t off, uint64_t val, unsigned size) {
    HvSepSide *s = &mb->side[side];
    (void)size;
    switch (off) {
    case HV_SEPMBX_CONTROL:
        s->control = (uint32_t)val;
        break;
    case HV_SEPMBX_STATUS:
        /* "The ownership register acknowledges writes": setting bit 31 reads back as set. Bit 4 clears on 1. */
        if (val & HV_SEPMBX_STATUS_ACTIVE) s->status |= HV_SEPMBX_STATUS_ACTIVE;
        if (val & HV_SEPMBX_STATUS_RX_EVENT) s->status &= ~HV_SEPMBX_STATUS_RX_EVENT;
        break;
    case HV_SEPMBX_IRQ_EN:
        s->irq_enable = (uint32_t)val;
        break;
    case HV_SEPMBX_TX_SLOT: /* the low half of an stp: latch it */
        s->tx_lo = val;
        break;
    case HV_SEPMBX_TX_SLOT + 8: /* the high half completes the message; a full queue drops it */
        deliver(mb, 1 - side, s->tx_lo, val);
        break;
    default:
        break;
    }
}
