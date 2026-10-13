#ifndef OPENGT4_SPU_H
#define OPENGT4_SPU_H
/* The PS2 sound chip's voice and reverb as the PC plays them, 48 kHz.
   The game's driver decides the registers (0055DF70 packet: PITCH, VOLL,
   VOLR, SSA, ADSR1, ADSR2, key on/off, EON, reverb attributes); this
   module only does what the hardware does with them: ADPCM blocks with
   their loop flags, Gaussian interpolation, the ADSR generator, fixed
   volume and the reverb unit. No allocation, no file access. */
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *adpcm;           /* 16-byte SPU ADPCM blocks from SSA */
    size_t blocks;
} gt4_spu_wave;

enum { GT4_SPU_OFF, GT4_SPU_ATTACK, GT4_SPU_DECAY, GT4_SPU_SUSTAIN, GT4_SPU_RELEASE };

typedef struct {
    /* registers */
    const gt4_spu_wave *wave;
    uint16_t pitch, adsr1, adsr2;
    int16_t voll, volr;             /* 15-bit fixed volume as written (bits 14..0) */
    uint8_t reverb;                 /* EON bit */
    /* hardware state */
    uint8_t phase;
    int16_t envx;
    int32_t env_counter;
    uint32_t counter;               /* 12-bit fraction of the sample position */
    size_t block, loop;             /* current block, loop address (LSA) */
    int index, h1, h2;              /* sample within block, ADPCM history */
    int16_t decoded[28];
    int16_t ring[4];                /* the last four decoded samples, [3] newest */
} gt4_spu_voice;

/* One voice's registers as a driver publishes them. key_on/key_off are
   driver-wide serials, not independent per-voice counters: their order
   survives multiple driver ticks before the next host audio callback. */
typedef struct {
    const gt4_spu_wave *wave;
    uint16_t pitch, adsr1, adsr2;
    int16_t voll, volr;
    uint8_t reverb;
    uint32_t key_on, key_off;
} gt4_spu_regs;

/* KON: the envelope starts at zero in attack, decoding at SSA. */
void gt4_spu_key_on(gt4_spu_voice *voice);
/* KOFF: release from the current level. */
void gt4_spu_key_off(gt4_spu_voice *voice);
/* Adds `frames` stereo samples of the voice to dry (always) and, when
   EON is set, to wet. Both are int32 L/R interleaved. */
void gt4_spu_voice_render(gt4_spu_voice *voice, int32_t *dry, int32_t *wet, int frames);

/* Reverb unit: the 32 PS1-layout registers (dAPF1 .. vRIN) a reverb mode
   loads, the output volumes (EVOL) and its work area. */
typedef struct {
    uint16_t regs[32];
    int16_t evoll, evolr;
    int16_t *buffer;                /* caller-owned, `size` halfwords */
    uint32_t size, at;
    int32_t half[2];                /* left/right accumulated over a 48 kHz pair */
    int16_t out[2][2];              /* last two 24 kHz outputs per side */
    int phase;
} gt4_spu_reverb;

/* Reverb mode `mode` (sceSdSetEffectAttr numbering, 1..9) from the disc's
   /IRX/LIBSD.IRX image: the preset table that module loads. Returns 0
   when the image doesn't carry the table. */
int gt4_spu_reverb_preset(gt4_spu_reverb *reverb, const uint8_t *libsd, size_t size, unsigned mode);
/* Mixes the reverb's output for `frames` 48 kHz samples of `wet` into dry. */
void gt4_spu_reverb_render(gt4_spu_reverb *reverb, const int32_t *wet, int32_t *dry, int frames);

#endif
