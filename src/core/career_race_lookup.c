#include "career_race_lookup.h"
#include "specdb.h"
#include <stdlib.h>
#include <string.h>
/* func_00430E58: resolve exact label, then prefix 1, then prefix 2.
   005A6AB0/005A6900 concatenate into the 0x40-byte stack buffer. A
   resolved label with no valid record ordinal does not try another label. */
int gt4_career_race_record_index(const gt4_career_race_lookup_data *data,
 const uint8_t *labels,size_t labels_size,const uint8_t *db,size_t db_size,const char *label){
    uint32_t id;if(!label)return -1;
    if(!gt4_specdb_label(labels,labels_size,label,&id)){
        int found=0;
        if(data)for(unsigned i=0;i<2;i++){
            /* 005A6900 decrements a2 once while scanning a nonempty
               destination, then copies at most 63 source bytes. It does
               not subtract the full two-byte prefix length. Preserve the
               produced name, without reproducing its stack-buffer overrun. */
            char name[66];size_t p=2,n=strlen(label);if(n>63)n=63;
            memcpy(name,data->prefixes[i],p);memcpy(name+p,label,n);name[p+n]=0;
            if(gt4_specdb_label(labels,labels_size,name,&id)){found=1;break;}
        }
        if(!found)return -1;
    }
    /* func_004483B8 -> 00449190 -> 00430F70: namespace removed, 768 slots. */
    int index=gt4_specdb_row_index(db,db_size,id&65535u);
    return index>=0 && index<768?index:-1;
}
/* func_001A0E48 -> 00447188. Constructor retries after any failed row
   load. Unlike record lookup, formatting here uses a dynamically sized string. */
int gt4_career_race_row(const gt4_career_race_lookup_data *data,
 const uint8_t *labels,size_t labels_size,const uint8_t *db,size_t db_size,const char *label,uint8_t row[151]){
    uint32_t id;if(!label)return 0;
    if(gt4_specdb_label(labels,labels_size,label,&id) && gt4_specdb_row(db,db_size,id,row,151)==151)return 1;
    if(!data)return 0;size_t n=strlen(label);if(n>SIZE_MAX-3)return 0;
    char *name=malloc(n+3);if(!name)return 0;int ok=0;
    for(unsigned i=0;i<2;i++){
        memcpy(name,data->prefixes[i],2);memcpy(name+2,label,n+1);
        if(gt4_specdb_label(labels,labels_size,name,&id) && gt4_specdb_row(db,db_size,id,row,151)==151){ok=1;break;}
    }
    free(name);return ok;
}

/* func_00431C60 -> 0057B1E8/0057B210: first exact string match;
   missing names retain the original default 0xf, result narrowed to a byte. */
unsigned gt4_career_result_grade(const gt4_career_race_lookup_data *data,const char *name){
    if(data && name)for(unsigned i=0;i<16;i++)if(!strcmp(data->grade_names[i],name))return data->grade_values[i];
    return 15;
}

/* func_00447598: lb from the original table, sign extended.
   Positions outside 0..5 would read outside the championship table. */
int gt4_career_driver_points(const gt4_career_race_lookup_data *data,unsigned position){
    if(!data || position>=6)return 0;
    unsigned v=data->driver_points[position];return v<128?(int)v:(int)v-256;
}

/* func_00430990 -> 004308D8: first matching string, default signed -1. */
int gt4_career_record_type(const gt4_career_race_lookup_data *data,const char *name){
    if(data && name)for(unsigned i=0;i<4;i++)if(!strcmp(data->type_names[i],name))return (int)i;
    return -1;
}
/* func_00431C38 -> 0057B1A8: lookup by byte value, null on a miss. */
const char *gt4_career_result_name(const gt4_career_race_lookup_data *data,unsigned grade){
    if(data)for(unsigned i=0;i<16;i++)if(data->grade_values[i]==(grade&255u))return data->grade_names[i];
    return NULL;
}

/* func_00431CB8 -> 0057B1E8: first exact match, default 14, narrowed to byte. */
unsigned gt4_career_race_method(const gt4_career_race_lookup_data *data,const char *name){
    if(data && name)for(unsigned i=0;i<15;i++)if(!strcmp(data->method_names[i],name))return data->method_values[i];
    return 14;
}
