#ifndef OPENGT4_DISC_CHECKPOINT_H
#define OPENGT4_DISC_CHECKPOINT_H
#include <stdint.h>
/* Serialized replay state at the app boundary; leaves the solver intact. */
void disc_checkpoint_restore(uint32_t conductor,unsigned index,const uint8_t state[0x14f4]);
#endif
