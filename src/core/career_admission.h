#ifndef OPENGT4_CAREER_ADMISSION_H
#define OPENGT4_CAREER_ADMISSION_H
#include "vol.h"
#include <stdint.h>
/* 001A1C60: zero/unknown restriction gives an empty array, not nil. */
unsigned gt4_career_countries(uint8_t requirement,const char **out);
/* 00110498: RACE +0x45, bits 0..4 are passed B/A/IB/IA/S series. */
int gt4_career_aspec_allowed(uint8_t requirement,unsigned passed);
/* 001A2BB0 -> 00448CD0: absent/unreadable ALLOW_ENTRY gives nil.
   A readable row gives an array, even when every key is absent. */
int gt4_career_allowed_cars(gt4_vol *vol,const uint8_t race[151],
                           char labels[60][128],unsigned *count);
#endif
