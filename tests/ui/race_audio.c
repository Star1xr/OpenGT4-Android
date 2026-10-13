/* The native race's sound end to end, offline: six cars on Tsukuba, the
   camera behind a rival, the callback rendered into a WAV (local only).
   Checks the transport, not SPU2 parity: audible output without
   clipping runs, voices given out and returned, the callback's ownership
   and restart. --negative silences the driver and must fail.
   usage: race_audio_probe <disc.iso> [out.wav] [--negative] */
#define SDL_MAIN_HANDLED
#include "../../src/app/race_audio.c"
#include "specdb.h"
#include <time.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"race audio line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static plat_audio_cb callback;
static void *owner;
static uint64_t test_clock;
uint64_t plat_time_ns(void){return test_clock?test_clock:(uint64_t)SDL_GetTicks()*1000000u;}
int plat_audio_set_music(plat_audio_cb cb,void *user){callback=cb;owner=user;return 0;}
void plat_audio_release_music(void *user){if(owner==user){callback=NULL;owner=NULL;}}
int plat_audio_play(const int16_t *p,size_t n,int r,int c){(void)p;(void)n;(void)r;(void)c;return 1;}
static void foreign(int16_t *p,int n,void *u){(void)p;(void)n;(void)u;}
static int pad_read(void *u,uint32_t car,uint8_t *cmd,const uint8_t *ev)
{(void)u;(void)car;(void)ev;memset(cmd,0,16);return 1;}
static int pad_wheel(void *u,uint32_t car,int16_t state[2])
{(void)u;(void)car;state[0]=state[1]=0;return 1;}
static int pad_write(void *u,uint32_t car,const uint8_t *out)
{(void)u;(void)car;(void)out;return 1;}
static void put16(FILE *f,unsigned v){fputc(v&255,f);fputc(v>>8&255,f);}
static void put32(FILE *f,uint32_t v){put16(f,v&0xffff);put16(f,v>>16);}
static void pit_effects(race_audio *assets,int negative)
{
    /* Actual producer and sound manager. Enum/gain sources: 00393428,
       003934A0, 003D01AC..003D02CC, 003D3358/003D33C0. */
    int32_t groups[8]={0,7065,0x3fff,0,0x3fff,0x3fff,0,0};
    for(unsigned effect=0x3f;effect<=0x46;effect++){
        race_audio a={0};a.sd=gt4_sd_open(gt4_ptr(0x6c8ac0),gt4_ptr(0x654738),groups,1);CHECK(a.sd);
        a.have_setup=1;a.setup.master=1;a.refueling.voice=-1;
        memcpy(a.effect_by_enum,assets->effect_by_enum,sizeof a.effect_by_enum);CHECK(a.effect_by_enum[effect]);
        if(negative && effect==0x43)a.effect_by_enum[effect]=a.effect_by_enum[0x41];
        gt4_pit_crew_sound command={0};if(effect==0x46)command.refueling=1;else{command.count=1;command.effect[0]=(uint8_t)effect;}
        race_audio_pit(&a,&command);gt4_sd_tick(a.sd,NULL,a.regs);
        int hw=-1;unsigned on=0;for(int i=0;i<VOICES;i++)if(a.regs[i].key_on){hw=i;on++;}CHECK(on==1 && hw>=0);
        CHECK(a.regs[hw].wave==&assets->effect_by_enum[effect]->sample.wave);CHECK(a.regs[hw].voll>0 && a.regs[hw].volr>0);
        if(effect==0x46){
            uint32_t serial=a.regs[hw].key_on;
            for(unsigned tick=0;tick<120;tick++){race_audio_pit(&a,&command);gt4_sd_tick(a.sd,NULL,a.regs);CHECK(a.regs[hw].key_on==serial);}
            race_audio_pause(&a,1);gt4_sd_tick(a.sd,NULL,a.regs);CHECK(a.regs[hw].key_off>serial);
            race_audio_pit(&a,&command);gt4_sd_tick(a.sd,NULL,a.regs);int resumed=0;for(int i=0;i<VOICES;i++)resumed|=a.regs[i].wave==&assets->effect_by_enum[effect]->sample.wave && a.regs[i].key_on>serial;CHECK(resumed);
            command.refueling=0;race_audio_pit(&a,&command);gt4_sd_tick(a.sd,NULL,a.regs);CHECK(a.refueling.voice<0);
        }
        gt4_sd_close(a.sd);
    }
    puts("PASS actual pit producer: eight disc effects, one sustained fuel voice, pause/resume and stop.");
}
int main(int argc,char **argv)
{
    CHECK(argc>=2);int completion_negative=argc>2 && !strcmp(argv[argc-1],"--completion-negative");int seed_negative=argc>2 && !strcmp(argv[argc-1],"--seed-negative");int replay_negative=argc>2 && !strcmp(argv[argc-1],"--replay-negative");int persistence_negative=argc>2 && !strcmp(argv[argc-1],"--persistence-negative");int music_negative=argc>2 && !strcmp(argv[argc-1],"--music-negative");int key_negative=argc>2 && !strcmp(argv[argc-1],"--key-negative");int negative=0;const char *wav=NULL;
    for(int i=2;i<argc;i++){if(!strcmp(argv[i],"--negative"))negative=1;else if(strcmp(argv[i],"--key-negative") && strcmp(argv[i],"--music-negative") && strcmp(argv[i],"--persistence-negative") && strcmp(argv[i],"--replay-negative") && strcmp(argv[i],"--seed-negative") && strcmp(argv[i],"--completion-negative"))wav=argv[i];}
    CHECK(!SDL_Init(SDL_INIT_TIMER));const char *error;gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v);
    size_t bytes;uint8_t *labels=gt4_vol_read(v,"/specdb/GT4_US2560/GENERIC_CAR.idi",&bytes);CHECK(labels);
    gt4_race_desc d={0};strcpy(d.course,getenv("PROBE_COURSE")?getenv("PROBE_COURSE"):"tsukuba");d.cars=6;
    const char *cars[]={"ka_01","_1100_standard_type11_49","z_act_70","today_g_85","midget2_d_type_98","s500_63"};
    if(getenv("PROBE_CAR"))cars[0]=cars[1]=getenv("PROBE_CAR");
    for(int i=0;i<6;i++)CHECK(gt4_specdb_label(labels,bytes,cars[i],d.car+i));free(labels);
    char msg[256];gt4_race_run *run=gt4_race_run_open(argv[1],&d,msg,sizeof msg);CHECK(run);
    gt4_phys_pad.read=pad_read;gt4_phys_pad.wheel_state=pad_wheel;gt4_phys_pad.write=pad_write;
    char crs[96];snprintf(crs,sizeof crs,"/crs/%s",d.course);uint8_t *data=gt4_vol_read(v,crs,&bytes);gt4_course course;CHECK(data && gt4_course_open(&course,data,bytes));
    race_audio *a=race_audio_open(argv[1],run,&course);CHECK(a && a->cars==6 && a->effects->count==50);
    if(argc==3 && (!strcmp(argv[2],"--pit-only") || !strcmp(argv[2],"--pit-negative"))){
        pit_effects(a,!strcmp(argv[2],"--pit-negative"));race_audio_close(a);free(data);gt4_race_run_close(run);gt4_vol_close(v);SDL_Quit();return 0;
    }
    for(int i=0;i<6;i++)CHECK(a->bank[i][0] && a->bank[i][2] && a->bank[i][1]==a->road);
    CHECK(a->reverb[0].buffer && a->reverb[0].regs[0]==0x00e3);   /* Studio C's dAPF1 from LIBSD.IRX */
    FILE *out=wav?fopen(wav,"wb"):NULL;
    enum{SECONDS=40,RATE=48000};uint32_t total=0;
    if(out){fputs("RIFF",out);put32(out,0);fputs("WAVEfmt ",out);put32(out,16);put16(out,1);put16(out,2);
        put32(out,RATE);put32(out,RATE*4);put16(out,4);put16(out,16);fputs("data",out);put32(out,0);}
    uint64_t energy=0;int used_max=0,peak=0;uint64_t clipped=0;int16_t buf[800*2];
    for(int tick=0;tick<SECONDS*60;tick++){
        CHECK(gt4_race_run_frame(run));
        /* behind car 1, as the chase view: 6 back, 1.7 up, looking at it */
        float p[3],yaw;CHECK(gt4_race_run_place(run,1,p,&yaw));
        float fx=-sinf(yaw),fz=-cosf(yaw),eye[3]={p[0]-fx*6,p[1]+1.7f,p[2]-fz*6};
        float rows[9]={cosf(yaw),0,-sinf(yaw),0,1,0,sinf(yaw),0,cosf(yaw)};
        race_audio_update(a,run,eye,rows,1,-1,negative?0:120);race_audio_step(a,run);
        CHECK(callback && owner==a);callback(buf,800,owner);
        int used=0;for(int i=0;i<VOICES;i++)used+=a->voice[i].phase!=GT4_SPU_OFF;
        if(used>used_max)used_max=used;
        for(int i=0;i<1600;i++){energy+=(uint64_t)abs(buf[i]);if(abs(buf[i])>peak)peak=abs(buf[i]);clipped+=abs(buf[i])>=32767;}
        if(out){fwrite(buf,4,800,out);total+=800*4;}
    }
    if(out){fseek(out,4,SEEK_SET);put32(out,36+total);fseek(out,40,SEEK_SET);put32(out,total);fclose(out);}
    printf("energy %.1f per sample, peak %d, voices up to %d of %d\n",(double)energy/(SECONDS*60*1600),peak,used_max,VOICES);
    CHECK(energy>(uint64_t)SECONDS*60*1600*20);
    printf("clipped samples %llu\n",(unsigned long long)clipped);
    CHECK(used_max>6 && clipped*10000<(uint64_t)SECONDS*60*1600);   /* under 0.01% at the rail */
    race_audio_close(a);CHECK(!callback && !owner);
    a=race_audio_open(argv[1],run,&course);CHECK(a);
    float eye[3]={0,2,0},rows[9]={1,0,0,0,1,0,0,0,1};race_audio_update(a,run,eye,rows,1,-1,120);race_audio_step(a,run);
    /* Exercise the actual race producer: per-stream option policy, signed
       mute transport, Q15 gain and every tick of a session transition. */
    uint32_t session=GT4_U32(gt4_race_run_conductor(run));gt4_race_queue_reset(session+0xf4);
    for(int request=11;request<=12;request++)for(int volume=-1;volume<=128;volume++) {
        a->active_music=a->requested_music=request;SDL_AtomicSet(&a->music_ended,0);
        GT4_F32(session+0xd4)=1.0f;GT4_F32(session+0xd8)=1.0f;
        race_audio_update(a,run,eye,rows,1,volume,120);
        float base=a->sdvol_music;if(request==12)base*=volume/128.0f;
        float expected=(float)gt4_music_gain_q15(volume<0?0:base)/32768.0f;
        if(music_negative && request==11 && volume==0)expected+=1.0f/32768.0f;
        CHECK(a->music_volume==expected);
        for(int tick=0;tick<61;tick++) {
            CHECK(gt4_race_run_frame(run));race_audio_step(a,run);
            expected=(float)gt4_music_gain_q15(volume<0?0:base*gt4_music_fade_gain(GT4_F32(session+0xd4),1.0f))/32768.0f;
            CHECK(a->music_volume==expected);
        }
    }
    GT4_F32(session+0xd4)=-1.0f;
    puts("PASS native race producer: jingle/racing volume, mute, Q15 gain and 15860 transition ticks.");
    /* A looping skid starts and stops between audio callbacks. The last
       command must win even if the callback never saw the KON. */
    /* A completion from an old KON must not free the current occupant.
       Exercise a real ENGN voice, sustained RPM, without audio callbacks. */
    int32_t group[8]={0,7065,0x3fff,0,0x3fff,0x3fff,0,0};
    gt4_sd *steady_sd=gt4_sd_open(gt4_ptr(0x6c8ac0),gt4_ptr(0x654738),group,1);CHECK(steady_sd);
    gt4_sd_engine steady={0};steady.bank=a->bank[0][0];steady.hw[0]=steady.hw[1]=-1;gt4_engine_control_init(&steady.control);
    race_audio report={0};uint8_t completed[VOICES];
    gt4_sd_engine_tick(steady_sd,&steady,3000,12000,12000,4096,100,0,a->sine);
    gt4_sd_tick(steady_sd,NULL,report.regs);
    int hardware=steady.hw[0]>=0?steady.hw[0]:steady.hw[1];CHECK(hardware>=0);
    uint32_t serial=report.regs[hardware].key_on;
    report.ended[hardware]=1;report.ended_on[hardware]=serial-1;
    for(int tick=0;tick<600;tick++) {
        gt4_sd_engine_tick(steady_sd,&steady,3000,12000,12000,4096,100,0,a->sine);
        completed_voices(&report,completed);
        if(completion_negative)completed[hardware]=1;
        gt4_sd_tick(steady_sd,completed,report.regs);
        CHECK(steady.hw[0]==hardware || steady.hw[1]==hardware);
        CHECK(report.regs[hardware].key_on==serial);
    }
    report.ended_on[hardware]=serial;completed_voices(&report,completed);
    gt4_sd_tick(steady_sd,completed,report.regs);
    CHECK(steady.hw[0]!=hardware && steady.hw[1]!=hardware);
    gt4_sd_close(steady_sd);
    puts("PASS sustained ENGN RPM: stale completion cannot restart voice across 600 ticks; current completion releases it.");
    gt4_spu_regs before[VOICES];memcpy(before,a->regs,sizeof before);
    gt4_sd_handle skid={-1,0};
    gt4_sd_play(a->sd,&skid,by_name(a,"on_skill"),0,1,1,1,0x7fffffff,64);
    gt4_sd_tick(a->sd,NULL,a->regs);
    int skid_hw=-1;for(int i=0;i<VOICES;i++)if(a->regs[i].key_on!=before[i].key_on && a->regs[i].wave==&by_name(a,"on_skill")->sample.wave)skid_hw=i;
    CHECK(skid_hw>=0);
    gt4_sd_stop(a->sd,&skid);
    gt4_sd_tick(a->sd,NULL,a->regs);
    if(key_negative)for(int i=0;i<VOICES;i++)a->seen_off[i]=a->regs[i].key_off;
    callback(buf,800,owner);
    CHECK(a->voice[skid_hw].phase==GT4_SPU_OFF);
    /* Reusing the same hardware voice reverses the order: the new KON
       must survive the prior owner's KOFF, including across serial wrap. */
    a->regs[skid_hw].key_off=0xfffffffeu;a->regs[skid_hw].key_on=1;
    callback(buf,800,owner);CHECK(a->voice[skid_hw].phase!=GT4_SPU_OFF && a->voice[skid_hw].phase!=GT4_SPU_RELEASE);
    puts("PASS stopped skid stays off; reused voice stays on, including key serial wrap.");
    int destination;plat_audio_set_music(foreign,&destination);race_audio_close(a);CHECK(callback==foreign && owner==&destination);
    plat_audio_release_music(&destination);
    char directory[160];snprintf(directory,sizeof directory,"build/music-producer-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)SDL_GetPerformanceCounter());
    gt4_save_init(directory);CHECK(gt4_save_create(1,"Music producer"));
    gt4_music_playlist saved,readback;CHECK(gt4_music_playlist_default(&saved,v,0x12345678));
    gt4_music_playlist_order(&saved,1);
    CHECK(gt4_music_playlist_next(&saved)>=0);
    CHECK(gt4_save_option_blob_set("bgm_playlist_ps2",saved.bytes,sizeof saved.bytes));
    a=race_audio_open(argv[1],run,&course);CHECK(a && a->playlist_preferences);
    CHECK(!memcmp(a->playlist.bytes,saved.bytes,sizeof saved.bytes));
    CHECK(gt4_music_playlist_next(&a->playlist)>=0);a->playlist_changed=1;saved=a->playlist;
    if(persistence_negative)a->playlist_preferences=0;
    race_audio_close(a);CHECK(gt4_save_option_blob_get("bgm_playlist_ps2",readback.bytes,sizeof readback.bytes));
    CHECK(!memcmp(saved.bytes,readback.bytes,sizeof saved.bytes));
    test_clock=1000000001;
    a=race_audio_open_replay(argv[1],run,&course,1);CHECK(a && !a->playlist_preferences && a->playlist.bytes[0]==65);
    CHECK(a->playlist.bytes[6]==1);gt4_music_playlist replay_first=a->playlist;
    for(unsigned i=0;i<65;i++) {
        CHECK(!memcmp(a->playlist.bytes+16+i*16,saved.bytes+16+i*16,8));
        CHECK(a->playlist.bytes[16+i*16+9]==saved.bytes[16+i*16+9]);
    }
    /* A replay must ignore even a queued autodemo/title-track request. */
    a->active_music=12;
    if(replay_negative)a->replay=0;
    gt4_race_queue_push(session+0xf4,0x18,26,0);music_update(a,run);
    CHECK(a->requested_music==12 && !a->music_worker);
    CHECK(gt4_music_playlist_next(&a->playlist)>=0);a->playlist_changed=1;race_audio_close(a);
    CHECK(gt4_save_option_blob_get("bgm_playlist_ps2",readback.bytes,sizeof readback.bytes));
    CHECK(!memcmp(saved.bytes,readback.bytes,sizeof saved.bytes));
    test_clock=seed_negative?1000000001:1000000002;
    a=race_audio_open_replay(argv[1],run,&course,1);CHECK(a);
    int order_changed=0;
    for(unsigned i=0;i<65;i++)order_changed|=replay_first.bytes[16+i*16+10]!=a->playlist.bytes[16+i*16+10];
    CHECK(order_changed);race_audio_close(a);test_clock=0;
    CHECK(gt4_save_store());gt4_save_shutdown();gt4_save_init(directory);
    CHECK(gt4_save_option_blob_get("bgm_playlist_ps2",readback.bytes,sizeof readback.bytes));
    CHECK(!memcmp(saved.bytes,readback.bytes,sizeof saved.bytes));gt4_save_shutdown();
    puts("PASS actual producer restores/writes race playlist, isolates shuffled replay, rejects autodemo music and survives profile reload.");
    free(data);gt4_race_run_close(run);gt4_vol_close(v);SDL_Quit();
    puts("PASS six cars through the game's car sound, driver and SPU2 voices: audible, clipping below 0.01%, voices given out, ownership and restart.");
    return 0;
}
