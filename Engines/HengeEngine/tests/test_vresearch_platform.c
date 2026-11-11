/* Tests of the vresearch1 platform block (devices/vresearch_platform.c). Pure C: runs on any host.
   Builds: cc -std=c11 -Wall -Wextra -Idevices tests/test_vresearch_platform.c devices/vresearch_platform.c

   The expected offsets and values below are written out again here, from OpenAVB's src/platform/platform.c,
   on purpose: if the header's constants drift, this fails instead of agreeing with itself. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vresearch_platform.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

enum { BLOCK = 0x1000 };

static uint32_t rd32(const uint8_t *b, size_t off) {
    return (uint32_t)b[off] | (uint32_t)b[off + 1] << 8 | (uint32_t)b[off + 2] << 16 | (uint32_t)b[off + 3] << 24;
}

static void test_offsets_match_openavb(void) {
    CHECK(VRESEARCH_PLAT_BOARD_ID == 0x10);
    CHECK(VRESEARCH_PLAT_CHIP_REV == 0x14);
    CHECK(VRESEARCH_PLAT_ECID_LO == 0x18);
    CHECK(VRESEARCH_PLAT_ECID_HI == 0x1c);
    CHECK(VRESEARCH_PLAT_SECURITY_DOM == 0x2c);
    CHECK(VRESEARCH_PLAT_BOOT_CONFIG == 0x30);
    CHECK(VRESEARCH_PLAT_BOOT_OPTIONS == 0x34);
    CHECK(VRESEARCH_PLAT_ENTROPY == 0x38);
    CHECK(VRESEARCH_PLAT_VARIANT == 0x5c8);
    CHECK(VRESEARCH_PLAT_VARIANT_SIZE == 64);
    CHECK(VRESEARCH_PLAT_CHIP_ID == 0x6f0);
}

static void test_default_identity(void) {
    uint8_t b[BLOCK];
    memset(b, 0xaa, sizeof b);
    HvVresearchIdentity id = hv_vresearch_identity(0x0123456789abcdefull, 0);
    CHECK(hv_vresearch_fill_platform(b, sizeof b, &id) == 0);
    CHECK(rd32(b, 0x10) == 0x90);                 /* BDID */
    CHECK(rd32(b, 0x14) == 0);                    /* CPRV */
    CHECK(rd32(b, 0x18) == 0x89abcdefu);          /* ECID low */
    CHECK(rd32(b, 0x1c) == 0x01234567u);          /* ECID high */
    CHECK(rd32(b, 0x2c) == 1);                    /* SDOM */
    CHECK(rd32(b, 0x30) == 1);                    /* the ROM boots a platform source only for 1 or 2 */
    CHECK(rd32(b, 0x34) == 0);                    /* no forced DFU */
    CHECK(rd32(b, 0x6f0) == 0);                   /* the ROM substitutes 0xfe01 itself */
    for (int i = 0; i < 64; i++) CHECK(b[0x5c8 + i] == 0); /* empty variant: the ROM keeps its DFU product string */
}

static void test_force_dfu_bit(void) {
    uint8_t b[BLOCK] = {0};
    HvVresearchIdentity id = hv_vresearch_identity(1, 1);
    CHECK(hv_vresearch_fill_platform(b, sizeof b, &id) == 0);
    CHECK(rd32(b, 0x34) == 1);
    /* The ROM clears bit 0 once it has claimed the policy; filling again sets it again. */
    b[0x34] = 0;
    CHECK(hv_vresearch_fill_platform(b, sizeof b, &id) == 0);
    CHECK(rd32(b, 0x34) == 1);
}

static void test_leaves_other_registers_alone(void) {
    uint8_t b[BLOCK];
    memset(b, 0x5a, sizeof b);
    HvVresearchIdentity id = hv_vresearch_identity(2, 0);
    CHECK(hv_vresearch_fill_platform(b, sizeof b, &id) == 0);
    for (size_t off = 0x38; off < 0x3c; off++) CHECK(b[off] == 0x5a); /* entropy */
    for (size_t off = 0x300; off < 0x400; off++) CHECK(b[off] == 0x5a); /* boot context */
    CHECK(b[0x3ac] == 0x5a);                                            /* stage marker */
    for (size_t off = 0x64; off < 0x84; off++) CHECK(b[off] == 0x5a);   /* progress words */
    CHECK(b[0x0f] == 0x5a && b[0x20] == 0x5a);                          /* neighbours of the written words */
}

static void test_variant(void) {
    uint8_t b[BLOCK];
    memset(b, 0xff, sizeof b);
    HvVresearchIdentity id = hv_vresearch_identity(3, 0);
    id.variant = "vresearch101ap";
    CHECK(hv_vresearch_fill_platform(b, sizeof b, &id) == 0);
    CHECK(strcmp((const char *)b + 0x5c8, "vresearch101ap") == 0);
    CHECK(b[0x5c8 + 63] == 0);
    CHECK(b[0x5c8 + 64] == 0xff);                 /* does not run past the field */
    char longname[80];
    memset(longname, 'x', sizeof longname - 1);
    longname[sizeof longname - 1] = 0;
    id.variant = longname;
    CHECK(hv_vresearch_fill_platform(b, sizeof b, &id) == -1);
    memset(longname, 'x', 63);
    longname[63] = 0;                             /* 63 characters + terminator fits exactly */
    CHECK(hv_vresearch_fill_platform(b, sizeof b, &id) == 0);
}

static void test_bad_arguments(void) {
    uint8_t b[BLOCK];
    HvVresearchIdentity id = hv_vresearch_identity(0, 0);
    CHECK(hv_vresearch_fill_platform(NULL, sizeof b, &id) == -1);
    CHECK(hv_vresearch_fill_platform(b, sizeof b, NULL) == -1);
    CHECK(hv_vresearch_fill_platform(b, 0x6f0, &id) == -1);     /* chip ID register would be out of range */
    CHECK(hv_vresearch_fill_platform(b, 0x6f4, &id) == 0);
}

int main(void) {
    test_offsets_match_openavb();
    test_default_identity();
    test_force_dfu_bit();
    test_leaves_other_registers_alone();
    test_variant();
    test_bad_arguments();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("test_vresearch_platform: ok\n");
    return 0;
}
