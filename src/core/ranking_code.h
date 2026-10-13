#ifndef OPENGT4_RANKING_CODE_H
#define OPENGT4_RANKING_CODE_H
#include "vol.h"
/* 0044B258's ten packed fields. Runtime key comes from CORE, never embedded. */
int gt4_ranking_code_key(gt4_vol *,uint8_t key[16]);
void gt4_ranking_code(const uint32_t fields[10],const uint8_t key[16],uint32_t clock,char out[33]);
#endif
