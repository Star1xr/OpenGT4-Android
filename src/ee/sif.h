/* Bounded SIF transport and FILEIO subset for the _init experiment. */
#ifndef OPENGT4_SIF_H
#define OPENGT4_SIF_H
#include "ee.h"
#define EE_IOP_SIZE (2u * 1024 * 1024)
#define EE_SIF_QUEUE 32
#define EE_SIF_IOP_CMD 0x10000u
#define EE_SIF_IOP_SERVER 0x11000u
#define EE_SIF_IOP_RECV 0x12000u

typedef struct {
    uint8_t packet[0x70], data[0x440];
    uint32_t size, dest, data_size, delay;
} ee_sif_reply;
struct ee_sif {
    uint8_t *iop;
    uint32_t reg[3], ee_recv, handler, dstat, chcr;
    int enabled, in_handler, next_dma, chain;
    ee_sif_reply queue[EE_SIF_QUEUE];
    unsigned head, count, delay, delivered;
    uint32_t slots[2], slot;
    int opened, open_fail;
    uint32_t file_slot, file_pos;
    uint8_t romver[14];
};
int ee_sif_init(ee_ctx *c, int fresh, const uint8_t romver[14], int open_fail, unsigned delay);
void ee_sif_free(ee_ctx *c);
int ee_sif_syscall(ee_ctx *c, int num, int32_t *ret);
int ee_sif_mmio_read(ee_ctx *c, uint32_t a, uint32_t *v);
int ee_sif_mmio_write(ee_ctx *c, uint32_t a, uint32_t v);
#endif
