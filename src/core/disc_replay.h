#ifndef OPENGT4_DISC_REPLAY_H
#define OPENGT4_DISC_REPLAY_H
#include "vol.h"
#include "replay_file.h"
typedef struct gt4_disc_replay gt4_disc_replay;
/* Borrow volume; read feedback table from this disc's CORE.GT4 once. */
gt4_disc_replay *gt4_disc_replay_open(gt4_vol *vol);
void gt4_disc_replay_close(gt4_disc_replay *d);
int gt4_disc_replay_read(gt4_disc_replay *d,const char *path,gt4_replay_file *r,
    char *error,size_t capacity);
int gt4_disc_replay_decode(gt4_disc_replay *,const char *,const uint8_t *,size_t,gt4_replay_file *,char *,size_t);
int gt4_disc_replay_saved(gt4_disc_replay *,const char *,gt4_replay_file *,char *,size_t);
/* Icon header/RLE and original 64-column row selection (001D3B20/001D3D70). */
int gt4_disc_replay_icon(const uint8_t *data,size_t size,uint16_t pixels[4096]);
int gt4_disc_replay_commands(const uint8_t *blob,size_t size,uint8_t **tape,size_t *steps);
/* Base cipher, 00579E00. Table comes from D_00655888 on the user's disc. */
int gt4_disc_replay_base(uint8_t *bytes,size_t size,uint32_t key,const uint32_t table[256]);
int gt4_disc_replay_list(gt4_vol *vol,char (*paths)[64],int max,int sub);
#endif
