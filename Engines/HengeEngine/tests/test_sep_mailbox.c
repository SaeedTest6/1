/* Tests of the SEP mailbox model (devices/sep_mailbox_core.c). Pure C: runs on any host.
   Builds: cc -std=c11 -Wall -Wextra -Idevices tests/test_sep_mailbox.c devices/sep_mailbox_core.c */
#include <stdio.h>
#include <stdlib.h>

#include "sep_mailbox_core.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static int notified[2];
static void notify(void *opaque, int side) {
    (void)opaque;
    notified[side]++;
}

/* What an ldp/stp of a slot looks like once core/machine.c has split it. */
static void send_pair(HvSepMailbox *mb, int side, uint64_t lo, uint64_t hi) {
    hv_sepmbx_write(mb, side, HV_SEPMBX_TX_SLOT, lo, 8);
    hv_sepmbx_write(mb, side, HV_SEPMBX_TX_SLOT + 8, hi, 8);
}

static HvSepMsg recv_pair(HvSepMailbox *mb, int side) {
    HvSepMsg m;
    m.lo = hv_sepmbx_read(mb, side, HV_SEPMBX_RX_SLOT, 8);
    m.hi = hv_sepmbx_read(mb, side, HV_SEPMBX_RX_SLOT + 8, 8);
    return m;
}

static void test_reset_state(void) {
    HvSepMailbox mb;
    hv_sepmbx_init(&mb, NULL, NULL);
    for (int side = 0; side < 2; side++) {
        CHECK(hv_sepmbx_read(&mb, side, HV_SEPMBX_RX_STATUS, 4) & HV_SEPMBX_FIFO_EMPTY);
        CHECK(hv_sepmbx_read(&mb, side, HV_SEPMBX_TX_STATUS, 4) & HV_SEPMBX_FIFO_EMPTY);
        CHECK(!(hv_sepmbx_read(&mb, side, HV_SEPMBX_TX_STATUS, 4) & HV_SEPMBX_TX_FULL));
        CHECK(hv_sepmbx_read(&mb, side, HV_SEPMBX_STATUS, 4) == 0);
    }
}

static void test_ownership_acknowledged(void) {
    /* OpenAVB's sep_acquire writes status | bit 31 and expects to read bit 31 back. */
    HvSepMailbox mb;
    hv_sepmbx_init(&mb, NULL, NULL);
    hv_sepmbx_write(&mb, HV_SEPMBX_AP, HV_SEPMBX_CONTROL, 1, 4);
    hv_sepmbx_write(&mb, HV_SEPMBX_AP, HV_SEPMBX_STATUS, HV_SEPMBX_STATUS_ACTIVE, 4);
    CHECK(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_STATUS, 4) & HV_SEPMBX_STATUS_ACTIVE);
    CHECK(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_CONTROL, 4) == 1);
    CHECK(!(hv_sepmbx_read(&mb, HV_SEPMBX_SEP, HV_SEPMBX_STATUS, 4) & HV_SEPMBX_STATUS_ACTIVE)); /* the other side is separate */
}

static void test_send_and_receive(void) {
    HvSepMailbox mb;
    hv_sepmbx_init(&mb, notify, NULL);
    notified[0] = notified[1] = 0;
    send_pair(&mb, HV_SEPMBX_AP, 0x1122334455667788ull, 0);
    CHECK(notified[HV_SEPMBX_SEP] == 1 && notified[HV_SEPMBX_AP] == 0);
    CHECK(!(hv_sepmbx_read(&mb, HV_SEPMBX_SEP, HV_SEPMBX_RX_STATUS, 4) & HV_SEPMBX_FIFO_EMPTY));
    CHECK(!(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_TX_STATUS, 4) & HV_SEPMBX_FIFO_EMPTY));
    CHECK(hv_sepmbx_read(&mb, HV_SEPMBX_SEP, HV_SEPMBX_STATUS, 4) & HV_SEPMBX_STATUS_RX_EVENT);
    HvSepMsg m = recv_pair(&mb, HV_SEPMBX_SEP);
    CHECK(m.lo == 0x1122334455667788ull && m.hi == 0);
    CHECK(hv_sepmbx_read(&mb, HV_SEPMBX_SEP, HV_SEPMBX_RX_STATUS, 4) & HV_SEPMBX_FIFO_EMPTY);
    CHECK(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_TX_STATUS, 4) & HV_SEPMBX_FIFO_EMPTY);
}

static void test_event_bit_clears_on_write_one(void) {
    HvSepMailbox mb;
    hv_sepmbx_init(&mb, NULL, NULL);
    send_pair(&mb, HV_SEPMBX_SEP, 1, 0);
    CHECK(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_STATUS, 4) & HV_SEPMBX_STATUS_RX_EVENT);
    hv_sepmbx_write(&mb, HV_SEPMBX_AP, HV_SEPMBX_STATUS, HV_SEPMBX_STATUS_RX_EVENT, 4);
    CHECK(!(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_STATUS, 4) & HV_SEPMBX_STATUS_RX_EVENT));
    /* Clearing the event does not drop the message. */
    CHECK(!(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_RX_STATUS, 4) & HV_SEPMBX_FIFO_EMPTY));
}

static void test_fifo_order_and_depth(void) {
    HvSepMailbox mb;
    hv_sepmbx_init(&mb, NULL, NULL);
    for (uint64_t i = 0; i < HV_SEPMBX_DEPTH; i++) send_pair(&mb, HV_SEPMBX_AP, 0x100 + i, 0);
    CHECK(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_TX_STATUS, 4) & HV_SEPMBX_TX_FULL);
    send_pair(&mb, HV_SEPMBX_AP, 0xdead, 0); /* dropped: the queue is full */
    for (uint64_t i = 0; i < HV_SEPMBX_DEPTH; i++) CHECK(recv_pair(&mb, HV_SEPMBX_SEP).lo == 0x100 + i);
    CHECK(hv_sepmbx_read(&mb, HV_SEPMBX_SEP, HV_SEPMBX_RX_STATUS, 4) & HV_SEPMBX_FIFO_EMPTY);
    CHECK(!(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_TX_STATUS, 4) & HV_SEPMBX_TX_FULL));
    /* The ring wraps. */
    for (uint64_t i = 0; i < 20; i++) {
        send_pair(&mb, HV_SEPMBX_AP, 0x200 + i, i);
        HvSepMsg m = recv_pair(&mb, HV_SEPMBX_SEP);
        CHECK(m.lo == 0x200 + i && m.hi == i);
    }
}

static void test_pop_of_empty_queue(void) {
    HvSepMailbox mb;
    hv_sepmbx_init(&mb, NULL, NULL);
    HvSepMsg m = recv_pair(&mb, HV_SEPMBX_AP);
    CHECK(m.lo == 0 && m.hi == 0);
    CHECK(hv_sepmbx_read(&mb, HV_SEPMBX_AP, HV_SEPMBX_RX_STATUS, 4) & HV_SEPMBX_FIFO_EMPTY);
}

static void test_startup_event_decodes(void) {
    /* OpenSEP's first message after reset: 0x00d200ff | 1 << 32, tag 0xd2, payload phase 1. */
    HvSepMailbox mb;
    hv_sepmbx_init(&mb, NULL, NULL);
    CHECK(hv_sepmbx_inject(&mb, HV_SEPMBX_AP, 0x00d200ffull | 1ull << 32, 0) == 0);
    HvSepMsg m = recv_pair(&mb, HV_SEPMBX_AP);
    CHECK((m.lo & 0xff) == 0xff);          /* protocol frame */
    CHECK(((m.lo >> 16) & 0xff) == 0xd2);  /* tag */
    CHECK((m.lo >> 32) == 1);              /* phase */
}

static void test_irq_lines(void) {
    HvSepMailbox mb;
    hv_sepmbx_init(&mb, NULL, NULL);
    CHECK(!hv_sepmbx_irq_pending(&mb, HV_SEPMBX_AP));
    hv_sepmbx_write(&mb, HV_SEPMBX_AP, HV_SEPMBX_IRQ_EN, HV_SEPMBX_IRQ_RX_NOT_EMPTY, 4);
    CHECK(!hv_sepmbx_irq_pending(&mb, HV_SEPMBX_AP));
    hv_sepmbx_inject(&mb, HV_SEPMBX_AP, 5, 0);
    CHECK(hv_sepmbx_irq_pending(&mb, HV_SEPMBX_AP));
    recv_pair(&mb, HV_SEPMBX_AP);
    CHECK(!hv_sepmbx_irq_pending(&mb, HV_SEPMBX_AP));
    hv_sepmbx_write(&mb, HV_SEPMBX_AP, HV_SEPMBX_IRQ_EN, HV_SEPMBX_IRQ_TX_EMPTY, 4);
    CHECK(hv_sepmbx_irq_pending(&mb, HV_SEPMBX_AP)); /* nothing waiting in its TX queue */
    send_pair(&mb, HV_SEPMBX_AP, 6, 0);
    CHECK(!hv_sepmbx_irq_pending(&mb, HV_SEPMBX_AP));
}

int main(void) {
    test_reset_state();
    test_ownership_acknowledged();
    test_send_and_receive();
    test_event_bit_clears_on_write_one();
    test_fifo_order_and_depth();
    test_pop_of_empty_queue();
    test_startup_event_decodes();
    test_irq_lines();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("test_sep_mailbox: ok\n");
    return 0;
}
