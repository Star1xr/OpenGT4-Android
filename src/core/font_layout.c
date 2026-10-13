#include "font_layout.h"
#include "rtext.h"
#include <string.h>
#include <math.h>
#include <fenv.h>
static int saturated(int n){return n<-128?-128:n>127?127:n;}
float gt4_font_original_step(const gt4_font *f,unsigned previous,unsigned code,
                             float scale,float *ink)
{
    /* 00492EF0 -> 00492748: a KNJI cell, one more pixel after a glyph. */
    if(code>=GT4_KANJI_FIRST && f->kanji_advance){
        int after=previous!=0;*ink=(float)(f->kanji_x+after)*scale;
        return (float)(f->kanji_advance+after)*scale;
    }
    const gt4_glyph *g=gt4_font_glyph(f,code),*p=gt4_font_glyph(f,previous);
    /* 00492978 +0xAC..+0x130: missing characters follow the space path. */
    if(!g){*ink=0;return f->space_advance*scale;}
    float correction=0;
    /* 00492978 +0x2C..+0x98: space breaks pair kerning. */
    if(code==' '){*ink=0;return g->advance*scale;}
    if(p && f->contours && !(code>='0' && code<='9') && code!=127){
        unsigned current=(unsigned)(g-f->glyphs),last=(unsigned)(p-f->glyphs);
        const int8_t *left=f->contours+current*f->contour_height*2;
        const int8_t *right=f->contours+(last*2+1)*f->contour_height;
        int minimum=128;
        /* 00492978 +0x210..+0x344: signed saturated byte sums, minimum
           across the face's rows, then AND 127 and pair_gap - minimum. */
        for(unsigned i=0;i<f->contour_height;i++){
            int sum=saturated(left[i]+right[i]);if(sum<minimum)minimum=sum;
        }
        correction=(float)f->pair_gap-(float)(minimum&127);
    }
    /* 00492978 +0x38C: the bearing enters the advance only after a glyph. */
    if(p)correction+=g->x;
    *ink=correction*scale;
    return ((float)g->advance+correction)*scale;
}
float gt4_font_original_width(const gt4_font *f,const char *text,float scale)
{
    unsigned previous=0,last=0;float pen=0;
    while(*text && *text!='\n' && *text!='\r'){
        unsigned code=gt4_text_next(&text);if(code<32)continue;
        float ink;pen+=gt4_font_original_step(f,previous,code,scale,&ink);
        previous=code==' '?0:code;
        last=code;
    }
    /* 00492978 +0x478: the final glyph uses bitmap width rather than its
       cell advance (context +0xA0 is the following character, zero at end). */
    if(last && last!=' ' && !(last>=GT4_KANJI_FIRST && f->kanji_advance)){const gt4_glyph *g=gt4_font_glyph(f,last);if(g)pen+=(g->bitmap_width-g->advance)*scale;}
    /* 00491418 +0x1A0..+0x1CC: subtract half the space cell. */
    return pen-(f->space_advance/2)*scale;
}
static int abbreviate_original(const char *text,char *out,size_t capacity,float width,
                        gt4_text_measure_fn measure,void *user)
{
    size_t bytes=strlen(text);if(bytes+3>=capacity || !isfinite(width))return 0;
    float total=measure(user,text);if(total<width || total<=0)return 0;
    int count=0;const char *s=text;while(*s){gt4_text_next(&s);count++;}
    int limit=(int)((float)count*(width/total));
    /* 0048F790 +0xB0..+0xEC: decrease the estimated character count until
       the candidate including its three dots is strictly narrower. */
    for(;limit>=0;limit--){
        s=text;size_t copied=0;int characters=0;
        while(characters<limit && *s){
            const char *begin=s;gt4_text_next(&s);size_t n=(size_t)(s-begin);
            memcpy(out+copied,begin,n);copied+=n;characters++;
            /* Preserve 0048F708 +0x58..+0x70's character-copy condition. */
            if((int)n+1>=limit)break;
        }
        memcpy(out+copied,"...",4);
        if(measure(user,out)<width)return 1;
    }
    return 1;
}

int gt4_text_abbreviate(const char *text,char *out,size_t capacity,float width,
                        gt4_text_measure_fn measure,void *user)
{
    /* 0048F790 arithmetic runs under EE round-toward-zero. Restore the PC
       caller's mode after the original estimate/candidate comparisons. */
    int rounding=fegetround();fesetround(FE_TOWARDZERO);
    int changed=abbreviate_original(text,out,capacity,width,measure,user);
    fesetround(rounding);return changed;
}
