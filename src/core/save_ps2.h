#ifndef OPENGT4_SAVE_PS2_H
#define OPENGT4_SAVE_PS2_H
#include "save.h"
/* Read-only input: extracted GAMEDATA + garage, EMS .psu, or PCSX2 .ps2 card.
   Requires a complete, consistent GT4 save; builds a staging game, never writes it. */
int gt4_ps2_read(const char *path,ah_vm *vm,ah_obj *game,char *error,size_t error_size);
int gt4_ps2_decode(const uint8_t *raw,size_t size,uint8_t **payload,size_t *payload_size,int *old_crypto);
/* EncryptUnit: CRC-checked body, caller supplies the two original float
   bits and swap alignment. Replays and saves use different constants. */
int gt4_ps2_unwrap(const uint8_t *raw,size_t size,uint32_t first,uint32_t second,
    size_t alignment,int old_crypto,uint8_t **body,size_t *length);
int gt4_save_import_ps2(int slot,const char *path);
/* Read-only enumeration of media in an EMS archive, image or extracted
   directory. Callback borrows bytes only for its duration. */
typedef int (*gt4_ps2_media_visitor)(void *,const char *,const uint8_t *,size_t);
int gt4_ps2_visit_media(const char *path,gt4_ps2_media_visitor visit,void *user,
    char *error,size_t capacity);
#endif
