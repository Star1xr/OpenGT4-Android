/* GDGT / RNW4 selection, authored meshes and 003E24F0 draw matrices.
   Original sin/cos uses an explicit host boundary; this is C, not GS E. */
#include "ee.h"
#include "course_gadget.h"
#include "career_license_disc.h"
#include "career_original.h"
#include "vol.h"
#include <math.h>
#include <fenv.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"course gadget line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void sincos_boundary(ee_ctx *e)
{float a=e->fpr[12].f,s=sinf(a),c=cosf(a);memcpy(e->ram+e->r[4].uw[0],&s,4);memcpy(e->ram+e->r[5].uw[0],&c,4);}
static ee_fn boundary(uint32_t a){return a==0x57d8a0?sincos_boundary:NULL;}
static ee_fn lookup(uint32_t a){ee_fn f=boundary(a);return f?f:recomp_lookup(a);}
int main(int argc,char **argv)
{
    CHECK(argc==3);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[2],0x100000)>0x5a3900);
    e.lookup=lookup;e.hook=boundary;
    const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol);
    gt4_career_race_lookup_data data;CHECK(gt4_career_race_lookup_data_load(argv[1],&data));
    const char *prefixes[]={"l0b","l0a","lib","lia","l0s"};char courses[80][32];unsigned nc=0,exams=0,instances=0,models=0;
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    for(unsigned group=0;group<5;group++)for(unsigned number=1;number<=16;number++){
        char code[32],msg[256];snprintf(code,sizeof code,"%s%04u",prefixes[group],number);
        gt4_career_license_exam exam;CHECK(gt4_career_license_prepare(&exam,argv[1],&data,code,msg,sizeof msg));exams++;
        unsigned i;for(i=0;i<nc;i++)if(!strcmp(courses[i],exam.desc.course))break;
        if(i<nc)continue;strcpy(courses[nc++],exam.desc.course);
        char path[80];snprintf(path,sizeof path,"/crs/%s",exam.desc.course);size_t size;uint8_t *raw=gt4_vol_read(vol,path,&size);CHECK(raw);
        gt4_course c;CHECK(gt4_course_open(&c,raw,size));gt4_course_gadgets *g=gt4_course_gadgets_open(&c);
        uint32_t gdgt;memcpy(&gdgt,raw+0xbc,4);
        CHECK(!gdgt || g);if(g){
            for(unsigned j=0;j<g->types;j++){
                CHECK(g->model[j] && g->model[j]->count>0 && !g->model[j]->unsupported);
                models++;for(int t=0;t<g->model[j]->count;t++)for(unsigned k=0;k<3;k++)
                    for(unsigned a=0;a<3;a++)CHECK(isfinite(g->model[j]->triangles[t].v[k].p[a]));
            }
            for(unsigned j=0;j<g->count;j++){
                gt4_course_gadget *p=g->instance+j;CHECK(p->type<g->types);
                e.r[4].uw[0]=0x1800000;e.r[29].uw[0]=0x1ef0000;
                for(unsigned k=0;k<3;k++)e.fpr[12+k].f=p->angles[k];
                jmp_buf jump;e.exit_jmp=&jump;CHECK(!setjmp(jump));CHECK(recomp_lookup(0x3e24f0));recomp_lookup(0x3e24f0)(&e);
                float r[9];gt4_course_gadget_rotation(p->angles,r);
                if(getenv("GT4_GADGET_MATRIX_NEGATIVE"))r[0]+=.01f;
                for(unsigned a=0;a<3;a++)for(unsigned b=0;b<3;b++){
                    float expected;memcpy(&expected,e.ram+0x1800000+b*16+a*4,4);
                    CHECK(!memcmp(&r[a*3+b],&expected,4));
                }instances++;
            }
            if(!strcmp(exam.desc.course,"deepforest")){
                CHECK(g->sections==4 && g->length[3]==25);
                const unsigned expected[]={2,2,5,3,3,1};
                for(unsigned j=0;j<6;j++){
                    unsigned type=g->instance[g->first[3]+j].type;
                    if(getenv("GT4_GADGET_SELECTION_NEGATIVE"))type++;
                    CHECK(type==expected[j]);
                }
            }
            printf("objects %s: %u types, %u sections, %u placements\n",exam.desc.course,g->types,g->sections,g->count);
            /* Invalid dynamic mass must never reach the native solver. */
            for(unsigned j=0;j<g->count;j++)if(!g->instance[j].fixed && g->instance[j].descriptor[3]){
                size_t at=gdgt+32+g->instance[j].type*160+0x94;
                uint8_t saved[4];memcpy(saved,raw+at,4);memset(raw+at,0,4);
                CHECK(!gt4_course_gadgets_open_state(&c));memcpy(raw+at,saved,4);break;
            }
            /* Corrupt a section's placement pointer: rejection, not OOB. */
            if(g->sections){uint32_t table;memcpy(&table,raw+c.runway+0x9c,4);uint32_t bad=0xffffffffu;memcpy(raw+c.runway+table+28,&bad,4);
                CHECK(!gt4_course_gadgets_open(&c));}
            gt4_course_gadgets_close(g);
        }free(raw);
    }
    fesetenv(&env);printf("PASS %u licenses, %u courses, %u GDGT models, %u original draw matrices; malformed pointer rejected.\n",exams,nc,models,instances);
    gt4_vol_close(vol);ee_free(&e);return 0;
}
