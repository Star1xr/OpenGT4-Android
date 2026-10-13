#ifndef OPENGT4_FONT_LAYOUT_H
#define OPENGT4_FONT_LAYOUT_H
#include "font.h"
/* Proportional FT01 placement from 00492978; previous=0 after a space. */
float gt4_font_original_step(const gt4_font *f,unsigned previous,unsigned code,
                             float scale,float *ink);
float gt4_font_original_width(const gt4_font *f,const char *text,float scale);
typedef float (*gt4_text_measure_fn)(void *,const char *);
/* 0048F790 / 00245658: preserves scale and replaces the suffix with "...". */
int gt4_text_abbreviate(const char *text,char *out,size_t capacity,float width,
                        gt4_text_measure_fn measure,void *user);
#endif
