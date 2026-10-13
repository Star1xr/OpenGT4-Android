/*
 * race_run_test: a race run natively (src/core/physics/race_run.c), with
 * no recompiled code at all, the player's car on the made-up pad of
 * pad_fixed.h; each frame's cars to <out>/cars.bin, the memory at the end
 * to <out>/ram.bin, as physics_run writes them.
 *
 *   race_run_test <disc.iso> <course> <frames> <out> <car id> [car id ...]
 */
#include "race_run.h"
#include "car.h"
#include "career_original.h"
#include "career_race_lookup.h"
#include "vol.h"
#include "spectables.h"
#include "specdb.h"
#include "pad_fixed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    gt4_race_desc d;
    gt4_race_run *r;
    char msg[256], path[512];
    int frames;
    FILE *cars, *f;
    uint8_t owned[0x4b8],selected[0x178];
    uint8_t event[151];
    uint32_t event_seed = 17; /* explicit integration fixture input */
    memset(&d, 0, sizeof d);
    if (argc < 6) {
        fprintf(stderr, "usage: %s <disc.iso> <course> <frames> <out> <car id> [car id ...]\n", argv[0]);
        return 2;
    }
    if (!strcmp(argv[2], "--rate")) {   /* race_run_test <disc> --rate <id> ...: the cars' ratings */
        gt4_car_ratings *cr = gt4_car_ratings_open(argv[1]);
        for (int i = 3; i < argc; i++) {
            uint64_t key = strtoull(argv[i], NULL, 0);
            float v = 0;
            int power = 0, weight = 0;
            if (!gt4_car_ratings_get(cr, &key, 1, &v) || !gt4_car_ratings_spec(cr, key, &power, &weight)) return 1;
            printf("0x%llx %.6f %d PS %d kg\n", (unsigned long long)key, v, power, weight);
        }
        gt4_car_ratings_close(cr);
        return 0;
    }
    snprintf(d.course, sizeof d.course, "%s", argv[2]);
    frames = atoi(argv[3]);
    for (int i = 5; i < argc && d.cars < 6; i++) d.car[d.cars++] = (uint32_t)strtoul(argv[i], NULL, 0);
    if (getenv("RACE_TIRE")) d.tire = atoi(getenv("RACE_TIRE")) + 1;   /* the Arcade's tyre level */
    if (getenv("RACE_MANUAL")) d.manual = atoi(getenv("RACE_MANUAL"));
    if (getenv("RACE_ASM")) d.assist_asm = atoi(getenv("RACE_ASM"));
    if (getenv("RACE_TCS")) d.assist_tcs = atoi(getenv("RACE_TCS"));
    if(getenv("RACE_OWNED_CAR")){
        f=fopen(getenv("RACE_OWNED_CAR"),"rb");
        if(!f)return 2;
        int ok=fread(owned,1,sizeof owned,f)==sizeof owned && fgetc(f)==EOF;fclose(f);
        if(!ok || !gt4_career_active_setup(owned,selected))return 2;
        d.owned_car[0]=owned;
    }
    if(getenv("RACE_EVENT")){
        gt4_career_race_lookup_data lookup;const char *error=NULL;
        gt4_vol *v=gt4_vol_open(argv[1],&error);size_t ln=0,dn=0;
        uint8_t *labels=v?gt4_vol_read(v,"/specdb/GT4_US2560/RACE.idi",&ln):NULL;
        uint8_t *rows=v?gt4_vol_read(v,"/specdb/GT4_US2560/RACE.dbt",&dn):NULL;
        int ok=gt4_career_race_lookup_data_load(argv[1],&lookup) && labels && rows &&
            gt4_career_race_row(&lookup,labels,ln,rows,dn,getenv("RACE_EVENT"),event);
        free(labels);free(rows);gt4_vol_close(v);
        if(!ok){fprintf(stderr,"cannot decode RACE_EVENT\n");return 2;}
        d.event_row=event;d.championship=getenv("RACE_CHAMPIONSHIP")!=NULL;
        if(getenv("RACE_EVENT_GRID")){
            /* Integration fixture: seed is explicit. NTSC-U opponent
               exclusion is 00448C10 -> 0044A5A8 (always zero), E oracle.
               The real candidate list,
               course, count and prepared keys are read from the disc. */
            v=gt4_vol_open(argv[1],&error);uint32_t race_id;size_t cn=0,ln2=0,en=0;
            labels=gt4_vol_read(v,"/specdb/GT4_US2560/RACE.idi",&ln2);
            uint8_t *courses=gt4_vol_read(v,"/specdb/GT4_US2560/COURSE.idi",&cn);
            if(!gt4_specdb_label(labels,ln2,getenv("RACE_EVENT"),&race_id))return 2;
            char ep[96];snprintf(ep,sizeof ep,"/specdb/GT4_US2560/RACES/%016llX",(unsigned long long)(UINT64_C(0x2300000000)|race_id));
            uint8_t *enemies=gt4_vol_read(v,ep,&en);unsigned candidates=0;
            if(enemies && en>=8)memcpy(&candidates,enemies,4);
            uint32_t course_id=event[0]|(uint32_t)event[1]<<8;
            const char *course_name=gt4_specdb_label_for_id(courses,cn,course_id);
            if(!course_name || !candidates || candidates>256 || en<8+candidates*16 || event[0x3e]<1 || event[0x3e]>6)return 2;
            snprintf(d.course,sizeof d.course,"%s",course_name);d.cars=event[0x3e];
            uint8_t blocked[256]={0};unsigned chosen[5];
            for(unsigned i=0;i<candidates;i++){
                uint64_t key;memcpy(&key,enemies+8+i*16,8);
                blocked[i]=(uint8_t)gt4_career_enemy_blocked(key,0);
            }
            if(!gt4_career_enemy_indices(blocked,candidates,(unsigned)d.cars-1,&event_seed,chosen))return 2;
            d.event_rng=&event_seed;
            gt4_spec_tables *tables=gt4_spec_tables_open(v,NULL);
            gt4_specdb_src db={gt4_spec_tables_row,tables,gt4_spec_tables_str,gt4_spec_tables_car_parts};
            for(int i=1;i<d.cars;i++){
                memcpy(&d.tuned_car[i],enemies+8+chosen[i-1]*16,8);uint8_t tuned[32];
                memcpy(&d.event_paint[i],enemies+16+chosen[i-1]*16,8);
                if(db.row(db.ctx,0x26,(uint32_t)d.tuned_car[i],0,tuned,sizeof tuned)<8)return 2;
                memcpy(&d.car[i],tuned,4);
            }
            gt4_spec_tables_close(tables);free(labels);free(courses);free(enemies);gt4_vol_close(v);
            printf("Event disc grid: %s, %d entries, %u candidates.\n",d.course,d.cars,candidates);
        }
    }
    r = gt4_race_run_open(argv[1], &d, msg, sizeof msg);
    if (!r) {
        fprintf(stderr, "race: %s\n", msg);
        return 1;
    }
    if(d.event_row){
        uint32_t cond=gt4_race_run_conductor(r),race=GT4_U32(cond),world=GT4_U32(race+0x84);
        uint8_t rules[0x80];gt4_career_session_rules(event,rules);
        if(getenv("GT4_CAREER_EVENT_NEGATIVE"))rules[0x2c]^=1;
        /* Start modifies grid/penalty fields; retain a bounded assertion
           on the copied wear rule range, original world type and laps. */
        if(GT4_U32(world+0xd68)!=(d.championship?14u:13u) || GT4_U32(race)!=event[0x43] ||
           memcmp(gt4_ptr(cond+0xf818+0x2c),rules+0x2c,9)){
            fprintf(stderr,"event world/laps/wear rules changed\n");gt4_race_run_close(r);return 1;
        }
        printf("PASS event %s: GT mode %u, %u laps, original wear rules.\n",getenv("RACE_EVENT"),GT4_U32(world+0xd68),GT4_U32(race));
    }
    if(d.owned_car[0]){
        uint32_t race=GT4_U32(gt4_race_run_conductor(r));
        uint32_t entries=GT4_U32(race+0x60),entry=GT4_U32(GT4_U32(entries+8));
        if(getenv("GT4_CAREER_TEST_NEGATIVE"))selected[0x110]^=1;
        if(memcmp(gt4_ptr(entry+0x20),selected,sizeof selected) ||
           memcmp(gt4_ptr(entry+0x3468),owned+0x498,0x20)){
            fprintf(stderr,"owned setup/wear changed during race assembly\n");gt4_race_run_close(r);return 1;
        }
        puts("PASS owned setup and wear survive native race assembly.");
    }
    pad_fixed_car = gt4_race_run_car(r, 0);
    if (getenv("RACE_AUTOPILOT")) gt4_car_take_over(pad_fixed_car, 5);   /* the model drives the player's car too */
    gt4_phys_pad.read = pad_fixed_read;
    gt4_phys_pad.wheel_state = pad_fixed_wheel;
    gt4_phys_pad.write = pad_fixed_write;
    snprintf(path, sizeof path, "%s/cars.bin", argv[4]);
    cars = fopen(path, "wb");
    if (!cars) {
        fprintf(stderr, "cannot write %s\n", path);
        return 2;
    }
    fwrite(gt4_ptr(gt4_race_run_car(r, 0)), 1, (size_t)CAR_SIZE * (size_t)d.cars, cars);
    for (int k = 0; k < frames; k++) {
        if (!gt4_race_run_frame(r)) {
            fprintf(stderr, "frame %d failed\n", k);
            return 1;
        }
        fwrite(gt4_ptr(gt4_race_run_car(r, 0)), 1, (size_t)CAR_SIZE * (size_t)d.cars, cars);
        if (getenv("RACE_LOG") && k % 600 == 0) {
            uint32_t c = gt4_race_run_car(r, 0);
            printf("frame %d: lap %d finished %d dist %.0f laps %u\n", k, (int16_t)GT4_U16(c + 0x104 + 0x4ac), GT4_U8(c + 0x5b6), GT4_F32(c + 0x5a8),
                   GT4_U32(GT4_U32(gt4_race_run_conductor(r))));
        }
    }
    fclose(cars);
    snprintf(path, sizeof path, "%s/ram.bin", argv[4]);
    f = fopen(path, "wb");
    if (f) {
        fwrite(gt4_ptr(0), 1, 0x02000000u, f);
        fclose(f);
    }
    {
        uint32_t c = gt4_race_run_car(r, 0);
        printf("%d frames: car 0 at %.1f km/h, (%.1f, %.1f)\n", frames, GT4_F32(c + CAR_SPEED) * 3.6f, GT4_F32(c + CAR_POSITION), GT4_F32(c + CAR_POSITION + 4));
    }
    gt4_race_run_close(r);
    return 0;
}
