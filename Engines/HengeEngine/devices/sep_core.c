#include "sep_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/* The device block answers every access with zero and ignores writes (the contract says the ROM may be given
   exactly that). */
static uint64_t inert_read(void *opaque, uint64_t off, unsigned size) {
    (void)opaque, (void)off, (void)size;
    return 0;
}

static void inert_write(void *opaque, uint64_t off, uint64_t val, unsigned size) {
    (void)opaque, (void)off, (void)val, (void)size;
}

static void *map_anon(uint64_t size) {
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

int hv_sepcore_create(HvSepCore *s, HvMachine *m, HvCpu *cpu, HvSepMailbox *mb, uint8_t *ap_ram, uint64_t ap_ram_size,
                      uint64_t runtime_bytes) {
    memset(s, 0, sizeof *s);
    if (!ap_ram || !ap_ram_size || !runtime_bytes || (runtime_bytes & 0xfff)) return -1;
    s->m = m;
    s->cpu = cpu;
    s->runtime_size = runtime_bytes;
    s->rom = map_anon(SEPCORE_ROM_WINDOW);
    s->ram = map_anon(SEPCORE_RAM_SIZE);
    s->runtime = map_anon(runtime_bytes);
    if (!s->rom || !s->ram || !s->runtime) goto fail;
    if (hv_machine_add_mmio(m, 0, SEPCORE_DEVICE_BLOCK_SIZE, NULL, inert_read, inert_write, "sep-device-block")) goto fail;
    if (hv_machine_add_ram(m, VRESEARCH_SEP_ROM, SEPCORE_ROM_WINDOW, s->rom, 0)) goto fail;
    if (hv_machine_add_ram(m, VRESEARCH_SEP_RAM, SEPCORE_RAM_SIZE, s->ram, 0)) goto fail;
    if (hv_machine_add_ram(m, VRESEARCH_SEP_RUNTIME, runtime_bytes, s->runtime, 0)) goto fail;
    if (hv_machine_add_ram(m, VRESEARCH_RAM, ap_ram_size, ap_ram, 0)) goto fail;
    s->view = hv_sepmbx_attach(m, VRESEARCH_SEP_MAILBOX_ON_SEP, mb, HV_SEPMBX_SEP, "sep-mailbox-sep");
    if (!s->view) goto fail;
    return 0;
fail:
    hv_sepcore_destroy(s);
    return -1;
}

int hv_sepcore_load_rom(HvSepCore *s, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(s->rom, 1, SEPCORE_ROM_WINDOW, f);
    int more = fgetc(f) != EOF;
    fclose(f);
    return n == 0 || more ? -1 : 0;
}

void hv_sepcore_reset_cpu(HvSepCore *s) {
    hv_cpu_reset_index(s->cpu, s->m, 0);
    s->cpu->mpidr = 0x80000000u;
    s->cpu->pc = VRESEARCH_SEP_ROM;
    s->cpu->off = 0;
}

static void wake_side(void *opaque, int side) {
    HvSepCore *s = opaque;
    HvMachine *target = side == HV_SEPMBX_SEP ? s->m : s->ap;
    if (target) hv_machine_wake(target);
}

void hv_sepcore_connect(HvSepCore *s, HvSepMailbox *mb, HvMachine *ap) {
    s->ap = ap;
    mb->notify = wake_side;
    mb->notify_opaque = s;
}

void hv_sepcore_destroy(HvSepCore *s) {
    if (s->rom) munmap(s->rom, SEPCORE_ROM_WINDOW);
    if (s->ram) munmap(s->ram, SEPCORE_RAM_SIZE);
    if (s->runtime) munmap(s->runtime, s->runtime_size);
    free(s->view);
    memset(s, 0, sizeof *s);
}
