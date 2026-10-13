/* Diagnostic: the Tune Shop's before/after power for a car found in a
   PCSX2 RAM snapshot (its 0x4b8 garage body, located by the car key of a
   freshly built body of the same label), and for that fresh body. Power
   through gt4_career_vehicle_stats, as getCurrentPower; the muffler part
   is installed by key only. No original code is executed. */
#include "career_vehicle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int hp(void *v,const uint8_t body[0x4b8]){int32_t s[3];return gt4_career_vehicle_stats(v,body,s)?(int)((float)s[0]/100.f+.5f):-1;}
static void report(void *v,const char *what,uint8_t body[0x4b8],uint64_t part){
    printf("%s: odometer=%u wear=",what,r32(body+0x498));
    for(int i=0;i<32;i++)printf("%02x",body[0x498+i]);
    printf(" power=%d HP",hp(v,body));
    if(part!=UINT64_MAX){uint8_t copy[0x4b8];memcpy(copy,body,sizeof copy);
        if(gt4_career_install_key_only(copy,part))printf(" -> %d HP with part %016llx",hp(v,copy),(unsigned long long)part);
        else printf(" (part %016llx needs defaults)",(unsigned long long)part);}
    printf("\n");
}
int main(int argc,char **argv){
    if(argc<3){fprintf(stderr,"usage: tune_power_probe ISO label [eeMemory.bin [part-key]]\n");return 2;}
    void *v=gt4_career_vehicle_open(argv[1]);if(!v){fprintf(stderr,"vehicle data unavailable\n");return 2;}
    uint8_t unit[32],body[0x4b8];
    if(!gt4_career_vehicle_create(v,argv[2],0,0,unit,body)){fprintf(stderr,"cannot build %s\n",argv[2]);return 2;}
    uint64_t part=argc>4?strtoull(argv[4],NULL,0):UINT64_MAX;
    report(v,"new",body,part);
    if(argc<4)return 0;
    if(!strcmp(argv[3],"--mileage")){   /* used bodies: odometer values from the arguments that follow */
        for(int i=4;i<argc;i++){uint64_t km=strtoull(argv[i],NULL,0);
            if(!gt4_career_vehicle_create(v,argv[2],0,km,unit,body))return 2;
            char what[64];snprintf(what,sizeof what,"mileage %llu",(unsigned long long)km);report(v,what,body,UINT64_MAX);}
        return 0;
    }
    FILE *f=fopen(argv[3],"rb");if(!f)return 2;
    uint8_t *ram=malloc(32u<<20);size_t n=fread(ram,1,32u<<20,f);fclose(f);
    for(size_t i=8;i+0x4b8<=n;i+=4)if(!memcmp(ram+i,body,8) && r32(ram+i+0x488)<3){
        uint8_t found[0x4b8];memcpy(found,ram+i,sizeof found);
        char what[64];snprintf(what,sizeof what,"snapshot %08zx",i);report(v,what,found,part);
    }
    return 0;
}
