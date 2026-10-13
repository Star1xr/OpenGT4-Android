/* Original HLE of the bounded protocol documented in docs/analysis/INIT_IOP.md.
 * This is not an IOP CPU emulator. Addresses and scheduling are test inputs.
 * Replies are queued, not injected into client state: the game's DMAC handler
 * and command handlers consume them. Unknown requests stop the experiment.
 */
#include "sif.h"
#include "thread.h"
#include <stdlib.h>
#include <string.h>

static uint32_t word(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void put(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static void require(ee_ctx *c, int ok, const char *why)
{ if (!ok) ee_end(c, EE_END_UNSUPPORTED, 0, "SIF: %s", why); }

int ee_sif_init(ee_ctx *c, int fresh, const uint8_t romver[14], int open_fail, unsigned delay)
{
    if (c->sif) return -1;
    c->sif = calloc(1, sizeof *c->sif);
    if (!c->sif) return -1;
    c->sif->iop = calloc(1, EE_IOP_SIZE);
    if (!c->sif->iop) { ee_sif_free(c); return -1; }
    c->sif->reg[0] = fresh ? 0 : EE_SIF_IOP_CMD;
    c->sif->reg[2] = fresh ? 0 : 1;
    c->sif->open_fail = open_fail;
    c->sif->delay = delay;
    memcpy(c->sif->romver, romver, 14);
    return 0;
}

void ee_sif_free(ee_ctx *c)
{
    if (c->sif) { free(c->sif->iop); free(c->sif); c->sif = NULL; }
}

static ee_sif_reply *reply(ee_ctx *c, uint32_t cmd, uint32_t size)
{
    ee_sif *s = c->sif;
    require(c, s->count < EE_SIF_QUEUE, "reply queue full");
    ee_sif_reply *q = &s->queue[(s->head + s->count++) % EE_SIF_QUEUE];
    memset(q, 0, sizeof *q);
    q->size = size;
    q->delay = s->delay;
    put(q->packet, size);
    put(q->packet + 8, cmd);
    return q;
}

static void rpc_end(ee_ctx *c, const uint8_t *p, const uint8_t *data, uint32_t len)
{
    ee_sif_reply *q = reply(c, 0x80000008, 0x40);
    put(q->packet + 0x1c, word(p + 0x1c));
    put(q->packet + 0x20, word(p + 8));
    if (word(p + 8) == 0x80000009) {
        put(q->packet + 0x24, EE_SIF_IOP_SERVER);
        put(q->packet + 0x28, EE_SIF_IOP_RECV);
    } else {
        require(c, len <= word(p + 0x2c) && len <= sizeof q->data, "RPC receive size");
        q->dest = word(p + 0x28);
        q->data_size = len;
        memcpy(q->data, data, len);
    }
}

static void trace_bytes(ee_ctx *c, const char *ev, const uint8_t *p, unsigned n)
{
    char hex[0x841];
    require(c, n <= (sizeof hex - 1) / 2, "trace packet too large");
    for (unsigned i = 0; i < n; i++) snprintf(hex + 2*i, 3, "%02x", p[i]);
    hex[2*n] = 0;
    ee_trace(c, "\"ev\":\"%s\",\"bytes\":\"%s\",\"status\":\"implemented\"", ev, hex);
}

static void fileio(ee_ctx *c, const uint8_t *p)
{
    ee_sif *s = c->sif;
    const uint8_t *a = s->iop + EE_SIF_IOP_RECV;
    uint32_t fno = word(p + 0x20), n = word(p + 0x24);
    uint8_t result[8] = {0};
    require(c, word(p + 0x34) == EE_SIF_IOP_SERVER, "unknown RPC server pointer");
    require(c, n <= 0x418, "fileio request size");
    ee_trace(c, "\"ev\":\"fileio\",\"fno\":%u,\"send_size\":%u", fno, n);
    trace_bytes(c, "fileio_request", a, n);
    if (fno == 0xff) {
        require(c, n == 8, "fileio init request");
        s->slots[0] = word(a); s->slots[1] = word(a + 4);
        memcpy(result, "3000", 4); put(result + 4, 2);
        rpc_end(c, p, result, 8);
        return;
    }
    require(c, s->slots[0] && s->slots[1], "fileio before initialization");
    require(c, (fno == 0 && n == 0x418) || (fno == 1 && n == 0x14) ||
               (fno == 2 && n == 0x20), "unsupported fileio operation");
    require(c, word(a + 8) == 4, "fileio result size");
    put(result, 1); /* immediate acceptance, actual result is asynchronous */
    rpc_end(c, p, result, 4);
    ee_sif_reply *q = reply(c, 0x80000011, 0x10);
    put(q->packet + 0xc, s->slot);
    q->dest = s->slots[s->slot]; s->slot ^= 1;
    q->data_size = fno == 2 ? 0xa4 : 0x20;
    put(q->data, word(a));
    put(q->data + 4, fno);
    put(q->data + 8, word(a + 4));
    put(q->data + 12, 4);
    if (fno == 0) {
        require(c, word(a + 12) == 1 && !memcmp(a + 20, "rom0:ROMVER", 12), "only ROMVER read supported");
        require(c, !s->opened, "file already open");
        s->opened = !s->open_fail;
        s->file_slot = word(a + 0x414); s->file_pos = 0;
        put(q->data + 16, s->open_fail ? (uint32_t)-2 : 1);
    } else {
        require(c, s->opened && word(a + 12) == 1 &&
                   word(a + (fno == 2 ? 0x1c : 0x10)) == s->file_slot, "invalid file handle/slot");
        if (fno == 1) { s->opened = 0; put(q->data + 16, 0); }
        else {
            /* Short, unaligned ROMVER read: FILEIO returns its leading fragment
             * through the completion record consumed by the type-2 handler.
             * General aligned bulk reads are deliberately not implemented. */
            uint32_t size = word(a + 0x14), dest = word(a + 0x10);
            require(c, size <= 14 && s->file_pos + size <= 14, "ROMVER read size");
            put(q->data + 16, size);
            put(q->data + 20, size);  /* leading bytes */
            put(q->data + 24, 0);     /* trailing bytes */
            put(q->data + 28, dest);
            memcpy(q->data + 36, s->romver + s->file_pos, size);
            s->file_pos += size;
        }
    }
}

static void command(ee_ctx *c, const uint8_t *p, uint32_t size)
{
    ee_sif *s = c->sif;
    require(c, size >= 16 && size <= 0x70 && p[0] >= 16 && p[0] <= size, "bad command size");
    uint32_t cmd = word(p + 8);
    trace_bytes(c, "sif_send", p, p[0]);
    switch (cmd) {
    case 0x80000000:
        require(c, p[0] == 0x14, "init command size");
        s->ee_recv = word(p + 16); break;
    case 0x80000002:
        if (word(p + 12) == 0) {
            require(c, p[0] == 0x14, "command init size");
            s->ee_recv = word(p + 16);
        } else {
            require(c, word(p + 12) == 1 && p[0] == 16, "RPC init option");
            ee_sif_reply *q = reply(c, 0x80000001, 24);
            put(q->packet + 16, 0); put(q->packet + 20, 1);
        }
        break;
    case 0x80000009:
        require(c, p[0] == 0x40 && word(p + 0x20) == 0x80000001, "unknown bind server");
        rpc_end(c, p, NULL, 0); break;
    case 0x8000000a:
        require(c, p[0] == 0x40, "RPC call size");
        fileio(c, p); break;
    default: ee_end(c, EE_END_UNSUPPORTED, 0, "SIF command 0x%08x", cmd);
    }
}

void ee_sif_interrupt_point(ee_ctx *c)
{
    ee_sif *s = c->sif;
    if (!s || !s->count || s->in_handler || !s->enabled || !s->chain ||
        !(c->cop0[EE_COP0_STATUS] & EE_STATUS_EIE)) return;
    ee_sif_reply *q = &s->queue[s->head];
    if (q->delay) { q->delay--; return; }
    require(c, s->ee_recv && s->handler, "missing EE receive buffer/handler");
    /* Save CPU context as an interrupt boundary; handler changes to memory,
     * semaphores and device state remain visible to the interrupted program. */
    ee_ctx saved = *c;
    s->in_handler = 1; s->chain = 0; s->chcr = 0; s->dstat |= 0x20;
    put(q->packet, q->size | (q->data_size << 8));
    put(q->packet + 4, q->data_size ? q->dest : 0);
    for (unsigned i = 0; i < q->data_size; i++) ee_w8(c, q->dest + i, q->data[i]);
    for (unsigned i = 0; i < q->size; i++) ee_w8(c, s->ee_recv + i, q->packet[i]);
    trace_bytes(c, "sif_receive", q->packet, q->size);
    ee_trace(c, "\"ev\":\"sif_irq\",\"handler\":\"0x%08x\",\"policy\":\"safe_points\",\"status\":\"stubbed\"", s->handler);
    s->head = (s->head + 1) % EE_SIF_QUEUE; s->count--; s->delivered++;
    c->r[4].ud[0] = 5;
    ee_call(c, s->handler, 0);
    memcpy(c->r, saved.r, sizeof c->r);
    c->hi = saved.hi; c->lo = saved.lo; c->hi1 = saved.hi1; c->lo1 = saved.lo1;
    c->sa = saved.sa; c->acc = saved.acc; c->fcr31 = saved.fcr31;
    memcpy(c->fpr, saved.fpr, sizeof c->fpr);
    memcpy(c->cop0, saved.cop0, sizeof c->cop0);
    s->dstat &= ~0x20u; s->in_handler = 0;
    ee_sched_after_interrupt(c);   /* a handler may have readied a better thread */
}

int ee_sif_syscall(ee_ctx *c, int num, int32_t *ret)
{
    ee_sif *s = c->sif;
    if (!s) return 0;
    uint32_t a = c->r[4].uw[0], b = c->r[5].uw[0];
    *ret = 0;
    switch (num) {
    case 0x12:
        require(c, a == 5, "only DMAC channel 5 supported");
        s->handler = b; *ret = ++c->k.next_handler_id; break;
    case 0x16:
        require(c, a == 5, "only DMAC channel 5 enable supported");
        *ret = !s->enabled; s->enabled = 1; break;
    case 0x78: case -0x78: s->chain = 1; s->chcr = 0x100; break;
    case 0x79:
        require(c, a >= 0x80000000 && a <= 0x80000002, "SIF set register");
        s->reg[a - 0x80000000] = b; *ret = b; break;
    case 0x7a:
        if (a >= 0x80000000 && a <= 0x80000002) *ret = s->reg[a - 0x80000000];
        else if (a == 4) *ret = 0x20000; /* test input: IOP already ready */
        else if (a == 2) *ret = EE_SIF_IOP_CMD;
        else require(c, 0, "SIF get register");
        break;
    case 0x77: case -0x77:
        require(c, b > 0 && b <= 16, "DMA descriptor count");
        for (unsigned i = 0; i < b; i++) {
            uint32_t d = a + 16*i;
            uint32_t src = ee_r32(c, d), dst = ee_r32(c, d+4), n = ee_r32(c, d+8), attr = ee_r32(c, d+12);
            require(c, dst < EE_IOP_SIZE && n <= EE_IOP_SIZE - dst, "DMA outside IOP RAM");
            require(c, attr == 0 || attr == 0x44, "DMA attributes");
            for (unsigned k = 0; k < n; k++) s->iop[dst+k] = ee_r8(c, src+k);
            if (dst == EE_SIF_IOP_CMD) command(c, s->iop + dst, n);
        }
        *ret = ++s->next_dma; break;
    default: return 0;
    }
    ee_trace(c, "\"ev\":\"sif_syscall\",\"num\":%d,\"args\":[%u,%u],\"ret\":%d,\"status\":\"implemented\"", num, a, b, *ret);
    return 1;
}

int ee_sif_mmio_read(ee_ctx *c, uint32_t a, uint32_t *v)
{
    if (!c->sif) return 0;
    if (a == 0x1000e010) *v = c->sif->dstat;
    else if (a == 0x1000c000) *v = c->sif->chcr;
    else return 0;
    ee_trace(c, "\"ev\":\"sif_mmio\",\"rw\":\"r\",\"addr\":%u,\"val\":%u", a, *v);
    return 1;
}
int ee_sif_mmio_write(ee_ctx *c, uint32_t a, uint32_t v)
{
    if (!c->sif) return 0;
    if (a == 0x1000e010) {
        c->sif->dstat &= ~(v & 0xffff); c->sif->dstat ^= v & 0xffff0000;
    } else if (a == 0x1000c000) c->sif->chcr = v;
    else return 0;
    ee_trace(c, "\"ev\":\"sif_mmio\",\"rw\":\"w\",\"addr\":%u,\"val\":%u", a, v);
    return 1;
}
