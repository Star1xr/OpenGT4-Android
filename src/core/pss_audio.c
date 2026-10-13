#include "pss_audio.h"
#include <stdlib.h>
#include <string.h>

static uint32_t le32(const uint8_t *p)
{return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}

int gt4_pss_audio(const uint8_t *data,size_t size,unsigned track,int16_t **pcm,size_t *frames)
{
    *pcm=NULL;*frames=0;
    if(!data || track<1 || track>255)return 0;
    uint8_t *stream=NULL;size_t used=0,capacity=0;
    int16_t *out=NULL;
    /* MPEG-2 PS packets; the Sony private payload is ff a1 00 <track>,
       followed by fragments of a single SShd/SSbd stream. */
    for(size_t p=0;p+4<=size;) {
        if(data[p] || data[p+1] || data[p+2]!=1) {p++;continue;}
        unsigned id=data[p+3];
        if(id==0xb9)break;
        if(id==0xba) {
            if(size-p<14 || (data[p+4]&0xc0)!=0x40)goto fail;
            size_t n=14+(data[p+13]&7);if(n>size-p)goto fail;p+=n;continue;
        }
        if(size-p<6)goto fail;
        size_t end=p+6+((unsigned)data[p+4]<<8|data[p+5]);
        if(end>size)goto fail;
        if(id==0xbd) {
            if(end-p<9 || (data[p+6]&0xc0)!=0x80)goto fail;
            size_t at=p+9+data[p+8];if(at>end || end-at<4)goto fail;
            if(data[at]==0xff && data[at+1]==0xa1 && !data[at+2] && data[at+3]==track) {
                at+=4;size_t n=end-at;
                if(n>64u*1024*1024-used)goto fail;
                if(used+n>capacity) {
                    size_t cap=(used+n)*2;if(cap>64u*1024*1024)cap=64u*1024*1024;
                    uint8_t *grown=realloc(stream,cap);if(!grown)goto fail;
                    stream=grown;capacity=cap;
                }
                memcpy(stream+used,data+at,n);used+=n;
            }
        }
        p=end;
    }
    if(used<40 || memcmp(stream,"SShd",4) || le32(stream+4)!=24 ||
       le32(stream+8)!=0x10 || le32(stream+12)!=48000 || le32(stream+16)!=2 ||
       memcmp(stream+32,"SSbd",4))goto fail;
    size_t interleave=le32(stream+20),bytes=le32(stream+36);
    if(!interleave || interleave%16 || interleave>65536 || !bytes ||
       bytes>used-40 || bytes%(interleave*2))goto fail;
    size_t count=bytes/32*28;
    if(count>48000u*600)goto fail;
    out=malloc(count*2*sizeof *out);if(!out)goto fail;
    static const int co[5][2]={{0,0},{60,0},{115,-52},{98,-55},{122,-60}};
    int hist[2][2]={{0}};size_t frame=0;
    for(size_t group=40;group<40+bytes;group+=interleave*2) {
        for(int ch=0;ch<2;ch++) {
            size_t sample=frame;
            for(size_t off=0;off<interleave;off+=16) {
                const uint8_t *b=stream+group+ch*interleave+off;
                int shift=b[0]&15,filter=b[0]>>4;
                if(shift>12 || filter>4 || b[1]>7)goto fail;
                for(int j=0;j<28;j++) {
                    int v=(b[2+j/2]>>((j&1)*4))&15;if(v>=8)v-=16;
                    v=(v*4096>>shift)+((hist[ch][0]*co[filter][0]+hist[ch][1]*co[filter][1]+32)>>6);
                    if(v>32767)v=32767;if(v<-32768)v=-32768;
                    /* The interleaving and predictor history continue across
                       PES and channel-block boundaries; loop flags don't loop. */
                    out[sample++*2+ch]=(int16_t)v;
                    hist[ch][1]=hist[ch][0];hist[ch][0]=v;
                }
            }
        }
        frame+=interleave/16*28;
    }
    free(stream);*pcm=out;*frames=count;return 1;
fail:
    free(stream);free(out);return 0;
}
