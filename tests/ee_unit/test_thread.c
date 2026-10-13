/* Synthetic SetupThread boundary/context tests; no game or BIOS bytes. */
#include "ee.h"
#include <string.h>

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static ee_ctx c;

static void setup(uint32_t base, uint32_t size)
{
    c.r[3].sw[0] = 0x3c;
    c.r[4].uw[0] = 0x123450;
    c.r[5].uw[0] = base;
    c.r[6].uw[0] = size;
    c.r[7].uw[0] = 0x1000;
    c.r[8].uw[0] = 0x234560;
    ee_syscall(&c, 0x2000);
}

static int good_stack(uint32_t base, uint32_t size, uint32_t expected)
{
    jmp_buf j;
    CHECK(ee_init(&c) == 0);
    c.exit_jmp = &j;
    if (setjmp(j)) { printf("unexpected stop: %s\n", c.end_detail); return 1; }
    memset(c.ram + expected, 0xa5, 0x2a0);
    setup(base, size);
    CHECK(c.r[2].uw[0] == expected);
    CHECK(ee_r32(&c, expected + 0x1c0) == 0x123450);
    CHECK(ee_r32(&c, expected + 0x1d0) == base + size - 0x20);
    CHECK(ee_r32(&c, expected + 0x1e0) == base + size - 0x20);
    CHECK(ee_r32(&c, expected + 0x1f0) == 0x234560);
    CHECK(ee_r32(&c, expected) == 0xa5a5a5a5);
    CHECK(ee_r32(&c, expected + 0x1c4) == 0xa5a5a5a5);
    CHECK(ee_r32(&c, expected + 0x29c) == 0xa5a5a5a5);
    CHECK(ee_r32(&c, 0x1000) == 0);
    ee_free(&c);
    return 0;
}

static int bad_stack(uint32_t base, uint32_t size)
{
    jmp_buf j;
    CHECK(ee_init(&c) == 0);
    c.exit_jmp = &j;
    if (!setjmp(j)) { setup(base, size); CHECK(0); }
    CHECK(c.end_reason == EE_END_UNSUPPORTED);
    CHECK(strstr(c.end_detail, "explicit RAM stack") != NULL);
    CHECK(ee_r32(&c, 0x1000) == 0);
    ee_free(&c);
    return 0;
}

int main(void)
{
    CHECK(good_stack(0x01ff8000, 0x8000, 0x01fffd60) == 0);
    CHECK(good_stack(0x4000, 0x1000, 0x4d60) == 0);
    CHECK(good_stack(0x4000, 0x2a0, 0x4000) == 0);
    CHECK(bad_stack(0x4000, 0x29f) == 0);
    CHECK(bad_stack(0x01ffff00, 0x400) == 0);
    CHECK(bad_stack(0xfffffff0, 0x1000) == 0);
    CHECK(bad_stack(0xffffffff, 0x8000) == 0);
    puts("PASS SetupThread explicit stack and context");
    return 0;
}
