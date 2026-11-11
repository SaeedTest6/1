/* Layout test of the SEP core's address space (devices/sep_core.c) against a stub machine, on any host.
   The stub only records what the core asks for; it does not run code. Builds:
   cc -std=c11 -Wall -Wextra -Icore -Ihost -Idevices tests/test_sep_core.c devices/sep_core.c devices/sep_mailbox.c \
      devices/sep_mailbox_core.c

   The expected addresses are written out here from OpenSEP's docs/external-contract.md, not taken from the header. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sep_core.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

typedef struct { uint64_t pa, size; void *host; int mmio; } Region;
static Region regions[16];
static int nregions, fail_at = -1, wakes;
static HvMachine *last_woken;
static uint32_t reset_index = 99;

int hv_machine_add_ram(HvMachine *m, uint64_t pa, uint64_t size, void *host, int readonly) {
    (void)m, (void)readonly;
    if (fail_at == nregions) return -1;
    regions[nregions++] = (Region){pa, size, host, 0};
    return 0;
}

int hv_machine_add_mmio(HvMachine *m, uint64_t pa, uint64_t size, void *opaque,
                        uint64_t (*read)(void *, uint64_t, unsigned), void (*write)(void *, uint64_t, uint64_t, unsigned),
                        const char *name) {
    (void)m, (void)opaque, (void)read, (void)write, (void)name;
    if (fail_at == nregions) return -1;
    regions[nregions++] = (Region){pa, size, NULL, 1};
    return 0;
}

void hv_machine_wake(HvMachine *m) { last_woken = m; wakes++; }
void hv_cpu_reset_index(HvCpu *cpu, HvMachine *m, uint32_t index) { (void)cpu, (void)m; reset_index = index; }

static const Region *find(uint64_t pa) {
    for (int i = 0; i < nregions; i++) if (regions[i].pa == pa) return &regions[i];
    return NULL;
}

static void test_layout(void) {
    static HvMachine sep_m, ap_m;
    static HvCpu cpu;
    HvSepMailbox mb;
    HvSepCore s;
    static uint8_t ap_ram[0x4000];
    nregions = 0, fail_at = -1;
    hv_sepmbx_init(&mb, NULL, NULL);
    CHECK(hv_sepcore_create(&s, &sep_m, &cpu, &mb, ap_ram, sizeof ap_ram, 0x400000) == 0);
    CHECK(nregions == 7);

    const Region *r;
    CHECK((r = find(0x00000000)) && r->mmio && r->size == 0x02000000);      /* device block */
    CHECK((r = find(0x02000000)) && !r->mmio && r->size >= 0x50000);        /* OpenSEP.bin */
    CHECK((r = find(0x04010000)) && !r->mmio && r->size == 0x10000);        /* boot breadcrumb at 0x04010028 */
    CHECK((r = find(0x04020000)) && !r->mmio && r->size == 0x30000);        /* SEP RAM ..0x04050000 */
    CHECK((r = find(0x06000000)) && !r->mmio && r->size == 0x400000);       /* runtime region */
    CHECK((r = find(0x30250000)) && r->mmio && r->size >= 0x1000);          /* mailbox */
    CHECK((r = find(0x70000000)) && !r->mmio && r->host == ap_ram && r->size == sizeof ap_ram); /* the AP's RAM, shared */

    /* Nothing overlaps. */
    for (int i = 0; i < nregions; i++)
        for (int j = i + 1; j < nregions; j++)
            CHECK(regions[i].pa + regions[i].size <= regions[j].pa || regions[j].pa + regions[j].size <= regions[i].pa);
    /* The device block ends where the ROM begins, so the ROM's platform page (+0x400000) and sink (+0x600000) are in it. */
    CHECK(0x400000 < 0x02000000 && 0x600000 < 0x02000000);

    hv_sepcore_reset_cpu(&s);
    CHECK(reset_index == 0 && cpu.pc == 0x02000000 && cpu.off == 0);

    /* The mailbox wakes the machine on the side that has something to read. */
    hv_sepcore_connect(&s, &mb, &ap_m);
    wakes = 0;
    hv_sepmbx_write(&mb, HV_SEPMBX_AP, HV_SEPMBX_TX_SLOT, 1, 8);
    hv_sepmbx_write(&mb, HV_SEPMBX_AP, HV_SEPMBX_TX_SLOT + 8, 0, 8);
    CHECK(wakes == 1 && last_woken == &sep_m);
    hv_sepmbx_write(&mb, HV_SEPMBX_SEP, HV_SEPMBX_TX_SLOT, 2, 8);
    hv_sepmbx_write(&mb, HV_SEPMBX_SEP, HV_SEPMBX_TX_SLOT + 8, 0, 8);
    CHECK(wakes == 2 && last_woken == &ap_m);

    hv_sepcore_destroy(&s);
    CHECK(s.rom == NULL && s.runtime == NULL);
}

static void test_failure_leaves_nothing(void) {
    static HvMachine m;
    static HvCpu cpu;
    HvSepMailbox mb;
    HvSepCore s;
    static uint8_t ap_ram[0x4000];
    hv_sepmbx_init(&mb, NULL, NULL);
    for (int at = 0; at < 7; at++) {
        nregions = 0, fail_at = at;
        CHECK(hv_sepcore_create(&s, &m, &cpu, &mb, ap_ram, sizeof ap_ram, 0x100000) == -1);
        CHECK(s.rom == NULL && s.scratch == NULL && s.ram == NULL && s.runtime == NULL && s.view == NULL);
    }
    fail_at = -1;
    nregions = 0;
    CHECK(hv_sepcore_create(&s, &m, &cpu, &mb, NULL, 0x4000, 0x100000) == -1);        /* no AP RAM */
    CHECK(hv_sepcore_create(&s, &m, &cpu, &mb, ap_ram, sizeof ap_ram, 0) == -1);      /* no runtime */
    CHECK(hv_sepcore_create(&s, &m, &cpu, &mb, ap_ram, sizeof ap_ram, 0x1234) == -1); /* not page-sized */
}

static void test_rom_size(void) {
    static HvMachine m;
    static HvCpu cpu;
    HvSepMailbox mb;
    HvSepCore s;
    static uint8_t ap_ram[0x4000];
    hv_sepmbx_init(&mb, NULL, NULL);
    nregions = 0, fail_at = -1;
    CHECK(hv_sepcore_create(&s, &m, &cpu, &mb, ap_ram, sizeof ap_ram, 0x100000) == 0);
    const char *path = "test_sep_core_rom.tmp"; /* in the working directory; removed below */
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); failures++; return; }
    static uint8_t big[0x50001];
    memset(big, 0xab, sizeof big);
    fwrite(big, 1, 0x50000, f); /* exactly the window */
    fclose(f);
    CHECK(hv_sepcore_load_rom(&s, path) == 0 && s.rom[0] == 0xab && s.rom[0x4ffff] == 0xab);
    f = fopen(path, "wb");
    fwrite(big, 1, sizeof big, f); /* one byte more than the ROM maps */
    fclose(f);
    CHECK(hv_sepcore_load_rom(&s, path) == -1);
    CHECK(hv_sepcore_load_rom(&s, "/nonexistent/rom.bin") == -1);
    remove(path);
    hv_sepcore_destroy(&s);
}

int main(void) {
    test_layout();
    test_failure_leaves_nothing();
    test_rom_size();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("test_sep_core: ok\n");
    return 0;
}
