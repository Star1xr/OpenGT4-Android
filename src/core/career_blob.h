#ifndef OPENGT4_CAREER_BLOB_H
#define OPENGT4_CAREER_BLOB_H
#include "adhoc/adhoc_vm.h"
#include <stddef.h>
int gt4_career_blob_set(ah_vm *vm,ah_obj *obj,const char *name,const unsigned char *data,size_t size);
int gt4_career_blob_get(ah_vm *vm,ah_obj *obj,const char *name,unsigned char *data,size_t size);
#endif
