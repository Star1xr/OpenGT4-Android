/* Native GDGT contact -> RaceLicense coffee counter, dynamics and reset. */
#include "career_license_disc.h"
#include "physics/course_objects.h"
#include "physics/car.h"
#include "physics/license_session.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"coffee objects line %d: %s (%s)\n",__LINE__,#x,error);return 1;}}while(0)
int main(int argc,char **argv)
{
    if(argc<2)return 2;char error[256]="";gt4_career_race_lookup_data lookup;
    CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
    gt4_career_license_exam exam;
    const char *code=argc>2?argv[2]:"l0c0001";
    CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,code,error,sizeof error));
    gt4_race_run *run=gt4_race_run_open(argv[1],&exam.desc,error,sizeof error);CHECK(run);
    uint32_t cond=gt4_race_run_conductor(run),car=gt4_race_run_car(run,0),h=cond+0xda78;
    gt4_course_gadgets *g=(gt4_course_gadgets *)gt4_race_run_objects(run);CHECK(g);
    int section=(int32_t)GT4_U32(GT4_U32(cond+0xcbd8)+0xb0);
    unsigned slot=section>=0?(unsigned)section+1:0;
    CHECK(slot<=g->sections);unsigned hits=0;int limit=GT4_U8(cond+0xf85e);
    printf("%s coffee: %s car %x, target %d, %u global / %u local props\n",code,exam.desc.course,exam.desc.car[0],limit,g->length[0],slot?g->length[slot]:0);
    CHECK(limit==exam.row[0x75]);unsigned target=limit?(unsigned)limit:1;
    /* Feed stationary cones into the real car hull, one at a time. This
       isolates contact from steering; geometry, masses and supports come
       unchanged from the disc. RaceSession receives the actual impacts. */
    gt4_walls_in(cond);GT4_U8(car+0x56a)=GT4_U8(car+0x56b)=0;
    for(unsigned pass=0;pass<2;pass++){
        if(pass && !slot)continue;
        unsigned s=pass?slot:0;
        for(unsigned j=0;j<g->length[s] && hits<target;j++){
            gt4_course_gadget *p=g->instance+g->first[s]+j;
            if(p->fixed || !p->descriptor[3])continue;
            float old[3];memcpy(old,p->pos,12);
            GT4_F32(h+0x454)=p->pos[0];GT4_F32(h+0x458)=-p->pos[2];GT4_F32(h+0x45c)=p->pos[1];
            GT4_F32(h+0x474)=0;GT4_F32(h+0x460)=5;GT4_F32(h+0x464)=5;GT4_F32(h+0x468)=1;GT4_F32(h+0x480)=0;
            if(!getenv("GT4_COFFEE_CONTACT_NEGATIVE"))gt4_course_objects_step(g,cond,gt4_f32(0x3c888888));
            hits++;if(limit){CHECK(p->age==1);CHECK(GT4_U16(cond+0x1003c)==hits);}
            else CHECK(GT4_U8(car+0x5b7)&1);
            CHECK(fabsf(p->velocity[0])+fabsf(p->velocity[1])+fabsf(p->velocity[2])>0);
            gt4_course_objects_step(g,cond,gt4_f32(0x3c888888));if(limit)CHECK(GT4_U16(cond+0x1003c)==hits);
            CHECK(memcmp(old,p->pos,12));
            for(int frame=0;frame<110;frame++)gt4_course_objects_step(g,cond,gt4_f32(0x3c888888));
            if(limit)CHECK(!p->active);
        }
    }
    CHECK(hits==target);gt4_license_evaluate(cond,car);CHECK(gt4_race_run_finished(run));
    gt4_race_run_close(run);
    run=gt4_race_run_open(argv[1],&exam.desc,error,sizeof error);CHECK(run);
    cond=gt4_race_run_conductor(run);g=(gt4_course_gadgets *)gt4_race_run_objects(run);
    CHECK(g && !GT4_U16(cond+0x1003c));for(unsigned i=0;i<g->count;i++)CHECK(g->instance[i].active && !g->instance[i].age);
    gt4_race_run_close(run);puts("PASS coffee rules, cone impulses/movement, single counting, finish and retry reset.");return 0;
}
