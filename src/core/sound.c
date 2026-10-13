#include "sound.h"
#include "sound_sequence.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
static unsigned u16(const uint8_t *b) { return b[0] | (unsigned)b[1]<<8; }
static uint32_t u32(const uint8_t *b) { return u16(b) | (uint32_t)u16(b+2)<<16; }
static int span(size_t n, size_t p, size_t len) { return p<=n && len<=n-p; }
static int clamp(int n) { return n<-32768?-32768:n>32767?32767:n; }

int gt4_adpcm_decode(const uint8_t *b, size_t n, int16_t **pcm, size_t *frames)
{
    *pcm=NULL; *frames=0;
    size_t blocks=0; int ended=0;
    for (size_t p=0; span(n,p,16) && blocks<48000; p+=16) {
        if ((b[p]>>4)>4 || (b[p]&15)>12 || b[p+1]>7) return 0;
        blocks++;
        if (b[p+1]&1) { ended=1; break; }
    }
    if (!ended || !blocks) return 0;
    int16_t *out=malloc(blocks*28*sizeof *out);
    if (!out) return 0;
    static const int co[5][2]={{0,0},{60,0},{115,-52},{98,-55},{122,-60}};
    int h1=0,h2=0; size_t k=0;
    for (size_t p=0;p<blocks*16;p+=16) {
        int shift=b[p]&15, filter=b[p]>>4;
        for (int j=0;j<28;j++) {
            int v=(b[p+2+j/2]>>((j&1)*4))&15;
            if (v>=8) v-=16;
            v=clamp((v*4096 >> shift)+((h1*co[filter][0]+h2*co[filter][1]+32)>>6));
            out[k++]=(int16_t)v; h2=h1; h1=v;
        }
    }
    *pcm=out; *frames=k; return 1;
}

/* Damped stereo room for voices with the original JamSplit reverb flag. This is a provisional
   replacement for the original hardware reverb; exact preset parity still
   requires a captured original impulse response. Music has its own path. */
static size_t menu_reverb(float *mix,const float *send,size_t dry,size_t capacity)
{
    enum {LINES=4,MAX_DELAY=2300};
    static const int delay[2][LINES]={{1423,1781,1973,2099},{1459,1811,2011,2143}};
    float *buffer=calloc(2*LINES*MAX_DELAY,sizeof *buffer);
    if(!buffer)return dry;
    int pos[2][LINES]={{0}};float damp[2][LINES]={{0}};
    size_t end=dry+48000;if(end>capacity)end=capacity;
    for(size_t f=0;f<end;f++) {
        float input[2]={send[f*2],send[f*2+1]};
        for(int c=0;c<2;c++) {
            float wet=0;
            for(int k=0;k<LINES;k++) {
                float *line=buffer+(c*LINES+k)*MAX_DELAY;
                float echo=line[pos[c][k]];
                damp[c][k]=echo*.65f+damp[c][k]*.35f;
                line[pos[c][k]]=(input[c]*.8f+input[1-c]*.2f)*.25f+damp[c][k]*.73f;
                wet+=echo;if(++pos[c][k]==delay[c][k])pos[c][k]=0;
            }
            /* Provisional depth: the original SPU2 preset is not yet captured. */
            mix[f*2+c]+=wet*.60f;
        }
    }
    free(buffer);return end;
}

/* The system bank uses Se A0 note events, followed by a big-endian VLQ
   delay. Reject other commands rather than misinterpreting them as notes. */
int gt4_sound_render(const uint8_t *b, size_t n, unsigned seq, int16_t **pcm, size_t *frames)
{
    *pcm=NULL; *frames=0;
    if (n<0xa0 || memcmp(b,"INST",4) || memcmp(b+0x2c,"SShd",4)) return 0;
    size_t data=u32(b+20), data_size=u32(b+16);
    if (!span(n,data,data_size)) return 0;
    size_t st=0x20ull+u32(b+0x3c), pt=0x20ull+u32(b+0x44);
    if (!span(n,st,2) || !span(n,pt,2) || seq>u16(b+st)) return 0;
    if (!span(n,st+2+2*seq,2)) return 0;
    size_t entry=st+u16(b+st+2+2*seq);
    if (!span(n,entry,4) || u16(b+entry)!=0) return 0;
    size_t pos=st+u16(b+entry+2);
    /* A bounded mix buffer keeps malformed sequences from allocating endlessly. */
    enum { LIMIT=48000*8 };
    float *mix=calloc(LIMIT*4,sizeof *mix);
    if (!mix) return 0;
    float *send=mix+LIMIT*2;int has_reverb=0;
    size_t time=0, used=0;unsigned ticks=0; int ended=0;
    for (int ev=0; ev<64 && span(n,pos,2);ev++) {
        if (b[pos]==0xff && b[pos+1]==0x2f) { ended=1; break; }
        if (!span(n,pos,4) || b[pos++]!=0xa0) goto fail;
        unsigned note=b[pos++], velocity=b[pos++], program=b[pos++];
        if (!velocity || velocity>127 || note>127 || program>u16(b+pt) || !span(n,pt+2+2*program,2)) goto fail;
        size_t prog=pt+u16(b+pt+2+2*program);
        if (!span(n,prog,8) || b[prog]!=255 || note<b[prog+6] || note>b[prog+7]) goto fail;
        size_t split=prog+8+16*(note-b[prog+6]);
        if (!span(n,split,16) || (b[split+4]&2)) goto fail;
        int reverb=(b[split+4]&0x80)!=0;
        has_reverb|=reverb;
        /* These are 8-byte address units in the supported US system bank.
           All 18 sample starts and bank end were checked on the user's disc. */
        size_t sample=((size_t)b[split+5]<<16 | u16(b+split+6))*8;
        if (sample>=data_size) goto fail;
        int16_t *raw=NULL; size_t count=0;
        if (!gt4_adpcm_decode(b+data+sample,data_size-sample,&raw,&count)) goto fail;
        /* Same tone rule as the music banks (music.c): the note transposes
           from the split's root, fine in 1/16 semitone, samples at 44.1 kHz.
           Every system effect's root lies near the note it plays (cursor:
           note 49, root 48); the former root-vs-C4 rule played cursor,
           select and param over an octave low and twice as long. */
        double rate=(44100.0/48000.0)*pow(2.0,((int)note-(int)b[split+2]+(int8_t)b[split+3]/16.0)/12.0);
        double gain=(velocity/127.0)*(b[prog+1]/127.0)*(b[split+12]/127.0)*0.65;
        double pan=fmin(1.0,b[split+13]/127.0);
        size_t length=(size_t)ceil(count/rate);
        if (length>LIMIT-time) { free(raw); goto fail; }
        for (size_t j=0;j<length;j++) {
            double at=j*rate; size_t k=(size_t)at;
            if (k>=count) break;
            double v=raw[k]+(raw[k+1<count?k+1:k]-raw[k])*(at-k);
            float left=(float)(v*gain*fmin(1.0,2*(1-pan)));
            float right=(float)(v*gain*fmin(1.0,2*pan));
            mix[(time+j)*2]+=left;mix[(time+j)*2+1]+=right;
            if(reverb){send[(time+j)*2]+=left;send[(time+j)*2+1]+=right;}
        }
        free(raw);
        if (time+length>used) used=time+length;
        unsigned delay=0; int done=0;
        for (int i=0;i<4 && pos<n;i++) { unsigned c=b[pos++]; delay=(delay<<7)|(c&127); if (!(c&128)) { done=1; break; } }
        if (!done || delay>8*480-ticks) goto fail;
        ticks+=delay;time=gt4_se_sample_time(ticks);
        if (time>=LIMIT) goto fail;
    }
    if (!ended || !used) goto fail;
    if(has_reverb)used=menu_reverb(mix,send,used,LIMIT);
    *pcm=malloc(used*2*sizeof **pcm);
    if (!*pcm) goto fail;
    for (size_t i=0;i<used*2;i++) (*pcm)[i]=(int16_t)clamp((int)mix[i]);
    *frames=used; free(mix); return 1;
fail:
    free(mix); return gt4_sound_render_extended(b,n,seq,pcm,frames);
}
struct gt4_sound {
    uint8_t *bank,*names,*count_bank;size_t bank_size,names_size,count_size,shutter_size;
    uint8_t *shutter_bank;float gain;gt4_vol *vol;
    gt4_se_tables service_tables;int service_tables_ready;
    uint8_t *libsd;size_t libsd_size;
    int16_t *service_pcm[4];size_t service_frames[4];
};
gt4_sound *gt4_sound_open(gt4_vol *vol)
{
    gt4_sound *s=calloc(1,sizeof *s); if (!s) return NULL;
    s->gain=1; s->vol=vol;
    s->bank=gt4_vol_read(vol,"/sound/gt4sys.ins",&s->bank_size);
    s->names=gt4_vol_read(vol,"/sound/gt4se.inf",&s->names_size);
    if (!s->bank || !s->names || s->names_size<16 || memcmp(s->names,"GTSE",4)) { gt4_sound_close(s); return NULL; }
    return s;
}
void gt4_sound_close(gt4_sound *s) { if(s) { free(s->bank); free(s->names); free(s->count_bank); free(s->shutter_bank);
    free(s->libsd);for(unsigned i=0;i<4;i++)free(s->service_pcm[i]);free(s); } }
int gt4_sound_shutter(gt4_sound *s,const char *kind)
{
    if(!s)return 0;char path[64];size_t n=kind?strlen(kind):0;
    /* 00462EC8 +B4..DC lowercases the selection; +D8..10C builds
       /sound/gt4shutter<kind>.ins. Only the authored typea..types options. */
    if(n && (n!=5 || strncmp(kind,"type",4) || kind[4]<'a' || kind[4]>'s'))return 0;
    snprintf(path,sizeof path,"/sound/gt4shutter%s.ins",kind?kind:"");
    size_t size;uint8_t *bank=gt4_vol_read(s->vol,path,&size);if(!bank)return 0;
    free(s->shutter_bank);s->shutter_bank=bank;s->shutter_size=size;return 1;
}
void gt4_sound_gain(gt4_sound *s,float gain)
{if(s)s->gain=isfinite(gain)?fminf(2,fmaxf(0,gain)):1;}
int gt4_sound_play(gt4_sound *s, const char *name)
{
    unsigned count=u32(s->names+12);
    if (count>(s->names_size-16)/8) return 0;
    for (unsigned i=0;i<count;i++) {
        const uint8_t *e=s->names+16+8*i;
        size_t off=u32(e);
        if (off>=s->names_size || !memchr(s->names+off,0,s->names_size-off)) return 0;
        if (strcmp((char *)s->names+off,name)) continue;
        /* 00463098 installs gt4count at bank 2; GetCarRoot plays old_buy,
           GTSE bank 2 / sequence 6, after preparing the acquired car. */
        const uint8_t *bank=s->bank;size_t bank_size=s->bank_size;
        if(e[7]==2) {
            if(!s->count_bank)s->count_bank=gt4_vol_read(s->vol,"/sound/gt4count.ins",&s->count_size);
            bank=s->count_bank;bank_size=s->count_size;
        } else if(e[7]==4) {
            if(!s->shutter_bank && !gt4_sound_shutter(s,NULL))return 0;
            bank=s->shutter_bank;bank_size=s->shutter_size;
        } else if(e[7]!=0)return 0;
        if(!bank)return 0;
        int16_t *pcm=NULL;size_t frames=0;int owned=1;
        int service=e[7]==0 && e[5]==5?0:e[7]==2 && e[5]==5?1:e[7]==2 && e[5]==7?2:e[7]==2 && e[5]==8?3:-1;
        if(service>=0){
            if(!s->service_tables_ready){
                s->service_tables_ready=gt4_se_tables_load(s->vol,&s->service_tables)?1:-1;
                s->libsd=gt4_disc_read_file(gt4_vol_source(s->vol),"IRX/LIBSD.IRX",&s->libsd_size);
            }
            if(s->service_tables_ready<0 || !s->libsd)return 0;
            if(!s->service_pcm[service] && !gt4_sound_render_service(bank,bank_size,e[5],&s->service_tables,s->libsd,s->libsd_size,
                 &s->service_pcm[service],&s->service_frames[service]))return 0;
            pcm=s->service_pcm[service];frames=s->service_frames[service];owned=0;
        }else if(!gt4_sound_render(bank,bank_size,e[5],&pcm,&frames))return 0;
        if(s->gain!=1){
            if(!owned){int16_t *copy=malloc(frames*4);if(!copy)return 0;memcpy(copy,pcm,frames*4);pcm=copy;owned=1;}
            for(size_t j=0;j<frames*2;j++)pcm[j]=(int16_t)clamp((int)(pcm[j]*s->gain));
        }
        if(getenv("OPENGT4_SOUND_TRACE"))fprintf(stderr,"sound: %s bank=%u sequence=%u frames=%zu\n",name,e[7],e[5],frames);
        int ok=plat_audio_play(pcm,frames,48000,2);
        if(owned)free(pcm);return ok;
    }
    return 0;
}
