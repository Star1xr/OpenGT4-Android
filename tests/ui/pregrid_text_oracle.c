/* Original pair kerning on the disc's own FT01 faces. Abbreviation is compared
   with the same metrics boundary on both sides; this is not a GS pixel test. */
#include "ee.h"
#include "font_layout.h"
#include "rtext.h"
#include "pcm_music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"text oracle line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static gt4_font face;static float scale;
static float measured(void *unused,const char *s){(void)unused;return gt4_font_original_width(&face,s,scale);}
static void width(ee_ctx *c){float value=measured(NULL,(const char *)c->ram+c->r[7].uw[0]);memcpy(c->ram+c->r[5].uw[0],&value,4);ee_w32(c,c->r[6].uw[0],0);}
static void length(ee_ctx *c){c->r[2].uw[0]=(uint32_t)strlen((const char *)c->ram+c->r[4].uw[0]);}
static void copy(ee_ctx *c){strcpy((char *)c->ram+c->r[4].uw[0],(const char *)c->ram+c->r[5].uw[0]);}
static void font_name_boundary(ee_ctx *c){c->r[2].uw[0]=0x659fb8;}
static ee_fn hook(uint32_t pc){return pc==0x5c2560 || pc==0x5c2630?font_name_boundary: pc==0x490b98?width:pc==0x57f260?length:pc==0x5a609c?copy:NULL;}
static ee_fn lookup(uint32_t pc){ee_fn fn=hook(pc);return fn?fn:recomp_lookup(pc);}
void pregrid_text_boundary(ee_ctx *c,uint32_t target,uint32_t pc)
{ee_fn fn=hook(target);if(fn)fn(c);else ee_unrecompiled(c,target,pc);}
static void run(ee_ctx *c,uint32_t pc){jmp_buf j;c->exit_jmp=&j;CHECK(!setjmp(j));CHECK(c->lookup(pc));c->lookup(pc)(c);}
static uint32_t u32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static void putf(ee_ctx *c,uint32_t at,float value){memcpy(c->ram+at,&value,4);}
static float getf(ee_ctx *c,uint32_t at){float value;memcpy(&value,c->ram+at,4);return value;}
int main(int argc,char **argv)
{
    CHECK(argc==3 || argc==4);ee_ctx c;CHECK(!ee_init(&c));c.lookup=lookup;c.hook=hook;
    CHECK(ee_load_image(&c,argv[1],0x100000)>0x5a0000);const char *err;gt4_vol *vol=gt4_vol_open(argv[2],&err);CHECK(vol);
    size_t bytes;uint8_t *data=gt4_vol_read(vol,"/font/system.fnt",&bytes);CHECK(data);
    const uint32_t bank=0x1600000,context=0x1700000,source=0x1710000,destination=0x1720000;
    memcpy(c.ram+bank,data,bytes);const char *names[]={"hm_16","hn_14","hb_24","FC_26"};unsigned pairs=0,abbreviations=0;
    int rounding=fegetround();fesetround(FE_TOWARDZERO);
    /* Constructor string allocation is a boundary; scalar style fields execute EE. */
    memset(c.r,0,sizeof c.r);c.r[4].uw[0]=context;c.r[29].uw[0]=0x1ef0000;
    run(&c,0x246f58);
    CHECK(fabsf(getf(&c,context)-212.f/255.f)<.000001f && ee_r32(&c,context+0x24)==4);
    CHECK(ee_r32(&c,context+0x3c)==0 && getf(&c,context+0x40)==0 && getf(&c,context+0x4c)==1);
    CHECK(!strcmp((char *)c.ram+0x6991a8,"hn_14"));
    for(unsigned f=0;f<4;f++){
        CHECK(gt4_font_decode(data,bytes,names[f],&face)==1);
        uint32_t faceoff=0;
        for(unsigned i=0;i<(unsigned)(data[16]|data[17]<<8);i++){
            uint32_t off=u32(data+u32(data+20)+i*4);
            if(!strcmp((const char *)data+u32(data+off),names[f]))faceoff=off;
        }
        CHECK(faceoff);uint32_t glyphs=u32(data+faceoff+4);
        ee_w32(&c,bank+faceoff+4,bank+glyphs);
        for(unsigned i=0;i<face.count;i++)for(unsigned side=0;side<2;side++)
            ee_w32(&c,bank+glyphs+i*20+12+side*4,bank+u32(data+glyphs+i*20+12+side*4));
        for(unsigned previous=face.first;previous<127 && previous<face.first+face.count;previous++)for(unsigned code=face.first;code<127;code++){
            memset(c.r,0,sizeof c.r);
            memset(c.ram+context,0,0xa4);scale=.875f;
            ee_w32(&c,context+0x14,bank+faceoff);putf(&c,context+0x24,scale);putf(&c,context+0x28,1);
            ee_w32(&c,context+0x6c,1);ee_w32(&c,context+0x4c,1);
            ee_w32(&c,context+0xa0,1); /* non-final glyph, retaining its cell advance */
            if(previous!=' ')ee_w32(&c,context+0x78,bank+glyphs+(previous-face.first)*20);
            putf(&c,context+0x30,100);c.r[4].uw[0]=code;c.r[5].uw[0]=0;c.r[6].uw[0]=0;c.r[7].uw[0]=context;c.r[29].uw[0]=0x1ef0000;
            run(&c,0x492978);float ink,step=gt4_font_original_step(&face,previous==' '?0:previous,code,scale,&ink);
            if(argc==4 && !pairs)step+=1;
            if(fabsf((100+step)-getf(&c,context+0x30))>=0.00001f)fprintf(stderr,"%s pair %u/%u native %.9g original %.9g\n",names[f],previous,code,100+step,getf(&c,context+0x30));
            CHECK(fabsf((100+step)-getf(&c,context+0x30))<0.00001f);pairs++;
        }
        const char *texts[]={"VW Karmann Ghia Coupe (Type-1) '68","Dodge RAM 1500 LARAMIE Hemi '04","Ford Ka '01","A","","Toyota CELICA 2000GT-FOUR (ST165) '86","Mercedes-Benz Patent Motor Wagen 1886", "Alfa Rom\xc3\xa9o 156 2.5 V6 24V '98"};
        for(unsigned t=0;t<sizeof texts/sizeof texts[0];t++){
            memset(c.ram+context,0,0xa4);scale=.875f;
            ee_w32(&c,context+0x14,bank+faceoff);putf(&c,context+0x24,scale);putf(&c,context+0x28,1);
            ee_w32(&c,context+0x6c,1);ee_w32(&c,context+0x4c,1);
            const char *s=texts[t];while(*s){
                unsigned code=gt4_text_next(&s);const char *peek=s;unsigned next=*peek?gt4_text_next(&peek):0;
                memset(c.r,0,sizeof c.r);ee_w32(&c,context+0xa0,next);
                c.r[4].uw[0]=code;c.r[7].uw[0]=context;c.r[29].uw[0]=0x1ef0000;run(&c,0x492978);
            }
            const gt4_glyph *space=gt4_font_glyph(&face,' ');
            unsigned space_width=space?space->advance:data[faceoff+12]>>1;
            float expected=getf(&c,context+0x30)-(space_width/2)*scale;
            CHECK(fabsf(expected-measured(NULL,texts[t]))<.00002f);
        }
        for(unsigned t=0;t<sizeof texts/sizeof texts[0];t++)for(unsigned w=1;w<=640;w++){
            memset(c.r,0,sizeof c.r);
            char out[0x104]={0};scale=.875f;strcpy((char *)c.ram+source,texts[t]);memset(c.ram+destination,0,0x104);
            c.r[4].uw[0]=context;c.r[5].uw[0]=destination;c.r[6].uw[0]=0x104;c.r[7].uw[0]=source;c.r[29].uw[0]=0x1ef0000;c.fpr[12].f=(float)w;
            run(&c,0x48f790);fesetround(FE_TONEAREST);int changed=gt4_text_abbreviate(texts[t],out,sizeof out,(float)w,measured,NULL);
            CHECK(fegetround()==FE_TONEAREST);fesetround(FE_TOWARDZERO);
            if(changed!=(int)c.r[2].uw[0])fprintf(stderr,"abbrev %s text %u width %u native %d original %u total %.9g\n",names[f],t,w,changed,c.r[2].uw[0],measured(NULL,texts[t]));
            CHECK(changed==(int)c.r[2].uw[0]);if(changed)CHECK(!strcmp(out,(const char *)c.ram+destination));abbreviations++;
        }
        gt4_font_free(&face);
    }
    uint32_t state=0;
    for(unsigned i=0;i<10000;i++){
        memset(c.r,0,sizeof c.r);
        uint32_t original=state;ee_w32(&c,context,original);c.r[4].uw[0]=context;c.r[29].uw[0]=0x1ef0000;
        run(&c,0x579330);CHECK(gt4_pcm_music_random(&state,0)==c.r[2].uw[0] && state==ee_r32(&c,context));
    }
    fesetround(rounding);free(data);gt4_vol_close(vol);ee_free(&c);
    printf("PASS %u original FT01 kerning steps, %u abbreviations, 10000 PCM RNG states.\n",pairs,abbreviations);return 0;
}
