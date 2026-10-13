/* Read-only real-disc assembly audit. Unlike gt4model_survey, this checks
   the showroom/race loader after factory dimensions and wheels are applied.
   usage: gt4car_survey disc.iso [--race=1..6] [--labels LABEL ...]
   Original implementation; prints counts and names, no extracted assets. */
#include "car_model.h"
#include "specdb.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
typedef struct {unsigned checked,failed,missing_wheels,nonfinite;} audit;
static int race_cars;
static void check_car(gt4_vol *v,const car_model_tables *tables,const char *label,audit *s)
{
    s->checked++;car_model *a=race_cars?car_model_load_race(v,tables,label,0,race_cars,0):car_model_load(v,tables,label,0);
    if(!a){s->failed++;printf("FAIL %s: load\n",label);return;}
    int wheels=0,triangles=0,bad=0;
    for(int i=0;i<4;i++)if(a->wheel[i] && a->wheel[i]->count){wheels++;triangles+=a->wheel[i]->count;}
    for(int i=0;i<a->car->count;i++)for(int j=0;j<3;j++)for(int k=0;k<3;k++)
        if(!isfinite(a->car->triangles[i].v[j].p[k]))bad=1;
    if(!triangles)s->missing_wheels++;if(bad)s->nonfinite++;
    printf("%s %s model=%s body=%d wheel_parts=%d wheel_triangles=%d total=%d\n",
        triangles && !bad?"OK":"REVIEW",label,a->model,a->body_count,wheels,triangles,a->car->count);
    car_model_free(a);
}
int main(int argc,char **argv)
{
    if(argc>2 && !strncmp(argv[2],"--race=",7)) {
        race_cars=atoi(argv[2]+7);if(race_cars<1 || race_cars>6)return 2;
        for(int i=2;i+1<argc;i++)argv[i]=argv[i+1];argc--;
    }
    if(argc<2 || (argc>2 && (strcmp(argv[2],"--labels") || argc<4))){fprintf(stderr,"usage: gt4car_survey disc.iso [--race=1..6] [--labels LABEL ...]\n");return 2;}
    const char *err=NULL;gt4_vol *v=gt4_vol_open(argv[1],&err);if(!v){fprintf(stderr,"volume: %s\n",err?err:"failed");return 1;}
    car_model_tables *tables=car_model_tables_open(v);if(!tables){gt4_vol_close(v);return 1;}
    audit s={0};int malformed=0;
    if(argc>2)for(int i=3;i<argc;i++)check_car(v,tables,argv[i],&s);
    else {
        size_t size=0;uint8_t *labels=gt4_vol_read(v,"/specdb/GT4_US2560/GENERIC_CAR.idi",&size);
        unsigned count=labels && size>=16?u32(labels+4):0;
        if(!labels || size<16 || memcmp(labels,"GTID",4) || !count || count>(size-16)/8)malformed=1;
        else for(unsigned i=0;i<count;i++) {
            const char *label=gt4_specdb_label_for_id(labels,size,u32(labels+20+(size_t)i*8));
            if(!label){malformed=1;break;}check_car(v,tables,label,&s);
        }
        free(labels);
    }
    printf("assembly: %u checked, %u failed loads, %u missing wheel assemblies, %u nonfinite models\n",s.checked,s.failed,s.missing_wheels,s.nonfinite);
    car_model_tables_close(tables);gt4_vol_close(v);
    return malformed || s.failed || s.missing_wheels || s.nonfinite?1:0;
}
