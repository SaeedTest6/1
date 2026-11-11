/* Headless first-boot runner for the vresearch1 board: the AP machine runs OpenAVB, a second machine runs
   OpenSEP, the two share the mailbox and the AP's RAM. Prints the console and, at the end, where each CPU is.

   usage: vresearch_run --avb OpenAVB.bin --sep OpenSEP.bin [--seconds N] [--dfu] [--ecid HEX] [--nosep]
   Experimental: written without being able to run it; the CI log is the first feedback. */
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dispatch.h"
#include "machine.h"
#include "sep_core.h"
#include "vresearch_board.h"

static HvMachine ap_m, sep_m;
static HvCpu ap_cpu, sep_cpu;
static HvVresearch board;
static HvSepCore sep;
static atomic_int stop;
static int ap_reason = -1, sep_reason = -1;

static void console(void *opaque, uint8_t c) {
    (void)opaque;
    putchar(c);
    if (c == '\n') fflush(stdout);
}

/* Guest code runs with the guest's SP in the host's SP register: no asynchronous signal may land on these threads. */
static void block_async_signals(void) {
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGSEGV), sigdelset(&set, SIGBUS), sigdelset(&set, SIGILL), sigdelset(&set, SIGTRAP), sigdelset(&set, SIGFPE);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
}

static void *ap_thread(void *unused) {
    (void)unused;
    block_async_signals();
    ap_reason = hv_run(&ap_cpu);
    return NULL;
}

static void *sep_thread(void *unused) {
    (void)unused;
    block_async_signals();
    sep_reason = hv_run(&sep_cpu);
    return NULL;
}

static void *kicker(void *unused) {
    (void)unused;
    block_async_signals();
    while (!atomic_load(&stop)) {
        usleep(1000);
        hv_machine_kick_all(&ap_m);
        if (sep.m) hv_machine_kick_all(&sep_m);
    }
    return NULL;
}

static int make_blank(const char *path, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    if (fseek(f, (long)size - 1, SEEK_SET) || fputc(0, f) == EOF) return -1;
    return fclose(f);
}

int main(int argc, char **argv) {
    const char *avb = NULL, *seprom = NULL;
    int seconds = 20, dfu = 0, nosep = 0;
    uint64_t ecid = 0x0123456789abcdefull;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--avb") && i + 1 < argc) avb = argv[++i];
        else if (!strcmp(argv[i], "--sep") && i + 1 < argc) seprom = argv[++i];
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ecid") && i + 1 < argc) ecid = strtoull(argv[++i], NULL, 16);
        else if (!strcmp(argv[i], "--dfu")) dfu = 1;
        else if (!strcmp(argv[i], "--nosep")) nosep = 1;
        else { fprintf(stderr, "bad argument %s\n", argv[i]); return 2; }
    }
    if (!avb || (!seprom && !nosep)) { fprintf(stderr, "needs --avb and --sep (or --nosep)\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (hv_machine_init(&ap_m, (size_t)256 << 20)) { fprintf(stderr, "AP JIT region\n"); return 1; }
    ap_m.log = getenv("HV_LOG") != NULL;
    if (make_blank("vr-root.img", 1 << 20) || make_blank("vr-aux.img", 256 << 10)) { fprintf(stderr, "disks\n"); return 1; }
    static HvDisk root, aux;
    hv_disk_open(&root, "vr-root.img", 0, 0);
    hv_disk_open(&aux, "vr-aux.img", 16384, 0);
    setenv("HV_BALLOON", "0", 1);
    setenv("HV_RAM_FILE", "vr-ram", 1);
    setenv("HV_RAM_NO_LEDGER", "1", 1);
    setenv("HV_RAM_NO_PREFAULT", "1", 1);
    static const uint8_t mac[6] = {2, 0, 0, 0x12, 0x34, 0x56};
    HvCfgParams cfg = {.nr_cpus = 1, .mac_en0 = {2, 0, 0, 0x12, 0x34, 0x56}};
    HvVresearchIdentity id = hv_vresearch_identity(ecid, dfu);
    if (hv_vresearch_create(&board, &ap_m, &ap_cpu, (uint64_t)256 << 20, &root, &aux, &cfg, &id, mac)) {
        fprintf(stderr, "hv_vresearch_create\n");
        return 1;
    }
    if (hv_vresearch_load_rom(&board, avb)) { fprintf(stderr, "cannot load %s\n", avb); return 1; }
    hv_vresearch_reset_cpu(&board);
    board.base.uart->output = console;

    if (!nosep) {
        if (hv_machine_init(&sep_m, (size_t)64 << 20)) { fprintf(stderr, "SEP JIT region\n"); return 1; }
        sep_m.log = ap_m.log;
        if (hv_sepcore_create(&sep, &sep_m, &sep_cpu, &board.mailbox, board.base.ram, board.base.ram_size, 16 << 20)) {
            fprintf(stderr, "hv_sepcore_create\n");
            return 1;
        }
        if (hv_sepcore_load_rom(&sep, seprom)) { fprintf(stderr, "cannot load %s\n", seprom); return 1; }
        hv_sepcore_reset_cpu(&sep);
        hv_sepcore_connect(&sep, &board.mailbox, &ap_m);
    }

    pthread_t a, s, k;
    pthread_create(&k, NULL, kicker, NULL);
    pthread_create(&a, NULL, ap_thread, NULL);
    if (!nosep) pthread_create(&s, NULL, sep_thread, NULL);
    for (int t = 0; t < seconds * 10 && !atomic_load(&stop); t++) {
        usleep(100000);
        if (ap_reason != -1 && (nosep || sep_reason != -1)) break; /* both machines stopped */
        if (ap_reason != -1) break;
    }
    atomic_store(&stop, 1);
    hv_machine_halt(&ap_m, HV_HALT_HOST);
    if (!nosep) hv_machine_halt(&sep_m, HV_HALT_HOST);
    pthread_join(a, NULL);
    if (!nosep) pthread_join(s, NULL);
    pthread_join(k, NULL);

    printf("\n--- end of run ---\n");
    printf("AP : exit %d, pc %llx, el%u\n", ap_reason, (unsigned long long)ap_cpu.pc, ap_cpu.el);
    if (!nosep) {
        printf("SEP: exit %d, pc %llx, el%u\n", sep_reason, (unsigned long long)sep_cpu.pc, sep_cpu.el);
        printf("SEP: esr %llx elr %llx far %llx spsr %llx sctlr %llx vbar %llx insn %08x\n",
               (unsigned long long)sep_cpu.esr_el1, (unsigned long long)sep_cpu.elr_el1, (unsigned long long)sep_cpu.far_el1,
               (unsigned long long)sep_cpu.spsr_el1, (unsigned long long)sep_cpu.sctlr_el1,
               (unsigned long long)sep_cpu.vbar_el1, sep_cpu.exit_insn);
        for (int i = 0; i < 31; i += 4)
            printf("SEP: x%-2d %016llx %016llx %016llx %016llx\n", i, (unsigned long long)sep_cpu.x[i],
                   i + 1 < 31 ? (unsigned long long)sep_cpu.x[i + 1] : 0ull, i + 2 < 31 ? (unsigned long long)sep_cpu.x[i + 2] : 0ull,
                   i + 3 < 31 ? (unsigned long long)sep_cpu.x[i + 3] : 0ull);
        uint64_t at = sep_cpu.elr_el1;
        if (at >= VRESEARCH_SEP_ROM && at + 32 <= VRESEARCH_SEP_ROM + SEPCORE_ROM_WINDOW) {
            printf("SEP: code at elr:");
            for (int i = 0; i < 8; i++) printf(" %08x", ((uint32_t *)(sep.rom + (at - VRESEARCH_SEP_ROM)))[i]);
            printf("\n");
        }
    }
    printf("stage marker %08x, progress:", *(uint32_t *)(board.base.cfg + 0x3ac));
    for (int i = 0; i < 8; i++) printf(" %08x", *(uint32_t *)(board.base.cfg + 0x64 + 4 * i));
    printf("\n");
    return 0;
}
