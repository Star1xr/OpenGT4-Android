#include "pcm_music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"PCM check line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(int argc,char **argv)
{
    CHECK(argc==3 || argc==4);const char *err;gt4_vol *vol=gt4_vol_open(argv[1],&err);CHECK(vol);
    CHECK(gt4_pcm_music_count(vol,"default_quick")==2 && !gt4_pcm_music_count(vol,"invalid"));
    for(unsigned sequence=0;sequence<2;sequence++){
        gt4_pcm_music *m=gt4_pcm_music_open(vol,"default_quick",sequence);CHECK(m);
        CHECK(gt4_pcm_music_rate(m)==44100 && gt4_pcm_music_frames(m)>44100*60);
        char path[1024];snprintf(path,sizeof path,"%s-%u.s16",argv[2],sequence);
        FILE *f=fopen(path,"wb");CHECK(f);
        size_t frames=gt4_pcm_music_frames(m);CHECK(fwrite(gt4_pcm_music_samples(m),4,frames,f)==frames);fclose(f);
        int16_t output[4096],again[4096];gt4_pcm_music_render(output,2048,m);
        gt4_pcm_music_rewind(m);gt4_pcm_music_render(again,2048,m);CHECK(!memcmp(output,again,sizeof output));
        uint64_t energy=0;for(int i=0;i<44100/2048*10;i++){
            gt4_pcm_music_render(output,2048,m);for(unsigned j=0;j<4096;j++)energy+=(uint64_t)abs(output[j]);
        }
        CHECK(energy>0);printf("PCM %u: %zu frames, stereo 44100 Hz; nonzero 48000 Hz callback.\n",sequence,frames);
        gt4_pcm_music_close(m);
    }
    /* Full-scale alternating source samples expose 32-bit interpolation overflow.
       44100/48000 between 28672 and -32768 must give -27776. */
    uint8_t extreme[72]={0};memcpy(extreme,"SShd",4);extreme[4]=24;extreme[8]=16;
    extreme[12]=0x44;extreme[13]=0xac;extreme[16]=2;extreme[20]=16;
    memcpy(extreme+32,"SSbd",4);extreme[36]=32;
    for(unsigned at=40;at<72;at+=16)for(unsigned i=2;i<16;i++)extreme[at+i]=0x87;
    gt4_pcm_music *edge=gt4_pcm_music_decode(extreme,sizeof extreme);CHECK(edge);
    int16_t interpolated[4];gt4_pcm_music_render(interpolated,2,edge);
    CHECK(interpolated[0]==28672 && interpolated[2]==-27776);gt4_pcm_music_close(edge);
    uint8_t malformed[40]={0};CHECK(!gt4_pcm_music_decode(malformed,sizeof malformed));
    if(argc==4){CHECK(gt4_pcm_music_count(vol,"default_quick")==3);}
    gt4_vol_close(vol);return 0;
}
