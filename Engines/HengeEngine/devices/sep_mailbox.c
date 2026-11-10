#include "sep_mailbox.h"

#include <stdlib.h>

#define SEPMBX_WINDOW 0x10000

static uint64_t view_read(void *opaque, uint64_t off, unsigned size) {
    HvSepMailboxView *v = opaque;
    return hv_sepmbx_read(v->mb, v->side, off, size);
}

static void view_write(void *opaque, uint64_t off, uint64_t val, unsigned size) {
    HvSepMailboxView *v = opaque;
    hv_sepmbx_write(v->mb, v->side, off, val, size);
}

HvSepMailboxView *hv_sepmbx_attach(HvMachine *m, uint64_t pa, HvSepMailbox *mb, int side, const char *name) {
    HvSepMailboxView *v = calloc(1, sizeof *v);
    if (!v) return NULL;
    v->mb = mb;
    v->side = side;
    if (hv_machine_add_mmio(m, pa, SEPMBX_WINDOW, v, view_read, view_write, name)) {
        free(v);
        return NULL;
    }
    return v;
}
