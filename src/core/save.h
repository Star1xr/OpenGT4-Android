#ifndef OPENGT4_SAVE_H
#define OPENGT4_SAVE_H
#include "adhoc/adhoc_vm.h"
#include <stddef.h>
#include <stdint.h>
#define GT4_SAVE_SLOTS 8
#define GT4_SAVE_LIMIT (4u*1024u*1024u)
typedef struct { char name[97]; uint64_t timestamp; int status; } gt4_save_info;
/* 1 valid, 2 recovered backup, 0 empty, negative error. */
int gt4_save_read(const char *path,uint8_t **data,size_t *size,uint64_t *stamp);
int gt4_save_write(const char *path,const uint8_t *data,size_t size);
int gt4_save_mkdir(const char *path);
/* Bounded, version-neutral typed VM data. No callbacks/host objects are saved. */
int gt4_state_pack(ah_vm *vm,ah_obj *game,uint8_t **data,size_t *size);
int gt4_state_replace(ah_vm *vm,ah_obj *game,const uint8_t *data,size_t size);
int gt4_state_merge(ah_vm *vm,ah_obj *game,const uint8_t *data,size_t size);
/* One session shared across cached menu VMs, independent of UI/callback state. */
void gt4_save_init(const char *directory);
void gt4_save_shutdown(void);
int gt4_save_active(void);
int gt4_save_dirty(void);
const char *gt4_save_error(void);
int gt4_save_info_get(int slot,gt4_save_info *info);
int gt4_save_valid_name(const char *name);
int gt4_save_create(int slot,const char *name);
int gt4_save_load_slot(int slot);
/* Read another profile without changing the active player or its dirty state. */
int gt4_save_read_profile(ah_vm *vm,ah_obj *game,int slot);
int gt4_save_store(void);
int gt4_save_delete_slot(int slot);
int gt4_save_rename(const char *name);
int gt4_save_capture(ah_vm *vm,ah_obj *game,int dirty);
void gt4_save_restore(ah_vm *vm,ah_obj *game);
/* Refresh only shared preferences, preserving live garage/record changes. */
int gt4_save_restore_options(ah_vm *vm,ah_obj *game);
double gt4_save_option_number(const char *name,double fallback);
/* Host-owned numeric option arrays; no conversion of PS2 save bytes. */
int gt4_save_option_array_get(const char *name,int *data,size_t count);
int gt4_save_option_blob_get(const char *name,unsigned char *data,size_t size);
int gt4_save_option_blob_set(const char *name,const unsigned char *data,size_t size);
int gt4_save_auto_enabled(void);
/* the save folder ("" when none) and the active profile's name */
const char *gt4_save_directory(void);
const char *gt4_save_player(void);
#endif
