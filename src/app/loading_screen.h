#ifndef OPENGT4_LOADING_SCREEN_H
#define OPENGT4_LOADING_SCREEN_H
#include "vol.h"
void loading_screen_open(gt4_vol *vol,const char *disc);
void loading_screen_close(void);
int loading_screen_active(void);
int loading_screen_ready(void);
int loading_screen_revealing(void);
/* Diagnostic override; -1 restores the normal two-image selection. */
void loading_screen_preview_variant(int index);
void loading_screen_begin(void);
void loading_screen_end(void);
void loading_screen_draw(void);
#endif
