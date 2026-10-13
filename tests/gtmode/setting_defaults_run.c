/* Original installation setters with actual-length database rows. The row
   lookup is the boundary; complete setup/body/buffer mutations are compared. */
#include "ee.h"
#include "career_original.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
static uint8_t row[112];static unsigned row_size;
static int fetch(void *ctx,uint64_t key,uint8_t *out,size_t cap){
    (void)ctx;(void)key;if(row_size>cap)return -1;memcpy(out,row,row_size);return (int)row_size;
}
static void read_row(ee_ctx *c){fetch(NULL,c->r[5].ud[0],c->ram+c->r[6].uw[0],0x142);}
static ee_fn boundary(uint32_t pc){return pc==0x443ed0?read_row:NULL;}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static unsigned next(uint32_t *s){return *s=*s*1664525u+1013904223u;}
int main(int argc,char **argv){
    if(argc!=2)return 2;ee_ctx c;if(ee_init(&c) || ee_load_image(&c,argv[1],0x100000)<=0x500000)return 2;
    c.lookup=lookup;c.hook=boundary;uint32_t seed=0x10082026;
    const unsigned types[]={2,3,4,5,10,11,14,24,27};
    const unsigned funcs[]={0x440268,0x4404b8,0x440820,0x4407b0,0x4403c0,0x4402f0,0x440430,0x4406c8,0x4408d8};
    const unsigned sizes[]={12,112,26,10,22,45,20,28,8};
    const unsigned short_sizes[]={11,110,25,10,21,41,16,27,6};
    const uint32_t live=0x1800000,buf=0x1801000,stack=0x1ef0000;
    unsigned cases=0;
    for(unsigned group=0;group<9;group++)for(unsigned trial=0;trial<1024;trial++){
        uint8_t body[0x4b8],buffer[0x142];uint32_t slot=trial%3;
        for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);
        for(unsigned j=0;j<sizeof buffer;j++)buffer[j]=(uint8_t)(next(&seed)>>24);
        for(unsigned j=0;j<sizeof row;j++)row[j]=(uint8_t)(next(&seed)>>24);
        row_size=trial%3==0?0:trial%3==1?sizes[group]:short_sizes[group];
        memcpy(body+0x488,&slot,4);memcpy(c.ram+live+8,body,sizeof body);memcpy(c.ram+buf,buffer,sizeof buffer);
        uint64_t key=((uint64_t)types[group]<<32)|next(&seed);
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=live;c.r[5].ud[0]=key;c.r[6].uw[0]=buf;c.r[29].uw[0]=stack;
        jmp_buf jump;c.exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"setter %x stopped: %s\n",funcs[group],c.end_detail);return 1;}recomp_lookup(funcs[group])(&c);
        if(!gt4_career_install_defaults(body,key,buffer,fetch,NULL))return 1;
        if(getenv("GT4_SETTING_DEFAULT_ROW_NEGATIVE") && group==0 && row_size==11)buffer[11]^=1;
        if(memcmp(body,c.ram+live+8,sizeof body) || memcmp(buffer,c.ram+buf,sizeof buffer)){
            for(unsigned j=0;j<sizeof body;j++)if(body[j]!=c.ram[live+8+j])fprintf(stderr,"body +%x native=%x original=%x\n",j,body[j],c.ram[live+8+j]);
            for(unsigned j=0;j<sizeof buffer;j++)if(buffer[j]!=c.ram[buf+j])fprintf(stderr,"buffer +%x native=%x original=%x\n",j,buffer[j],c.ram[buf+j]);
            fprintf(stderr,"setter type=%u trial=%u row=%u differs\n",types[group],trial,row_size);return 1;
        }cases++;
    }
    printf("PASS %u original default installations, nine categories, three sheets, full/short/missing rows and preserved buffer padding.\n",cases);
    ee_free(&c);return 0;
}
