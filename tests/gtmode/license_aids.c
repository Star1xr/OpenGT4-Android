/* Licence aids integration: authored exam setups, not global user options.
   No dynamics formulas or parameters are changed by this test. */
#include "career_license_disc.h"
#include "car.h"
#include "license_session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"licence aids %s line %d: %s\n",code,__LINE__,#x); return 1; } } while (0)
static uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
int main(int argc,char **argv)
{
    if(argc<2)return 2;const char *code="lookup";
    uint8_t *reference=NULL;int negative=argc>2 && !strcmp(argv[2],"--negative");
    if(argc>3 && !strcmp(argv[2],"--reference")){
        reference=malloc(32u*1024*1024);CHECK(reference);FILE *in=fopen(argv[3],"rb");CHECK(in);
        CHECK(fread(reference,1,32u*1024*1024,in)==32u*1024*1024);fclose(in);
        negative=argc>4;
    }
    gt4_career_race_lookup_data lookup;CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
    const char *groups[]={"l0b","l0a","lib","lia","l0s"};unsigned count=0;
    for(unsigned g=0;g<5;g++)for(unsigned n=1;n<=16;n++){
        char name[32],error[256];snprintf(name,sizeof name,"%s%04u",groups[g],n);code=name;
        gt4_career_license_exam exam;CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,code,error,sizeof error));
        CHECK(exam.desc.assist_asm==1 && exam.desc.assist_tcs==1);
        if(negative && !reference && !count)exam.desc.assist_tcs=0;
        gt4_race_run *run=gt4_race_run_open(argv[1],&exam.desc,error,sizeof error);CHECK(run);
        uint32_t cond=gt4_race_run_conductor(run),race=GT4_U32(cond);
        uint32_t entry=GT4_U32(GT4_U32(GT4_U32(race+0x60)+8));
        /* 003B4B38 defaults; 0010F880 explicitly sets 1/1 for training. */
        CHECK(GT4_U32(entry+0x3450)==1 && GT4_U32(entry+0x3454)==1);
        uint32_t car=gt4_race_run_car(run,0),params=GT4_U32(car+CAR_PARAMS),a=params+0x1098;
        if(reference && !count){
            uint32_t captured=0;
            for(uint32_t at=0x100000;at<0x2000000-0x10144;at+=4){
                if(word(reference+at+0x10140)!=GT4_LICENSE_SESSION_VT)continue;
                uint32_t r=word(reference+at),entries=r<0x2000000-0x64?word(reference+r+0x60):0;
                uint32_t list=entries && entries<0x2000000-12?word(reference+entries+8):0;
                uint32_t e=list && list<0x2000000-4?word(reference+list):0;
                if(!e || e>=0x2000000-0x3460 || word(reference+e+0x20)!=exam.desc.car[0])continue;
                uint32_t c=word(reference+e+0x18),p=c && c<0x2000000-0x1000?word(reference+c+CAR_PARAMS):0;
                if(!p || p>=0x2000000-0x1100)continue;
                CHECK(word(reference+e+0x3450)==GT4_U32(entry+0x3450));
                CHECK(word(reference+e+0x3454)==GT4_U32(entry+0x3454));
                uint8_t actual[0x34];memcpy(actual,gt4_ptr(a),sizeof actual);
                if(negative)actual[0x20]^=1; /* Wrong TCS level must be detected. */
                CHECK(!memcmp(actual,reference+p+0x1098,sizeof actual));
                CHECK(GT4_U8(car+0x5e9)==reference[c+0x5e9]);
                gt4_race_hud hud;CHECK(gt4_race_run_hud(run,0,&hud));
                int displayed=reference[c+0x6fc]>reference[c+0x6fd]?reference[c+0x6fc]:reference[c+0x6fd];
                CHECK(hud.aid==displayed);captured=c;break;
            }
            CHECK(captured);printf("PASS B-1 PCSX2: entry enables, aid mask and 52 parameter bytes, zero tolerance.\n");
        }
        printf("%s car %x flags %u levels %g/%g/%g/%g switches %u/%u/%u/%u\n",code,exam.desc.car[0],GT4_U8(car+0x5e9),
            GT4_F32(a),GT4_F32(a+8),GT4_F32(a+0x10),GT4_F32(a+0x20),
            GT4_U8(a+0x30),GT4_U8(a+0x31),GT4_U8(a+0x32),GT4_U8(a+0x33));
        gt4_race_run_close(run);count++;
    }
    free(reference);printf("PASS %u authored licence assist entry flags.\n",count);return 0;
}
