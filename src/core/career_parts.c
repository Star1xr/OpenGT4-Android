#include "career_parts.h"
#include <string.h>
static uint32_t u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t u64(const uint8_t *p){return u32(p)|(uint64_t)u32(p+4)<<32;}
/* func_0044A048 / 00449F58 / 00449F68 / 00449F80. The CARS file
   is the original counted array: eight-byte count, sixteen-byte entries.
   Count and namespace are sign-extended low32; arguments remain signed64.
   Search uses unsigned comparison to stop past the requested namespace. */
uint64_t gt4_career_part_key(const uint8_t *list,size_t size,int64_t type,int64_t stage){
    if(type==31)return UINT64_C(0x1f00000000);
    if(!list || size<8)return UINT64_MAX;
    uint32_t count=u32(list);if(count>INT32_MAX || count>(size-8)/16)return UINT64_MAX;
    for(uint32_t i=0;i<count;i++){
        const uint8_t *entry=list+8+(size_t)i*16;uint64_t key=u64(entry);
        uint32_t bits=(uint32_t)(key>>32);int32_t kind;memcpy(&kind,&bits,4);
        if(type==kind && stage==entry[8])return key;
        if((uint64_t)type<(uint64_t)(int64_t)kind)break;
    }return UINT64_MAX;
}
/* func_0044A2E0: switch branch targets are loaded from CORE.GT4.
   These offsets/operations are instruction logic, not copied price data. */
int32_t gt4_career_part_row_price(uint32_t branch,const uint8_t *row,size_t size){
    if(branch==0x44a458)return 30000; /* original addiu 0x7530 */
    if(branch==0x44a3b8){
        if(!row || size<4)return -1;uint32_t bits=u32(row);int32_t price;memcpy(&price,&bits,4);return price;
    }
    unsigned offset;
    switch(branch){
    case 0x44a338:case 0x44a3d8:offset=0;break;
    case 0x44a358:case 0x44a378:offset=12;break;
    case 0x44a398:offset=32;break;
    case 0x44a3f8:offset=56;break;
    case 0x44a418:offset=2;break;
    case 0x44a438:offset=4;break;
    default:return -1;
    }
    if(!row || size<offset+2)return -1;return row[offset]|(uint32_t)row[offset+1]<<8;
}

/* func_00448928 -> 00448908 / 004488E8 / 0057B1A8.
   Names are read from the original enum initializer, never embedded here. */
const char *gt4_career_car_category_name(const gt4_career_part_data *data,const uint8_t *row){
    unsigned key=row[17]%100u;
    return data && data->category_names[key][0]?data->category_names[key]:NULL;
}

/* 00140AE0 -> 0044A250, branches jtbl_006A7B20. */
int gt4_career_part_removable(int32_t type){
    switch(type){case 13:case 18:case 19:case 20:case 22:case 23:case 27:case 28:return 1;default:return 0;}
}
/* 00140BA8 -> 0044A288: full signed argument equality, not low-byte. */
int gt4_career_part_removed_normal(int32_t type){return type==20 || type==22;}
