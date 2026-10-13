/* The course's runway (RNW4 block of /crs/<course>: road and wall
   collision, the kd tree, the course length) as the step reads it: the
   block copied to memory and its offsets turned into addresses
   (func_003E5818). */
#include "car.h"

/* add delta to a nonzero pointer field */
static void fix(uint32_t at, uint32_t delta)
{
    if (GT4_U32(at)) GT4_U32(at) = GT4_U32(at) + delta;
}

/* func_003E5818 (with func_003E6CE0, func_003E6CC0, func_003E6D00) */
void gt4_runway_relocate(uint32_t rnw)
{
    static const uint16_t fields[] = {
        0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84, 0x8c, 0x90, 0x9c,
    };
    uint32_t delta = rnw - GT4_U32(rnw + 4);
    if (!delta) return;
    GT4_U32(rnw + 4) = rnw;
    for (size_t k = 0; k < sizeof fields / sizeof fields[0]; k++) fix(rnw + fields[k], delta);
    for (uint32_t i = 0; i < GT4_U16(rnw + 0x38); i++) fix(GT4_U32(rnw + 0x58) + i * 8 + 4, delta);
    for (uint32_t i = 0; i < GT4_U16(rnw + 0x4a); i++) fix(GT4_U32(rnw + 0x7c) + i * 16 + 8, delta);
    for (uint32_t i = 0; i < GT4_U16(rnw + 0x54); i++) {
        uint32_t e = GT4_U32(rnw + 0x9c) + i * 32;
        fix(e + 0x14, delta);
        fix(e + 0x18, delta);
        fix(e + 0x1c, delta);
        fix(e + 0x10, delta);
        /* the original fixes the same word each time round */
        for (uint32_t j = 0; j < GT4_U16(e + 8); j++) fix(GT4_U32(e + 0x14) + 4, delta);
    }
}
