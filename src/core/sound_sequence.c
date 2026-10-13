/* Extended Se sequences used by GT Auto. Event evidence: 0055AED8,
   0055AF48 +0x158..0x230 (B0/41), 0055D240 -> 0055D580 -> 0055D420.
   Scalar clock/ramps are compared by service_sound_clock. The final
   waveform and its phase against IOP/GS presentation remain A. */
#include "sound.h"
#include "sound_sequence.h"
#include "race_sound.h"
#include "inflate.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {size_t at;unsigned note,velocity,program,kind;int delta;unsigned duration;} event;
static unsigned u16(const uint8_t *b){return b[0]|(unsigned)b[1]<<8;}
static unsigned u32(const uint8_t *b){return u16(b)|(u16(b+2)<<16);}
static int span(size_t n,size_t at,size_t len){return at<=n && len<=n-at;}
enum{EVENTS=128,LIMIT=48000*60};
static unsigned sequence_events(const uint8_t *b,size_t n,unsigned seq,event events[EVENTS],size_t *end){
 unsigned count=0,ticks=0;size_t time=0;int ended=0;
 if(n<0xa0 || memcmp(b,"INST",4) || memcmp(b+0x2c,"SShd",4))return 0;
 size_t st=32ull+u32(b+0x3c);if(!span(n,st,2) || seq>u16(b+st) || !span(n,st+2+seq*2,2))return 0;
 size_t entry=st+u16(b+st+2+seq*2);if(!span(n,entry,4) || u16(b+entry))return 0;
 size_t pos=st+u16(b+entry+2);
 while(count<EVENTS && span(n,pos,2)){
  if(b[pos]==0xff && b[pos+1]==0x2f){ended=1;break;}
  event e={0};e.at=time;
  if(b[pos]==0xa0){if(!span(n,pos,4))return 0;e.note=b[pos+1];e.velocity=b[pos+2];e.program=b[pos+3];pos+=4;
   if(e.note>127 || e.velocity>127)return 0;
  }else if(b[pos]==0xb0 && b[pos+1]==0x41){
   if(!span(n,pos,6))return 0;e.kind=1;e.duration=b[pos+2]*1000/15;
   /* Original signed byte * 1200000 / 1000000: delta in 1/16 semitone.
      The last byte selects the currently sounding note, not a new note. */
   e.delta=(int8_t)b[pos+3]*1200000/1000000;e.program=b[pos+4];e.note=b[pos+5];pos+=6;
   if(e.note>127)return 0;
  }else return 0;
  events[count++]=e;unsigned delay=0;int done=0;
  for(int i=0;i<4 && pos<n;i++){unsigned v=b[pos++];delay=(delay<<7)|(v&127);if(!(v&128)){done=1;break;}}
  /* 0055AD20 +0x24/0x40 and 0055B1E0 +0x2c: 120 BPM, 240 ppqn.
     0055AB98 +0x60..0xe0 dispatches at 8 ticks per 60 Hz driver frame. */
  if(!done || delay>480*60-ticks)return 0;ticks+=delay;time=gt4_se_sample_time(ticks);
  if(time>=LIMIT)return 0;
 }
 if(!ended || !count)return 0;*end=time;return count;
}
int gt4_sound_render_extended(const uint8_t *b,size_t n,unsigned seq,int16_t **pcm,size_t *frames){
 event events[EVENTS];size_t end=0;unsigned count=sequence_events(b,n,seq,events,&end);
 *pcm=NULL;*frames=0;if(!count)return 0;
 float *mix=calloc((size_t)LIMIT*2,sizeof *mix);if(!mix)return 0;size_t used=0;int any=0;
 for(unsigned i=0;i<count;i++){
  const event *e=&events[i];if(e->kind || !e->velocity)continue;
  gt4_race_effect tone;if(!gt4_race_tone_open(&tone,b,n,e->program,e->note))goto fail;
  double base=(44100.0/48000.0)*pow(2.0,((int)e->note-(int)tone.root+tone.fine/16.0)/12.0);
  double gain=(e->velocity/127.0)*(tone.volume/(127.0*127.0))*.65,pan=fmin(1.0,tone.pan/127.0);
  size_t stop=LIMIT;unsigned next=i+1;
  for(unsigned j=i+1;j<count;j++)if(!events[j].kind && !events[j].velocity && events[j].note==e->note && events[j].program==e->program){stop=events[j].at;break;}
  if(tone.sample.repeat && stop==LIMIT){gt4_race_sample_clear(&tone.sample);goto fail;}
  double at=0;int32_t bend=0;gt4_se_ramp ramp={0,0,0,1};
  for(size_t f=e->at;f<stop && f<LIMIT;f++){
   while(next<count && events[next].at<=f){const event *a=&events[next++];if(a->kind && a->note==e->note && a->program==e->program){gt4_se_ramp_start(&ramp,(int)a->duration,bend+a->delta,bend,60);}}
   if(!(f%800))gt4_se_ramp_step(&ramp,&bend);
   if(at>=tone.sample.frames){if(!tone.sample.repeat)break;double len=(double)(tone.sample.frames-tone.sample.loop);if(len<=0)break;at=tone.sample.loop+fmod(at-tone.sample.loop,len);}
   size_t k=(size_t)at,k1=k+1;if(k1==tone.sample.frames)k1=tone.sample.repeat?tone.sample.loop:k;
   double v=(tone.sample.pcm[k]+(tone.sample.pcm[k1]-tone.sample.pcm[k])*(at-k))*gain;
   mix[f*2]+=(float)(v*fmin(1.0,2*(1-pan)));mix[f*2+1]+=(float)(v*fmin(1.0,2*pan));
   at+=base*pow(2.0,bend/(16.0*12));if(f+1>used)used=f+1;
  }
  gt4_race_sample_clear(&tone.sample);any=1;
 }
 if(!any || !used)goto fail;*pcm=malloc(used*2*sizeof **pcm);if(!*pcm)goto fail;
 for(size_t i=0;i<used*2;i++){float v=mix[i];(*pcm)[i]=(int16_t)(v>32767?32767:v<-32768?-32768:v);}
 *frames=used;free(mix);return 1;
fail:free(mix);return 0;
}

int gt4_se_tables_load(gt4_vol *vol,gt4_se_tables *out){
 size_t n=0;uint8_t *raw=gt4_disc_read_root(gt4_vol_source(vol),"CORE.GT4",&n),*data=NULL;
 int pan=0,octave=0,ok=0;gt4_se_tables tables={0};
 if(!raw || n<6 || raw[0]!=1 || raw[1]!=1)goto done;
 unsigned size=u32(raw+2);if(size<0x12c || size>32u*1024*1024)goto done;
 data=malloc(size);if(!data || core_inflate_raw(raw+6,n-6,data,size)!=size)goto done;
 for(size_t at=0x12c;at<size;){
  if(size-at<8)goto done;unsigned base=u32(data+at),len=u32(data+at+4);at+=8;if(len>size-at)goto done;
  /* 0055D890 +0x208 / +0x468: tables from the user's executable. */
  if(base<=0x6c8ac0 && (uint64_t)base+len>=0x6c8bc0){memcpy(tables.pan,data+at+0x6c8ac0-base,256);pan=1;}
  if(base<=0x654738 && (uint64_t)base+len>=0x654a38){memcpy(tables.octave,data+at+0x654738-base,768);octave=1;}
  at+=len;
 }
 ok=pan && octave;if(ok)*out=tables;
done:free(raw);free(data);return ok;
}

/* Prepared service voices share the race's native SPU backend: the tone's
   ADPCM flags, ADSR, Gaussian interpolation and integer register pitch.
   No .65 gain, linear resampler or universal envelope in this path.
   A: isolated offline reverb cannot retain the shared IOP work area across
   independent effects; final PCM/driver-to-presentation phase unverified. */
int gt4_sound_render_service(const uint8_t *b,size_t n,unsigned seq,const gt4_se_tables *tables,
                             const uint8_t *libsd,size_t libsd_size,int16_t **pcm,size_t *frames){
 typedef struct {gt4_race_effect tone;gt4_sd_voice driver;gt4_spu_voice voice;gt4_se_ramp ramp;unsigned note,program;} voice;
 event events[EVENTS];size_t end=0;unsigned count=sequence_events(b,n,seq,events,&end),voices=0,next=0;
 voice *v=calloc(EVENTS,sizeof *v);int16_t *out=NULL;size_t used=0,dry_end=0;
 gt4_spu_reverb reverb={0};int16_t *area=NULL;
 *pcm=NULL;*frames=0;if(!count || !tables || !v)goto fail;
 /* D_006518C8 slots 06/07: mode 4, EVOL 0FFFh; same native preset
    decoder as race_audio.c. This is a routing observation, not PCM proof. */
 if(libsd && gt4_spu_reverb_preset(&reverb,libsd,libsd_size,4)){
  area=calloc(1u<<18,sizeof *area);if(!area)goto fail;reverb.buffer=area;reverb.size=1u<<18;reverb.evoll=reverb.evolr=0x0fff;
 }
 out=calloc((size_t)LIMIT*2,sizeof *out);if(!out)goto fail;
 for(size_t at=0;at<LIMIT;at+=800){
  while(next<count && events[next].at<=at){const event *e=&events[next++];
   if(!e->kind && e->velocity){
    if(voices==EVENTS)goto fail;voice *x=v+voices++;
    if(!gt4_race_tone_open(&x->tone,b,n,e->program,e->note))goto fail;
    x->note=e->note;x->program=e->program;
    /* 0055A8B0: sequence notes use group 2, not effect group 5.
       Se ctor 0055AD20 initializes left/right/master and pitch scale. */
    gt4_sd_voice_tone(&x->driver,&x->tone,0x3fff,0x3fff,4096,64,0);
    x->driver.group=2;x->driver.note=e->note*16;x->driver.v60=e->velocity;
    x->ramp=(gt4_se_ramp){64,0,0,1};x->voice.wave=&x->tone.sample.wave;
    x->voice.adsr1=x->tone.adsr1;x->voice.adsr2=x->tone.adsr2;
    /* 0055A8B0 +0xe0..0x134 sets ADSR and KON. */
    gt4_spu_key_on(&x->voice);
   }else for(unsigned i=0;i<voices;i++){voice *x=v+i;
    if(x->note!=e->note || x->program!=e->program || x->voice.phase==GT4_SPU_OFF)continue;
    if(e->kind)gt4_se_ramp_start(&x->ramp,(int)e->duration,x->driver.bend+e->delta,x->driver.bend,60);
    else gt4_spu_key_off(&x->voice);
   }
  }
  int32_t dry[1600]={0},wet[1600]={0};int alive=0;
  for(unsigned i=0;i<voices;i++){voice *x=v+i;
   if(x->voice.phase==GT4_SPU_OFF)continue;alive=1;
   gt4_se_ramp_step(&x->ramp,&x->driver.bend);x->driver.pitch=gt4_sd_pitch(&x->driver,tables->octave);
   x->voice.pitch=gt4_sd_pitch_register(&x->driver);int16_t volume[2];gt4_sd_volume(&x->driver,0x3fff,1,tables->pan,volume);
   x->voice.voll=volume[0];x->voice.volr=volume[1];x->voice.reverb=x->tone.flags&0x80;
   gt4_spu_voice_render(&x->voice,dry,wet,800);
  }
  for(unsigned i=0;i<1600;i++)if(dry[i])dry_end=at+i/2+1;
  if(area)gt4_spu_reverb_render(&reverb,wet,dry,800);
  for(unsigned i=0;i<1600;i++){int value=dry[i];out[at*2+i]=(int16_t)(value<-32768?-32768:value>32767?32767:value);if(value)used=at+i/2+1;}
  /* A: finite offline tail, rather than shared continuously running SPU.
     Cap at the existing one-second menu tail after the last dry sample. */
  if(next==count && !alive && at>=end && (!area || at>dry_end+48000))break;
  if(at+800==LIMIT && alive)goto fail;
 }
 if(!used)goto fail;int16_t *compact=realloc(out,used*4);if(compact)out=compact;
 *pcm=out;*frames=used;out=NULL;
 for(unsigned i=0;i<voices;i++)gt4_race_sample_clear(&v[i].tone.sample);
 free(v);free(area);return 1;
fail:for(unsigned i=0;i<voices;i++)gt4_race_sample_clear(&v[i].tone.sample);free(v);free(area);free(out);return 0;
}
