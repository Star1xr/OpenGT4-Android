#include "career_license_disc.h"
#include "career_license.h"
#include "vol.h"
#include "specdb.h"
#include "spectables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
/* kind 0 a licence, 1 a driving mission (0011BEC0) */
static int prepare(gt4_career_license_exam *e,const char *iso,
    const gt4_career_race_lookup_data *lookup,const char *code,char *error,size_t size,int mission)
{
    const char *why="arguments",*vol_error=NULL;gt4_vol *vol=NULL;gt4_spec_tables *tables=NULL;
    uint8_t *labels=NULL,*rows=NULL,*courses=NULL,*list=NULL;size_t nl=0,nr=0,nc=0,ns=0;int ok=0;
    if(!e || !iso || !lookup || !code || !*code || strlen(code)>=32)goto done;
    memset(e,0,sizeof *e);why="disc";vol=gt4_vol_open(iso,&vol_error);if(!vol)goto done;
    labels=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.idi",&nl);
    rows=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.dbt",&nr);
    courses=gt4_vol_read(vol,"/specdb/GT4_US2560/COURSE.idi",&nc);uint32_t id;
    why="RACE row";
    if(!labels || !rows || !courses || !gt4_career_race_row(lookup,labels,nl,rows,nr,code,e->row))goto done;
    /* 00447188 retries the two regional prefixes; IDI keys and persistent
       record ordinals are separate namespaces. */
    int found=gt4_specdb_label(labels,nl,code,&id);
    for(unsigned i=0;!found && i<2;i++){
        char regional[35];snprintf(regional,sizeof regional,"%.2s%s",lookup->prefixes[i],code);
        uint8_t check[151];
        found=gt4_specdb_label(labels,nl,regional,&id) && gt4_specdb_row(rows,nr,id,check,sizeof check)==sizeof check;
    }
    if(!found)goto done;
    const char *course=gt4_specdb_label_for_id(courses,nc,e->row[0]|(uint32_t)e->row[1]<<8);
    why="COURSE label";if(!course || strlen(course)>=sizeof e->desc.course)goto done;
    snprintf(e->desc.course,sizeof e->desc.course,"%s",course);
    char path[96];snprintf(path,sizeof path,"/specdb/GT4_US2560/RACES/%016llX",(unsigned long long)(UINT64_C(0x2300000000)|id));
    why="examination car";list=gt4_vol_read(vol,path,&ns);if(!list || ns<24 || !word(list))goto done;
    /* 0010FBD8 +0x20..0x84: B/A/IB/IA exams 5,10,15 enter
       00114E38 (RaceTraining); all others enter 0011CB38 (RaceLicense).
       003D5AB8 +0x28..0x2c reserves six training entries. */
    int index=mission?-1:gt4_career_license_index(code),number=index%16+1;
    int training=index>=0 && index<64 && (number==5 || number==10 || number==15);
    /* 003BB478 +0x20..0x28 clamps that capacity to RACE +0x3e;
       six is the maximum, not the number of active pace cars. A
       mission's descriptor (003ECAD0 -> 003D5AB8) reserves six too. */
    int capacity=training || mission?6:2;
    e->desc.cars=e->row[0x3e]<capacity?e->row[0x3e]:capacity;
    if(!e->desc.cars)goto done;
    e->desc.license=mission?3:training?2:1;
    /* RaceLicense (0011CB38) never sets the student's aids: they keep the
       entry's 1, 1 (003B4B38 +0x11c..0x124); RaceTraining sets 1, 1 too
       (0010F880 -> 003C0D78 / 003C0D98). The game's options do not reach them. */
    e->desc.assist_asm=e->desc.assist_tcs=1;
    /* These are enables, not replacement levels. 00363B10 +0x1d0..0x1f8
       also checks the authored ASCC/TCSC record +0x1b8/+0x1c2. B-1's
       prepared Demio has zero effective levels in the PCSX2 capture;
       B-3's authored TCSC produces level 3. Keep the examination setup. */
    uint32_t count=word(list);
    if(count>(ns-8)/16)goto done;
    tables=gt4_spec_tables_open(vol,NULL);if(!tables)goto done;
    for(int i=0;i<e->desc.cars;i++){
    /* 003ECB68 +0x74..0xa8 clamps each guide index to count-1;
       standard examinations clone the first entry (003BDC88). A
       mission's player is the list's first (0011BEC0: 00447400(0),
       00447458(0) -> 003D6300); the others its following entries. */
    uint32_t at=(training || mission) && i?(uint32_t)i:0;
    if(at>=count)at=count-1;
    uint64_t key;memcpy(&key,list+8+at*16,8);uint32_t car=(uint32_t)key;
    memcpy(&e->desc.event_paint[i],list+16+at*16,8);
    if((uint32_t)(key>>32)==0x26){
        uint8_t tuned[32];
        if(!tables || gt4_spec_tables_row(tables,0x26,car,0,tuned,sizeof tuned)<24 || word(tuned+4))goto done;
        car=word(tuned);e->desc.tuned_car[i]=key;
    }else if(key>>32)goto done;
    e->desc.car[i]=car;
    }
    e->desc.event_row=e->row;e->desc.laps=e->row[0x43];snprintf(e->code,sizeof e->code,"%s",code);snprintf(e->desc.license_code,sizeof e->desc.license_code,"%s",code);ok=1;
done:
    free(labels);free(rows);free(courses);free(list);gt4_spec_tables_close(tables);if(vol)gt4_vol_close(vol);
    if(!ok && e)memset(e,0,sizeof *e);
    if(error && size){if(ok)*error=0;else snprintf(error,size,"cannot prepare %s %s: %s",mission?"mission":"licence",code?code:"",why);}return ok;
}
int gt4_career_license_prepare(gt4_career_license_exam *e,const char *iso,
    const gt4_career_race_lookup_data *lookup,const char *code,char *error,size_t size)
{return prepare(e,iso,lookup,code,error,size,0);}
int gt4_career_mission_prepare(gt4_career_license_exam *e,const char *iso,
    const gt4_career_race_lookup_data *lookup,const char *code,char *error,size_t size)
{return prepare(e,iso,lookup,code,error,size,1);}
