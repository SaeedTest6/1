/* The vresearch1 board on the real engine: a guest program does the 16-byte accesses OpenAVB does against the
   SEP mailbox, and reads the platform identity. Needs the machine, so it only builds where core/ does (macOS). */
#include "harness.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "vresearch_board.h"

DECLARE_PROGRAM(p_sep_pair);
DECLARE_PROGRAM(p_platform);
extern const char vresearch_programs_begin[], vresearch_programs_finish[];

#define RAM_SIZE (384ull << 20)

static HvMachine m;
static HvCpu cpu;
static HvVresearch board;
static int checks, failures;

static void check(const char *what, uint64_t got, uint64_t want) {
    checks++;
    if (got != want) {
        printf("  FAIL %s: %llx, expected %llx\n", what, (unsigned long long)got, (unsigned long long)want);
        failures++;
    }
}

static const char *work_dir(void) {
    const char *w = getenv("HV_TEST_WORK");
    if (w && *w) return w;
    w = getenv("TMPDIR");
    return w && *w ? w : "/tmp";
}

static int make_disk(const char *path, uint64_t size) {
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    static uint8_t buf[4096];
    for (uint64_t off = 0; off < size; off += sizeof buf)
        if (write(fd, buf, sizeof buf) != (ssize_t)sizeof buf) return -1;
    close(fd);
    return 0;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (hv_machine_init(&m, 4 << 20) != 0) return 1;
    char root_path[1024], aux_path[1024], ram_path[1024];
    snprintf(root_path, sizeof root_path, "%s/hv-vr-root.img", work_dir());
    snprintf(aux_path, sizeof aux_path, "%s/hv-vr-aux.img", work_dir());
    snprintf(ram_path, sizeof ram_path, "%s/hv-vr-ram", work_dir());
    if (make_disk(root_path, 1 << 20) || make_disk(aux_path, 256 << 10)) {
        printf("cannot create test disks in %s\n", work_dir());
        return 1;
    }
    static HvDisk root, aux;
    hv_disk_open(&root, root_path, 0, 0);
    hv_disk_open(&aux, aux_path, 16384, 0);
    setenv("HV_BALLOON", "0", 1);
    setenv("HV_RAM_FILE", ram_path, 1);
    setenv("HV_RAM_NO_LEDGER", "1", 1);
    setenv("HV_RAM_NO_PREFAULT", "1", 1);
    static const uint8_t mac[6] = {2, 0, 0, 0x12, 0x34, 0x56};
    HvCfgParams cfg = {.nr_cpus = 1, .mac_en0 = {2, 0, 0, 0x12, 0x34, 0x56}};
    HvVresearchIdentity id = hv_vresearch_identity(0x0123456789abcdefull, 0);
    if (hv_vresearch_create(&board, &m, &cpu, RAM_SIZE, &root, &aux, &cfg, &id, mac) != 0) {
        printf("hv_vresearch_create failed\n");
        return 1;
    }
    hv_vresearch_reset_cpu(&board);
    check("reset pc is the ROM", cpu.pc, VRESEARCH_ROM);

    hv_machine_add_ram(&m, (uint64_t)(uintptr_t)vresearch_programs_begin,
                       vresearch_programs_finish - vresearch_programs_begin, (void *)vresearch_programs_begin, 1);
    static const TestProgram pair = TEST_PROGRAM(p_sep_pair), plat = TEST_PROGRAM(p_platform);
    static HvCpu init;

    /* The platform block, read the way the ROM reads it. */
    memset(&init, 0, sizeof init);
    seed_cpu(&init, 1, VRESEARCH_RAM + 0x10000);
    init.x[1] = VRESEARCH_PLATFORM;
    if (run_jit(&m, &plat, &init, &cpu, NULL) == 0) {
        check("board ID", cpu.x[10], 0x90);
        check("ECID low", cpu.x[11], 0x89abcdef);
        check("ECID high", cpu.x[12], 0x01234567);
        check("security domain", cpu.x[13], 1);
        check("boot configuration", cpu.x[14], 1);
    } else {
        failures++;
    }

    /* A message from the SEP, popped with one ldp; one stp back. */
    hv_sepmbx_inject(&board.mailbox, HV_SEPMBX_AP, 0x00d200ffull | 1ull << 32, 0xcafe);
    memset(&init, 0, sizeof init);
    seed_cpu(&init, 2, VRESEARCH_RAM + 0x10000);
    init.x[0] = VRESEARCH_SEP_MAILBOX;
    init.x[7] = 0x1111222233334444ull;
    init.x[8] = 0x5555666677778888ull;
    if (run_jit(&m, &pair, &init, &cpu, NULL) == 0) {
        check("RX not empty before the pop", (cpu.x[6] >> 17) & 1, 0);
        check("ldp low half", cpu.x[4], 0x00d200ffull | 1ull << 32);
        check("ldp high half", cpu.x[5], 0xcafe);
        check("RX empty after the pop", (cpu.x[10] >> 17) & 1, 1);
        check("TX not empty after the stp", (cpu.x[9] >> 17) & 1, 0);
        uint64_t lo = hv_sepmbx_read(&board.mailbox, HV_SEPMBX_SEP, HV_SEPMBX_RX_SLOT, 8);
        uint64_t hi = hv_sepmbx_read(&board.mailbox, HV_SEPMBX_SEP, HV_SEPMBX_RX_SLOT + 8, 8);
        check("SEP got the stp, low", lo, 0x1111222233334444ull);
        check("SEP got the stp, high", hi, 0x5555666677778888ull);
    } else {
        failures++;
    }

    hv_disk_close(&root);
    hv_disk_close(&aux);
    unlink(root_path);
    unlink(aux_path);
    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
