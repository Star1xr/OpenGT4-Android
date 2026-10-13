/* Original 00453028 submission order against the native car loader.
   Resource queries and graphics/frame operations are fixture boundaries;
   the original conditional calls and their order execute unchanged. */
#include "ee.h"
#include "car_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"wheel order line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned submitted[2],count;
static void noop(ee_ctx *c){(void)c;}
static void tire(ee_ctx *c){c->r[2].uw[0]=0x1801000;}
static void rim(ee_ctx *c){c->r[2].uw[0]=0x1801100;}
static void submit(ee_ctx *c)
{CHECK(count<2);submitted[count++]=c->r[4].uw[0];}
static ee_fn boundary(uint32_t pc)
{
    switch(pc) {
    case 0x451078:return tire;
    case 0x451020:return rim;
    case 0x455598:return submit;
    case 0x4a53f8:case 0x105250:case 0x107ae8:case 0x453a28:
    case 0x4519a8:case 0x49b448:case 0x49b478:case 0x4a79b0:
    case 0x4a7844:case 0x4a1638:case 0x4a7900:case 0x4568f0:
    case 0x4ab040:case 0x4519d8:case 0x453a48:case 0x4a5400:
    case 0x107b00:case 0x105280:return noop;
    default:return NULL;
    }
}
static ee_fn lookup(uint32_t pc){ee_fn fn=boundary(pc);return fn?fn:recomp_lookup(pc);}
static int invoke(ee_ctx *c,uint32_t pc)
{jmp_buf j;c->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"%s\n",c->end_detail);return 0;}ee_fn fn=c->lookup(pc);if(!fn)return 0;fn(c);return 1;}
static unsigned u32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static void compare(const gt4_model *assembly,const gt4_model *source,int first,int textures,int mirror)
{
    CHECK(assembly && source && first+source->count<=assembly->count);
    for(int i=0;i<source->count;i++) {
        const gt4_triangle *a=&assembly->triangles[first+i],*b=&source->triangles[i];
        CHECK(a->kind==b->kind);
        CHECK(a->texture==(b->texture<0?b->texture:b->texture+textures));
        CHECK(a->wheel_blur_texture==(b->wheel_blur_texture?b->wheel_blur_texture+textures:0));
        /* Negative X scale reverses triangle winding, retaining authored UVs. */
        for(int v=0;v<3;v++){int at=mirror && v?3-v:v;CHECK(!memcmp(a->v[v].uv,b->v[at].uv,sizeof b->v[at].uv));}
    }
}
int main(int argc,char **argv)
{
    CHECK(argc==3 || (argc==4 && !strcmp(argv[3],"--negative")));
    ee_ctx c;CHECK(!ee_init(&c));c.lookup=lookup;c.hook=boundary;
    CHECK(ee_load_image(&c,argv[1],0x100000)>0x453240);
    ee_w32(&c,0x1800000,1);ee_w32(&c,0x1801000,1);ee_w32(&c,0x1801100,0);
    unsigned order[4][2];
    for(unsigned slot=0;slot<4;slot++) {
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1800000;c.r[5].uw[0]=slot;c.r[29].uw[0]=0x1ef0000;count=0;
        CHECK(invoke(&c,0x453028));CHECK(count==2);
        for(unsigned i=0;i<2;i++)order[slot][i]=submitted[i];
        CHECK(order[slot][0]==1 && order[slot][1]==0);
    }
    const char *err;gt4_vol *vol=gt4_vol_open(argv[2],&err);CHECK(vol);
    car_model_tables *tables=car_model_tables_open(vol);CHECK(tables);
    static const char *codes[]={"ka_01","s2000_a_99"};unsigned cases=0;
    for(unsigned car=0;car<2;car++)for(int mode=0;mode<3;mode++) {
        int cars=mode==0?0:mode==1?1:6;
        car_model *a=cars?car_model_load_race(vol,tables,codes[car],0,cars,0):car_model_load(vol,tables,codes[car],0);CHECK(a);
        char path[96];snprintf(path,sizeof path,"/car/%s/%s",cars?(a->race_open?"open":"lod"):"menu",a->model);
        size_t size;uint8_t *data=gt4_vol_read(vol,path,&size);CHECK(data && size>=52);
        for(unsigned slot=0;slot<4;slot++) {
            int first=0,textures=0;
            for(unsigned event=0;event<2;event++) {
                unsigned piece=order[slot][event];if(argc==4)piece=1-piece;
                unsigned off=u32(data+(piece?44+(slot/2)*4:32));CHECK(off<size);
                gt4_model *part=piece?gt4_model_decode_part(data+off,size-off,0,0):gt4_model_decode_wheel_part(data+off,size-off,0);CHECK(part);
                compare(a->wheel[slot],part,first,textures,!(slot&1));first+=part->count;textures+=part->ntextures;gt4_model_free(part);
            }
            CHECK(first==a->wheel[slot]->count && textures==a->wheel[slot]->ntextures);cases++;
        }
        free(data);car_model_free(a);
    }
    car_model_tables_close(tables);gt4_vol_close(vol);ee_free(&c);
    printf("PASS %u native wheel streams match original tire/rim submission order (showroom, race 1/6).\n",cases);return 0;
}
