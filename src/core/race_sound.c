/* Race bank formats/control: 005595C8, 00559470, 00558FD8,
   00558DB0, 00558E28, 00559B10 and 005599D0 (US v2.00).
   Files come exclusively from the user's disc; no prerecorded substitute. */
#include "race_sound.h"
#include <math.h>
#include "sound.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

static unsigned u16(const uint8_t *b){return b[0]|(unsigned)b[1]<<8;}
static uint32_t u32(const uint8_t *b){return u16(b)|(uint32_t)u16(b+2)<<16;}
static int span(size_t n,size_t p,size_t size){return p<=n && size<=n-p;}
void gt4_race_sample_clear(gt4_race_sample *s){if(s){free(s->pcm);free((void *)s->wave.adpcm);memset(s,0,sizeof *s);}}
int gt4_race_sample_decode(gt4_race_sample *s,const uint8_t *b,size_t n)
{
    if(!s || !b)return 0;
    memset(s,0,sizeof *s);size_t blocks=0,loop=0;int found=0;
    /* SPU ADPCM flag 4 sets the repeat address; end+repeat = 3.
       ENGN samples on this disc use 0,6,2,...,3. INST one-shots end
       with 1 or 7. Preserve the recorded start, rather than looping
       the first silent block (the previous menu decoder discards it). */
    for(size_t p=0;span(n,p,16);p+=16){
        if((b[p]>>4)>4 || (b[p]&15)>12 || b[p+1]>7)return 0;
        if(b[p+1]&4)loop=blocks*28;
        blocks++;
        if(b[p+1]&1){s->repeat=!!(b[p+1]&2);found=1;break;}
    }
    if(!found || !gt4_adpcm_decode(b,blocks*16,&s->pcm,&s->frames))return 0;
    uint8_t *copy=malloc(blocks*16);if(!copy){gt4_race_sample_clear(s);return 0;}
    memcpy(copy,b,blocks*16);s->wave.adpcm=copy;s->wave.blocks=blocks;
    s->loop=loop;return 1;
}
gt4_engine_bank *gt4_engine_bank_decode(const uint8_t *b,size_t n,unsigned index)
{
    if(!b || n<48 || index>1 || memcmp(b,"ENGN",4))return NULL;
    unsigned count=u32(b+24+index*8);size_t at=u32(b+28+index*8);
    size_t data=u32(b+20),length=u32(b+16);
    if(!count || count>256 || !span(n,at,(size_t)count*16) || !span(n,data,length))return NULL;
    gt4_engine_bank *bank=calloc(1,sizeof *bank);if(!bank)return NULL;
    bank->layers=calloc(count,sizeof *bank->layers);if(!bank->layers){free(bank);return NULL;}
    bank->count=count;
    for(unsigned i=0;i<count;i++){
        const uint8_t *r=b+at+i*16;gt4_engine_layer *s=bank->layers+i;
        s->rpm=(int16_t)u16(r);s->minimum=(int16_t)u16(r+2);
        s->maximum=(int16_t)u16(r+4);s->gain=(int16_t)u16(r+6);
        s->pitch=u32(r+8);s->offset=u32(r+12);
        if(s->rpm<=0 || s->minimum>s->maximum || s->offset>=length ||
           !gt4_race_sample_decode(&s->sample,b+data+s->offset,length-s->offset)){
            gt4_engine_bank_close(bank);return NULL;
        }
    }
    return bank;
}
gt4_engine_bank *gt4_engine_bank_open(gt4_vol *v,unsigned id,int variant)
{
    /* 003923D0 +0x48, +0x94: engine/%05d and folder table 006213F8.
       A missing file stays missing; never replace one car with another. */
    static const char *folder[]={"normal0","normal1","normal2","normal3","turbo0","turbo1","turbo2","turbo3"};
    if(variant < -1 || variant>7 || id>99999)return NULL;
    char path[64];snprintf(path,sizeof path,"/sound/%s/%05u",variant<0?"engine":folder[variant],id);
    size_t n;uint8_t *b=gt4_vol_read(v,path,&n);
    gt4_engine_bank *bank=b?gt4_engine_bank_decode(b,n,0):NULL;free(b);return bank;
}
void gt4_engine_bank_close(gt4_engine_bank *b)
{if(b){for(unsigned i=0;i<b->count;i++)gt4_race_sample_clear(&b->layers[i].sample);free(b->layers);free(b);}}
void gt4_engine_control_init(gt4_engine_control *c)
{memset(c,0,sizeof *c);c->voice[0].layer=c->voice[1].layer=-1;}
static int slew(int value,int old)
{
    /* 00558E28 +0xEC..+0x17C limits magnitudes, retaining new sign. */
    int negative=value<0;if(negative)value=-value;if(old<0)old=-old;
    if(value<old-4096)value=old-4096;if(old+4096<value)value=old+4096;
    return negative?-value:value;
}
void gt4_engine_control_step(const gt4_engine_bank *b,gt4_engine_control *c,
                             int rpm,int16_t left,int16_t right,const int16_t *sine)
{
    static const int voiced[2]={1,1};
    gt4_engine_control_step_ex(b,c,rpm,left,right,sine,voiced);
}
void gt4_engine_control_step_ex(const gt4_engine_bank *b,gt4_engine_control *c,
                                int rpm,int16_t left,int16_t right,const int16_t *sine,const int voiced[2])
{
    if(!b || !c || !sine)return;
    /* 00558FD8 +0x50: minimum RPM 200. Choose one plateau or two
       adjacent layers between their maximum/minimum, inclusively. */
    int chosen[2]={c->voice[0].layer,c->voice[1].layer};if(rpm<200)rpm=200;
    for(unsigned i=0;i<b->count;i++){
        const gt4_engine_layer *s=b->layers+i;
        if(s->minimum<=rpm && rpm<=s->maximum){chosen[0]=(int)i;chosen[1]=-1;break;}
        if(i && b->layers[i-1].maximum<rpm && rpm<s->minimum){chosen[0]=(int)i-1;chosen[1]=(int)i;break;}
    }
    /* 00558FD8 +0x100..+0x12C keeps active sample identity in its slot. */
    if(chosen[0]==c->voice[1].layer || chosen[1]==c->voice[0].layer){int t=chosen[0];chosen[0]=chosen[1];chosen[1]=t;}
    for(unsigned i=0;i<2;i++){
        gt4_engine_voice old=c->voice[i],*v=c->voice+i;v->layer=chosen[i];
        if(v->layer<0){v->pitch=0;v->left=v->right=0;continue;}
        const gt4_engine_layer *s=b->layers+v->layer;
        uint32_t pitch=(s->pitch*(uint32_t)rpm)/(uint32_t)s->rpm;
        if((int32_t)pitch<=0)pitch=1;
        /* 00558FD8 +0x148..+0x190: the slew only for a slot that kept
           both its layer and its hardware voice */
        int continuing=old.layer==v->layer && old.layer>=0 && voiced[i];
        if(continuing){if(pitch<old.pitch/2)pitch=old.pitch/2;if(old.pitch*2<pitch)pitch=old.pitch*2;}
        v->pitch=pitch;int fade=16384;
        if(chosen[1-i]>=0){
            const gt4_engine_layer *other=b->layers+chosen[1-i];
            if(rpm<s->minimum)fade=((rpm-other->maximum)*16384)/(s->minimum-other->maximum);
            else if(s->maximum<rpm)fade=((other->minimum-rpm)*16384)/(other->minimum-s->maximum);
        }
        if(fade<0)fade=0;
        int wave=sine[((uint32_t)fade>>4)&4095];
        int curve=(wave*16383)>>12,gain=(s->gain*curve)>>14;
        int l=(left*gain)>>14,r=(right*gain)>>14;
        v->left=(int16_t)(continuing?slew(l,old.left):l);
        v->right=(int16_t)(continuing?slew(r,old.right):r);
    }
}
/* 00559B10 (first program table), 00559D90 (split iterator),
   005599D0 (24-bit sample address, address unit 8 on this disc). */
static int instrument(gt4_race_effect *s,const uint8_t *b,size_t n)
{
    if(n<0xa0 || memcmp(b,"INST",4) || memcmp(b+44,"SShd",4))return 0;
    /* SShd's table list: banks with sequences (gt4count.ins) keep the
       program table at +0x44, the race banks at +0x30 (the menu decoder
       reads gt4sys/gt4count at +0x44 too) */
    size_t pt=32ull+(u32(b+0x44)!=0xffffffffu?u32(b+0x44):u32(b+48)),data=u32(b+20),len=u32(b+16);
    if(!span(n,pt,2) || !span(n,data,len) || s->program>u16(b+pt) || !span(n,pt+2+s->program*2,2))return 0;
    unsigned off=u16(b+pt+2+s->program*2);if(off==65535)return 0;
    size_t prog=pt+off;if(!span(n,prog,8))return 0;
    unsigned count=b[prog]==255?(unsigned)b[prog+7]-b[prog+6]+1:(b[prog]&15)+1;
    if(count>128 || !span(n,prog+8,count*16))return 0;
    for(unsigned i=0;i<count;i++){
        const uint8_t *r=b+prog+8+i*16;
        /* 00611AA0 +0x58..+0x6C: a program marked FFh holds one tone per
           note from its +6 on; the others split by each tone's range */
        if(b[prog]==255?s->note!=b[prog+6]+i:(s->note<r[0] || s->note>r[1]))continue;
        size_t sample=((size_t)r[5]<<16|u16(r+6))*8;
        if(sample>=len || (r[4]&2))return 0;
        s->root=r[2];s->fine=(int8_t)r[3];s->flags=r[4];s->range=r[14];
        s->volume=b[prog+1]*r[12];s->pan=r[13];s->adsr1=(uint16_t)u16(r+8);s->adsr2=(uint16_t)u16(r+10);
        return gt4_race_sample_decode(&s->sample,b+data+sample,len-sample);
    }
    return 0;
}
int gt4_race_tone_open(gt4_race_effect *s,const uint8_t *ins,size_t n,unsigned program,unsigned note)
{
    if(!s || !ins)return 0;
    memset(s,0,sizeof *s);s->program=program;s->note=note;
    return instrument(s,ins,n);
}
gt4_race_effects *gt4_race_effects_open(gt4_vol *v)
{
    size_t n,bn[2]={0};uint8_t *b[2]={0},*names=gt4_vol_read(v,"/sound/gt4se.inf",&n);
    gt4_race_effects *bank=NULL;
    if(!names || n<16 || memcmp(names,"GTSE",4) || u32(names+4)!=1)goto done;
    unsigned count=u32(names+12);if(count>(n-16)/8)goto done;
    bank=calloc(1,sizeof *bank);if(!bank)goto done;
    bank->effects=calloc(count,sizeof *bank->effects);if(!bank->effects){free(bank);bank=NULL;goto done;}
    /* 00463000 +0x88 and 00462E18 +0x20 install banks 1 and 3. */
    b[0]=gt4_vol_read(v,"/sound/gt4race.ins",bn);b[1]=gt4_vol_read(v,"/sound/gt4race2.ins",bn+1);
    for(unsigned i=0;i<count;i++){
        const uint8_t *e=names+16+i*8;if(e[7]!=1 && e[7]!=3)continue;
        size_t name=u32(e);if(name>=n || !memchr(names+name,0,n-name) || strlen((char *)names+name)>=48)goto failed;
        gt4_race_effect *s=bank->effects+bank->count++;
        strcpy(s->name,(char *)names+name);s->note=e[4];s->program=e[5];s->kind=e[6];s->bank=e[7];
        /* Keep an absent program in the inventory; it cannot play. */
        if(!s->kind && b[s->bank==3])instrument(s,b[s->bank==3],bn[s->bank==3]);
    }
    goto done;
failed:gt4_race_effects_close(bank);bank=NULL;
done:free(names);free(b[0]);free(b[1]);return bank;
}
const gt4_race_effect *gt4_race_effect_find(const gt4_race_effects *b,const char *name)
{if(b && name)for(unsigned i=0;i<b->count;i++)if(!strcmp(name,b->effects[i].name))return b->effects+i;return NULL;}
void gt4_race_effects_close(gt4_race_effects *b)
{if(b){for(unsigned i=0;i<b->count;i++)gt4_race_sample_clear(&b->effects[i].sample);free(b->effects);free(b);}}
void gt4_race_sample_mix(const gt4_race_sample *s,gt4_race_cursor *c,uint32_t pitch,
                         int16_t left,int16_t right,int32_t *out,int frames)
{
    if(!s || !s->frames || !s->pcm || !pitch || c->ended)return;
    uint64_t end=(uint64_t)s->frames*4096,start=(uint64_t)s->loop*4096;
    for(int i=0;i<frames;i++){
        if(c->position>=end){
            if(!s->repeat){c->ended=1;break;}
            c->position=start+(c->position-end)%(end-start);
        }
        size_t at=(size_t)(c->position>>12),next=at+1;
        if(next==s->frames)next=s->repeat?s->loop:at;
        /* A: existing PC pipeline uses linear interpolation, not SPU2
           gaussian interpolation/ADSR/reverb. Decode/selection remain
           separate, testable original-data operations. */
        int v=s->pcm[at]+(int)(((int64_t)s->pcm[next]-s->pcm[at])*(c->position&4095)/4096);
        out[i*2]+=(v*left)>>14;out[i*2+1]+=(v*right)>>14;c->position+=pitch;
    }
}

/* MIPS div: quotient truncated toward zero, as C. */
void gt4_sd_volume(const gt4_sd_voice *v,int32_t group_volume,int stereo,const uint8_t *pan_table,int16_t out[2])
{
    int32_t t4=(v->master*v->level)>>14;
    t4=(t4*group_volume)>>14;
    t4=t4*v->v58/127;t4=t4*v->v5c/127;t4=t4*v->v60/127;
    if(v->group==2)t4=t4*v->group2/127;
    int32_t pan=v->pan+v->pan6c+v->pan70-0x80;
    if(pan<0)pan=0;if(pan>127)pan=127;
    int32_t l=((t4*pan_table[pan*2]/127)*v->left)>>14,r=((t4*pan_table[pan*2+1]/127)*v->right)>>14;
    l=l<-0x4000?-0x4000:l>0x3fff?0x3fff:l;r=r<-0x4000?-0x4000:r>0x3fff?0x3fff:r;
    /* +0x27C..+0x2B4: mono output averages the magnitudes. */
    if(!stereo)l=r=((l<0?-l:l)+(r<0?-r:r))/2;
    out[0]=(int16_t)l;out[1]=(int16_t)r;
}
uint16_t gt4_sd_pitch(const gt4_sd_voice *v,const uint8_t *octave)
{
    int32_t note=v->note,bend=v->bend,range=v->range;
    if(v->group==1){
        /* +0x42C..+0x450: group 1 folds the bend into the note. */
        int32_t b=(bend-0x40)*range;note+=(b<0?b+3:b)>>2;bend=0x40;range=1;
    }
    int32_t at=note+v->fine-v->root+(((bend-0x40)*range)>>2);
    int32_t q=at/192,r=at%192,shift=12-q;
    if(r<0){shift++;r+=192;}
    uint32_t p=u32(octave+r*4)>>(shift&31);
    return (uint16_t)(p<0x3fff?p:0x3fff);
}
uint16_t gt4_sd_pitch_register(const gt4_sd_voice *v){return (uint16_t)(((int32_t)v->pitch*v->scale)>>12);}
void gt4_sd_voice_tone(gt4_sd_voice *v,const gt4_race_effect *s,int16_t left,int16_t right,
                       int32_t scale,int32_t pan,int once)
{
    /* Defaults as every allocated voice holds them in the captures
       (ar_02..05, b_reta..: +0x4C 3FFFh, +0x50/+0x58/+0x5C/+0x60 127,
       +0x6C 64, +0x88 40h); group 5 for tones (00559EB0 +0xC8). */
    memset(v,0,sizeof *v);
    v->group=5;v->master=0x3fff;v->group2=v->v58=v->v5c=v->v60=127;v->pan6c=64;v->bend=0x40;
    v->note=0x3c0;v->root=(int32_t)s->root*16;v->fine=s->fine;v->range=s->range;
    v->level=(int32_t)s->volume;v->pan70=(int32_t)s->pan;
    v->adsr1=s->adsr1;v->adsr2=s->adsr2;
    v->left=left;v->right=right;v->scale=scale;v->pan=pan;
    /* +0x2B0..+0x2DC: reverb send when the tone has flags and the
       handle isn't a one-shot request. */
    v->reverb=s->flags && !once;
}
