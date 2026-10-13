/* Original input decompressor versus native reader, every byte per frame.
   CRC verifies the complete original payload independently of command code. */
#include "disc_replay.h"
#include "ee.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"disc replay line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static ee_ctx e;
static void call(uint32_t f,uint32_t a,uint32_t b){e.r[4].uw[0]=a;e.r[5].uw[0]=b;e.r[29].uw[0]=0x1ff0000;ee_fn fn=recomp_lookup(f);if(!fn)abort();fn(&e);}
static int verify(gt4_disc_replay *d,const char *path,unsigned *frames,int negative)
{
    gt4_replay_file r;char error[256];int ok=gt4_disc_replay_read(d,path,&r,error,sizeof error);
    if(!ok){fprintf(stderr,"%s: %s\n",path,error);return 1;}
    uint32_t object=0x1800000,blob=0x1801000,output=0x1800100;
    if(negative)r.tape[4]^=1;
    unsigned segments=r.disc_segments?r.disc_segments:1;
    for(unsigned segment=0;segment<segments;segment++){
    size_t offset=r.disc_segments?r.disc_segment_offset[segment]+0x14f4:0x1aa8;
    size_t bytes=*(uint32_t *)(r.disc_info+offset+4);
    memset(e.ram+object,0,0x100);memset(e.ram+blob,0,0x10000);memcpy(e.ram+blob,r.disc_info+offset,bytes);
    *(uint32_t *)(e.ram+object)=blob+8;*(uint32_t *)(e.ram+object+0x48)=blob;call(0x345be0,object,0);
    for(size_t i=0;i<r.disc_segment_steps[segment];i++){
        call(0x345d70,object,output);
        if(memcmp(e.ram+output,r.tape+(r.disc_segment_at[segment]+i)*32,32)){fprintf(stderr,"%s: command differs at segment %u frame %zu\n",path,segment,i);gt4_replay_free(&r);return 1;}
    }
    }
    printf("%s %s / %s cars=%d steps=%zu CRC and commands pass\n",path,r.course,r.car,r.cars,r.steps);
    *frames+=(unsigned)r.steps;gt4_replay_free(&r);return 0;
}
static int verify_icon(gt4_vol *vol,const char *path,int negative)
{
    char name[96];const char *slash=strrchr(path,'/');snprintf(name,sizeof name,"%.*s/icon0.ico",(int)(slash-path),path);
    size_t n=0;uint8_t *raw=gt4_vol_read(vol,name,&n);CHECK(raw && n<0x10000);
    uint16_t native[4096];CHECK(gt4_disc_replay_icon(raw,n,native));
    CHECK(!gt4_disc_replay_icon(raw,20,native));
    uint8_t saved=raw[0];raw[0]^=1;CHECK(!gt4_disc_replay_icon(raw,n,native));raw[0]=saved;
    uint32_t object=0x1800000,blob=0x1801000,output=0x1820000;
    memset(e.ram+object,0,32);memcpy(e.ram+blob,raw,n);*(uint32_t *)(e.ram+object)=blob;
    call(0x1d3b20,object,0);CHECK(e.r[2].uw[0]);*(uint32_t *)(e.ram+object+4)=e.r[2].uw[0];
    call(0x1d3d70,object,output);if(negative)native[0]^=1;
    CHECK(!memcmp(native,e.ram+output,sizeof native));free(raw);
    printf("PASS %s: all 4096 RGB555 texels match original 001D3B20/001D3D70; bad header/truncation rejected.\n",name);return 0;
}

int main(int argc,char **argv)
{
    CHECK(argc>=3 && argc<=5);int negative=0,licenses=0,icons=0,icon_negative=0;const char *why=NULL;
    for(int i=3;i<argc;i++){
        if(!strcmp(argv[i],"--negative"))negative=1;
        else if(!strcmp(argv[i],"--licenses"))licenses=1;
        else if(!strcmp(argv[i],"--icons"))icons=1;
        else if(!strcmp(argv[i],"--icon-negative"))icons=icon_negative=1;
        else CHECK(0);
    }
    ee_init(&e);CHECK(ee_load_image(&e,argv[1],0x100000));e.lookup=recomp_lookup;
    gt4_vol *vol=gt4_vol_open(argv[2],&why);CHECK(vol);gt4_disc_replay *d=gt4_disc_replay_open(vol);CHECK(d);
    unsigned frames=0,files=0;static const char *series[]={"l0b","l0a","lib","lia","l0s"};
    if(!icons)for(unsigned s=0;s<5;s++)for(unsigned i=1;i<=16;i++){
        char path[64];snprintf(path,sizeof path,"/replay/license/us%s%04u",series[s],i);
        CHECK(!verify(d,path,&frames,negative));files++;
    }
    /* US config-main.ad L146..160, independently checked authored catalog. */
    char paths[128][64];int count=gt4_disc_replay_list(vol,paths,128,0);CHECK(count==13);
    for(int i=0;i<count;i++){char expected[64];snprintf(expected,sizeof expected,"/replay/Rdemo%d/Rdemo%d",i,i);CHECK(!strcmp(paths[i],expected));}
    if(icons){for(int i=0;i<count;i++)CHECK(!verify_icon(vol,paths[i],icon_negative));gt4_disc_replay_close(d);gt4_vol_close(vol);ee_free(&e);return 0;}
    if(!licenses)for(int i=0;i<count;i++){CHECK(!verify(d,paths[i],&frames,0));files++;}
    printf("PASS %u original files, %u complete 32-byte commands; deliberate --negative changes steering.\n",files,frames);
    gt4_disc_replay_close(d);gt4_vol_close(vol);ee_free(&e);return 0;
}
