#include "career_calendar.h"
#include "gtmode_game.h"
#include "vol.h"
#include "inflate.h"
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int gt4_career_calendar_data_load(const char *iso,gt4_career_calendar_data *out){
    size_t size=0;uint8_t *raw=gt4_disc_read_root(iso,"CORE.GT4",&size),*decoded=NULL;
    int table=0,code=0,inverse=0,ok=0;gt4_career_calendar_data candidate;
    if(!raw || size<6 || raw[0]!=1 || raw[1]!=1)goto done;
    uint32_t n=u32(raw+2);if(n<0x12c || n>32u*1024*1024)goto done;
    decoded=malloc(n);if(!decoded || core_inflate_raw(raw+6,size-6,decoded,n)!=n || u32(decoded+0x100)!=0x176265b3)goto done;
    for(size_t at=0x12c;at<n;){
        if(n-at<8)goto done;
        uint32_t base=u32(decoded+at),length=u32(decoded+at+4);at+=8;if(length>n-at)goto done;
        if(base<=0x6cc718 && (uint64_t)base+length>=0x6cc728+52){memcpy(candidate.year_offsets,decoded+at+0x6cc718-base,16);memcpy(candidate.month_offsets,decoded+at+0x6cc728-base,52);table=1;}
        if(base<=0x5797a8 && (uint64_t)base+length>=0x5797a8+0xec){
            const uint8_t *p=decoded+at+0x5797a8-base;
            code=u32(p)==0x24a5ffff && u32(p+0xc8)==0x3c03006d && u32(p+0xd0)==0x9463c728;
        }
        if(base<=0x579898 && (uint64_t)base+length>=0x579898+0x1ac){
            const uint8_t *p=decoded+at+0x579898-base;
            inverse=u32(p)==0x3c02ffe5 && u32(p+0x78)==0x3c11006d && u32(p+0x80)==0x2631c718;
        }
        at+=length;
    }
    ok=table && code && inverse;if(ok)*out=candidate;
done:free(decoded);free(raw);return ok;
}

int gt4_game_load_disc(const char *iso){
    gt4_career_home_data home;int home_ready=gt4_career_home_data_load(iso,&home);gt4_game_set_home_data(home_ready?&home:NULL);
    gt4_career_calendar_data data;int ready=gt4_career_calendar_data_load(iso,&data);gt4_game_set_calendar_data(ready?&data:NULL);
    gt4_career_oil_data oil;int oil_ready=gt4_career_oil_data_load(iso,&oil);gt4_game_set_oil_data(oil_ready?&oil:NULL);
    gt4_career_price_data prices;int prices_ready=gt4_career_price_data_load(iso,&prices);gt4_game_set_price_data(prices_ready?&prices:NULL);
    gt4_career_race_lookup_data lookup;int lookup_ready=gt4_career_race_lookup_data_load(iso,&lookup);gt4_game_set_race_lookup_data(lookup_ready?&lookup:NULL);
    gt4_career_part_data parts;int parts_ready=gt4_career_part_data_load(iso,&parts);gt4_game_set_part_data(parts_ready?&parts:NULL);
    return home_ready && ready && oil_ready && prices_ready && lookup_ready && parts_ready;
}
