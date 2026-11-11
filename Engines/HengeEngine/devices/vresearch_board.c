#include "vresearch_board.h"

#include <stdlib.h>
#include <string.h>

/* The same address, so the layering is not a coincidence waiting to break. */
_Static_assert(VRESEARCH_ROM == VMAPPLE_FIRMWARE, "ROM address");
_Static_assert(VRESEARCH_PLATFORM == VMAPPLE_CONFIG, "platform block address");
_Static_assert(VRESEARCH_CONSOLE == VMAPPLE_UART, "console address");
_Static_assert(VRESEARCH_PVPANIC == VMAPPLE_PVPANIC, "pvpanic address");
_Static_assert(VRESEARCH_BDIF == VMAPPLE_BDOOR, "bdif address");
_Static_assert(VRESEARCH_RAM == VMAPPLE_MEM, "work RAM address");

int hv_vresearch_create(HvVresearch *v, HvMachine *m, HvCpu *cpus, uint64_t ram_size, HvDisk *root, HvDisk *aux,
                        const HvCfgParams *cfg, const HvVresearchIdentity *id, const uint8_t mac[6]) {
    memset(v, 0, sizeof *v);
    v->identity = *id;
    HvCfgParams p = *cfg;
    p.ecid = id->ecid;
    /* +0x34 is run_installer2 in cfg.c and boot options in the ROM: one register. A forced DFU boot also
       needs the USB device port, which hv_vmapple_create adds for an installer boot. */
    p.run_installer2 = id->force_dfu ? 1 : 0;
    if (hv_vmapple_create(&v->base, m, cpus, ram_size, root, aux, &p, mac)) return -1;
    if (hv_vresearch_fill_platform(v->base.cfg, 0x10000, &v->identity)) return -1;
    hv_sepmbx_init(&v->mailbox, NULL, NULL);
    v->ap_view = hv_sepmbx_attach(m, VRESEARCH_SEP_MAILBOX, &v->mailbox, HV_SEPMBX_AP, "sep-mailbox-ap");
    return v->ap_view ? 0 : -1;
}

int hv_vresearch_load_rom(HvVresearch *v, const char *path) {
    return hv_vmapple_load_rom(&v->base, path);
}

void hv_vresearch_reset_cpu(HvVresearch *v) {
    hv_vmapple_reset_cpu(&v->base);
}

void hv_vresearch_destroy(HvVresearch *v) {
    hv_vmapple_destroy(&v->base);
    free(v->ap_view);
    memset(v, 0, sizeof *v);
}
