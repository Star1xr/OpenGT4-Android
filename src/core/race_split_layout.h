#ifndef OPENGT4_RACE_SPLIT_LAYOUT_H
#define OPENGT4_RACE_SPLIT_LAYOUT_H
#include <stdint.h>

typedef struct { int x,y,w,h; uint8_t ha,va; } gt4_split_place;
enum { SPLIT_RANK, SPLIT_MAP, SPLIT_TIMER, SPLIT_BEST, SPLIT_LAST,
       SPLIT_BEST_LABEL, SPLIT_LAST_LABEL, SPLIT_LINES, SPLIT_MESSAGE,
       SPLIT_INTRO, SPLIT_SIGNAL, SPLIT_BOTTOM, SPLIT_SPECIAL, SPLIT_ELEMENTS };
/* 003A2360 +48..90: horizontal Battle2P mode 8, D_006A0DE0.
   003A2630 stores alignment & 3; the remaining bits are visibility flags. */
static const gt4_split_place gt4_split_layout[SPLIT_ELEMENTS] = {
    {45,8,42,45,2,0}, {24,52,0,0,0,0}, {612,170,0,0,1,1},
    {550,42,82,15,2,0}, {550,90,82,15,2,0},
    {550,8,0,0,2,0}, {550,55,0,0,2,0}, {320,68,500,0,2,1},
    {320,120,0,0,2,2}, {32,16,400,107,0,0}, {20,16,132,20,0,0},
    {40,220,350,21,0,1}, {320,45,0,0,2,2}
};
/* 003A23C0/D0: +6C = 240. 0039F698 +20..30 translates the lower
   display by -self[6C], preserving the panel's authored 464 baseline. */
#define GT4_SPLIT_HEIGHT 240
/* 0032FCC8 +8C/B4/C4 / +140 / +2FC: world+E2E0 mirror rectangle
   and lens scale, each in its own horizontal panel. */
static const float gt4_split_mirror[2][5]={{200,16,240,40,2},{200,8,240,40,2}};
#endif
