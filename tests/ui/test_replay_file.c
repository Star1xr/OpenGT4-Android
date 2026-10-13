/* A saved replay's record (replay_file.c) packed and read back: the setup,
   the delta-coded tape, the picture and the player's name; a record cut
   short or with a name too long is refused. Synthetic data only. */
#include "replay_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void sample(gt4_replay_file *r, uint8_t *tape, size_t steps, uint8_t *thumb)
{
    memset(r, 0, sizeof *r);
    snprintf(r->course, sizeof r->course, "tsukuba");
    snprintf(r->car, sizeof r->car, "ka_01");
    snprintf(r->car_name, sizeof r->car_name, "Ford Ka `01");
    r->laps = 2;
    r->best_lap = 98436;
    for (size_t i = 0; i < steps * 0x20; i++) tape[i] = (uint8_t)(i / 0x60 * 7 + (i % 0x20 == 4 ? i : 0));
    r->tape = tape;
    r->steps = steps;
    for (size_t i = 0; i < GT4_REPLAY_THUMB_W * GT4_REPLAY_THUMB_H * 3; i++) thumb[i] = (uint8_t)(i * 13);
    r->thumb = thumb;
}

int main(void)
{
    static uint8_t tape[600 * 0x20], thumb[GT4_REPLAY_THUMB_W * GT4_REPLAY_THUMB_H * 3];
    gt4_replay_file r, back;
    uint8_t *data;
    size_t size;

    /* with a name */
    sample(&r, tape, 600, thumb);
    snprintf(r.title, sizeof r.title, "Tsukuba run");
    CHECK(gt4_replay_pack(&r, &data, &size));
    CHECK(gt4_replay_unpack(data, size, &back));
    CHECK(back.steps == 600 && !memcmp(back.tape, tape, sizeof tape));
    CHECK(back.thumb && !memcmp(back.thumb, thumb, sizeof thumb));
    CHECK(!strcmp(back.title, "Tsukuba run"));
    CHECK(!strcmp(back.car_name, "Ford Ka `01") && back.best_lap == 98436 && back.laps == 2);
    gt4_replay_free(&back);
    for (size_t n = size - 12; n < size; n++) CHECK(!gt4_replay_unpack(data, n, &back));   /* cut in the name */
    free(data);

    /* without one (as files saved before names): no title read */
    sample(&r, tape, 600, thumb);
    CHECK(gt4_replay_pack(&r, &data, &size));
    CHECK(gt4_replay_unpack(data, size, &back));
    CHECK(back.title[0] == 0 && back.thumb);
    gt4_replay_free(&back);
    free(data);

    /* without a picture, the name alone */
    sample(&r, tape, 600, thumb);
    r.thumb = NULL;
    snprintf(r.title, sizeof r.title, "Lap");
    CHECK(gt4_replay_pack(&r, &data, &size));
    CHECK(gt4_replay_unpack(data, size, &back));
    CHECK(!back.thumb && !strcmp(back.title, "Lap"));
    CHECK(back.world_mode == 0);
    gt4_replay_free(&back);
    free(data);

    /* GT Mode's Practice: its world's mode kept; another value refused */
    sample(&r, tape, 600, thumb);
    r.world_mode = 12;
    CHECK(gt4_replay_pack(&r, &data, &size));
    CHECK(gt4_replay_unpack(data, size, &back));
    CHECK(back.world_mode == 12 && back.steps == 600);
    gt4_replay_free(&back);
    data[size - 4] = 13;
    CHECK(!gt4_replay_unpack(data, size, &back));
    free(data);
    printf("replay file: ok\n");
    return 0;
}
