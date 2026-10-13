#include "sound.h"
#include "sound_sequence.h"
#include "music.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static const int16_t *played_pcm;static size_t played_frames;static unsigned play_calls;
int plat_audio_play(const int16_t *p,size_t n,int r,int c) { played_pcm=p;played_frames=n;play_calls++;(void)r;(void)c;return 1; }
static void put32(FILE *f,unsigned n) { for(int i=0;i<4;i++) fputc((n>>(i*8))&255,f); }
static void put16(FILE *f,unsigned n) { fputc(n&255,f);fputc(n>>8,f); }
static void wav(const char *path,const int16_t *pcm,size_t frames)
{
    FILE *f=fopen(path,"wb");CHECK(f);fwrite("RIFF",1,4,f);put32(f,36+(unsigned)frames*4);
    fwrite("WAVEfmt ",1,8,f);put32(f,16);put16(f,1);put16(f,2);put32(f,48000);put32(f,192000);put16(f,4);put16(f,16);
    fwrite("data",1,4,f);put32(f,(unsigned)frames*4);fwrite(pcm,4,frames,f);fclose(f);
}
static void store32(uint8_t *p,unsigned n) { for(int i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i)); }
static unsigned read16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned read32(const uint8_t *p){return read16(p)|(read16(p+2)<<16);}
typedef struct {gt4_music *stream,*reference;unsigned previous,calls;} prefix_test;
static void check_prefix(void *user,unsigned ready)
{
    prefix_test *t=user;unsigned count=ready-t->previous;CHECK(count>0 && count<=512);
    int16_t actual[1026],expected[1024],silence[2]={1,1};
    gt4_music_render_available(silence,1,t->stream,0);CHECK(!silence[0] && !silence[1]);
    gt4_music_render_available(actual,(int)count,t->stream,ready);
    gt4_music_render(expected,(int)count,t->reference);
    if(getenv("GT4_MUSIC_PREFIX_NEGATIVE"))actual[0]^=1;
    CHECK(!memcmp(actual,expected,count*4));t->previous=ready;t->calls++;
}
static void service_test(const char *disc,int negative)
{
    const char *error=NULL;gt4_vol *vol=gt4_vol_open(disc,&error);CHECK(vol);
    gt4_se_tables tables;CHECK(gt4_se_tables_load(vol,&tables));size_t irx_size=0;
    uint8_t *irx=gt4_disc_read_file(disc,"IRX/LIBSD.IRX",&irx_size);CHECK(irx);
    const char *paths[]={"/sound/gt4sys.ins","/sound/gt4count.ins"};
    const unsigned sequences[2][3]={{5,5,5},{5,7,8}};
    for(unsigned bankno=0;bankno<2;bankno++) {
        size_t bytes=0;uint8_t *bank=gt4_vol_read(vol,paths[bankno],&bytes);CHECK(bank);
        for(unsigned i=0;i<(bankno?3u:1u);i++) {
            int16_t *pcm=NULL;size_t frames=0;unsigned seq=sequences[bankno][i];
            CHECK(gt4_sound_render_service(bank,bytes,seq,&tables,irx,irx_size,&pcm,&frames));
            long long power=0;for(size_t j=0;j<frames*2;j++)power+=(long long)pcm[j]*pcm[j];
            CHECK(power>100000 && frames>100 && frames<=48000*60);free(pcm);
            printf("service bank %u sequence %u: %zu nonzero frames\n",bankno,seq,frames);
        }
        if(!bankno) {
            /* Detect an ignored B0/41 ramp with the actual oil sequence.
               This proves playback responds to automation, not SPU parity. */
            size_t st=32ull+read32(bank+0x3c),entry=st+read16(bank+st+2+5*2);
            size_t at=st+read16(bank+entry+2);uint8_t *flat=malloc(bytes);CHECK(flat);memcpy(flat,bank,bytes);unsigned ramps=0;
            while(at+2<=bytes && !(bank[at]==255 && bank[at+1]==47)) {
                if(bank[at]==0xa0){CHECK(at+4<=bytes);at+=4;}
                else {CHECK(at+6<=bytes && bank[at]==0xb0 && bank[at+1]==0x41);ramps++;if(!negative)flat[at+3]=0;at+=6;}
                unsigned k=0;while(at<bytes && k++<4 && (bank[at++]&128)){}CHECK(k<=4);
            }
            CHECK(ramps>0);int16_t *a=NULL,*b=NULL;size_t an=0,bn=0;
            CHECK(gt4_sound_render_service(bank,bytes,5,&tables,irx,irx_size,&a,&an) &&
                  gt4_sound_render_service(flat,bytes,5,&tables,irx,irx_size,&b,&bn));
            CHECK(an!=bn || memcmp(a,b,an*4));free(a);free(b);free(flat);
            for(size_t n=0;n<0x500;n++)CHECK(!gt4_sound_render_service(bank,n,5,&tables,irx,irx_size,&a,&an));
        }
        free(bank);
    }
    gt4_sound *s=gt4_sound_open(vol);CHECK(s);size_t names_size=0;uint8_t *names=gt4_vol_read(vol,"/sound/gt4se.inf",&names_size);CHECK(names);
    unsigned cached=0;for(unsigned i=0;i<read32(names+12);i++){
        const uint8_t *e=names+16+i*8;
        if(!((e[7]==0 && e[5]==5) || (e[7]==2 && (e[5]==5 || e[5]==7 || e[5]==8))))continue;
        const char *name=(const char *)names+read32(e);CHECK(gt4_sound_play(s,name));
        const int16_t *pointer=played_pcm;size_t frames=played_frames;unsigned calls=play_calls;
        CHECK(gt4_sound_play(s,name) && played_pcm==pointer && played_frames==frames && play_calls==calls+1);cached++;
    }
    CHECK(cached==4);free(names);gt4_sound_close(s);free(irx);
    gt4_vol_close(vol);puts("PASS four native SPU service sounds, cached playback, bounded rejection and audible oil pitch automation (not waveform parity)");
}
static void navigation_reverb_test(void)
{
    uint8_t bank[512]={0};
    memcpy(bank,"INST",4);memcpy(bank+0x2c,"SShd",4);
    store32(bank+20,496);store32(bank+16,16);
    store32(bank+0x3c,0x80);store32(bank+0x44,0xc0);
    bank[0xa2]=4;bank[0xa6]=8;
    const uint8_t events[]={0xa0,60,127,0,0,0xff,0x2f};
    memcpy(bank+0xa8,events,sizeof events);
    bank[0xe2]=4;bank[0xe4]=255;bank[0xe5]=127;
    bank[0xea]=60;bank[0xeb]=60;bank[0xee]=60;
    bank[0xf0]=0x80; /* Original JamSplit routing enables the wet send. */
    bank[0xf8]=127;bank[0xf9]=64;
    bank[496]=8;bank[497]=1;memset(bank+498,0x77,14);
    int16_t *pcm=NULL;size_t frames=0;
    CHECK(gt4_sound_render(bank,sizeof bank,0,&pcm,&frames));
    long long direct=0,tail=0,early=0,late=0;
    for(size_t f=0;f<frames;f++)for(int c=0;c<2;c++) {
        long long v=pcm[f*2+c],energy=v*v;
        if(f<31)direct+=energy;else tail+=energy;
        if(f>=1400 && f<12000)early+=energy;
        if(f>=48000)late+=energy;
    }
    CHECK(direct>0 && tail>direct/100 && tail<direct/2);
    CHECK(early>0 && late<early/100);
    free(pcm);
    bank[0xf0]=0;
    CHECK(gt4_sound_render(bank,sizeof bank,0,&pcm,&frames));
    CHECK(frames==31); /* Unflagged voices stay dry, with no artificial tail. */
    free(pcm);
}
static void music_test(void)
{
    uint8_t bank[512]={0},seq[128]={0};
    memcpy(bank,"INST",4);memcpy(bank+0x2c,"SShd",4);
    store32(bank+20,496);store32(bank+16,16);
    store32(bank+0x30,0x80);store32(bank+0x34,0xe0);
    bank[0xa2]=4; /* program table: one program at a4 */
    bank[0xa5]=127;bank[0xab]=127;
    bank[0xad]=127;bank[0xae]=60;bank[0xb8]=127;bank[0xb9]=64;
    for(int i=0;i<128;i++)bank[0x102+i]=(uint8_t)i;
    bank[496]=0x08;bank[497]=7;memset(bank+498,0x71,14); /* repeating synthetic sample */
    memcpy(seq,"Ssqt",4);store32(seq+8,12);memcpy(seq+12,"Sssq",4);store32(seq+16,480);
    const uint8_t events[]={0,0xb0,99,20, 0,0x90,60,100, 0x81,0x70,0x80,60, 0x81,0x70,0xb0,99,30, 0,0xff,0x2f,0};
    memcpy(seq+20,events,sizeof events);size_t qn=20+sizeof events;
    gt4_music *m=gt4_music_decode(bank,sizeof bank,seq,qn);CHECK(m);
    int16_t out[960];long long power=0;
    for(int i=0;i<150;i++){gt4_music_render(out,480,m);for(int j=0;j<960;j++)power+=(long long)out[j]*out[j];}
    CHECK(power>100000);gt4_music_close(m); /* three loops */
    /* A quarter note at the default 120 BPM must be exactly 0.5 seconds.
       Assert event timing across arbitrary render block sizes. */
    const uint8_t delayed[]={0x83,0x60,0x90,60,100,0x83,0x60,0xff,0x2f,0};
    uint8_t timing_seq[128];memcpy(timing_seq,seq,sizeof seq);memcpy(timing_seq+20,delayed,sizeof delayed);
    m=gt4_music_decode(bank,sizeof bank,timing_seq,20+sizeof delayed);CHECK(m);
    for(int i=0;i<50;i++){gt4_music_render(out,480,m);for(int j=0;j<960;j++)CHECK(out[j]==0);}
    gt4_music_render(out,480,m);power=0;for(int i=0;i<960;i++)power+=(long long)out[i]*out[i];CHECK(power>0);
    gt4_music_close(m);
    /* Tempo affects note scheduling, while prepared playback is independent
       of callback sizes and rewinds exactly. */
    m=gt4_music_decode(bank,sizeof bank,timing_seq,20+sizeof delayed);CHECK(m);
    CHECK(!gt4_music_prepare(m,0));CHECK(gt4_music_prepare(m,0.5));
    for(int i=0;i<100;i++){gt4_music_render(out,480,m);for(int j=0;j<960;j++)CHECK(out[j]==0);}
    gt4_music_render(out,480,m);power=0;for(int i=0;i<960;i++)power+=(long long)out[i]*out[i];CHECK(power>0);
    gt4_music_rewind(m);gt4_music_render(out,480,m);for(int j=0;j<960;j++)CHECK(out[j]==0);
    gt4_music_close(m);
    /* Prepared PCM must match streaming synthesis across uneven block sizes. */
    m=gt4_music_decode(bank,sizeof bank,seq,qn);CHECK(m);
    gt4_music *prepared=gt4_music_decode(bank,sizeof bank,seq,qn);CHECK(prepared);
    CHECK(gt4_music_prepare(prepared,1));int16_t compare[960];
    for(int i=0;i<49;i++){gt4_music_render(out,479,m);gt4_music_render(compare,479,prepared);CHECK(!memcmp(out,compare,479*4));}
    gt4_music_close(prepared);gt4_music_close(m);
    prefix_test prefix={gt4_music_decode(bank,sizeof bank,seq,qn),gt4_music_decode(bank,sizeof bank,seq,qn),0,0};
    CHECK(prefix.stream && prefix.reference && gt4_music_prepare(prefix.reference,1));
    CHECK(gt4_music_prepare_stream(prefix.stream,1,check_prefix,&prefix) && prefix.calls>1);
    gt4_music_close(prefix.stream);gt4_music_close(prefix.reference);
    /* Menu tracks must advance, not repeat. Distinct lengths and the second
       track's leading silence detect wrong boundaries and leftover loop PCM.
       Check exact sample concatenation across callback sizes and three cycles. */
    {
        gt4_music *tracks[2]={gt4_music_decode(bank,sizeof bank,seq,qn),
            gt4_music_decode(bank,sizeof bank,timing_seq,20+sizeof delayed)};
        CHECK(tracks[0] && tracks[1]);
        CHECK(gt4_music_prepare(tracks[0],1) && gt4_music_prepare(tracks[1],1));
        int16_t *expected=malloc(72000*4),*block=malloc(4097*4);CHECK(expected && block);
        gt4_music_render(expected,24000,tracks[0]);
        gt4_music_render(expected+48000,48000,tracks[1]);
        const unsigned sizes[]={1,479,480,511,4097};
        for(unsigned test=0;test<sizeof sizes/sizeof sizes[0];test++) {
            gt4_music_rewind(tracks[0]);gt4_music_rewind(tracks[1]);unsigned current=0;
            for(unsigned at=0;at<216017;) {
                unsigned frames=sizes[test];if(frames>216017-at)frames=216017-at;
                current=gt4_music_render_playlist(block,(int)frames,tracks,2,current);
                for(unsigned i=0;i<frames;i++)CHECK(!memcmp(block+i*2,expected+((at+i)%72000)*2,4));
                at+=frames;CHECK(current==(at%72000>=24000));
            }
        }
        free(block);free(expected);gt4_music_close(tracks[0]);gt4_music_close(tracks[1]);
    }
    /* CC99/30 must not discard the authored outro. The old early break
       fails this fixture: the next track starts at 24000 instead of 48000. */
    {
        const uint8_t outro[]={0,0xb0,99,20,0,0x90,60,100,
            0x83,0x60,0xb0,99,30,0,0x80,60,0x83,0x60,0xff,0x2f,0};
        uint8_t q[128];memcpy(q,seq,sizeof q);memcpy(q+20,outro,sizeof outro);
        if(getenv("GT4_MUSIC_OUTRO_NEGATIVE")){q[30]=0xff;q[31]=0x2f;q[32]=0;}
        gt4_music *tracks[2]={gt4_music_decode(bank,sizeof bank,q,20+sizeof outro),
            gt4_music_decode(bank,sizeof bank,timing_seq,20+sizeof delayed)};
        CHECK(tracks[0] && tracks[1]);
        CHECK(gt4_music_prepare(tracks[0],1) && gt4_music_prepare(tracks[1],1));
        unsigned current=0;
        for(int i=0;i<99;i++)current=gt4_music_render_playlist(out,480,tracks,2,current);
        CHECK(current==0);
        current=gt4_music_render_playlist(out,480,tracks,2,current);CHECK(current==1);
        gt4_music_close(tracks[0]);gt4_music_close(tracks[1]);
    }
    /* Two matching splits with opposite pans must both survive loading. */
    uint8_t layered[512];memcpy(layered,bank,sizeof bank);
    layered[0xa4]=1;memcpy(layered+0xbc,layered+0xac,16);
    layered[0xb9]=0;layered[0xc9]=127;
    m=gt4_music_decode(layered,sizeof layered,seq,qn);CHECK(m);
    gt4_music_render(out,480,m);long long left=0,right=0;
    for(int i=0;i<480;i++){left+=(long long)out[i*2]*out[i*2];right+=(long long)out[i*2+1]*out[i*2+1];}
    printf("stereo layers: %lld / %lld\n",left,right);CHECK(left>0 && right>0);gt4_music_close(m);
    /* Instrument release zero must end a sustained loop within a few samples,
       not use the old universal 80 ms tail. */
    uint8_t envelope_bank[512];memcpy(envelope_bank,bank,sizeof bank);
    envelope_bank[0xb4]=15;envelope_bank[0xb5]=0;
    envelope_bank[0xb6]=0xc0;envelope_bank[0xb7]=0x5f;
    m=gt4_music_decode(envelope_bank,sizeof envelope_bank,seq,qn);CHECK(m);
    for(int i=0;i<25;i++)gt4_music_render(out,480,m); /* note-off at .25 s */
    gt4_music_render(out,480,m);
    for(int i=8;i<960;i++)CHECK(out[i]==0);gt4_music_close(m);
    /* A decreasing sustain envelope can finish without receiving note-off. */
    envelope_bank[0xb6]=0;envelope_bank[0xb7]=0x40;
    m=gt4_music_decode(envelope_bank,sizeof envelope_bank,seq,qn);CHECK(m);
    gt4_music_render(out,480,m);power=0;
    for(int i=0;i<16;i++)power+=(long long)out[i]*out[i];CHECK(power>0);
    for(int i=16;i<960;i++)CHECK(out[i]==0);gt4_music_close(m);
    for(size_t i=0;i<sizeof bank;i++){m=gt4_music_decode(bank,i,seq,qn);CHECK(!m);}
    for(size_t i=0;i<qn;i++){m=gt4_music_decode(bank,sizeof bank,seq,i);CHECK(!m);}
    bank[0xa2]=255;bank[0xa3]=127;CHECK(!gt4_music_decode(bank,sizeof bank,seq,qn));
}
static void source_rate_test(void)
{
    /* A one-second 44.1 kHz sample must occupy one real output second.
       Its note lasts longer so the sample boundary, not key-off, ends it. */
    const size_t data=496,blocks=1575,frames=44100;
    size_t size=data+blocks*16;
    uint8_t *bank=calloc(size,1);CHECK(bank);uint8_t seq[64]={0};
    memcpy(bank,"INST",4);memcpy(bank+0x2c,"SShd",4);
    store32(bank+20,(unsigned)data);store32(bank+16,(unsigned)(blocks*16));
    store32(bank+0x30,0x80);store32(bank+0x34,0xe0);
    bank[0xa2]=4;bank[0xa5]=127;bank[0xab]=127;bank[0xad]=127;bank[0xae]=60;
    bank[0xb4]=15;bank[0xb6]=0xc0;bank[0xb7]=0x5f;bank[0xb8]=127;bank[0xb9]=64;
    for(int i=0;i<128;i++)bank[0x102+i]=(uint8_t)i;
    for(size_t i=0;i<blocks;i++){bank[data+i*16]=8;memset(bank+data+i*16+2,0x77,14);}
    bank[data+(blocks-1)*16+1]=1;
    memcpy(seq,"Ssqt",4);store32(seq+8,12);memcpy(seq+12,"Sssq",4);store32(seq+16,480);
    const uint8_t events[]={0,0x90,60,127,0x8b,0x20,0xff,0x2f,0}; /* end at 1.5 s */
    memcpy(seq+20,events,sizeof events);
    for(int mode=0;mode<2;mode++) {
        gt4_music *m=gt4_music_decode(bank,size,seq,20+sizeof events);CHECK(m);
        if(mode)CHECK(gt4_music_prepare(m,0.5));
        int16_t out[960];int seconds=mode?2:1;
        for(int i=0;i<seconds*100-1;i++)gt4_music_render(out,480,m);
        gt4_music_render(out,480,m);CHECK(out[400]>0); /* last 10 ms still audible */
        gt4_music_render(out,480,m);for(int i=4;i<960;i++)CHECK(out[i]==0);
        gt4_music_close(m);
    }
    free(bank);printf("source rate: %zu samples occupy 1 s; half speed stays aligned\n",frames);
}
int main(int argc,char **argv)
{
    if(argc==3 && (!strcmp(argv[1],"--services") || !strcmp(argv[1],"--service-negative"))) {
        service_test(argv[2],!strcmp(argv[1],"--service-negative"));return 0;
    }
    music_test();source_rate_test();navigation_reverb_test();
    uint8_t block[16]={0x0c,1,0x1f};int16_t *pcm;size_t n;
    CHECK(gt4_adpcm_decode(block,16,&pcm,&n));CHECK(n==28 && pcm[0]==-1 && pcm[1]==1);free(pcm);
    for(size_t len=0;len<16;len++) CHECK(!gt4_adpcm_decode(block,len,&pcm,&n));
    block[0]=0x50;CHECK(!gt4_adpcm_decode(block,16,&pcm,&n));
    block[0]=0x0d;CHECK(!gt4_adpcm_decode(block,16,&pcm,&n));
    block[0]=0x0c;block[1]=0;CHECK(!gt4_adpcm_decode(block,16,&pcm,&n));
    CHECK(!gt4_sound_render(block,16,0,&pcm,&n));
    if(argc==3 || argc==4) {
        const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&err);CHECK(vol);
        size_t size;uint8_t *bank=gt4_vol_read(vol,"/sound/gt4sys.ins",&size);CHECK(bank);
        const char *names[]={"buy","cancel","cursor","disable","next","oil","ok","param","return","select","license_cursor","license_ok"};
        for(unsigned i=0;i<12;i++) {
            int ok=gt4_sound_render(bank,size,i,&pcm,&n);
            CHECK(ok);long long power=0;
            for(size_t j=0;j<n*2;j++)power+=(long long)pcm[j]*pcm[j];
            CHECK(power>100000 && n>100 && n<48000*(i==5?60:8));
            char path[512];snprintf(path,sizeof path,"%s/%s.wav",argv[2],names[i]);wav(path,pcm,n);
            printf("%s: %zu frames, nonzero PCM\n",names[i],n);free(pcm);
        }
        for(size_t len=0;len<0x500;len++)CHECK(!gt4_sound_render(bank,len,2,&pcm,&n));
        free(bank);
        uint8_t *count_bank=gt4_vol_read(vol,"/sound/gt4count.ins",&size);CHECK(count_bank);
        CHECK(gt4_sound_render(count_bank,size,6,&pcm,&n));
        long long buy_energy=0;for(size_t j=0;j<n*2;j++)buy_energy+=(long long)pcm[j]*pcm[j];
        CHECK(buy_energy>100000 && n>48000);
        char buy_path[512];snprintf(buy_path,sizeof buy_path,"%s/old_buy.wav",argv[2]);wav(buy_path,pcm,n);
        printf("old_buy: bank 2 sequence 6, %zu frames, nonzero PCM\n",n);free(pcm);free(count_bank);
        gt4_sound *effects=gt4_sound_open(vol);CHECK(effects);
        CHECK(gt4_sound_play(effects,"old_buy"));gt4_sound_close(effects);
        const char *groups[]={"home","powerspeed","race","hall","license","mission","gtauto","map","replaytheater"};
        for(unsigned group=0;group<sizeof groups/sizeof groups[0];group++) {
            unsigned count=gt4_music_sequence_count(vol,groups[group]);CHECK(count);
            gt4_music **playlist=!strcmp(groups[group],"map")?calloc(count,sizeof *playlist):NULL;
            if(!strcmp(groups[group],"map"))CHECK(playlist);
            for(unsigned sequence=0;sequence<count;sequence++) {
            gt4_music *track=gt4_music_open_sequence(vol,groups[group],sequence);CHECK(track);
            CHECK(gt4_music_prepare(track,1));
            int16_t block[960];long long energy=0;
            for(int tick=0;tick<1200;tick++) {
                gt4_music_render(block,480,track);
                for(int i=0;i<960;i++)energy+=(long long)block[i]*block[i];
            }
            CHECK(energy>1000000);
            if(playlist)playlist[sequence]=track;else gt4_music_close(track);
            printf("%s[%u]: 12 seconds, nonzero PCM\n",groups[group],sequence);
            }
            if(playlist) {
                for(unsigned i=0;i<count;i++)gt4_music_rewind(playlist[i]);
                unsigned current=0,changes=0;int16_t block[8192];
                for(unsigned tick=0;tick<48000 && changes<count*2;tick++) {
                    unsigned next=gt4_music_render_playlist(block,4096,playlist,count,current);
                    if(next!=current){CHECK(next==(current+1)%count);current=next;changes++;}
                }
                CHECK(changes==count*2 && current==0);
                printf("map playlist: %u automatic changes, two full cycles, no repeat\n",changes);
                for(unsigned i=0;i<count;i++)gt4_music_close(playlist[i]);free(playlist);
            }
        }
        gt4_music *music=gt4_music_open(vol,"arcade");CHECK(music);
        CHECK(gt4_music_prepare(music,argc==4?strtod(argv[3],NULL):1.0));
        size_t frames=48000*12;pcm=calloc(frames*2,sizeof *pcm);CHECK(pcm);
        for(size_t f=0;f<frames;f+=480)gt4_music_render(pcm+f*2,480,music);
        long long power=0;for(size_t i=0;i<frames*2;i++)power+=(long long)pcm[i]*pcm[i];CHECK(power>1000000);
        char path[512];snprintf(path,sizeof path,"%s/arcade_music.wav",argv[2]);wav(path,pcm,frames);
        free(pcm);gt4_music_close(music);puts("Arcade music: 12 seconds, nonzero PCM");gt4_vol_close(vol);
    }
    puts("native sound: passed");return 0;
}
