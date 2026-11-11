#!/usr/bin/env python3
"""Checks devices/vresearch_platform.h against OpenAVB's platform.c.

usage: tools/check_openavb_offsets.py /path/to/OpenAVB
Exits 1 on any mismatch. Needs only the OpenAVB source tree, no build."""
import re
import sys

if len(sys.argv) != 2:
    sys.exit(__doc__)
root = sys.argv[1]
src = open(root + "/src/platform/platform.c").read()
inc = open(root + "/include/avp/platform/platform.h").read()
hdr = open(__file__.rsplit("/tools/", 1)[0] + "/devices/vresearch_platform.h").read()

pairs = {"BOARD_ID": "BOARD_ID", "CHIP_REVISION": "CHIP_REV", "ECID_LOW": "ECID_LO", "ECID_HIGH": "ECID_HI",
         "SECURITY_DOMAIN": "SECURITY_DOM", "BOOT_CONFIGURATION": "BOOT_CONFIG", "BOOT_OPTIONS": "BOOT_OPTIONS",
         "ENTROPY": "ENTROPY", "VARIANT": "VARIANT"}
bad = 0
for theirs, ours in pairs.items():
    a = re.search(r"PLATFORM_%s_OFFSET = (0x[0-9a-f]+)" % theirs, src)
    b = re.search(r"VRESEARCH_PLAT_%s\s+(0x[0-9a-f]+)" % ours, hdr)
    ok = bool(a and b and int(a.group(1), 16) == int(b.group(1), 16))
    print("%-20s openavb=%-8s ours=%-8s %s" % (theirs, a and a.group(1), b and b.group(1), "ok" if ok else "MISMATCH"))
    bad += not ok
a = re.search(r"AVP_PLATFORM_CHIP_ID = AVP_PLATFORM_BASE \+ (0x[0-9a-f]+)", inc)
b = re.search(r"VRESEARCH_PLAT_CHIP_ID\s+(0x[0-9a-f]+)", hdr)
ok = bool(a and b and int(a.group(1), 16) == int(b.group(1), 16))
print("%-20s openavb=%-8s ours=%-8s %s" % ("CHIP_ID", a and a.group(1), b and b.group(1), "ok" if ok else "MISMATCH"))
bad += not ok
a = re.search(r"AVP_PLATFORM_VARIANT_CAPACITY = (\d+)", inc)
b = re.search(r"VRESEARCH_PLAT_VARIANT_SIZE\s+(\d+)", hdr)
ok = bool(a and b and a.group(1) == b.group(1))
print("%-20s openavb=%-8s ours=%-8s %s" % ("VARIANT_SIZE", a and a.group(1), b and b.group(1), "ok" if ok else "MISMATCH"))
bad += not ok
sys.exit(1 if bad else 0)
