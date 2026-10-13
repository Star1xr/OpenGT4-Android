/* The race's sound on the PC: each car's sound (race_car_sound.c, the
   game's 003B5E80/00392640), the game's sound driver (sound_driver.c) and
   the SPU2's voices and reverb (spu.c) behind the existing SDL producer.
   One driver tick per race frame, as the game. Decoding and file IO happen
   before the callback is installed; the callback neither allocates nor
   touches the race's memory. */
#include "race_audio.h"
#include "career_license.h"
#include "race_sound.h"
#include "race_car_sound.h"
#include "sound_driver.h"
#include "spu.h"
#include "pcm_music.h"
#include "save.h"
#include "car.h"
#include "platform/platform.h"
#include <SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum { CARS = 6, BLOCK = 512, VOICES = GT4_SD_VOICES, REVERB = 1 << 17 };
struct race_audio {
    gt4_vol *vol;
    gt4_race_effects *effects;
    gt4_engine_bank *bank[CARS][3], *road;
    gt4_sd *sd;
    gt4_sd_engine engine[CARS][3];
    gt4_sd_handle tone[CARS][GT4_CAR_TONES], ambient[10];
    gt4_sd_handle refueling;
    const gt4_race_effect *tone_effect[GT4_CAR_TONES], *effect_by_enum[80];
    gt4_car_sound_state state[CARS];
    gt4_car_sound_setup setup;
    gt4_ambient_state ambient_state;
    int16_t depth;
    /* gt4count.ins effect sequences: count_d (0), count_s (1), chime (9) */
    struct { unsigned count; struct { uint32_t at; uint8_t note, velocity; gt4_race_effect tone; } note[8]; } sequence[6];
    struct { uint32_t due; const gt4_race_effect *tone; uint8_t note, velocity; float level; } pending[16];
    double hold;           /* the pause's time not yet ticked */
    int pendings;
    uint32_t clock240;
    int last_count;
    float camera_eye[3], camera_rows[9];
    int camera_view, have_camera, have_setup;
    int cars, playing, frames;
    float eye_previous[3];
    int have_eye;
    uint32_t random;
    gt4_car_backfire backfire[6];unsigned exhaust_count[6];
    float sdvol, sdvol_music;
    int16_t sine[4096];
    /* the registers the callback plays, and what it reports back */
    SDL_mutex *mutex;
    gt4_spu_regs regs[VOICES];
    uint8_t ended[VOICES];
    uint32_t ended_on[VOICES];
    /* callback side */
    gt4_spu_voice voice[VOICES];
    uint32_t seen_on[VOICES], seen_off[VOICES];
    gt4_spu_reverb reverb[2];
    int16_t *reverb_area;
    float music_volume;
    int bgm_option;
    /* music, as before */
    gt4_pcm_music *music;
    const char *disc;
    SDL_Thread *music_worker;
    SDL_atomic_t music_ready, music_ended;
    gt4_pcm_music *prepared;
    int requested_music, worker_music, active_music;
    int music_serial;      /* a new stream each time one starts */
    gt4_music_cue cue;
    uint32_t random_state, worker_random;
    gt4_music_playlist playlist;
    int playlist_ready, playlist_changed, playlist_preferences, worker_sequence;
    int start_frames, finished, replay;
    /* 0033D568 +0x16C..0x1F8: the playlist record (+0x18, its +8 title and
       +0xC artist) copied only when a racing stream (enum 12) starts; the
       jingles leave it. 0038AB98 hands it (0033C450 -> 0033D950) to the
       display's 003A22A0 every frame. */
    char record_title[96], record_artist[96];
};

static void completed_voices(const race_audio *a,uint8_t ended[VOICES])
{
    for(int i=0;i<VOICES;i++)ended[i]=a->ended[i] && a->ended_on[i]==a->regs[i].key_on;
}
static void render(int16_t *out, int frames, void *user)
{
    race_audio *a = user;
    gt4_spu_regs regs[VOICES];
    float music_volume;
    SDL_LockMutex(a->mutex);
    memcpy(regs, a->regs, sizeof regs);
    music_volume = a->music_volume;
    SDL_UnlockMutex(a->mutex);
    for (int i = 0; i < VOICES; i++) {
        gt4_spu_voice *v = a->voice + i;
        const gt4_spu_regs *r = regs + i;
        v->pitch = r->pitch;
        v->adsr1 = r->adsr1;
        v->adsr2 = r->adsr2;
        v->voll = r->voll;
        v->volr = r->volr;
        v->reverb = r->reverb;
        /* 0055D688 +0x10..+0x18 / 0055DF70 +0x10C..+0x11C: KOFF
           after a pending KON is deferred to the next tick. The SDL
           callback can miss either tick; honour the last command too. */
        int on = r->key_on != a->seen_on[i], off = r->key_off != a->seen_off[i];
        int off_last = on && off && (int32_t)(r->key_off - r->key_on) > 0;
        if (off && !off_last) {
            a->seen_off[i] = r->key_off;
            gt4_spu_key_off(v);
        }
        if (on) {
            a->seen_on[i] = r->key_on;
            v->wave = r->wave;
            gt4_spu_key_on(v);
        }
        if (off_last) {
            a->seen_off[i] = r->key_off;
            gt4_spu_key_off(v);
        }
    }
    while (frames > 0) {
        int n = frames > BLOCK ? BLOCK : frames;
        int32_t dry[BLOCK * 2] = { 0 }, wet[2][BLOCK * 2] = { { 0 } };
        int16_t music[BLOCK * 2];
        /* core 0 holds voices 0..23, core 1 the rest; each core its own
           reverb unit */
        for (int i = 0; i < VOICES; i++) gt4_spu_voice_render(a->voice + i, dry, wet[i >= 24], n);
        for (int c = 0; c < 2; c++) gt4_spu_reverb_render(a->reverb + c, wet[c], dry, n);
        SDL_LockMutex(a->mutex);
        gt4_pcm_music_render(music, n, a->music);
        if (gt4_pcm_music_ended(a->music)) SDL_AtomicSet(&a->music_ended, 1);
        SDL_UnlockMutex(a->mutex);
        for (int i = 0; i < n * 2; i++) {
            int64_t v = dry[i] + (int64_t)(music[i] * music_volume);
            out[i] = (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
        }
        out += n * 2;
        frames -= n;
    }
    SDL_LockMutex(a->mutex);
    for (int i = 0; i < VOICES; i++) {
        a->ended_on[i]=a->seen_on[i];
        a->ended[i] = a->voice[i].phase == GT4_SPU_OFF && a->seen_on[i] == a->regs[i].key_on;
    }
    SDL_UnlockMutex(a->mutex);
}

int race_audio_music_title(race_audio *a, char *title, size_t title_size, char *artist, size_t artist_size)
{
    const char *t, *r;
    int ok;
    if (!a) return 0;
    SDL_LockMutex(a->mutex);
    t = a->record_title;
    r = a->record_artist;
    ok = *t != 0;
    if (ok) { snprintf(title, title_size, "%s", t); snprintf(artist, artist_size, "%s", r); }
    SDL_UnlockMutex(a->mutex);
    return ok;
}

float race_audio_music_cue(race_audio *a, int *passed)
{
    const uint32_t *cues = NULL;
    unsigned n = 0, rate = 0;
    float now = 0, next;
    *passed = 0;
    if (!a) return NAN;
    SDL_LockMutex(a->mutex);
    if (a->music) { n = gt4_pcm_music_cues(a->music, &cues, &rate); now = (float)gt4_pcm_music_seconds(a->music); }
    next = gt4_music_cue_step(&a->cue, n ? cues : NULL, n, rate, now, a->music ? a->music_serial : 0, passed);
    SDL_UnlockMutex(a->mutex);
    return next;
}

static int fetch_row(uint32_t garage, uint32_t slot, uint8_t row[256])
{
    uint32_t low = GT4_U32(garage + slot), high = GT4_U32(garage + slot + 4);
    memset(row, 0, 256);
    return gt4_phys_specdb && !(low == UINT32_MAX && high == UINT32_MAX) &&
           gt4_phys_specdb->row(gt4_phys_specdb->ctx, high, low, 0, row, 256) > 0;
}

static const gt4_race_effect *by_name(race_audio *a, const char *name) { return gt4_race_effect_find(a->effects, name); }

/* An effect bank's sequence: Se A0 note velocity program, then a VLQ
   delay at 240 ticks a second (0055AD20's 0x43700000), FF 2F the end
   (0055B1A0). */
static void load_sequence(race_audio *a, int q, const char *path, unsigned which)
{
    size_t n;
    uint8_t *b = gt4_vol_read(a->vol, path, &n);
    if (!b || n < 0x48) {
        free(b);
        return;
    }
    size_t st = 0x20u + (b[0x3c] | b[0x3d] << 8 | (size_t)b[0x3e] << 16);
    if (st + 2 + 2 * which + 2 <= n) {
        size_t entry = st + (b[st + 2 + 2 * which] | b[st + 3 + 2 * which] << 8);
        size_t pos = entry + 4 <= n ? st + (b[entry + 2] | b[entry + 3] << 8) : n;
        uint32_t at = 0;
        while (pos + 4 <= n && a->sequence[q].count < 8 && b[pos] == 0xa0) {
            unsigned note = b[pos + 1], velocity = b[pos + 2], program = b[pos + 3];
            pos += 4;
            uint32_t delay = 0;
            for (int k = 0; k < 4 && pos < n; k++) {
                unsigned c = b[pos++];
                delay = delay << 7 | (c & 127);
                if (!(c & 128)) break;
            }
            unsigned i = a->sequence[q].count;
            if (gt4_race_tone_open(&a->sequence[q].note[i].tone, b, n, program, note)) {
                a->sequence[q].note[i].at = at;
                a->sequence[q].note[i].note = (uint8_t)note;
                a->sequence[q].note[i].velocity = (uint8_t)velocity;
                a->sequence[q].count++;
            }
            at += delay;
        }
    }
    free(b);
}

/* 00336DE8 seeds the world jukebox with D_006464DC, 12345678h only when
   it is zero; the captures hold a different value each boot (8C507570h,
   8C579D70h). A: its writer is not found, the PC clock stands in, once
   per run. User-requested replay shuffle also mixes the opening clock. */
static uint32_t boot_seed(void)
{
    static uint32_t seed;
    if(!seed){uint64_t t=plat_time_ns();seed=(uint32_t)(t^(t>>32));if(!seed)seed=0x12345678;}
    return seed;
}

race_audio *race_audio_open_replay(const char *disc, const gt4_race_run *run, const gt4_course *course,int replay)
{
    if (!run || !gt4_phys_mem) return NULL;
    race_audio *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    const char *error;
    a->mutex = SDL_CreateMutex();
    a->disc = disc;
    a->replay = replay;
    a->requested_music = a->worker_music = a->active_music = -1;
    a->start_frames = -1;
    a->music_volume = 0;
    a->vol = gt4_vol_open(disc, &error);
    a->cars = gt4_race_run_cars(run);
    if (!a->mutex || !a->vol || a->cars < 1 || a->cars > CARS) goto fail;
    /* 0033C180 +0x1C..+0x80 restores the complete option jukebox;
       00336DE8 supplies 12345678h when no deterministic seed is set. */
    a->playlist_preferences=gt4_music_playlist_uses_preferences(replay?2:0,0);
    a->playlist_ready=gt4_save_option_blob_get("bgm_playlist_ps2",a->playlist.bytes,sizeof a->playlist.bytes);
    if(a->playlist_ready) {
        uint32_t count;memcpy(&count,a->playlist.bytes,4);
        if(count>128)a->playlist_ready=0;
    }
    if(!a->playlist_ready)a->playlist_ready=gt4_music_playlist_default(&a->playlist,a->vol,boot_seed());
    /* User request (P): replay shuffles the race's enabled library; do not
       substitute the 0033C180 fallback of 50 tracks. Sequential US profiles
       also become shuffled. Original ordering math: 00437E98/00437F58.
       A: native clock supplies the seed, not the PS2 timer. */
    if(a->playlist_ready && (replay || !a->playlist.bytes[6])) {
        uint64_t clock=plat_time_ns();uint32_t seed=boot_seed()^(uint32_t)(clock^(clock>>32));
        a->playlist.bytes[8]=(uint8_t)seed;a->playlist.bytes[9]=(uint8_t)(seed>>8);
        gt4_music_playlist_order(&a->playlist,1);
        if(!replay)a->playlist_changed=1;
    }
    a->effects = gt4_race_effects_open(a->vol);
    if (!a->effects) goto fail;
    (void)course;
    gt4_ambient_state_reset(&a->ambient_state);
    a->depth = 0xfff;
    memcpy(a->sine, gt4_ptr(0x64d3f8), sizeof a->sine);
    /* GTSE order is the effect enum (D_00623A78, 004632A0) */
    {
        size_t n;
        uint8_t *names = gt4_vol_read(a->vol, "/sound/gt4se.inf", &n);
        if (names && n >= 16) {
            unsigned count = names[12] | names[13] << 8;
            for (unsigned i = 0; i < count && i < 80 && 16 + i * 8 + 8 <= n; i++) {
                unsigned at = names[16 + i * 8] | names[17 + i * 8] << 8 | names[18 + i * 8] << 16;
                if (at < n && memchr(names + at, 0, n - at)) a->effect_by_enum[i] = by_name(a, (const char *)names + at);
            }
        }
        free(names);
    }
    for (int k = 0; k < 5; k++) a->tone_effect[k] = a->effect_by_enum[gt4_car_tone_effect[k]];
    for (int w = 0; w < 4; w++)
        for (int k = 0; k < 11; k++) a->tone_effect[GT4_CAR_WHEEL0 + w * 11 + k] = a->effect_by_enum[gt4_car_wheel_effect[k]];
    /* 00391CD8: sdvol.dat's output mode 1 (stereo, D_006518C8), column 2
       for effects; 0045FFC0: column 0 for the PCM stream */
    {
        size_t bytes;
        uint8_t *sd = gt4_vol_read(a->vol, "/sound/sdvol.dat", &bytes);
        if (sd && bytes >= 0x14 && !memcmp(sd, "SDVL", 4)) {
            uint32_t cols = sd[0x10] | sd[0x11] << 8;
            size_t at = 0x14 + (cols * 1 + 2) * 4;
            if (at + 4 <= bytes) memcpy(&a->sdvol, sd + at, 4);
            if (at - 8 + 4 <= bytes) memcpy(&a->sdvol_music, sd + at - 8, 4);
        }
        free(sd);
        if (!(a->sdvol > 0) || !(a->sdvol_music > 0)) goto fail;
    }
    /* group volumes 4 (ENGN) and 5 (tones) at 3FFFh, stereo: the
       manager's state in every capture (D_006518C8 +0x4C + group*12, +0) */
    {
        int32_t group[8] = { 0, 7065, 0x3fff, 0, 0x3fff, 0x3fff, 0, 0 };
        a->sd = gt4_sd_open((const uint8_t *)gt4_ptr(0x006c8ac0), (const uint8_t *)gt4_ptr(0x00654738), group, 1);
        if (!a->sd) goto fail;
    }
    /* reverb: mode 4 (Studio C) at depth 0FFFh on both cores, as the
       driver's attribute block holds it (D_006518C8 +0x10/+0x14, every
       capture); the preset from the disc's LIBSD.IRX */
    {
        size_t bytes;
        uint8_t *irx = gt4_disc_read_file(disc, "IRX/LIBSD.IRX", &bytes);
        a->reverb_area = calloc(2 * REVERB, sizeof *a->reverb_area);
        for (int c = 0; c < 2 && irx && a->reverb_area; c++)
            if (gt4_spu_reverb_preset(a->reverb + c, irx, bytes, 4)) {
                a->reverb[c].evoll = a->reverb[c].evolr = 0x0fff;
                a->reverb[c].buffer = a->reverb_area + c * REVERB;
                a->reverb[c].size = REVERB;
            }
        free(irx);
    }
    uint32_t cond = gt4_race_run_conductor(run), race = GT4_U32(cond);
    uint32_t entries = GT4_U32(GT4_U32(race + 0x60) + 8);
    size_t road_size;
    uint8_t *road = gt4_vol_read(a->vol, "/sound/roadnoiz.es", &road_size);
    if (road) a->road = gt4_engine_bank_decode(road, road_size, 0);
    free(road);
    for (int i = 0; i < a->cars; i++) {
        uint32_t garage = GT4_U32(entries + i * 4) + 0x20;
        uint8_t row[256];
        for (int k = 0; k < 3; k++) {
            a->engine[i][k].hw[0] = a->engine[i][k].hw[1] = -1;
            gt4_engine_control_init(&a->engine[i][k].control);
        }
        for (int k = 0; k < GT4_CAR_TONES; k++) a->tone[i][k].voice = -1;
        gt4_car_sound_state_reset(a->state + i);
        gt4_car_backfire_init(a->backfire+i);
        /* 00445518: ENGINE row +0x18 soundNum; 00445548: MUFFLER row +6
           picks normal0..3; 00445650: TURBINE row +9 adds 4. 003923D0:
           controller +0x28 the engine bank, +0x2C roadnoiz.es, +0x2CC the
           exhaust (folder bank). */
        if (!fetch_row(garage, 0x38, row)) continue;
        unsigned id = row[24] | (unsigned)row[25] << 8;
        int variant = 0;
        if (fetch_row(garage, 0xd0, row)) variant = row[6];
        if (fetch_row(garage, 0xb0, row) && row[9]) variant += 4;
        a->bank[i][0] = gt4_engine_bank_open(a->vol, id, -1);
        a->bank[i][1] = a->road;
        a->bank[i][2] = gt4_engine_bank_open(a->vol, id, variant);
        for (int k = 0; k < 3; k++) a->engine[i][k].bank = a->bank[i][k];
        printf("race audio: car %d engine=%05u variant=%d layers=%u/%u/%u\n", i, id, variant,
               a->bank[i][0] ? a->bank[i][0]->count : 0, a->road ? a->road->count : 0,
               a->bank[i][2] ? a->bank[i][2]->count : 0);
    }
    for (int k = 0; k < 10; k++) a->ambient[k].voice = -1;
    a->refueling.voice=-1;
    /* 00463098 installs gt4count.ins as bank 2: count_d, count_s, chime;
       the menus' "ok", "cursor", "cancel" from the system bank (GTSE:
       bank 0, sequences 6, 2, 1) */
    load_sequence(a, 0, "/sound/gt4count.ins", 0);
    load_sequence(a, 1, "/sound/gt4count.ins", 1);
    load_sequence(a, 2, "/sound/gt4count.ins", 9);
    load_sequence(a, 3, "/sound/gt4sys.ins", 6);
    load_sequence(a, 4, "/sound/gt4sys.ins", 2);
    load_sequence(a, 5, "/sound/gt4sys.ins", 1);
    a->last_count = -1;
    if (getenv("OPENGT4_SOUND_TRACE"))
        for (int q = 0; q < 6; q++)
            fprintf(stderr, "race sound: sequence %d has %u notes (first %u blocks)\n", q, a->sequence[q].count,
                    a->sequence[q].count ? (unsigned)a->sequence[q].note[0].tone.sample.wave.blocks : 0);
    a->random = 0x003b5e80u;
    fflush(stdout);
    return a;
fail:
    race_audio_close(a);
    return NULL;
}

static const char *music_group(int request)
{
    /* 00461C00/00461C28: PCM enums 10..36, table 006238C0. */
    static const char *groups[] = { "start", "goal", "racing", "default_quick", "result",
        "torophy_solo", "torophy_champ", "get_car", "replay_theater", "license_start",
        "license_goal", "license_gold", "license_silver", "license_bronze", "license_failed",
        "license_torophy", "autodemo", "champ_start", "champ_goal", "timeattack_start",
        "timeattack_goal", "coffee_start", "coffee_goal", "etc_start", "etc_goal", "last_start", "last_goal" };
    return request >= 10 && request <= 36 ? groups[request - 10] : NULL;
}
static int prepare_music(void *user)
{
    race_audio *a = user;
    const char *error = NULL;
    gt4_vol *v = gt4_vol_open(a->disc, &error);
    const char *group = music_group(a->worker_music);
    unsigned count = v && group ? gt4_pcm_music_count(v,group) : 0;
    int sequence=a->worker_music==12?a->worker_sequence:(count?(int)(a->worker_random%count):-1);
    a->prepared = count && sequence>=0 ? gt4_pcm_music_open(v, group, (unsigned)sequence) : NULL;
    gt4_pcm_music_once(a->prepared, 1);
    if (v) gt4_vol_close(v);
    SDL_AtomicSet(&a->music_ready, 1);
    return 0;
}
static void music_update(race_audio *a, const gt4_race_run *run)
{
    uint32_t race = GT4_U32(gt4_race_run_conductor(run)), value;
    /* 00389758 +0xD4..+0x1AC consumes event 0x18. */
    while (gt4_race_queue_take(race + 0xf4, 0x18, &value, 1))
        if (music_group((int)value)) a->requested_music = (int)value;
    /* User request: replay plays racing enum 12 from its opening. Native
       replay reuses the race session; ignore that session's start/goal BGM. */
    if(a->replay)a->requested_music=12;
    int frames = (int)GT4_U32(race + 0xbc);
    /* 00389D48 +0x24..+0x40 emits music 12 at the standing start. */
    int32_t license_state;int license=gt4_race_run_license_result(run,&license_state,NULL);
    /* LicenseDisplay (003BCC68) does not use the Arcade start callback
       00389D48. Keep its queued license_start (003C0208), enum 19. */
    if (!license && a->start_frames > 0 && !frames) a->requested_music = 12;
    a->start_frames = frames;
    int finished = gt4_race_run_finished(run);
    /* 003A0228 +0x18..+0x50: first finish -> goal music enum 11. */
    if (finished && !a->finished && !a->replay)
        a->requested_music = license?gt4_career_license_music(license_state):11; /* 003E1C80/003E1EA0 */
    a->finished = finished;
    if (a->active_music == 12 && SDL_AtomicGet(&a->music_ended)) {
        /* 0033D170 +0x2BC..+0x2FC: next racing stream at stream end. */
        a->active_music = -1;
        a->requested_music = 12;
    }
    if (a->music_worker) {
        if (!SDL_AtomicGet(&a->music_ready)) return;
        SDL_WaitThread(a->music_worker, NULL);
        a->music_worker = NULL;
        gt4_pcm_music *old = NULL;
        if (a->worker_music == a->requested_music) {
            SDL_LockMutex(a->mutex);
            old = a->music;
            a->music = a->prepared;
            a->prepared = NULL;
            if (a->worker_music == 12) {
                const char *artist, *title = gt4_pcm_music_title(a->music, &artist);
                snprintf(a->record_title, sizeof a->record_title, "%s", title);
                snprintf(a->record_artist, sizeof a->record_artist, "%s", artist);
            }
            SDL_AtomicSet(&a->music_ended, 0);
            a->music_serial++;
            SDL_UnlockMutex(a->mutex);
            a->active_music = a->worker_music;
            printf("race music: PCM/%s\n", music_group(a->active_music));
            fflush(stdout);
        }
        gt4_pcm_music_close(old);
        gt4_pcm_music_close(a->prepared);
        a->prepared = NULL;
    }
    if (a->active_music == a->requested_music) return;
    /* 0033D568 -> 00462188 -> 0045FFF8: stop the previous stream when
       the request changes, without a made-up crossfade during decode. */
    SDL_LockMutex(a->mutex);gt4_pcm_music *previous=a->music;a->music=NULL;SDL_UnlockMutex(a->mutex);
    gt4_pcm_music_close(previous);
    a->worker_music = a->requested_music;
    if(a->worker_music==12) {
        /* 0033D568 +0x16C -> 00438170, followed by 00462288 ID lookup. */
        int entry=a->playlist_ready?gt4_music_playlist_next(&a->playlist):-1;
        a->worker_sequence=entry<0?-1:gt4_music_playlist_sequence(a->vol,&a->playlist,(unsigned)entry);
        a->playlist_changed=1;
        if(getenv("OPENGT4_SOUND_TRACE"))fprintf(stderr,"race music: playlist entry %d, racing sequence %d\n",entry,a->worker_sequence);
    }
    /* 0045FFF8 +0x48. A: PC timer replaces PS2 GetSystemTime. */
    a->worker_random = gt4_pcm_music_random(&a->random_state, (uint32_t)plat_time_ns());
    SDL_AtomicSet(&a->music_ready, 0);
    a->music_worker = SDL_CreateThread(prepare_music, "race PCM preparation", a);
}

/* The camera as 003B5E80 asks it: the view the race draws (view = R *
   (world - eye), the VU0 matrix in its column layout), the eye of the
   previous race frame, the camera kind (vt+0x108 -> vt+0xA8: the views on
   the car 0, behind 2, the roof's +0x26C 1; the start and replay paths
   and the free camera 3). */
static void listener(race_audio *a, gt4_car_listener *l, const float *eye, const float *rows, int view)
{
    memset(l, 0, sizeof *l);
    for (int k = 0; k < 3; k++) {
        for (int j = 0; j < 3; j++) l->view[j * 4 + k] = rows[k * 3 + j];
        l->view[12 + k] = -((rows[k * 3] * eye[0] + rows[k * 3 + 1] * eye[1]) + rows[k * 3 + 2] * eye[2]);
    }
    l->view[15] = 1;
    for (int k = 0; k < 3; k++) {
        /* 00373718 -> 00379448 +00..14: f12=0 reads camera+10,
           the current eye. 00373850 +3C..58 passes f12=1, reading
           camera+7C (preceding eye). The legacy listener field names
           are reversed relative to their temporal meaning. */
        l->eye_previous[k] = eye[k];
        l->eye[k] = a->have_eye ? a->eye_previous[k] : eye[k];
    }
    l->continuous = 1;   /* A: the cut flag (camera +0x16B) is not tracked */
    l->type = view == 0 ? 0 : view == 1 ? 2 : view == 6 ? 1 : 3;
    l->angle = 0;
}

/* 00393428 -> 00463378 -> 00462A88 for a bank 2 sequence: left and right
   at 00391CD8's level, pitch 1, priority 7FFFFFFFh, pan 40h */
static void sequence(race_audio *a, int q, float level)
{
    static const char *const names[6] = { "count_d", "count_s", "chime", "ok", "cursor", "cancel" };
    if (getenv("OPENGT4_SOUND_TRACE")) fprintf(stderr, "race sound: sequence %s at %u\n", names[q], a->clock240 / 4);
    for (unsigned i = 0; i < a->sequence[q].count && a->pendings < 16; i++) {
        a->pending[a->pendings].due = a->clock240 + a->sequence[q].note[i].at;
        a->pending[a->pendings].tone = &a->sequence[q].note[i].tone;
        a->pending[a->pendings].note = a->sequence[q].note[i].note;
        a->pending[a->pendings].velocity = a->sequence[q].note[i].velocity;
        a->pending[a->pendings].level = level;
        a->pendings++;
    }
}

static void driver_tick(race_audio *a);
static void tick(race_audio *a, const gt4_race_run *run, const float *eye, const float *rows, int view)
{
    gt4_car_listener l;
    listener(a, &l, eye, rows, view);
    uint32_t cond = gt4_race_run_conductor(run), race = GT4_U32(cond);
    uint32_t list = GT4_U32(race + 0x60);
    for (int i = 0; i < a->cars; i++) {
        uint32_t entry = GT4_U32(GT4_U32(list + 8) + (uint32_t)i * 4);
        /* 003D6D30 posts the presentation pulse before 003B5E80 reads
           entry+2890+64. The physical signal is consumed read-only. */
        gt4_car_backfire_step(a->backfire+i,gt4_race_run_car(run,i),1.f/60.f);
        uint32_t pulse=gt4_car_backfire_take(a->backfire+i);
        GT4_U32(entry+0x2890+0x64)=a->exhaust_count[i]?pulse:0;
        gt4_car_sound_frame f;
        gt4_car_sound_out out;
        a->random = a->random * 1103515245u + 12345u;   /* A: 00579358's draw */
        gt4_car_sound_frame_build(&f, a->state + i, &out, entry, &l, i == 0 && view != -1, 0, a->random >> 16);
        gt4_car_sound_requests(&f, a->state + i, &a->setup, &out);
        static const int engine_order[3] = { 0, 1, 2 };
        for (int k = 0; k < 3; k++) {
            gt4_car_sound_engine *e = out.engine + engine_order[k];
            gt4_sd_engine_tick(a->sd, &a->engine[i][k], e->rpm, e->left, e->right, e->scale, e->priority, e->reverb_off, a->sine);
        }
        for (int k = 0; k < GT4_CAR_TONES; k++) {
            if (!out.tone_on[k]) continue;
            gt4_car_sound_tone *t = out.tone + k;
            gt4_sd_play(a->sd, &a->tone[i][k], a->tone_effect[k], 0, t->left, t->right, t->pitch, t->priority, 0x40);
        }
        for (int k = 0; k < out.shots; k++) {
            gt4_sd_handle h = { -1, 0 };
            if (out.shot[k].effect < 80)
                gt4_sd_play(a->sd, &h, a->effect_by_enum[out.shot[k].effect], 1, out.shot[k].left, out.shot[k].right, 1.0f,
                            out.shot[k].priority, 0x40);
        }
    }
    /* 00398B70: the course's speakers, the tunnels, the helicopter (the
       manager's vt+0x130 bit 0: no replay camera here) and the reverb
       depth. The camera's right axis is the view's first row. */
    {
        gt4_ambient_out amb;
        float right[3] = { rows[0], rows[1], rows[2] };
        gt4_ambient_frame(&a->ambient_state, race, &l, right, 0, a->setup.master, &amb);
        /* 00398A70: air, city, crowds, cheer, sea, tunnel1_l/r, tunnel2_l/r, heri */
        static const uint8_t effect[10] = { 31, 32, 33, 61, 34, 35, 36, 37, 38, 30 };
        for (int k = 0; k < 5; k++)
            gt4_sd_play(a->sd, &a->ambient[k], a->effect_by_enum[effect[k]], 0, amb.left[k], amb.right[k], amb.pitch[k],
                        amb.priority[k], 0x40);
        for (int k = 0; k < 4; k++)
            gt4_sd_play(a->sd, &a->ambient[5 + k], a->effect_by_enum[effect[5 + k]], 0, amb.tunnel[k][0], amb.tunnel[k][1], 1.0f,
                        amb.tunnel_priority[k / 2], 0x40);
        gt4_sd_play(a->sd, &a->ambient[9], a->effect_by_enum[effect[9]], 0, amb.heri, amb.heri, 1.0f, amb.heri_priority, 0x40);
        if (!amb.restart) a->depth = amb.depth;
    }
    /* 0039E9B0 +0x38..+0x88 / +0x0C4..+0x194, played by 0039DA80: count_d
       at 180, 120, 60 frames left (func_0034C1C8 under 240, the race's
       countdown on); count_s by race_audio_start, on the start event */
    {
        int left = (int)GT4_U32(race + 0xbc) + (int)GT4_U32(cond + 0xcbfcu);
        if (left > 0 && (int32_t)GT4_U32(race + 0xb8) > 0 && left < 0xf0 && !(left % 60)) sequence(a, 0, a->setup.master);
        a->last_count = left;
    }
    driver_tick(a);
    memcpy(a->eye_previous, eye, sizeof a->eye_previous);
    a->have_eye = 1;
}

/* the sequences' notes due and the driver's tick (the IOP's, which runs
   on while the race is held) */
static void driver_tick(race_audio *a)
{
    a->clock240 += 4;   /* 240 Hz sequence ticks per 60 Hz driver tick */
    for (int k = 0; k < a->pendings;) {
        if ((int32_t)(a->pending[k].due - a->clock240) > 0) {
            k++;
            continue;
        }
        gt4_sd_note(a->sd, a->pending[k].tone, a->pending[k].note, a->pending[k].velocity, a->pending[k].level,
                    a->pending[k].level, 1.0f, 0x7fffffff, 0x40);
        a->pending[k] = a->pending[--a->pendings];
    }
    uint8_t ended[VOICES];
    SDL_LockMutex(a->mutex);
    /* 0055D890 +0x2C..0x90 reads current IOP voice status. A: our async
       callback can report the previous occupant. Bind completion to KON
       serial so that stale OFF cannot release a newly started engine. */
    completed_voices(a,ended);
    SDL_UnlockMutex(a->mutex);
    gt4_spu_regs regs[VOICES];
    SDL_LockMutex(a->mutex);
    memcpy(regs, a->regs, sizeof regs);
    SDL_UnlockMutex(a->mutex);
    gt4_sd_tick(a->sd, ended, regs);
    SDL_LockMutex(a->mutex);
    memcpy(a->regs, regs, sizeof regs);
    for (int c = 0; c < 2; c++) a->reverb[c].evoll = a->reverb[c].evolr = a->depth;   /* 0055CD18, both cores */
    SDL_UnlockMutex(a->mutex);
}

static void music_volume_update(race_audio *a,const gt4_race_run *run)
{
    int bgm=a->bgm_option;
    SDL_LockMutex(a->mutex);
    /* 0033D568 +0xFC..0x15C applies option /128 only to enum 12;
       0033D4B8 gates every stream. -1 carries the disabled flag separately
       from a racing-volume setting of zero (jingles retain base gain).
       0033FD50 -> 003C0FB0 and 0033D510 multiply the external fade. */
    uint32_t session=GT4_U32(gt4_race_run_conductor(run));
    float gain=a->sdvol_music;
    if(a->requested_music==12)gain*=((float)(bgm>128?128:bgm)*0.0078125f);
    gain=gt4_music_stream_gain(gain,gt4_music_fade_gain(GT4_F32(session+0xd4),GT4_F32(session+0xd8)),bgm>=0);
    a->music_volume=(float)gt4_music_gain_q15(gain)/32768.0f;
    SDL_UnlockMutex(a->mutex);
}

void race_audio_update(race_audio *a, const gt4_race_run *run, const float *eye, const float *rows, int view, int bgm, int se)
{
    if (!a || !run || !eye || !rows) return;
    /* P: user-requested filming stems. Gate only the music stream, through
       its existing disabled option; engine/ambient/effect voices stay live. */
    if(getenv("OPENGT4_CAPTURE_NO_MUSIC"))bgm=-1;
    a->bgm_option=bgm;
    music_update(a, run);
    float master = (float)(se < 0 ? 0 : se > 128 ? 128 : se) / 128.0f * a->sdvol;
    if (!a->have_setup || a->setup.master != master) {
        /* 00391CD8: D_006213D8 (the SE option /128) x sdvol; x D_006213E0
           (1.0 from 003886C4). 003922B0 / 00391A60 at the race's start. */
        gt4_car_sound_setup_race(&a->setup, gt4_race_run_conductor(run), 1, master);
        a->have_setup = 1;
    }
    memcpy(a->camera_eye, eye, sizeof a->camera_eye);
    memcpy(a->camera_rows, rows, sizeof a->camera_rows);
    a->camera_view = view;
    a->have_camera = 1;
    music_volume_update(a,run);
    if (!a->playing) a->playing = plat_audio_set_music(render, a) == 0;
}

void race_audio_step(race_audio *a, const gt4_race_run *run)
{
    if (!a || !run || !a->have_camera || !a->have_setup) return;
    gt4_phys_fpu_enter();
    tick(a, run, a->camera_eye, a->camera_rows, a->camera_view);
    music_volume_update(a,run);
    if (getenv("OPENGT4_SOUND_TRACE") && !(++a->frames % 120)) {
        int on = 0, loud = 0;
        SDL_LockMutex(a->mutex);
        for (int i = 0; i < VOICES; i++) {
            on += a->voice[i].phase != GT4_SPU_OFF;
            loud += a->regs[i].voll || a->regs[i].volr;
        }
        SDL_UnlockMutex(a->mutex);
        fprintf(stderr, "race sound: frame %d master %.3f voices on %d with volume %d, view %d\n", a->frames, a->setup.master,
                on, loud, a->camera_view);
    }
}

/* 0039E9B0 sets D_00621515 with START on the watched car's start event;
   0039DA80 plays count_s for it */
void race_audio_start(race_audio *a)
{
    if (a && a->have_setup) sequence(a, 1, a->setup.master);
}
void race_audio_pit(race_audio *a,const gt4_pit_crew_sound *out)
{
    if(!a || !a->sd || !a->have_setup || !out)return;
    /* 00393428/003934A0: gain 1, pitch 1, priority 40h. Apply the
       existing manager's effect master, not an extra pit volume. */
    for(unsigned i=0;i<out->count;i++)if(out->effect[i]<80){
        gt4_sd_handle h={-1,0};
        gt4_sd_play(a->sd,&h,a->effect_by_enum[out->effect[i]],1,a->setup.master,a->setup.master,1,0x40,0x40);
    }
    if(out->refueling)gt4_sd_play(a->sd,&a->refueling,a->effect_by_enum[0x46],0,a->setup.master,a->setup.master,1,0x40,0x40);
    else gt4_sd_stop(a->sd,&a->refueling);
}

void race_audio_menu_sound(race_audio *a, const char *name, int se)
{
    /* A: the pause's and results' play_sound at the SE option x sdvol's
       effect column, as the race's effects; the callback's own volume is
       not located */
    int q = !strcmp(name, "ok") ? 3 : !strcmp(name, "cursor") ? 4 : !strcmp(name, "cancel") ? 5 : -1;
    if (!a || q < 0) return;
    sequence(a, q, (float)(se < 0 ? 0 : se > 128 ? 128 : se) / 128.0f * a->sdvol);
}

void race_audio_hold(race_audio *a, double dt)
{
    if (!a || !a->have_setup) return;
    /* the race held: no car or course requests, the voices keep their
       registers (A: what the game does to the cars' sound in the pause
       is not read); the menu's notes and the driver go on at 60 Hz */
    a->hold += dt;
    if (a->hold > 0.25) a->hold = 0.25;
    while (a->hold >= 1.0 / 60) {
        a->hold -= 1.0 / 60;
        gt4_phys_fpu_enter();
        driver_tick(a);
    }
}

/* the world's vt+0x2D0 / +0x2C8 (0033C3E8 / 0033C3B0), called by the
   pause's open 0038B428 and its close 0038B4B8: 0038A010 runs each car's
   00392FC8 (its voices keyed off), 00399300 stops the ambience with the
   depth at 0FFFh; back, 00389F90 restarts the cars (003924D0) and
   00398B58 marks the ambience started. 003D33C0 also stops the shared
   refueling handle; 003D3358 restores it on the next pit frame. */
void race_audio_pause(race_audio *a, int on)
{
    if (!a || !a->sd) return;
    if (on) {
        gt4_sd_stop(a->sd,&a->refueling);
        for (int i = 0; i < a->cars; i++) {
            for (int k = 0; k < 3; k++) gt4_sd_engine_stop(a->sd, &a->engine[i][k]);
            for (int k = 0; k < GT4_CAR_TONES; k++) gt4_sd_stop(a->sd, &a->tone[i][k]);
        }
        for (int k = 0; k < 10; k++) gt4_sd_stop(a->sd, &a->ambient[k]);
        a->depth = 0xfff;
    } else {
        for (int i = 0; i < a->cars; i++) gt4_car_sound_state_reset(a->state + i);
        gt4_ambient_state_reset(&a->ambient_state);
        a->ambient_state.started = 1;
    }
}

void race_audio_close(race_audio *a)
{
    if (!a) return;
    if (a->playing) plat_audio_release_music(a);
    if (a->music_worker) SDL_WaitThread(a->music_worker, NULL);
    /* 0033C278 +0x1C..+0x7C writes the playback jukebox back to options. */
    if(a->playlist_preferences && a->playlist_ready && a->playlist_changed)
        gt4_save_option_blob_set("bgm_playlist_ps2",a->playlist.bytes,sizeof a->playlist.bytes);
    for (int i = 0; i < CARS; i++) {
        gt4_engine_bank_close(a->bank[i][0]);
        gt4_engine_bank_close(a->bank[i][2]);
    }
    gt4_engine_bank_close(a->road);
    gt4_race_effects_close(a->effects);
    gt4_sd_close(a->sd);
    free(a->reverb_area);
    gt4_pcm_music_close(a->music);
    gt4_vol_close(a->vol);
    gt4_pcm_music_close(a->prepared);
    if (a->mutex) SDL_DestroyMutex(a->mutex);
    free(a);
}

race_audio *race_audio_open(const char *disc,const gt4_race_run *run,const gt4_course *course)
{return race_audio_open_replay(disc,run,course,0);}

int race_audio_afterfire(const race_audio *a,unsigned entry,float out[4][3])
{
 /* 003D6D30's four particles: time (+0), size (+4), turn (+8), on (+0x10) */
 int any=0;
 for(int k=0;k<4;k++){out[k][0]=0;out[k][1]=1;out[k][2]=0;}
 if(!a || entry>=6 || !a->exhaust_count[entry])return 0;
 for(int k=0;k<4;k++){
  const uint8_t *p=a->backfire[entry].b+0x10+k*0x14;uint32_t on;memcpy(&on,p+16,4);
  if(!on)continue;memcpy(out[k],p,12);any|=out[k][0]>0;
 }
 return any;
}
void race_audio_set_exhaust_count(race_audio *a,unsigned entry,unsigned count)
{
 if(a && entry<6)a->exhaust_count[entry]=count;
}
