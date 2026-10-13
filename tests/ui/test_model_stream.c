/* Distinguish the CAR4 VIF paint stream from the unsigned strip stream.
   0049D328, shape+0x10 descriptors, submits both unchanged to VIF1.
   No game bytes in this fixture: positions/rows/deltas are synthetic. */
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"stream line %d: %s\n",__LINE__,#x);return 1;} } while(0)
static void w32(unsigned char *p,unsigned v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void wf(unsigned char *p,float v){memcpy(p,&v,4);}
int main(int argc,char **argv)
{
    unsigned char d[512]={0};memcpy(d,"MDLS",4);w32(d+16,sizeof d);
    d[22]=d[24]=d[28]=d[30]=1;w32(d+56,128);w32(d+60,168);w32(d+152,180);w32(d+168,192);
    w32(d+68,172);w32(d+172,176);w32(d+176,400);memcpy(d+400,"Tex1",4);
    d[180]=4;d[181]=0;d[182]=0; /* CallShape 0; End */
    d[200]=1;d[202]=1;w32(d+208,32);d[212]=10;
    d[225]=0xc0;d[226]=3;d[227]=0x68;wf(d+240,1);wf(d+256,1);
    float bounds[3];CHECK(gt4_model_tire_bounds(d,sizeof d,bounds));
    CHECK(bounds[0]==0 && bounds[1]==1 && bounds[2]==1);
    CHECK(!gt4_model_tire_bounds(d,210,bounds));
    /* Signed UV V2-8 at 8040, STROW plus STMOD 0/1/2. */
    d[267]=0x30;w32(d+268,2048);w32(d+272,1024);
    d[289]=0x80;d[290]=3;d[291]=0x66;
    d[292]=16;d[293]=32;d[294]=240;d[295]=224;d[296]=8;d[297]=4;
    /* Unsigned C040 V2-8 is strip metadata, never a UV delta. */
    d[300]=0x40;d[301]=0xc0;d[302]=2;d[303]=0x66;d[306]=3;
    for(unsigned format=0;format<3;format++)for(unsigned mode=0;mode<3;mode++)for(unsigned start=0;start<2;start++) {
        memset(d+264,0,96);d[267]=0x30;w32(d+268,2048);w32(d+272,1024);
        d[284]=(unsigned char)mode;d[287]=5;d[288]=(unsigned char)(0x40+start);
        d[289]=0x80;d[290]=3;d[291]=(unsigned char)(0x64+format);
        unsigned size=format==0?4:format==1?2:1;
        static const int delta[6]={16,32,-16,-32,8,4};
        for(int i=0;i<6;i++)for(unsigned b=0;b<size;b++)d[292+i*size+b]=(unsigned char)((unsigned)delta[i]>>(b*8));
        unsigned reset=(292+6*size+3)&~3u;d[reset]=0x40;d[reset+1]=0xc0;d[reset+2]=2;d[reset+3]=0x66;d[reset+6]=3;
        gt4_model *m=gt4_model_decode(d,sizeof d,0);CHECK(m && m->count==1);
        int row[2]={2048,1024};
        for(int v=0;v<3;v++)for(int k=0;k<2;k++) {
            int value=delta[v*2+k];
            if(mode==2){row[k]+=value;value=row[k];}else if(mode==1)value+=row[k];
            float expected=(float)value/4096;
            if(argc>1 && !strcmp(argv[1],"--negative") && !v && !k)expected+=.25f;
            /* Offset 8041 targets vertex 1; no implicit overwrite of slot 0. */
            if(v+(int)start<3)CHECK(m->triangles[0].v[v+start].uv[k]==expected);
        }
        if(start)CHECK(m->triangles[0].v[0].uv[0]==0);
        gt4_model_free(m);
    }
    /* Rim callback's default skips its mesh, branch zero draws it.
       Independent command fixture, covering several texture-bank arguments. */
    d[180]=6;d[182]=0;d[183]=1;d[184]=5;d[185]=0;
    d[186]=8;d[187]=4;d[188]=0;d[189]=4;d[190]=d[191]=0;
    for(int id=1;id<=4;id++) {
        d[181]=(unsigned char)id;
        gt4_model *ordinary=gt4_model_decode_part(d,sizeof d,0,0);
        gt4_model *wheel=gt4_model_decode_wheel_part(d,sizeof d,0);
        CHECK(ordinary && ordinary->count==0);CHECK(wheel && wheel->count==1);
        d[184]=1;CHECK(!gt4_model_decode_wheel_part(d,sizeof d,0));d[184]=5;
        gt4_model_free(ordinary);gt4_model_free(wheel);
    }
    /* Authored external slot 2 must resolve to the callback's Tex1 bank,
       not the first bank or an untextured polygon. Synthetic one-pixel banks. */
    unsigned char rim[2048]={0};memcpy(rim,d,sizeof d);w32(rim+16,sizeof rim);
    rim[28]=5;w32(rim+172,800);w32(rim+152,512);rim[214]=1;
    const unsigned char commands[]={6,1,0,1,4,0,8,6,80,2,4,0,0,0};
    memcpy(rim+512,commands,sizeof commands);
    for(int id=0;id<5;id++) {
        unsigned at=1024+id*128;w32(rim+800+id*4,at);unsigned char *t=rim+at;
        memcpy(t,"Tex1",4);t[20]=t[22]=1;w32(t+24,48);w32(t+28,88);
        w32(t+48,0x4000);w32(t+52,4); /* PSMCT32, TBW=1, TCC */
        w32(t+88,100);t[94]=t[96]=t[98]=1;t[100]=(unsigned char)(32+id);t[103]=128;
    }
    for(int id=1;id<5;id++) {
        rim[513]=(unsigned char)id;gt4_model *wheel=gt4_model_decode_wheel_part(rim,sizeof rim,0);
        CHECK(wheel && wheel->count==1 && wheel->ntextures==5);
        CHECK(wheel->triangles[0].texture==id && wheel->textures[id].rgba[0]==32+id);
        CHECK(wheel->triangles[0].wheel_blur_texture==(id<4?id+2:0));
        /* Attaching a wheel shifts both bank indices; a barrel draw never
           gains a blur bank. Deliberately wrong expectation is rejected. */
        gt4_model merged={0};merged.ntextures=1;merged.textures=calloc(1,sizeof *merged.textures);
        float transform[1][6]={{0,0,0,1,1,1}};CHECK(gt4_model_attach(&merged,wheel,transform,1));
        CHECK(merged.triangles[0].texture==id+1);
        CHECK(merged.triangles[0].wheel_blur_texture==(id<4?id+3:0));
        tex1_free(merged.textures,merged.ntextures);free(merged.triangles);
        gt4_model_free(wheel);
    }
    rim[513]=5;CHECK(!gt4_model_decode_wheel_part(rim,sizeof rim,0));
    puts("model streams: signed UVs, unsigned strips, row modes, offsets and rim banks passed");return 0;
}
