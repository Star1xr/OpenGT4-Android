/* Execute the original controller, stubbing only SPU voice allocation.
   Selection, integer pitch, table crossfade and slew run as EE code. */
#include "ee.h"
#include "race_sound.h"
#include "pcm_music.h"
#include "save.h"
#include <time.h>
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"race sound line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
ee_fn recomp_lookup(uint32_t);
static unsigned fade_events;
static void no_op(ee_ctx *c){(void)c;}
static void fade_event(ee_ctx *c){CHECK(c->r[5].uw[0]==1 && !c->r[6].uw[0] && !c->r[7].uw[0]);fade_events++;}
static void allocate(ee_ctx *c){c->r[2].uw[0]=0x1810000;}
static uint32_t playlist_records;static unsigned playlist_count;
static void playlist_group(ee_ctx *c)
{
    CHECK(c->r[5].uw[0]==12);
    ee_w32(c,c->r[4].uw[0],playlist_records);
    ee_w32(c,c->r[4].uw[0]+4,playlist_count);
}
static void replay_actor(ee_ctx *c){c->r[2].uw[0]=0x1870000;}
static void replay_tape(ee_ctx *c){c->r[2].uw[0]=0x1870100;}
static ee_fn boundary(uint32_t pc)
{
    if(pc==0x3b76f0)return fade_event;
    if(pc==0x1fff000)return replay_actor;
    if(pc==0x1fff004)return replay_tape;
    if(pc==0x1fff008 || pc==0x373da0 || pc==0x391e50 || pc==0x3c0208 || pc==0x391ed0 ||
       pc==0x394d80 || pc==0x397cd8 || pc==0x34db30 || pc==0x3c0310 || pc==0x426bc8 || pc==0x387e88)return no_op;
    if(pc==0x461e50)return playlist_group;
    if(pc==0x55d0b0)return allocate;
    return pc==0x578500 || pc==0x578168 || pc==0x55d688 || pc==0x55cfe8 || pc==0x576100 || pc==0x576140 || pc==0x55d6d0?no_op:NULL;
}
static ee_fn lookup(uint32_t pc){ee_fn fn=boundary(pc);return fn?fn:recomp_lookup(pc);}
static int invoke(ee_ctx *c,uint32_t pc)
{jmp_buf j;c->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"%s\n",c->end_detail);return 0;}ee_fn fn=c->lookup(pc);if(!fn)return 0;fn(c);return 1;}
static unsigned seed=0x558fd8;
static unsigned random_word(void){seed=seed*1664525u+1013904223u;return seed;}
static void playlist_order_test(ee_ctx *c,int negative)
{
    const uint32_t object=0x1830000;
    for(unsigned test=0;test<10000;test++) {
        gt4_music_playlist native;
        for(unsigned i=0;i<sizeof native.bytes;i++)native.bytes[i]=(uint8_t)random_word();
        unsigned count=test<129?test:random_word()%129;
        memset(native.bytes,0,4);native.bytes[0]=(uint8_t)count;
        for(unsigned i=0;i<count;i++)native.bytes[16+i*16+9]=(uint8_t)(random_word()>>31);
        int shuffle=(int)(test&1);
        memcpy(c->ram+object,native.bytes,sizeof native.bytes);
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->r[5].uw[0]=shuffle;c->r[29].uw[0]=0x1ef0000;
        CHECK(invoke(c,0x437e98));gt4_music_playlist_order(&native,shuffle);
        if(negative && test==1)native.bytes[8]^=1;
        CHECK(!memcmp(c->ram+object,native.bytes,sizeof native.bytes));
        for(unsigned step=0;step<count*3+1;step++) {
            memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->r[29].uw[0]=0x1ef0000;
            CHECK(invoke(c,0x438170));int selected=gt4_music_playlist_next(&native);
            uint32_t expected=selected<0?0:object+16+(unsigned)selected*16;
            CHECK(c->r[2].uw[0]==expected);
            CHECK(!memcmp(c->ram+object,native.bytes,sizeof native.bytes));
        }
    }
    puts("PASS 10000 byte-exact original jukebox orders and three playback cycles each.");
}
static uint32_t disk_word(const uint8_t *data,size_t bytes,size_t at)
{
    CHECK(at<=bytes && bytes-at>=4);
    return data[at]|(uint32_t)data[at+1]<<8|(uint32_t)data[at+2]<<16|(uint32_t)data[at+3]<<24;
}
static const char *disk_string(const uint8_t *data,size_t bytes,size_t at)
{
    uint32_t off=disk_word(data,bytes,at);CHECK(off<bytes && memchr(data+off,0,bytes-off));
    return (const char *)data+off;
}
static void race_jingle_test(gt4_vol *vol,int negative)
{
    /* ADS group 2 is racing; 006238C0 groups 9..15 are license events.
       Compare file identities: race_84 and license_failed have the same
       title (Oh Yeah), but are distinct authored streams. */
    size_t bytes=0;uint8_t *data=gt4_vol_read(vol,"/bgm/us/ads.inf",&bytes);CHECK(data);
    unsigned sequences[128],count=gt4_pcm_music_race_sequences(vol,sequences,128);
    CHECK(count==50);uint32_t racing=disk_word(data,bytes,36);
    unsigned comparisons=0,shared_titles=0;
    for(unsigned group=9;group<=15;group++) {
        uint32_t records=disk_word(data,bytes,20+group*8),n=disk_word(data,bytes,24+group*8);
        CHECK(n>0);
        for(unsigned j=0;j<n;j++)for(unsigned i=0;i<count;i++) {
            const char *jingle=disk_string(data,bytes,records+j*24+4);
            const char *track=disk_string(data,bytes,racing+sequences[i]*24+4);
            if(negative && !comparisons)track=jingle;
            CHECK(strcmp(track,jingle)!=0);
            shared_titles+=!strcmp(disk_string(data,bytes,records+j*24+8),
                                   disk_string(data,bytes,racing+sequences[i]*24+8));
            comparisons++;
        }
    }
    CHECK(shared_titles==1);
    printf("PASS %u license/race file comparisons; shared title does not select a jingle.\n",comparisons);
    free(data);
}
static void race_playlist_test(ee_ctx *c,gt4_vol *vol,int negative)
{
    size_t bytes=0;uint8_t *data=gt4_vol_read(vol,"/bgm/us/ads.inf",&bytes);CHECK(data && bytes>44);
    const uint32_t base=0x1820000,object=0x1830000;
    memcpy(c->ram+base,data,bytes);free(data);
    playlist_records=base+ee_r32(c,base+36);playlist_count=ee_r32(c,base+40);
    CHECK(playlist_count==65);
    for(unsigned i=0;i<playlist_count;i++)for(unsigned field=0;field<6;field++) {
        uint32_t at=playlist_records+i*24+field*4;
        ee_w32(c,at,base+ee_r32(c,at));
    }
    unsigned sequences[128],count=gt4_pcm_music_race_sequences(vol,sequences,128);
    CHECK(count==50);
    memset(c->ram+object,0,0x810);memset(c->r,0,sizeof c->r);
    c->r[4].uw[0]=object;c->r[5].uw[0]=0x12345678;c->r[29].uw[0]=0x1ef0000;
    CHECK(invoke(c,0x437b10));CHECK(c->ram[object+6]==1);
    CHECK(ee_r32(c,object)==count);
    if(negative)sequences[0]=0; /* Reintroduce Photo Travel: must fail. */
    for(unsigned i=0;i<count;i++) {
        uint32_t at=object+16+i*16;
        CHECK(c->ram[at+9]==1);
        CHECK(gt4_music_playlist_sequence(vol,(const gt4_music_playlist *)(c->ram+object),i)==(int)sequences[i]);
    }
    CHECK(!gt4_pcm_music_race_sequences(vol,sequences,count-1));
    gt4_music_playlist native;CHECK(gt4_music_playlist_init(&native,vol,0x12345678));
    CHECK(!memcmp(native.bytes,c->ram+object,sizeof native.bytes));
    printf("PASS %u default race tracks against original 00437B10; Photo Travel excluded.\n",count);
}
static void defaults_test(ee_ctx *c,gt4_vol *vol,int negative)
{
    gt4_music_playlist native;CHECK(gt4_music_playlist_default(&native,vol,0x12345678));
    const uint32_t object=0x1830000;memset(c->ram+object,0,0x810);
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->r[5].uw[0]=0x12345678;c->r[29].uw[0]=0x1ef0000;
    CHECK(invoke(c,0x4379f0));
    size_t bytes;uint8_t *data=gt4_vol_read(vol,"/config/config-main.adc",&bytes);CHECK(data);
    char err[256];ah_code *code=ah_load(data,bytes,err,sizeof err);free(data);CHECK(code);
    ah_code *frame=NULL;for(int i=0;i<code->count;i++)if(code->ins[i].s && !strcmp(code->ins[i].s,"init_raceplaylist"))frame=code->ins[i].frame;CHECK(frame);
    int target=-1;
    for(int i=0;i+2<frame->count;i++)if(frame->ins[i].op==AH_STRING_CONST && !strcmp(frame->ins[i].s,"us") &&
        frame->ins[i+2].op==AH_JUMP_IF_TRUE)target=(int)frame->ins[i+2].i;
    CHECK(target>=0 && target<frame->count);unsigned indices[128],n=0;
    for(int i=target;i<frame->count;i++) {
        if(frame->ins[i].op==AH_INT_CONST){CHECK(n<128);indices[n++]=(unsigned)frame->ins[i].i;}
        if(frame->ins[i].op==AH_ARRAY_CONST_OLD){CHECK(frame->ins[i].i==(int)n);break;}
    }
    CHECK(n==36);
    for(unsigned i=0;i<65;i++){uint8_t *e=c->ram+object+16+i*16;e[8]=e[9]=0;e[10]=e[11]=255;}
    for(unsigned i=0;i<n;i++){CHECK(indices[i]<65);uint8_t *e=c->ram+object+16+indices[i]*16;e[9]=1;e[10]=e[11]=(uint8_t)i;}
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->r[5].uw[0]=65;CHECK(invoke(c,0x438150));
    if(negative)native.bytes[16+indices[0]*16+10]^=1;
    CHECK(!memcmp(native.bytes,c->ram+object,sizeof native.bytes));ah_free(code);
    puts("PASS all 0x810 default option bytes: original 004379F0 keys and authored US list/order/flags.");
}
static void preferences_test(ee_ctx *c,gt4_vol *vol,int negative)
{
    gt4_music_playlist original,restored;CHECK(gt4_music_playlist_default(&original,vol,0x12345678));
    CHECK(original.bytes[0]==65 && original.bytes[6]==0);
    unsigned enabled=0;for(unsigned i=0;i<65;i++)enabled+=original.bytes[16+i*16+9]!=0;CHECK(enabled==36);
    /* Full byte preservation follows 0033C180/0033C278; real disk reload
       proves saved seed, cursor, order, enabled and consumed entries survive. */
    char directory[160];snprintf(directory,sizeof directory,"build/music-oracle-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
    gt4_save_init(directory);CHECK(gt4_save_create(1,"Music validation"));
    ah_vm *vm=ah_vm_new();ah_obj *game_obj=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);
    ah_obj *option=ah_module_new(vm,"option",game_obj,NULL,NULL);ah_set_member(vm,game_obj,"option",ah_objv(option));
    const char *names[]={"race_bgm","replay_bgm","race_bgm_volume_raw","replay_bgm_volume_raw"};
    const int values[]={0,1,37,115};
    for(unsigned i=0;i<4;i++)ah_set_member(vm,option,names[i],i<2?ah_bool(values[i]):ah_int(values[i]));
    CHECK(gt4_save_capture(vm,game_obj,1));ah_vm_free(vm);
    for(unsigned i=0;i<17;i++)CHECK(gt4_music_playlist_next(&original)>=0);
    CHECK(gt4_save_option_blob_set("bgm_playlist_ps2",original.bytes,sizeof original.bytes));
    CHECK(gt4_save_store());gt4_save_shutdown();gt4_save_init(directory);CHECK(gt4_save_active()==1);
    CHECK(gt4_save_option_blob_get("bgm_playlist_ps2",restored.bytes,sizeof restored.bytes));
    for(unsigned i=0;i<4;i++)CHECK(gt4_save_option_number(names[i],-1)==values[i]);
    if(negative)restored.bytes[4]^=1;
    CHECK(!memcmp(original.bytes,restored.bytes,sizeof original.bytes));
    uint32_t world=0x1800000,game=0x1900000;ee_w32(c,0x622f4c,game);ee_w32(c,world+0xcc8,0);
    memcpy(c->ram+game+0x38d40,restored.bytes,sizeof restored.bytes);
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=world;CHECK(invoke(c,0x33c180));
    CHECK(!memcmp(c->ram+world+0xd150,original.bytes,sizeof original.bytes));
    memcpy(c->ram+world+0xd960,restored.bytes,sizeof restored.bytes);
    for(unsigned i=0;i<108;i++) {
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=world+0xd960;c->r[29].uw[0]=0x1ef0000;
        CHECK(invoke(c,0x438170));int entry=gt4_music_playlist_next(&restored);
        CHECK(c->r[2].uw[0]==(entry<0?0:world+0xd970+(unsigned)entry*16));
        CHECK(!memcmp(c->ram+world+0xd960,restored.bytes,sizeof restored.bytes));
    }
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=world;CHECK(invoke(c,0x33c278));
    CHECK(!memcmp(c->ram+game+0x38d40,restored.bytes,sizeof restored.bytes));
    for(unsigned mode=0;mode<4;mode++)for(int override=0;override<2;override++) {
        int use=gt4_music_playlist_uses_preferences(mode,override);
        ee_w32(c,world+0xcc8,mode);ee_w32(c,world+0xd6c,(uint32_t)override);
        memcpy(c->ram+game+0x38d40,restored.bytes,0x810);
        memset(c->ram+world+0xd150,0xa5,0x810);
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=world;CHECK(invoke(c,0x33c180));
        if(use)CHECK(!memcmp(c->ram+world+0xd150,restored.bytes,0x810));
        if(!use)for(unsigned i=0;i<0x810;i++)CHECK(c->ram[world+0xd150+i]==0xa5);
        memset(c->ram+game+0x38d40,0xa5,0x810);
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=world;CHECK(invoke(c,0x33c278));
        if(use)CHECK(!memcmp(c->ram+game+0x38d40,restored.bytes,0x810));
        else for(unsigned i=0;i<0x810;i++)CHECK(c->ram[game+0x38d40+i]==0xa5);
    }
    gt4_save_shutdown();puts("PASS complete jukebox native disk reload and original restore/writeback; 36 US defaults and 108 continued selections.");
}
static void fade_test(ee_ctx *c,int negative,int gain_negative,int event_negative)
{
    const uint32_t object=0x1840000;
    for(unsigned i=0;i<10000;i++) {
        float remaining=i%3==0?-1.0f:i%3==1?0.0f:2.0f,duration=1.0f;
        float seconds=i<3?(float)i-1.0f:(float)(random_word()%600)/60.0f;
        float scale=(float)(1+random_word()%600)/60.0f;
        memcpy(c->ram+object+0xd4,&remaining,4);memcpy(c->ram+object+0xd8,&duration,4);
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->fpr[12].f=seconds;c->fpr[13].f=scale;c->r[29].uw[0]=0x1ef0000;
        CHECK(invoke(c,0x3c0f40));gt4_music_fade_start(&remaining,&duration,seconds,scale);
        CHECK(!memcmp(c->ram+object+0xd4,&remaining,4));CHECK(!memcmp(c->ram+object+0xd8,&duration,4));
        for(unsigned step=0;step<64;step++) {
            memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->r[29].uw[0]=0x1ef0000;CHECK(invoke(c,0x3c0fb0));
            float gain=gt4_music_fade_gain(remaining,duration);if(negative && i==0 && step==0)gain*=0.5f;
            CHECK(!memcmp(&gain,&c->fpr[0].f,4));
            uint32_t bits=0x3c888888;float dt;memcpy(&dt,&bits,4);
            ee_w32(c,object+0xbc,1);memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->fpr[12].f=dt;c->r[29].uw[0]=0x1ef0000;
            fade_events=0;CHECK(invoke(c,0x3c04e8));int event=gt4_music_fade_step(&remaining,dt);
            if(event_negative && !i && !step)event^=1;
            CHECK((unsigned)event==fade_events);
            CHECK(!memcmp(c->ram+object+0xd4,&remaining,4));
        }
    }
    for(int i=-65536;i<=65536;i++) {
        float gain=(float)i/32768.0f;memset(c->r,0,sizeof c->r);c->fpr[12].f=gain;c->r[29].uw[0]=0x1ef0000;
        CHECK(invoke(c,0x548060));int native_gain=gt4_music_gain_q15(gain);if(gain_negative && i==0)native_gain++;
        CHECK((int32_t)ee_r32(c,0x86ca80)==native_gain);
    }
    puts("PASS 640000 bit-exact original transition gains/ticks, 10000 start/overlap cases and 131073 Q15 RPC gains.");
}
/* 0033D4B8/0033D510: real preference gate and final multiply. */
static void stream_gain_test(ee_ctx *c,int negative)
{
    const uint32_t object=0x1890000,owner=0x1891000,world=0x18a0000,game=0x18b0000;
    memset(c->ram+object,0,0x200);
    ee_w32(c,object,owner);ee_w32(c,owner+0x84,world);ee_w32(c,0x622f4c,game);
    unsigned cases=0;
    for(unsigned mode=0;mode<5;mode++)for(unsigned race=0;race<2;race++)
    for(unsigned replay=0;replay<2;replay++)for(unsigned toggle=0;toggle<2;toggle++)
    for(unsigned sample=0;sample<1000;sample++) {
        float base=(float)(random_word()%65537)/32768.0f;
        float external=(float)(random_word()%65537)/32768.0f;
        if(sample<3){base=sample==0?0.0f:sample==1?-0.0f:1.0f;external=1.0f;}
        ee_w32(c,world+0xcc8,mode);ee_w32(c,game+0x38d1c,race);
        ee_w32(c,game+0x38d2c,replay);ee_w32(c,object+0x34,toggle);
        memcpy(c->ram+object+0x1bc,&base,4);memcpy(c->ram+object+0x1c0,&external,4);
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->r[29].uw[0]=0x1ef0000;
        CHECK(invoke(c,0x33d510));
        int enabled=mode<2?race:(replay^toggle);
        float gain=gt4_music_stream_gain(base,external,enabled);
        if(negative && !cases)gain=1.0f;
        CHECK(!memcmp(&gain,&c->fpr[0].f,4));cases++;
    }
    printf("PASS %u original stream gains and race/replay preference gates, including signed zero.\n",cases);
}
static void replay_fade_test(ee_ctx *c,int negative)
{
    const uint32_t world=0x1850000,session=0x1860000,table=0x1880000;
    memset(c->ram+world,0,0x30000);
    ee_w32(c,world+0x64,table);ee_w32(c,world+0x6c,session);
    ee_w32(c,world+0x74,0x1881000);ee_w32(c,0x1881004,0x1881100);ee_w32(c,0x1881114,0x1fff008);
    ee_w32(c,table+0xd4,0x1fff000);ee_w32(c,table+0x15c,0x1fff008);ee_w32(c,table+0x244,0x1fff004);
    ee_w32(c,0x1870000,0x1882000);ee_w32(c,0x18820b4,0x1fff008);
    ee_w32(c,session+0x80,0x1883000);ee_w32(c,0x1883004,0x1883100);
    for(unsigned mode=0;mode<5;mode++)for(int timed=0;timed<2;timed++)for(int loop=0;loop<2;loop++)
    for(int left=-2;left<=120;left++)for(int active=0;active<3;active++) {
        float remaining=active==0?-1.0f:active==1?0.0f:0.5f,duration=2.0f;
        ee_w32(c,world+0xcc8,mode);ee_w32(c,world+0xd54,(uint32_t)loop);
        ee_w32(c,world+0xd2c,200);ee_w32(c,0x187021c,(uint32_t)(200-left));
        ee_w32(c,session+0xe0,(uint32_t)timed);
        memcpy(c->ram+session+0xd4,&remaining,4);memcpy(c->ram+session+0xd8,&duration,4);
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=world;c->r[29].uw[0]=0x1ef0000;
        CHECK(invoke(c,0x389b08));gt4_music_replay_fade(&remaining,&duration,left,mode,timed,loop);
        if(negative && mode==2 && !timed && !loop && left==60 && !active)remaining=0.5f;
        CHECK(!memcmp(c->ram+session+0xd4,&remaining,4));CHECK(!memcmp(c->ram+session+0xd8,&duration,4));
    }
    puts("PASS 7380 original replay fade triggers: modes, 60-frame boundary, active fade, timer and loop gates.");
}
int plat_audio_play(const int16_t *p,size_t n,int r,int ch){(void)p;(void)n;(void)r;(void)ch;return 0;}
int main(int argc,char **argv)
{
    CHECK(argc>=3 && argc<=4);int playlist_negative=argc==4 && !strcmp(argv[3],"--playlist-negative");
    int jingle_negative=argc==4 && !strcmp(argv[3],"--jingle-negative");
    int order_negative=argc==4 && !strcmp(argv[3],"--order-negative");
    int preferences_negative=argc==4 && !strcmp(argv[3],"--preferences-negative");
    int fade_negative=argc==4 && !strcmp(argv[3],"--fade-negative");
    int event_negative=argc==4 && !strcmp(argv[3],"--fade-event-negative");
    int gain_negative=argc==4 && !strcmp(argv[3],"--gain-negative");
    int replay_fade_negative=argc==4 && !strcmp(argv[3],"--replay-fade-negative");
    int defaults_negative=argc==4 && !strcmp(argv[3],"--defaults-negative");
    int stream_negative=argc==4 && !strcmp(argv[3],"--stream-negative");
    int negative=argc==4 && !stream_negative && !event_negative && !gain_negative && !replay_fade_negative && !defaults_negative && !playlist_negative && !jingle_negative && !order_negative && !preferences_negative && !fade_negative;
    ee_ctx c;CHECK(!ee_init(&c));c.lookup=lookup;c.hook=boundary;
    CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);
    const char *err;gt4_vol *vol=gt4_vol_open(argv[2],&err);CHECK(vol);
    race_playlist_test(&c,vol,playlist_negative);
    race_jingle_test(vol,jingle_negative);
    playlist_order_test(&c,order_negative);
    defaults_test(&c,vol,defaults_negative);
    preferences_test(&c,vol,preferences_negative);
    fade_test(&c,fade_negative,gain_negative,event_negative);
    replay_fade_test(&c,replay_fade_negative);
    stream_gain_test(&c,stream_negative);
    gt4_engine_bank *b=gt4_engine_bank_open(vol,1,-1);CHECK(b && b->count==8);
    gt4_engine_control control;gt4_engine_control_init(&control);
    const uint32_t object=0x1800000,rows=0x1801000;
    ee_w32(&c,object+0x30,rows);ee_w32(&c,object+0x34,b->count);
    for(unsigned i=0;i<2;i++){ee_w32(&c,object+i*24+4,0xffffffff);ee_w32(&c,object+i*24+8,0xffffffff);}
    for(unsigned i=0;i<b->count;i++){
        gt4_engine_layer *s=b->layers+i;uint32_t r=rows+i*16;
        ee_w16(&c,r,s->rpm);ee_w16(&c,r+2,s->minimum);ee_w16(&c,r+4,s->maximum);
        ee_w16(&c,r+6,s->gain);ee_w32(&c,r+8,s->pitch);ee_w32(&c,r+12,s->offset);
        CHECK(s->sample.frames>28 && s->sample.repeat && s->sample.loop<s->sample.frames);
    }
    unsigned cases=0;
    for(unsigned i=0;i<100000;i++){
        int rpm=i<32768?(int)i:(int)(random_word()%32768);
        int16_t left=(int16_t)((int)(random_word()%32767)-16383),right=(int16_t)((int)(random_word()%32767)-16383);
        ee_w32(&c,object+0x48,rpm);ee_w16(&c,object+0x4c,left);ee_w16(&c,object+0x4e,right);
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=object;c.r[29].uw[0]=0x1ef0000;
        CHECK(invoke(&c,0x558fd8));
        gt4_engine_control_step(b,&control,rpm,left,right,(int16_t *)(c.ram+0x64d3f8));
        for(unsigned k=0;k<2;k++){
            uint32_t at=object+k*24;gt4_engine_voice v=control.voice[k];
            if(negative && !i && !k)v.layer++;
            CHECK(v.layer==(int32_t)ee_r32(&c,at+4));
            if(v.layer<0)continue;
            CHECK(v.pitch==ee_r32(&c,at+16));
            CHECK(v.left==(int16_t)ee_r16(&c,at+12));CHECK(v.right==(int16_t)ee_r16(&c,at+14));
        }
        cases++;
    }
    gt4_race_effects *fx=gt4_race_effects_open(vol);CHECK(fx && fx->count==50);
    unsigned decoded=0,repeat=0;
    for(unsigned i=0;i<fx->count;i++){
        const gt4_race_effect *s=fx->effects+i;
        if(!s->sample.frames){CHECK(!strcmp(s->name,"off_bound") || !strcmp(s->name,"off_jump") || !strcmp(s->name,"on_jump") || !strcmp(s->name,"pushup"));continue;}
        decoded++;repeat+=s->sample.repeat;gt4_race_cursor cursor={0};int32_t out[2048]={0};
        gt4_race_sample_mix(&s->sample,&cursor,4096,16383,16383,out,1024);
        CHECK(cursor.position>0 || cursor.ended);
    }
    CHECK(gt4_pcm_music_count(vol,"racing")==65);
    CHECK(gt4_pcm_music_count(vol,"start") && gt4_pcm_music_count(vol,"goal") && gt4_pcm_music_count(vol,"result"));
    printf("PASS %u exact original ENGN selections/pitches/volumes; %u/%u effects decoded (%u loop flags); 65 race tracks.\n",cases,decoded,fx->count,repeat);
    gt4_race_effects_close(fx);gt4_engine_bank_close(b);gt4_vol_close(vol);ee_free(&c);return 0;
}
