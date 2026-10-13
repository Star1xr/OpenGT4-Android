/* Compare every horizontal HUD placement against the executable's table,
   not another copy of expected coordinates. */
#include "race_split_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"split layout line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    CHECK(argc>1);FILE *f=fopen(argv[1],"rb");CHECK(f);
    CHECK(!fseek(f,0x6a0de0-0x100000,SEEK_SET));
    for(int i=0;i<SPLIT_ELEMENTS;i++){
        int32_t r[9];CHECK(fread(r,sizeof r,1,f)==1);
        gt4_split_place p=gt4_split_layout[i];
        if(argc>2 && !strcmp(argv[2],"--negative") && i==SPLIT_TIMER)p.x--; /* must catch a displaced timer */
        CHECK(p.x==r[2] && p.y==r[3] && p.w==r[4] && p.h==r[5]);
        CHECK(p.ha==(r[6]&3) && p.va==r[7]);
    }
    uint32_t height_lui;CHECK(!fseek(f,0x3a23c0-0x100000,SEEK_SET));
    CHECK(fread(&height_lui,4,1,f)==1);
    uint32_t bits=(height_lui&0xffff)<<16;float height;memcpy(&height,&bits,4);
    CHECK(height==GT4_SPLIT_HEIGHT);
    const uint32_t address[2][5]={{0x32fd68,0x32fe08,0x32fd7c,0x32fd8c,0x32fd54},{0x32fd68,0x32fec4,0x32fd7c,0x32fd8c,0x32fd54}};
    for(int panel=0;panel<2;panel++)for(int k=0;k<5;k++){
        uint32_t instruction;CHECK(!fseek(f,address[panel][k]-0x100000,SEEK_SET));CHECK(fread(&instruction,4,1,f)==1);
        CHECK((instruction&0xffff0000u)==0x3c010000u);uint32_t bits=(instruction&0xffff)<<16;float original;memcpy(&original,&bits,4);
        float value=gt4_split_mirror[panel][k];if(argc>2 && !strcmp(argv[2],"--mirror-negative") && k==1)value++;
        CHECK(value==original);
    }
    fclose(f);
    puts("PASS 13 Battle2P HUD elements and lower-panel translation from original executable");return 0;
}
