#include "career_admission.h"
#include "specdb.h"
#include <stdlib.h>
#include <string.h>

int gt4_career_aspec_allowed(uint8_t requirement,unsigned passed)
{
    /* 00110498 +0x08/+0x18/+0x24: zero requires no licence; 1..5
       select l0b/l0a/lib/lia/l0s through jtbl_0068D2F0. 004312B0
       checks all sixteen records (existing race_record.isLicensePassed). */
    return !requirement || (requirement<=5 && ((passed>>(requirement-1))&1));
}

unsigned gt4_career_countries(uint8_t requirement,const char **out)
{
    /* 001A1C60 +0x40: RACE+0x96 indexes 00692860. Cases 1..6
       use strings 00692808..00692830; case 7 appends the eight
       strings at 001A1F68..001A25C0 in this order. No region heuristic. */
    static const char *single[]={"JP","US","GB","IT","FR","DE"};
    static const char *europe[]={"DE","BE","SE","FR","IT","GB","NL","ES"};
    unsigned n=0;
    if(requirement>=1 && requirement<=6)out[n++]=single[requirement-1];
    else if(requirement==7)for(unsigned i=0;i<8;i++)out[n++]=europe[i];
    return n;
}

int gt4_career_allowed_cars(gt4_vol *vol,const uint8_t race[151],
                           char labels[60][128],unsigned *count)
{
    uint64_t key;memcpy(&key,race+8,8);*count=0;
    /* 00448CD0 +0x10/+0x14: no key means no restriction. */
    if(key==UINT64_MAX)return 0;
    size_t db_size=0,generic_size=0,enemy_size=0;
    uint8_t *db=gt4_vol_read(vol,"/specdb/GT4_US2560/ALLOW_ENTRY.dbt",&db_size);
    uint8_t allowed[480];
    int ok=db && gt4_specdb_row(db,db_size,(uint32_t)key,allowed,sizeof allowed)==sizeof allowed;
    free(db);if(!ok)return 0;
    uint8_t *generic=gt4_vol_read(vol,"/specdb/GT4_US2560/GENERIC_CAR.idi",&generic_size);
    uint8_t *enemy=gt4_vol_read(vol,"/specdb/GT4_US2560/ENEMY_CARS.idi",&enemy_size);
    /* 001A2BB0 +0x54/+0x70/+0x160 walks all 60 qwords. -1 and failed
       00447CA0 label resolutions are skipped, not end-of-list markers. */
    for(unsigned i=0;i<60;i++){
        uint64_t car;memcpy(&car,allowed+i*8,8);if(car==UINT64_MAX)continue;
        const char *label=NULL;
        if((car>>32)==0)label=gt4_specdb_label_for_id(generic,generic_size,(uint32_t)car);
        else if((car>>32)==0x26)label=gt4_specdb_label_for_id(enemy,enemy_size,(uint32_t)car);
        if(label && strlen(label)<128)strcpy(labels[(*count)++],label);
    }
    free(generic);free(enemy);return 1;
}
