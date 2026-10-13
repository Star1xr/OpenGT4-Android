/*
 * The still screens before the opening movie: the legal notice drawn by the
 * boot executable while CORE loads, then "Sony Computer Entertainment America
 * Presents" drawn by CORE while the boot project loads. Evidence and timing:
 * docs/MOVIES.md, "Boot notices".
 */
#ifndef OPENGT4_BOOT_NOTICE_H
#define OPENGT4_BOOT_NOTICE_H

#include "tex1.h"
#include "vol.h"

enum { GT4_NOTICE_LEGAL, GT4_NOTICE_PRESENTS, GT4_NOTICE_COUNT };

typedef struct {
    tex1_image *legal, *main;
    int legal_count, main_count;
} gt4_boot_notices;

/* disc: the image path (the legal notice lives in its boot executable);
   vol: the opened GT4.VOL. Missing pieces leave that screen empty. */
void gt4_boot_notices_open(gt4_boot_notices *n, const char *disc, gt4_vol *vol);
void gt4_boot_notices_close(gt4_boot_notices *n);
/* The image for screen i, or NULL when it is unavailable. */
const tex1_image *gt4_boot_notice_image(const gt4_boot_notices *n, int i);
/* Brightness 0..1 of screen i, t seconds after it starts, or -1 once it
   has finished. Both screens are faded from and to black. */
float gt4_boot_notice_level(int i, double t);

#endif
