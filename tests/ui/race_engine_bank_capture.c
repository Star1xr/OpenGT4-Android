/* Compare the native ENGN loader with a running game's EE and SPU2 RAM.
   Local inputs only: disc.iso, eeMemory.bin and the savestate's SPU2.bin.
   003923D0 +0x48/+0x94 chooses the paths; 00559470 +0x30/+0x34
   publishes the converted layer table. No captured assets are embedded. */
#include "race_sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"engine bank line %d: %s\n",__LINE__,#x);exit(1); } } while(0)
int plat_audio_play(const int16_t *p,size_t n,int rate,int channels)
{ (void)p;(void)n;(void)rate;(void)channels;return 0; }
static unsigned u32(const unsigned char *p) { return p[0]|p[1]<<8|p[2]<<16|(unsigned)p[3]<<24; }
static unsigned u16(const unsigned char *p) { return p[0]|p[1]<<8; }
static unsigned char *read_file(const char *path,size_t *n)
{
    FILE *f=fopen(path,"rb");CHECK(f);CHECK(!fseek(f,0,SEEK_END));long length=ftell(f);CHECK(length>0);
    *n=(size_t)length;rewind(f);unsigned char *p=malloc(*n);CHECK(p);CHECK(fread(p,1,*n,f)==*n);fclose(f);return p;
}
int main(int argc,char **argv)
{
    CHECK(argc==4 || (argc==5 && !strcmp(argv[4],"--negative")));
    size_t en,sn;unsigned char *ee=read_file(argv[2],&en),*spu=read_file(argv[3],&sn);
    CHECK(en==0x2000000 && sn>=0x210004 && u32(spu)==0x01227521);
    /* PCSX2 SPU2Savestate::DataBlock: ID, 64 KiB registers, 2 MiB RAM. */
    const unsigned char *ram=spu+0x10004;unsigned manager=0;
    for(unsigned a=0x1000000;a<en;a+=4)
        if(u32(ee+a)==0x67a078 && u32(ee+a-0x80+0x6c)>0x100000 && u32(ee+a-0x80+0x6c)<en){manager=a;break;}
    CHECK(manager);unsigned race=u32(ee+manager-0x80+0x6c),list=u32(ee+race+0x60);
    CHECK(list<en-12);unsigned entry=u32(ee+u32(ee+list+8)),sound=entry+0x2900;CHECK(sound<en-0xb98);
    const char *error;gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol);
    unsigned layers=0,bytes=0;
    for(int k=0;k<3;k++) {
        unsigned object=u32(ee+sound+(k==0?0x28:k==1?0x2c:0x2cc));CHECK(object<en-0x38);
        unsigned count=u32(ee+object+0x34),rows=u32(ee+object+0x30);CHECK(count && count<256 && rows<en-count*16);
        const char *path=k==1?"/sound/roadnoiz.es":(const char *)(ee+sound+(k==0?0xaf8:0xb18));
        CHECK(memchr(path,0,128));size_t n;unsigned char *b=gt4_vol_read(vol,path,&n);CHECK(b);
        gt4_engine_bank *bank=gt4_engine_bank_decode(b,n,0);free(b);CHECK(bank && bank->count==count);
        for(unsigned i=0;i<count;i++) {
            const unsigned char *r=ee+rows+i*16;gt4_engine_layer *l=bank->layers+i;
            CHECK(l->rpm==(short)u16(r) && l->minimum==(short)u16(r+2) && l->maximum==(short)u16(r+4));
            CHECK(l->gain==(short)u16(r+6) && l->pitch==u32(r+8));
            unsigned address=u32(r+12);size_t size=l->sample.wave.blocks*16;CHECK(address<0x200000 && size<=0x200000-address);
            if(argc==5 && !k && !i)((unsigned char *)l->sample.wave.adpcm)[2]^=1;
            CHECK(!memcmp(l->sample.wave.adpcm,ram+address,size));layers++;bytes+=(unsigned)size;
        }
        gt4_engine_bank_close(bank);
    }
    gt4_vol_close(vol);free(ee);free(spu);
    printf("PASS original EE/SPU2 engine, road and exhaust: %u layer records, %u ADPCM bytes identical.\n",layers,bytes);
    return 0;
}
