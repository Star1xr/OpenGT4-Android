/* Native sample-based SQT player. No PS2 CPU, IOP, or sound-chip state. */
#include "music.h"
#include "sound.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
static unsigned u16(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8; }
static uint32_t u32(const uint8_t *p) { return u16(p)|(uint32_t)u16(p+2)<<16; }
static int span(size_t n,size_t p,size_t len) { return p<=n && len<=n-p; }
typedef struct { size_t offset,count,loop; int looped; int16_t *pcm; } sample;
typedef struct { int shift,step,exponential,target; } envelope_phase;
typedef struct { int sample,lo,hi; float root,fine,gain,pan; envelope_phase envelope[4]; } tone;
typedef struct { uint64_t frame; uint8_t type,a,b; } event;
typedef struct { int program,volume,expression,pan,bend,sustain; double bend_factor; } channel;
typedef struct { const tone *tone; double pos,step; float gain,env; int ch,note,release,held,phase,level; unsigned envelope_wait; } voice;
/* INST roots describe samples at 44.1 kHz. The PC mix remains 48 kHz.
   At the Arcade drum-loop root this yields 1.916 s against a 1.920 s bar;
   treating the source as 48 kHz ended the rhythm about 160 ms too early. */
enum { MUSIC_SOURCE_RATE=44100, MUSIC_OUTPUT_RATE=48000 };
struct gt4_music {
    sample samples[256]; int nsamples; double playback_speed;
    tone tones[128][128]; int ntones[128]; uint8_t velocity[128];
    int16_t *cache; size_t cache_pos,cache_frames,cache_loop;
    event *events; size_t nevents,index,loop_index; uint64_t frame,loop_frame,end;
    channel channels[16]; voice voices[64];
};
static int vlq(const uint8_t *b,size_t n,size_t *p,uint32_t *v)
{
    *v=0;
    for(int i=0;i<4 && *p<n;i++) { unsigned c=b[(*p)++]; *v=(*v<<7)|(c&127); if(!(c&128))return 1; }
    return 0;
}
static int load_sample(gt4_music *m,const uint8_t *b,size_t n,size_t off)
{
    for(int i=0;i<m->nsamples;i++)if(m->samples[i].offset==off)return i;
    if(m->nsamples==256 || off>=n)return -1;
    sample *s=&m->samples[m->nsamples];
    if(!gt4_adpcm_decode(b+off,n-off,&s->pcm,&s->count))return -1;
    s->offset=off;
    for(size_t p=0;p<s->count/28;p++) {
        unsigned flags=b[off+p*16+1];
        if((flags&4) && !(flags&1))s->loop=p*28;
        if(flags&1)s->looped=(flags&2)!=0;
    }
    return m->nsamples++;
}
void gt4_music_close(gt4_music *m)
{
    if(!m)return;
    for(int i=0;i<m->nsamples;i++)free(m->samples[i].pcm);
    free(m->cache);free(m->events);free(m);
}
static gt4_music *decode_sequence(const uint8_t *b,size_t n,const uint8_t *q,size_t qn,unsigned sequence)
{
    if(n<0xa0 || qn<12 || memcmp(b,"INST",4) || memcmp(b+0x2c,"SShd",4) || memcmp(q,"Ssqt",4))return NULL;
    size_t data=u32(b+20), size=u32(b+16), pt=0x20ull+u32(b+0x30), vt=0x20ull+u32(b+0x34);
    if(!span(n,data,size) || !span(n,pt,2) || !span(n,vt,130))return NULL;
    unsigned programs=u16(b+pt)+1;
    if(programs>128 || !span(n,pt+2,programs*2))return NULL;
    gt4_music *m=calloc(1,sizeof *m);if(!m)return NULL;
    m->playback_speed=1.0;
    for(int i=0;i<128;i++)for(int j=0;j<128;j++)m->tones[i][j].sample=-1;
    memcpy(m->velocity,b+vt+2,128);
    for(unsigned i=0;i<programs;i++) {
        unsigned offset=u16(b+pt+2+2*i); if(offset==65535)continue;
        size_t prog=pt+offset;
        if(!span(n,prog,8))goto fail;
        int ranged=b[prog]==255, first=b[prog+6], last=b[prog+7];
        int count=ranged?last-first+1:(b[prog]&15)+1;
        if(count<=0 || first>127 || last>127 || !span(n,prog+8,(size_t)count*16))goto fail;
        for(int j=0;j<count;j++) {
            const uint8_t *t=b+prog+8+16*j;
            if (u32(t)==0xffffffff && u32(t+4)==0xffffffff && u32(t+8)==0xffffffff && u32(t+12)==0xffffffff) continue;
            size_t addr=((size_t)t[5]<<16|u16(t+6))*16;
            if(addr>=size || (t[4]&2))goto fail;
            int index=load_sample(m,b+data,size,addr);if(index<0)goto fail;
            int lo=ranged?first+j:t[0],hi=ranged?lo:t[1];
            if(lo>hi || hi>127)goto fail;
            {
                tone *dst=&m->tones[i][m->ntones[i]++];dst->sample=index;dst->lo=lo;dst->hi=hi;
                dst->root=t[2];dst->fine=(int8_t)t[3]/16.0f;
                dst->gain=(b[prog+1]/127.0f)*(t[12]/127.0f);
                dst->pan=(t[13]-64)/64.0f;
                unsigned a=u16(t+8),r=u16(t+10);
                int sustain_step=7-((r>>6)&3);
                if(r&0x4000)sustain_step=-sustain_step-1;
                dst->envelope[0]=(envelope_phase){(a>>10)&31,7-((a>>8)&3),(a>>15)&1,32767};
                dst->envelope[1]=(envelope_phase){(a>>4)&15,-8,1,((a&15)+1)*2048};
                dst->envelope[2]=(envelope_phase){(r>>8)&31,sustain_step,(r>>15)&1,0};
                dst->envelope[3]=(envelope_phase){r&31,-8,(r>>5)&1,0};
            }
        }
    }
    unsigned last=u32(q+4);
    if(last>255 || sequence>last || !span(qn,8,4ull*(last+1)))goto fail;
    size_t start=u32(q+8+4*sequence);
    if(!span(qn,start,8) || memcmp(q+start,"Sssq",4))goto fail;
    unsigned ppqn=u32(q+start+4);if(!ppqn || ppqn>100000)goto fail;
    size_t pos=start+8; unsigned running=0,tempo=500000; double time=0;int ended=0;
    m->events=calloc(100000,sizeof *m->events);if(!m->events)goto fail;
    while(pos<qn && m->nevents<100000) {
        uint32_t delta;if(!vlq(q,qn,&pos,&delta))goto fail;
        time+=(double)delta*tempo*0.048/ppqn;
        if(time>48000.0*3600 || pos>=qn)goto fail;
        unsigned status=q[pos];
        if(status&128){running=status;pos++;}else status=running;
        if(status==255) {
            if(pos>=qn)goto fail;
            unsigned kind=q[pos++];uint32_t len;
            if(!vlq(q,qn,&pos,&len) || !span(qn,pos,len))goto fail;
            if(kind==0x2f) { ended=1;break; }
            if(kind==0x51) { if(len!=3)goto fail;tempo=(q[pos]<<16)|(q[pos+1]<<8)|q[pos+2];if(!tempo)goto fail; }
            pos+=len;continue;
        }
        int type=status>>4;
        int bytes=type==8||type==12||type==14?1:type==9||type==11?2:0;
        if(!bytes || !span(qn,pos,bytes) || q[pos]>127 || (bytes==2 && q[pos+1]>127))goto fail;
        event *e=&m->events[m->nevents++];e->frame=(uint64_t)time;e->type=(uint8_t)status;e->a=q[pos++];e->b=bytes==2?q[pos++]:0;
        if(type==11 && e->a==99 && e->b==20) { m->loop_index=m->nevents;m->loop_frame=e->frame; }
        /* func_0055B528 +0x298..0x2D8: CC99/30 is a loop boundary,
           not end-of-track. A single-pass playlist must read the outro after
           it; only FF2F ends the sequence (0055B948 +0x58..0x64).
           Loop repeat policy remains A in this prepared player. */
    }
    if(!ended || !m->nevents || time<=m->loop_frame)goto fail;
    m->end=(uint64_t)time;
    for(int i=0;i<16;i++){m->channels[i].volume=100;m->channels[i].expression=127;m->channels[i].pan=64;m->channels[i].bend=64;m->channels[i].bend_factor=1;}
    return m;
fail:
    gt4_music_close(m);return NULL;
}
gt4_music *gt4_music_decode(const uint8_t *b,size_t n,const uint8_t *q,size_t qn)
{return decode_sequence(b,n,q,qn,0);}

/* func_00461068 / 00460FE0: addiu wraps the stored uint32 index. */
unsigned gt4_music_next_sequence(unsigned current){return current+1u;}

unsigned gt4_music_sequence_count(gt4_vol *vol,const char *name)
{
    if(!name || !*name || strlen(name)>60)return 0;
    for(const char *p=name;*p;p++)if(!((*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='_'))return 0;
    char path[128];size_t size=0;snprintf(path,sizeof path,"/music/us/%s.sqt",name);
    uint8_t *q=gt4_vol_read(vol,path,&size);
    unsigned count=q && size>=12 && !memcmp(q,"Ssqt",4) && u32(q+4)<256?u32(q+4)+1:0;
    if(count && !span(size,8,4ull*count))count=0;
    free(q);return count;
}

gt4_music *gt4_music_open_sequence(gt4_vol *vol,const char *name,unsigned sequence)
{
    char path[128];size_t n,qn;
    if(!name || !*name || strlen(name)>60)return NULL;
    for(const char *p=name;*p;p++)if(!((*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='_'))return NULL;
    snprintf(path,sizeof path,"/music/us/%s.ins",name);uint8_t *b=gt4_vol_read(vol,path,&n);
    snprintf(path,sizeof path,"/music/us/%s.sqt",name);uint8_t *q=gt4_vol_read(vol,path,&qn);
    gt4_music *m=b&&q?decode_sequence(b,n,q,qn,sequence):NULL;free(b);free(q);return m;
}
gt4_music *gt4_music_open(gt4_vol *vol,const char *name)
{return gt4_music_open_sequence(vol,name,0);}

/* Native amplitude envelope driven by the instrument's ADSR parameters.
   Its clock stays at the output sample rate when sequence tempo is changed. */
static int advance_envelope(voice *v)
{
    if(v->release && v->phase!=3) { v->phase=3;v->envelope_wait=0;v->held=0; }
    if(v->envelope_wait) { v->envelope_wait--;return 1; }
    const envelope_phase *p=&v->tone->envelope[v->phase];
    unsigned interval=p->shift>11 ? 1u<<(p->shift-11) : 1;
    int step=p->step*(p->shift<11 ? 1<<(11-p->shift) : 1);
    if(p->exponential) {
        if(step>0 && v->level>24576)interval*=4;
        else if(step<0)step=(int)floor((double)step*v->level/32768.0);
    }
    v->envelope_wait=interval-1;
    v->level+=step;
    if(v->level<0)v->level=0;
    if(v->level>32767)v->level=32767;
    v->env=v->level/32767.0f;
    if((v->phase==2 || v->phase==3) && !v->level)return 0;
    if((v->phase==0 && v->level>=p->target) || (v->phase==1 && v->level<=p->target)) {
        v->phase++;v->envelope_wait=0;
    }
    return 1;
}
static void note_off(gt4_music *m,int ch,int key)
{
    for(int i=0;i<64;i++){voice *v=&m->voices[i];if(v->tone && v->ch==ch && v->note==key){if(m->channels[ch].sustain)v->held=1;else v->release=1;}}
}
static void handle(gt4_music *m,const event *e)
{
    int ch=e->type&15,type=e->type>>4;channel *c=&m->channels[ch];
    if(type==8 || (type==9 && !e->b)){note_off(m,ch,e->a);return;}
    if(type==12){c->program=e->a;return;}
    if(type==14){c->bend=e->a;c->bend_factor=pow(2.0,((c->bend-64)/64.0)*2/12.0);return;}
    if(type==11){
        if(e->a==7)c->volume=e->b;
        if(e->a==10)c->pan=e->b;
        if(e->a==11)c->expression=e->b;
        if(e->a==64){c->sustain=e->b>=64;if(!c->sustain)for(int i=0;i<64;i++)if(m->voices[i].ch==ch && m->voices[i].held)m->voices[i].release=1;}
        return;
    }
    if(type!=9)return;
    /* Overlapping splits are layers (including the stereo instrument pairs). */
    for(int layer=0;layer<m->ntones[c->program];layer++) {
    const tone *t=&m->tones[c->program][layer];if(e->a<t->lo || e->a>t->hi)continue;
    voice *v=NULL;
    for(int i=0;i<64;i++)if(!m->voices[i].tone){v=&m->voices[i];break;}
    if(!v)return;
    memset(v,0,sizeof *v);v->tone=t;v->ch=ch;v->note=e->a;
    v->step=((double)MUSIC_SOURCE_RATE/MUSIC_OUTPUT_RATE)*m->playback_speed*
        pow(2.0,(e->a-t->root+t->fine)/12.0);
    v->gain=t->gain*m->velocity[e->b]/127.0f;
    }
}
/* A menu playlist consumes each prepared sequence once. Other groups keep
   gt4_music_render's existing loop policy until their original rules are known.
   No disk reads, allocation or decoding runs in the audio callback. */
unsigned gt4_music_render_playlist(int16_t *out,int frames,gt4_music **tracks,
                                   unsigned count,unsigned current)
{return gt4_music_render_playlist_available(out,frames,tracks,NULL,count,current);}
unsigned gt4_music_render_playlist_available(int16_t *out,int frames,gt4_music **tracks,
                                   const unsigned *available,unsigned count,unsigned current)
{
    if(frames<=0)return current;
    memset(out,0,(size_t)frames*4);
    if(!tracks || !count)return current;
    current%=count;
    while(frames>0) {
        gt4_music *m=tracks[current];
        if(!m || !m->cache || !m->cache_frames)break;
        size_t take=m->cache_frames-m->cache_pos;
        if(available){
            if(available[current]<=m->cache_pos)break;
            size_t ready=available[current]-m->cache_pos;if(take>ready)take=ready;
        }
        if(take>(size_t)frames)take=(size_t)frames;
        memcpy(out,m->cache+m->cache_pos*2,take*4);
        m->cache_pos+=take;out+=take*2;frames-=(int)take;
        if(m->cache_pos==m->cache_frames) {
            current=gt4_music_next_sequence(current)%count;
            gt4_music_rewind(tracks[current]);
        }
    }
    return current;
}
static void render_synth(int16_t *out,int frames,gt4_music *m);
void gt4_music_render_available(int16_t *out,int frames,gt4_music *m,unsigned available)
{
    if(frames<=0)return;memset(out,0,(size_t)frames*4);
    if(!m || !m->cache)return;
    while(frames>0){
        if(m->cache_pos==m->cache_frames)m->cache_pos=m->cache_loop;
        if(available<=m->cache_pos)break;
        size_t take=m->cache_frames-m->cache_pos,ready=available-m->cache_pos;
        if(take>ready)take=ready;if(take>(size_t)frames)take=(size_t)frames;
        memcpy(out,m->cache+m->cache_pos*2,take*4);
        m->cache_pos+=take;out+=take*2;frames-=(int)take;
    }
}
void gt4_music_render(int16_t *out,int frames,void *ptr)
{
    gt4_music *m=ptr;memset(out,0,(size_t)frames*4);if(!m)return;
    if(m->cache) {
        while(frames>0) {
            if(m->cache_pos==m->cache_frames)m->cache_pos=m->cache_loop;
            size_t take=m->cache_frames-m->cache_pos;
            if(take>(size_t)frames)take=(size_t)frames;
            memcpy(out,m->cache+m->cache_pos*2,take*4);
            m->cache_pos+=take;out+=take*2;frames-=(int)take;
        }
        return;
    }
    render_synth(out,frames,m);
}
static void render_synth(int16_t *out,int frames,gt4_music *m)
{
    for(int i=0;i<frames;i++) {
        if(m->frame>=m->end){m->frame=m->loop_frame;m->index=m->loop_index;}
        while(m->index<m->nevents && m->events[m->index].frame<=m->frame)handle(m,&m->events[m->index++]);
        float l=0,r=0;
        for(int j=0;j<64;j++) {
            voice *v=&m->voices[j];if(!v->tone)continue;
            const tone *t=v->tone;const sample *s=&m->samples[t->sample];channel *c=&m->channels[v->ch];
            if(v->pos>=s->count){if(s->looped)v->pos=s->loop+fmod(v->pos-s->loop,s->count-s->loop);else{v->tone=NULL;continue;}}
            if(!advance_envelope(v)){v->tone=NULL;continue;}
            size_t k=(size_t)v->pos;
            size_t next=k+1<s->count?k+1:s->looped?s->loop:k;
            float value=(float)(s->pcm[k]+(s->pcm[next]-s->pcm[k])*(v->pos-k));
            value*=v->gain*v->env*(c->volume/127.0f)*(c->expression/127.0f)*0.20f;
            float pan=fmaxf(-1,fminf(1,t->pan+(c->pan-64)/64.0f));
            l+=value*fminf(1,1-pan);r+=value*fminf(1,1+pan);
            v->pos+=v->step*c->bend_factor;
        }
        out[i*2]=(int16_t)fmaxf(-32768,fminf(32767,l));out[i*2+1]=(int16_t)fmaxf(-32768,fminf(32767,r));m->frame++;
    }
}

/* Prepare before opening the audio device. The callback then copies PCM only.
   An optional speed override scales BOTH events and sample playback: changing
   event times alone separates prerecorded rhythmic phrases from their beats.
   This speed control changes pitch too; it is not pitch-preserving stretching. */
int gt4_music_prepare(gt4_music *m,double tempo)
{return gt4_music_prepare_stream(m,tempo,NULL,NULL);}
int gt4_music_prepare_stream(gt4_music *m,double tempo,gt4_music_prepared notify,void *user)
{
    if(!gt4_music_prepare_begin(m,tempo))return 0;
    while(gt4_music_prepare_step(m,notify,user)){}
    return 1;
}
int gt4_music_prepare_begin(gt4_music *m,double tempo)
{
    if(!m || m->cache || m->frame || !isfinite(tempo) || tempo<0.5 || tempo>1.5)return 0;
    uint64_t end=(uint64_t)(m->end/tempo),loop=(uint64_t)(m->loop_frame/tempo);
    if(!end || end>48000ull*600 || loop>=end)return 0;
    int16_t *pcm=malloc((size_t)end*4);if(!pcm)return 0;
    for(size_t i=0;i<m->nevents;i++)m->events[i].frame=(uint64_t)(m->events[i].frame/tempo);
    m->end=end;m->loop_frame=loop;m->playback_speed=tempo;
    m->cache=pcm;m->cache_frames=(size_t)end;m->cache_loop=(size_t)loop;m->cache_pos=0;
    return 1;
}
int gt4_music_prepare_step(gt4_music *m,gt4_music_prepared notify,void *user)
{
    if(!m || !m->cache || m->frame>=m->end)return 0;
    unsigned block=(unsigned)(m->end-m->frame);if(block>512)block=512;
    render_synth(m->cache+(size_t)m->frame*2,(int)block,m);
    if(notify)notify(user,(unsigned)m->frame);
    return m->frame<m->end;
}
void gt4_music_rewind(gt4_music *m) { if(m && m->cache)m->cache_pos=0; }
