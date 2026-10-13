/* The race's side of the game's sound driver (0055xxxx, US v2.00): effect
   voices, hardware voices by priority, ENGN slots and the registers. */
#include "sound_driver.h"
#include <stdlib.h>
#include <string.h>

typedef struct {                /* hardware voice, D_006518C8 +0xA8 */
    int used, kind, owner, slot;    /* owner: effect voice, or ENGN by pointer */
    gt4_sd_engine *engine;
    uint32_t age;               /* +0x14 */
    int32_t priority;           /* +0x34 */
    gt4_sd_voice f;
    const gt4_spu_wave *wave;
    uint32_t key_on, key_off;
} hw_voice;
typedef struct {                /* effect voice, D_00650500 */
    int state, hw, once;        /* +0x14, +0, +0x10 */
    uint32_t id;                /* +0x18 */
    const gt4_race_effect *tone;
    int16_t left, right;        /* +0x1C, +0x20 */
    int32_t priority, scale, pan;
} fx_voice;
struct gt4_sd {
    hw_voice hw[GT4_SD_VOICES];
    fx_voice fx[GT4_SD_EFFECTS];
    const uint8_t *pan, *octave;
    int32_t group[8];
    int stereo;
    uint32_t id, key_serial;
};
enum { OWN_NONE, OWN_FX, OWN_ENGINE };

gt4_sd *gt4_sd_open(const uint8_t *pan, const uint8_t *octave, const int32_t group[8], int stereo)
{
    gt4_sd *sd = calloc(1, sizeof *sd);
    if (!sd) return NULL;
    sd->pan = pan;
    sd->octave = octave;
    memcpy(sd->group, group, sizeof sd->group);
    sd->stereo = stereo;
    for (int i = 0; i < GT4_SD_EFFECTS; i++) sd->fx[i].hw = -1;
    return sd;
}
void gt4_sd_close(gt4_sd *sd) { free(sd); }

/* 0055D688 +0x10..+0x18 defers KOFF when KON (+0x28) is pending;
   0055DF70 +0x10C..+0x11C sends that KOFF on the next driver tick.
   A host callback may span both ticks: retain their order, not just counts. */
static void key_off(gt4_sd *sd, int h)
{
    if (h >= 0 && sd->hw[h].used) sd->hw[h].key_off = ++sd->key_serial;
}

/* 0055C648 / 0055D708: a hardware voice taken from its owner, told by the
   group's callback (group 5: 0055A498 -> 0055A1D0 frees a one-shot). A:
   a looping effect voice keeps the original's stale pointer only as
   "no voice"; it asks again on its next tick. */
static void release(gt4_sd *sd, int h)
{
    hw_voice *v = sd->hw + h;
    if (v->kind == OWN_FX) {
        fx_voice *x = sd->fx + v->owner;
        if (x->hw == h) {
            x->hw = -1;
            if (x->once) x->state = 0;
        }
    } else if (v->kind == OWN_ENGINE && v->engine->hw[v->slot] == h)
        v->engine->hw[v->slot] = -1;
    v->used = 0;
    v->kind = OWN_NONE;
}

/* 0055D0B0: a free voice (0055C758), else the lowest priority not above
   the request, the older on a tie (0055C9A0 with 0055D070), taken away
   from its owner (0055D028: 0055D708 reason 6). */
static int allocate(gt4_sd *sd, int32_t priority)
{
    for (int i = 0; i < GT4_SD_VOICES; i++)
        if (!sd->hw[i].used) return i;
    int best = -1;
    int32_t bp = priority;
    uint32_t ba = 0;
    for (int i = 0; i < GT4_SD_VOICES; i++) {
        hw_voice *v = sd->hw + i;
        if (v->priority < bp || (v->priority == bp && !(v->age < ba))) {
            best = i;
            bp = v->priority;
            ba = v->age;
        }
    }
    if (best >= 0) {
        key_off(sd, best);
        release(sd, best);
    }
    return best;
}

static int take(gt4_sd *sd, int h, int kind, int owner, gt4_sd_engine *engine, int slot, int32_t priority)
{
    hw_voice *v = sd->hw + h;
    uint32_t on = v->key_on, off = v->key_off;
    memset(v, 0, sizeof *v);
    v->key_on = on;
    v->key_off = off;
    v->used = 1;
    v->kind = kind;
    v->owner = owner;
    v->engine = engine;
    v->slot = slot;
    v->priority = priority;
    v->f.master = 0x3fff;
    v->f.group2 = v->f.v58 = v->f.v5c = v->f.v60 = 127;
    v->f.pan = v->f.pan6c = v->f.pan70 = 64;
    v->f.bend = 0x40;
    v->f.level = 0x3fff;
    return h;
}

/* 0055A5F0: a free effect voice, else the lowest priority below the
   request, stopped (0055A340 keys its voice off). */
static int effect_voice(gt4_sd *sd, int32_t priority)
{
    for (int i = 0; i < GT4_SD_EFFECTS; i++)
        if (!sd->fx[i].state) return i;
    int best = -1;
    int32_t bp = priority;
    for (int i = 0; i < GT4_SD_EFFECTS; i++)
        if (sd->fx[i].priority < bp) {
            bp = sd->fx[i].priority;
            best = i;
        }
    if (best >= 0) {
        fx_voice *x = sd->fx + best;
        if (x->hw >= 0) {
            key_off(sd, x->hw);
            sd->hw[x->hw].kind = OWN_NONE;
        }
        x->hw = -1;
        x->state = 0;
    }
    return best;
}

void gt4_sd_play(gt4_sd *sd, gt4_sd_handle *h, const gt4_race_effect *tone, int once,
                 float left, float right, float pitch, int32_t priority, int32_t pan)
{
    if (!sd || !h || !tone || !tone->sample.wave.blocks) return;
    fx_voice *x = h->voice >= 0 ? sd->fx + h->voice : NULL;
    if (!x || !x->state || x->id != h->id) {
        /* +0x88..+0xE0 */
        int v = effect_voice(sd, priority);
        if (v < 0) return;
        x = sd->fx + v;
        memset(x, 0, sizeof *x);
        x->hw = -1;
        x->tone = tone;
        x->once = once;
        x->id = ++sd->id;
        x->state = 2;
        h->voice = v;
        h->id = x->id;
    }
    /* +0x3C..+0x74 and 0055A2B8: cvt.w of x16383 and x4096 */
    x->left = (int16_t)(int32_t)(left * 16383.0f);
    x->right = (int16_t)(int32_t)(right * 16383.0f);
    x->scale = (int32_t)(pitch * 4096.0f);
    x->priority = priority;
    x->pan = pan;
    x->state = 3;
}

void gt4_sd_note(gt4_sd *sd, const gt4_race_effect *tone, unsigned note, unsigned velocity,
                 float left, float right, float pitch, int32_t priority, int32_t pan)
{
    if (!sd || !tone || !tone->sample.wave.blocks) return;
    int h = allocate(sd, priority);
    if (h < 0) return;
    take(sd, h, OWN_NONE, 0, NULL, 0, priority);
    hw_voice *v = sd->hw + h;
    gt4_sd_voice_tone(&v->f, tone, (int16_t)(int32_t)(left * 16383.0f), (int16_t)(int32_t)(right * 16383.0f),
                      (int32_t)(pitch * 4096.0f), pan, 0);
    /* 0055A8B0: group 2, note * 16, +0x60 the velocity, +0x50 the bank's
       volume; +0x4C the sequence's 3FFFh (0055A748) */
    v->f.group = 2;
    v->f.note = (int32_t)note * 16;
    v->f.v60 = (int32_t)velocity;
    v->f.group2 = 127;
    v->f.pitch = gt4_sd_pitch(&v->f, sd->octave);
    v->wave = &tone->sample.wave;
    v->key_on = ++sd->key_serial;
}

void gt4_sd_stop(gt4_sd *sd, gt4_sd_handle *h)
{
    if (!sd || !h || h->voice < 0) return;
    fx_voice *x = sd->fx + h->voice;
    if (x->state && x->id == h->id) {
        if (x->hw >= 0) {
            key_off(sd, x->hw);
            sd->hw[x->hw].kind = OWN_NONE;
        }
        x->hw = -1;
        x->state = 0;
    }
    h->voice = -1;
}

/* 00558FD8 +0x148..+0x310 */
void gt4_sd_engine_tick(gt4_sd *sd, gt4_sd_engine *e, int rpm, int16_t left, int16_t right,
                        int32_t scale, int32_t priority, int reverb_off, const int16_t *sine)
{
    if (!sd || !e || !e->bank) return;
    gt4_engine_control old = e->control;
    int voiced[2] = { e->hw[0] >= 0, e->hw[1] >= 0 };
    gt4_engine_control_step_ex(e->bank, &e->control, rpm, left, right, sine, voiced);
    for (int k = 0; k < 2; k++) {
        gt4_engine_voice *c = e->control.voice + k;
        int keep = c->layer == old.voice[k].layer && e->hw[k] >= 0;
        if (!keep) {
            if (c->layer != old.voice[k].layer && e->hw[k] >= 0) {
                key_off(sd, e->hw[k]);
                sd->hw[e->hw[k]].kind = OWN_NONE;
                e->hw[k] = -1;
            }
            if (c->layer < 0) continue;
            int h = allocate(sd, priority);
            if (h < 0) continue;
            take(sd, h, OWN_ENGINE, 0, e, k, priority);
            hw_voice *v = sd->hw + h;
            /* +0x274..+0x2B0: group 4, ADSR 0003h/000Ah, the layer's sample */
            v->f.group = 4;
            v->f.adsr1 = 3;
            v->f.adsr2 = 0xa;
            v->wave = &e->bank->layers[c->layer].sample.wave;
            v->key_on = ++sd->key_serial;
            e->hw[k] = h;
        }
        if (c->layer < 0 || e->hw[k] < 0) continue;
        hw_voice *v = sd->hw + e->hw[k];
        v->f.pitch = (uint16_t)c->pitch;
        v->f.scale = scale;
        v->f.left = c->left;
        v->f.right = c->right;
        v->priority = priority;
        v->f.reverb = !reverb_off;
    }
}

void gt4_sd_engine_stop(gt4_sd *sd, gt4_sd_engine *e)
{
    for (int k = 0; k < 2; k++)
        if (e->hw[k] >= 0) {
            key_off(sd, e->hw[k]);
            sd->hw[e->hw[k]].kind = OWN_NONE;
            e->hw[k] = -1;
        }
    gt4_engine_control_init(&e->control);
}

void gt4_sd_tick(gt4_sd *sd, const uint8_t ended[GT4_SD_VOICES], gt4_spu_regs regs[GT4_SD_VOICES])
{
    /* 0055D890 +0x2C..+0x90: voices whose sound is over go back */
    for (int i = 0; i < GT4_SD_VOICES; i++) {
        hw_voice *v = sd->hw + i;
        if (!v->used) continue;
        if (v->age++ && ended && ended[i]) release(sd, i);
    }
    /* 00559EB0: each playing effect voice */
    for (int i = 0; i < GT4_SD_EFFECTS; i++) {
        fx_voice *x = sd->fx + i;
        if (x->state != 3) continue;
        if (x->hw < 0) {
            int h = allocate(sd, x->priority);
            if (h < 0) continue;
            take(sd, h, OWN_FX, i, NULL, 0, x->priority);
            hw_voice *v = sd->hw + h;
            gt4_sd_voice_tone(&v->f, x->tone, x->left, x->right, x->scale, x->pan, 0);
            v->f.pitch = gt4_sd_pitch(&v->f, sd->octave);
            v->wave = &x->tone->sample.wave;
            v->key_on = ++sd->key_serial;
            x->hw = h;
        }
        hw_voice *v = sd->hw + x->hw;
        v->f.left = x->left;
        v->f.right = x->right;
        v->priority = x->priority;
        v->f.scale = x->scale;
        v->f.pan = x->pan;
    }
    /* 0055DF70: the registers */
    for (int i = 0; i < GT4_SD_VOICES; i++) {
        hw_voice *v = sd->hw + i;
        gt4_spu_regs *r = regs + i;
        r->key_on = v->key_on;
        r->key_off = v->key_off;
        if (!v->used && v->kind == OWN_NONE && !v->wave) continue;
        int16_t lr[2];
        gt4_sd_volume(&v->f, sd->group[v->f.group & 7], sd->stereo, sd->pan, lr);
        r->wave = v->wave;
        r->voll = (int16_t)(lr[0] & 0x7fff);
        r->volr = (int16_t)(lr[1] & 0x7fff);
        r->pitch = gt4_sd_pitch_register(&v->f);
        r->adsr1 = v->f.adsr1;
        r->adsr2 = v->f.adsr2;
        r->reverb = v->f.reverb;
    }
}
