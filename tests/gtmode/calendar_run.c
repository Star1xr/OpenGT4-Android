/* Test-only original date routine; no generated code enters the application. */
#include "ee.h"
#include "career_calendar.h"
#include "gtmode_game.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t address);
static int readfile(const char *path,uint8_t *out,size_t n){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
static int32_t bits(uint32_t v){int32_t n;memcpy(&n,&v,4);return n;}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
int main(int argc,char **argv){
    if(argc!=4){fprintf(stderr,"usage: gtmode_calendar_oracle RAM SPR ISO\n");return 2;}
    ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE);
    if(!ram || !spr || !readfile(argv[1],ram,EE_RAM_SIZE) || !readfile(argv[2],spr,EE_SPR_SIZE))return 2;
    gt4_career_calendar_data data;if(!gt4_career_calendar_data_load(argv[3],&data) || memcmp(data.year_offsets,ram+0x6cc718,16) || memcmp(data.month_offsets,ram+0x6cc728,52))return 2;
    uint8_t code[0xec];FILE *f=fopen("extracted/core.bin","rb");if(!f)return 2;
    int valid=!fseek(f,0x4797a8,SEEK_SET) && fread(code,1,sizeof code,f)==sizeof code && !memcmp(code,ram+0x5797a8,sizeof code);
    uint8_t elapsed_code[0x70];valid=valid && !fseek(f,0x32f4f0,SEEK_SET) && fread(elapsed_code,1,sizeof elapsed_code,f)==sizeof elapsed_code && !memcmp(elapsed_code,ram+0x42f4f0,sizeof elapsed_code);
    fclose(f);if(!valid)return 2;
    const uint32_t append_functions[]={0x42f0e0,0x6009f0,0x600a18,0x579898,0x579708,0x435148,0x42f4c8};const unsigned append_sizes[]={0xb4,0x28,0x28,0x1ac,0x40,0x5c,0x24};
    f=fopen("extracted/core.bin","rb");if(!f)return 2;
    for(unsigned i=0;i<7;i++){uint8_t checked[0x1ac];if(fseek(f,append_functions[i]-0x100000,SEEK_SET) || fread(checked,1,append_sizes[i],f)!=append_sizes[i] || memcmp(checked,ram+append_functions[i],append_sizes[i]))return 2;}fclose(f);
    memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);c.lookup=recomp_lookup;
    ee_fn fn=recomp_lookup(0x5797a8);if(!fn)return 2;jmp_buf jump;c.exit_jmp=&jump;
    unsigned cases=0;uint32_t random=0x92630;
    for(unsigned trial=0;trial<140000;trial++){
        int32_t y,m,d;
        if(trial<120000){y=1+trial/12;m=1+trial%12;d=1+trial%35;}
        else {random=random*1664525u+1013904223u;y=bits(random);random=random*1664525u+1013904223u;m=bits(random);random=random*1664525u+1013904223u;d=bits(random);}
        memset(c.r,0,sizeof c.r);c.r[4].sd[0]=y;c.r[5].sd[0]=m;c.r[6].sd[0]=d;c.end_reason=0;
        if(setjmp(jump)){fprintf(stderr,"original stopped: %s\n",c.end_detail);return 1;}fn(&c);
        int32_t expected=gt4_career_date_to_day(&data,y,m,d);
        if(c.r[2].uw[0]!=(uint32_t)expected){fprintf(stderr,"date mismatch y=%d m=%d d=%d original=%08x native=%08x\n",y,m,d,c.r[2].uw[0],(uint32_t)expected);return 1;}
        cases++;
        if(trial%512==511 && (memcmp(c.ram,ram,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)))return 1;
    }
    if(memcmp(c.ram,ram,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE))return 1;
    printf("func_005797A8: %u/%u identical date returns; full RAM/SPR unchanged every 512 calls and at end\n",cases,cases);
    const uint32_t profile=0x1edf000,stack=0x1ef0000;
    uint8_t *inverse_ram=malloc(EE_RAM_SIZE);if(!inverse_ram)return 2;
    for(unsigned trial=0;trial<2048;trial++){
        random=random*1664525u+1013904223u;
        int32_t input=trial<1024?gt4_career_date_to_day(&data,2005,4,2)+(int32_t)trial-512:bits(random);
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(inverse_ram,ram,EE_RAM_SIZE);
        memset(c.r,0,sizeof c.r);c.r[4].sd[0]=input;c.r[29].uw[0]=stack;c.end_reason=0;
        if(setjmp(jump))return 1;recomp_lookup(0x579898)(&c);
        uint32_t packed=gt4_career_day_to_date(&data,input);
        memset(inverse_ram+stack-48,0,48);put32(inverse_ram+stack-80,packed);put32(inverse_ram+stack-64,packed);
        if(c.r[2].uw[0]!=packed || memcmp(c.ram,inverse_ram,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
            fprintf(stderr,"inverse mismatch day=%d original=%08x native=%08x\n",input,c.r[2].uw[0],packed);return 1;
        }
    }
    free(inverse_ram);puts("func_00579898: 2048/2048 identical full-memory inverse date cases");
    uint8_t *acquired=malloc(EE_RAM_SIZE);if(!acquired)return 2;
    for(unsigned trial=0;trial<1024;trial++){
        memcpy(c.ram,ram,EE_RAM_SIZE);memset(c.r,0,sizeof c.r);
        for(unsigned i=0;i<32;i++){random=random*1664525u+1013904223u;c.ram[profile+i]=(uint8_t)(random>>24);}
        random=random*1664525u+1013904223u;int32_t day=trial<512?2453462+(int32_t)trial:bits(random);
        memcpy(acquired,c.ram,EE_RAM_SIZE);gt4_career_car_acquisition_day(acquired+profile,&data,day);
        memset(acquired+stack-32,0,24);put32(acquired+stack-48,0x435164);put32(acquired+stack-44,0);
        c.r[4].uw[0]=profile;c.r[5].sd[0]=day;c.r[29].uw[0]=stack;c.end_reason=0;
        if(setjmp(jump))return 1;recomp_lookup(0x435148)(&c);
        if(memcmp(c.ram,acquired,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
            for(unsigned i=0;i<EE_RAM_SIZE;i++)if(c.ram[i]!=acquired[i]){fprintf(stderr,"acquisition trial=%u mismatch=%08x original=%02x native=%02x\n",trial,i,c.ram[i],acquired[i]);break;}
            return 1;
        }
    }
    free(acquired);puts("func_00435148: 1024/1024 identical acquisition date cases; full RAM and SPR");
    unsigned elapsed_cases=0;
    const uint32_t functions[]={0x42f4f0,0x42f530};
    for(unsigned mode=0;mode<2;mode++)for(unsigned trial=0;trial<768;trial++){
        int32_t today;
        if(trial<256)today=gt4_career_date_to_day(&data,2005,4,2)+((int32_t)trial-128);
        else {random=random*1664525u+1013904223u;today=bits(random);}
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.ram+profile+0xb6c0,&today,4);
        uint8_t *expected_ram=malloc(EE_RAM_SIZE);if(!expected_ram)return 2;
        memcpy(expected_ram,c.ram,EE_RAM_SIZE);
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=profile;c.r[29].uw[0]=stack;c.end_reason=0;
        if(setjmp(jump))return 1;recomp_lookup(functions[mode])(&c);
        int32_t expected=mode?gt4_career_elapsed_weeks(&data,today):gt4_career_elapsed_days(&data,today);
        /* ABI-only stores: native arithmetic deliberately has no guest stack. */
        uint64_t zero=0,ra=mode?0x42f540:0;
        memcpy(expected_ram+stack-16,&zero,8);
        if(mode){memcpy(expected_ram+stack-32,&zero,8);memcpy(expected_ram+stack-24,&ra,8);}
        else {memcpy(expected_ram+stack-8,&zero,8);}
        if(c.r[2].uw[0]!=(uint32_t)expected || memcmp(c.ram,expected_ram,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
            fprintf(stderr,"elapsed mismatch mode=%u today=%d original=%08x native=%08x\n",mode,today,c.r[2].uw[0],(uint32_t)expected);return 1;
        }
        free(expected_ram);elapsed_cases++;
    }
    printf("func_0042F4F0/0042F530: %u/%u identical full-memory elapsed cases\n",elapsed_cases,elapsed_cases);
    unsigned event_cases=0;const uint32_t ring=0x1eb0000,event=0x1ea0000;
    uint8_t *expected_ram=malloc(EE_RAM_SIZE);if(!expected_ram)return 2;
    for(unsigned trial=0;trial<2048;trial++){
        memcpy(c.ram,ram,EE_RAM_SIZE);
        uint32_t count=trial%4==0?0:trial%4==1?1:trial%4==2?2921:2922;
        uint32_t next=trial<4?2921:trial<8?0:(trial*7919u)%2922;
        put32(c.ram+ring+8,next);put32(c.ram+ring+0xc,(next+2922-count)%2922);put32(c.ram+ring+0x10,count);
        random=random*1664525u+1013904223u;put32(c.ram+ring+0xb6c0,trial==0?UINT32_MAX:random);
        for(unsigned i=0;i<16;i++){random=random*1664525u+1013904223u;c.ram[event+i]=(uint8_t)(random>>24);}
        memcpy(expected_ram,c.ram,EE_RAM_SIZE);gt4_career_calendar_append(expected_ram+ring,expected_ram+event);
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=ring;c.r[5].uw[0]=event;c.r[29].uw[0]=stack;c.end_reason=0;
        if(setjmp(jump))return 1;recomp_lookup(0x42f0e0)(&c);
        memset(expected_ram+stack-48,0,40); /* five ABI-only sd stores */
        if(memcmp(c.ram,expected_ram,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
            fprintf(stderr,"calendar event memory mismatch count=%u next=%u\n",count,next);return 1;
        }event_cases++;
    }
    free(expected_ram);printf("func_0042F0E0: %u/%u identical full-memory append cases\n",event_cases,event_cases);
    if(!gt4_game_load_disc(argv[3]))return 2;
    ah_vm *vm=ah_vm_new();ah_obj *calendar=ah_module_new(vm,"calendar",ah_root(vm),NULL,NULL);gt4_calendar_bind(vm,calendar);
    ah_val args[]={ah_int(2000),ah_int(-14),ah_int(40)},ret;
    if(ah_call_member(vm,calendar,"dateToDay",3,args,&ret)<1 || ret.u.i!=gt4_career_date_to_day(&data,2000,-14,40))return 1;
    ah_val day_arg=ah_int(2453463),date_value;
    if(ah_call_member(vm,calendar,"dayToDate",1,&day_arg,&date_value)<1)return 1;
    if(ah_array_get(date_value.u.o,0).u.i!=2005 || ah_array_get(date_value.u.o,1).u.i!=4 || ah_array_get(date_value.u.o,2).u.i!=3)return 1;
    if(ah_call_member(vm,calendar,"dayToGameDay",1,&day_arg,&ret)<1 || ret.u.i!=1)return 1;
    day_arg=ah_int(1);
    if(ah_call_member(vm,calendar,"gameDayToDay",1,&day_arg,&ret)<1 || ret.u.i!=2453463)return 1;
    int decoded[3];if(!gt4_game_decode_calendar_day(2453463,decoded) || decoded[0]!=2005 || decoded[1]!=4 || decoded[2]!=3)return 1;
    ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);ah_set_member(vm,ah_root(vm),"game",ah_objv(game));gt4_game_bind(vm,game);
    ah_val owner;if(!ah_get_member(vm,game,"calendar",&owner))return 1;
    ah_obj *date=ah_array_new(vm);ah_array_push(vm,date,ah_int(2005));ah_array_push(vm,date,ah_int(4));ah_array_push(vm,date,ah_int(16));
    ah_set_member(vm,owner.u.o,"today",ah_objv(date));ah_set_member(vm,owner.u.o,"elapsed_days",ah_int(999));
    if(ah_call_member(vm,owner.u.o,"getElapsedDate",0,NULL,&ret)<1 || ret.u.i!=14)return 1;
    if(ah_call_member(vm,owner.u.o,"getElapsedWeek",0,NULL,&ret)<1 || ret.u.i!=2)return 1;
    ah_vm_free(vm);puts("disc-backed Adhoc date adapter: passed");
    free(spr);free(ram);ee_free(&c);return 0;
}
