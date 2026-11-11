#include "vresearch_platform.h"

#include <string.h>

static void put32(uint8_t *block, size_t off, uint32_t v) {
    for (int i = 0; i < 4; i++) block[off + i] = (uint8_t)(v >> (8 * i)); /* little endian, whatever the host is */
}

HvVresearchIdentity hv_vresearch_identity(uint64_t ecid, int force_dfu) {
    HvVresearchIdentity id;
    memset(&id, 0, sizeof id);
    id.board_id = VRESEARCH_BOARD_ID_VPHONE;
    id.chip_rev = 0;
    id.ecid = ecid;
    id.security_domain = 1;
    id.boot_config = VRESEARCH_BOOT_CONFIG_DEFAULT;
    id.force_dfu = force_dfu;
    id.chip_id = 0;
    id.variant = NULL;
    return id;
}

int hv_vresearch_fill_platform(uint8_t *block, size_t size, const HvVresearchIdentity *id) {
    if (!block || !id || size < VRESEARCH_PLAT_CHIP_ID + 4) return -1;
    size_t vlen = id->variant ? strlen(id->variant) : 0;
    if (vlen >= VRESEARCH_PLAT_VARIANT_SIZE) return -1; /* the ROM forces a terminator into the last byte */
    put32(block, VRESEARCH_PLAT_BOARD_ID, id->board_id);
    put32(block, VRESEARCH_PLAT_CHIP_REV, id->chip_rev);
    put32(block, VRESEARCH_PLAT_ECID_LO, (uint32_t)id->ecid);
    put32(block, VRESEARCH_PLAT_ECID_HI, (uint32_t)(id->ecid >> 32));
    put32(block, VRESEARCH_PLAT_SECURITY_DOM, id->security_domain);
    put32(block, VRESEARCH_PLAT_BOOT_CONFIG, id->boot_config);
    put32(block, VRESEARCH_PLAT_BOOT_OPTIONS, id->force_dfu ? VRESEARCH_BOOT_OPTION_FORCE_DFU : 0);
    memset(block + VRESEARCH_PLAT_VARIANT, 0, VRESEARCH_PLAT_VARIANT_SIZE);
    if (vlen) memcpy(block + VRESEARCH_PLAT_VARIANT, id->variant, vlen);
    put32(block, VRESEARCH_PLAT_CHIP_ID, id->chip_id);
    return 0;
}
