/* Disc-backed GT event grid; production never executes recompiled EE code. */
#include "career_event.h"
#include "inflate.h"
#include "career_original.h"
#include "spectables.h"
#include "specdb.h"
#include "vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static uint64_t key(const uint8_t *p){uint64_t v;memcpy(&v,p,8);return v;}

int gt4_career_event_rng_load(const char *iso,uint32_t *rng)
{
    size_t size=0;uint8_t *raw=gt4_disc_read_root(iso,"CORE.GT4",&size),*data=NULL;int ok=0;
    if(!rng || !raw || size<6 || raw[0]!=1 || raw[1]!=1)goto done;
    uint32_t n=word(raw+2);if(n<0x12c || n>32u*1024*1024)goto done;
    data=malloc(n);if(!data || core_inflate_raw(raw+6,size-6,data,n)!=n || word(data+0x100)!=0x176265b3)goto done;
    /* 0061736c's startup constructor 003BB820 calls 003BB7E8(1,ffff)
       then 005792D8. The loaded data word alone is not the process seed. */
    for(size_t at=0x12c;at+8<=n;){
        uint32_t base=word(data+at),length=word(data+at+4);at+=8;
        if(length>n-at)goto done;
        if(base<=0x621ae8 && (uint64_t)base+length>=0x621aec){gt4_career_enemy_rng_init(rng);ok=1;break;}
        at+=length;
    }
done:free(data);free(raw);return ok;
}

int gt4_career_event_prepare(gt4_career_event *event,const char *iso,
    const gt4_career_race_lookup_data *lookup,const char *race,
    const uint8_t player_body[0x4b8],int championship,uint32_t *rng,
    char *message,size_t capacity)
{
    gt4_vol *vol=NULL;gt4_spec_tables *tables=NULL;
    uint8_t *labels=NULL,*rows=NULL,*courses=NULL,*candidates=NULL;
    size_t labels_size=0,rows_size=0,courses_size=0,candidates_size=0;
    int ok=0;const char *error=NULL,*stage="arguments";uint8_t active[0x178];uint32_t id;
    if(message && capacity)message[0]=0;
    if(!event || !iso || !lookup || !race || !player_body || !rng)goto done;
    memset(event,0,sizeof *event);
    stage="owned setup";
    if(!gt4_career_active_setup(player_body,active) || word(active+4)!=0)goto done;
    stage="disc";
    vol=gt4_vol_open(iso,&error);if(!vol)goto done;
    labels=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.idi",&labels_size);
    rows=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.dbt",&rows_size);
    courses=gt4_vol_read(vol,"/specdb/GT4_US2560/COURSE.idi",&courses_size);
    stage="race lookup";
    if(!labels || !rows || !courses ||
       !gt4_career_race_row(lookup,labels,labels_size,rows,rows_size,race,event->race_row) ||
       !gt4_specdb_label(labels,labels_size,race,&id))goto done;
    /* 00447550: COURSE key is RACE's leading unsigned short.
       001A1370 entry-count getter: decoded RACE +0x3e. */
    const char *course=gt4_specdb_label_for_id(courses,courses_size,
        event->race_row[0]|(uint32_t)event->race_row[1]<<8);
    int count=gt4_career_race_field(event->race_row,GT4_RACE_ENTRY_NUM);
    stage="course or entry count";
    if(!course || strlen(course)>=sizeof event->desc.course || count<1 || count>6)goto done;
    snprintf(event->desc.course,sizeof event->desc.course,"%s",course);
    event->desc.cars=count;event->desc.car[0]=word(active);
    memcpy(event->player_body,player_body,sizeof event->player_body);
    event->desc.owned_car[0]=event->player_body;
    event->desc.event_row=event->race_row;event->desc.championship=!!championship;
    event->desc.event_rng=rng;
    /* 003BB5A8 uses the RACES list's original order and 00448C10 country
       predicate (0044A5A8 returns zero in this NTSC-U executable). */
    char path[96];snprintf(path,sizeof path,"/specdb/GT4_US2560/RACES/%016llX",
        (unsigned long long)(UINT64_C(0x2300000000)|id));
    stage="RACES list";candidates=gt4_vol_read(vol,path,&candidates_size);
    if(!candidates || candidates_size<8)goto done;
    uint32_t total=word(candidates);
    if(!total || total>256 || total>(candidates_size-8)/16)goto done;
    stage="opponent selection";
    uint8_t *blocked=calloc(total,1);unsigned indices[5];
    if(!blocked)goto done;
    uint32_t next_rng=*rng;
    ok=gt4_career_enemy_indices(blocked,total,(unsigned)(count-1),&next_rng,indices);
    free(blocked);if(!ok)goto done;ok=0;
    stage="prepared car table";tables=gt4_spec_tables_open(vol,NULL);if(!tables)goto done;
    for(int i=1;i<count;i++){
        const uint8_t *entry=candidates+8+16*(size_t)indices[i-1];
        uint64_t car_key=key(entry);uint8_t prepared[32];
        /* 00447400/00447458: two qwords, prepared car then raw paint.
           00444690 reads ENEMY_CARS' leading base-car key. */
        if((uint32_t)(car_key>>32)!=0x26 ||
           gt4_spec_tables_row(tables,0x26,(uint32_t)car_key,0,prepared,sizeof prepared)<0x18 ||
           word(prepared+4)!=0)goto done;
        event->desc.tuned_car[i]=car_key;event->desc.car[i]=word(prepared);
        event->desc.event_paint[i]=key(entry+8);
    }
    *rng=next_rng;ok=1;
done:
    free(labels);free(rows);free(courses);free(candidates);
    gt4_spec_tables_close(tables);if(vol)gt4_vol_close(vol);
    if(!ok){if(event)memset(event,0,sizeof *event);if(message && capacity)
        snprintf(message,capacity,"cannot prepare career event %s: %s",race?race:"(missing)",stage);}
    return ok;
}

int gt4_career_championship_label(const char *series,unsigned round,char label[32],unsigned *count){
    if(!series || !label || !count)return 0;size_t n=strlen(series);
    if(n<4 || n>=32 || series[n-4]<'0' || series[n-4]>'9' || series[n-3]<'0' || series[n-3]>'9')return 0;
    unsigned total=(unsigned)(series[n-4]-'0')*10+(unsigned)(series[n-3]-'0');
    if(!total || round>=total)return 0;
    memcpy(label,series,n+1);label[n-2]=(char)('0'+(round+1)/10);label[n-1]=(char)('0'+(round+1)%10);*count=total;return 1;
}
int gt4_career_championship_round(gt4_career_event *event,const char *iso,
    const gt4_career_race_lookup_data *lookup,const char *label,const gt4_race_desc *grid,char *message,size_t capacity){
    const char *error=NULL;gt4_vol *v=gt4_vol_open(iso,&error);size_t ni=0,nd=0,nc=0;int ok=0;
    uint8_t *idi=v?gt4_vol_read(v,"/specdb/GT4_US2560/RACE.idi",&ni):NULL;
    uint8_t *db=v?gt4_vol_read(v,"/specdb/GT4_US2560/RACE.dbt",&nd):NULL;
    uint8_t *courses=v?gt4_vol_read(v,"/specdb/GT4_US2560/COURSE.idi",&nc):NULL;
    if(event && grid && idi && db && courses && gt4_career_race_row(lookup,idi,ni,db,nd,label,event->race_row)){
        const char *course=gt4_specdb_label_for_id(courses,nc,event->race_row[0]|(uint32_t)event->race_row[1]<<8);
        if(course && strlen(course)<sizeof event->desc.course && event->race_row[0x3e]==grid->cars){
            event->desc=*grid;event->desc.event_row=event->race_row;event->desc.championship=1;
            snprintf(event->desc.course,sizeof event->desc.course,"%s",course);ok=1;
        }
    }
    free(idi);free(db);free(courses);if(v)gt4_vol_close(v);
    if(!ok && message && capacity)snprintf(message,capacity,"Cannot load championship round %s",label?label:"");return ok;
}

int gt4_career_championship_restore(gt4_career_event *event,const char *iso,
    const gt4_career_race_lookup_data *lookup,const uint8_t context[0x90],
    const uint8_t player_body[0x4b8],uint32_t *rng,char *message,size_t capacity){
    if(!event || !context || !player_body || !rng || !context[0x28])return 0;
    char series[33],label[32];unsigned count;memcpy(series,context+8,32);series[32]=0;
    if(!gt4_career_championship_label(series,context[0x29],label,&count) || context[0x2a]!=count-1)return 0;
    uint8_t active[0x178];if(!gt4_career_active_setup(player_body,active))return 0;
    gt4_race_desc grid={0};grid.cars=6;grid.championship=1;grid.event_rng=rng;grid.car[0]=word(active);
    const char *error=NULL;gt4_vol *v=gt4_vol_open(iso,&error);gt4_spec_tables *tables=v?gt4_spec_tables_open(v,NULL):NULL;int ok=tables!=NULL;
    for(unsigned i=1;ok && i<6;i++){
        uint8_t prepared[32];uint64_t car=key(context+0x30+i*16);int32_t paint;memcpy(&paint,context+0x38+i*16,4);
        if((uint32_t)(car>>32)!=0x26 || gt4_spec_tables_row(tables,0x26,(uint32_t)car,0,prepared,32)<0x18 || word(prepared+4)!=0){ok=0;break;}
        grid.car[i]=word(prepared);grid.tuned_car[i]=car;grid.event_paint[i]=(uint64_t)((int64_t)paint+1);
    }
    gt4_spec_tables_close(tables);if(v)gt4_vol_close(v);
    if(ok)ok=gt4_career_championship_round(event,iso,lookup,label,&grid,message,capacity);
    if(ok){memcpy(event->player_body,player_body,0x4b8);event->desc.owned_car[0]=event->player_body;}
    if(!ok && message && capacity)snprintf(message,capacity,"Cannot restore championship %s",series);return ok;
}
