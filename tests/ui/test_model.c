#include "model.h"
#include "ui_motion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void w32(uint8_t *p,unsigned v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void wf(uint8_t *p,float v){memcpy(p,&v,4);}
int main(void)
{
    uint8_t d[512]={0};memcpy(d,"MDLS",4);w32(d+16,sizeof d);d[22]=d[24]=d[28]=d[30]=1;
    w32(d+56,128);w32(d+60,168);w32(d+68,172);
    w32(d+128+24,180);w32(d+168,192);w32(d+172,176);w32(d+176,400);
    d[180]=43;d[182]=4;memcpy(d+400,"Tex1",4); /* absent texture is allowed */
    d[200]=1;d[202]=1;w32(d+208,32);d[212]=5;
    d[225]=0xc0;d[226]=3;d[227]=0x68;wf(d+240,1);wf(d+256,1);
    d[264]=0x40;d[265]=0xc0;d[266]=3;d[267]=0x64;
    d[292]=0x40;d[293]=0xc0;d[294]=2;d[295]=0x72;d[297]=3;
    gt4_model *m=gt4_model_decode(d,sizeof d,0);CHECK(m && m->count==1);
    CHECK(m->triangles[0].v[1].p[0]==1 && m->triangles[0].v[2].p[1]==1);gt4_model_free(m);
    for(size_t n=0;n<sizeof d;n++)CHECK(!gt4_model_decode(d,n,0));
    /* A strip restart longer than the packet stops at its last vertex, as in
       PDTools (course shapes rely on this). */
    d[297]=255;m=gt4_model_decode(d,sizeof d,0);CHECK(m && m->count==1);gt4_model_free(m);d[297]=3;
    w32(d+208,0xfffffff0);CHECK(!gt4_model_decode(d,sizeof d,0));w32(d+208,32);
    /* Authored smooth normals/materials survive conversion and mirrored
       attachments use the inverse transpose, not position transforms. */
    d[26]=1;w32(d+64,352);w32(d+176,464);memcpy(d+464,"Tex1",4);
    d[212]=8;d[215]=2;
    memset(d+292,0,60);d[292]=0x80;d[293]=0xc0;d[294]=3;d[295]=0x68;
    for(int i=0;i<3;i++){wf(d+296+i*12,1);wf(d+300+i*12,0);wf(d+304+i*12,0);}
    d[332]=0x40;d[333]=0xc0;d[334]=2;d[335]=0x72;d[337]=3;
    for(int i=0;i<4;i++){wf(d+352+i*4,1);wf(d+368+i*4,1);}
    wf(d+384,.5f);wf(d+416,32);
    m=gt4_model_decode(d,sizeof d,0);CHECK(m && m->count==1);
    CHECK(m->triangles[0].v[0].normal[0]==1 && m->triangles[0].v[0].specular[0]==.5f);
    CHECK(m->triangles[0].v[0].shine==32);gt4_model *combined=calloc(1,sizeof *combined);
    const float transform[1][6]={{3,4,5,-2,1,1}};
    CHECK(gt4_model_attach(combined,m,transform,1));
    CHECK(combined->triangles[0].v[0].normal[0]==-.5f);
    CHECK(combined->triangles[0].v[0].p[0]==3);gt4_model_free(combined);gt4_model_free(m);
    CHECK(!gt4_model_decode_part(d,sizeof d,0,1));
    wf(d+296,NAN);CHECK(!gt4_model_decode(d,sizeof d,0));wf(d+296,1);
    /* Repeated draws must stay ordered: one mesh can establish a mask,
       then be submitted again with destination-alpha blending and no Z write. */
    uint8_t ordered[768]={0};memcpy(ordered,d,sizeof d);w32(ordered+16,sizeof ordered);
    w32(ordered+128+24,512);
    const uint8_t sequence[]={43,0,36,255,255,255,0,4,0,37,32,0x58,64,29,31,2,36,0,0,0,255,4,0,0};
    memcpy(ordered+512,sequence,sizeof sequence);
    m=gt4_model_decode(ordered,sizeof ordered,0);CHECK(m && m->count==2);
    CHECK(m->triangles[0].state.color_mask==0x00ffffff && m->triangles[0].state.depth_write);
    CHECK(m->triangles[1].state.color_mask==0xff000000 && !m->triangles[1].state.depth_write);
    CHECK(m->triangles[1].state.blend==0x58 && m->triangles[1].state.fixed_alpha==64);
    CHECK(m->triangles[1].state.destination_test && !m->triangles[1].state.destination_bit);
    combined=calloc(1,sizeof *combined);CHECK(gt4_model_attach(combined,m,transform,1));
    CHECK(combined->triangles[1].state.blend==0x58);gt4_model_free(combined);gt4_model_free(m);
    /* Matrix commands (OpenGL order): push, translate(2,3,0), scale(-1,1,1),
       draw, pop, draw. The mirror reverses the winding; the pop restores the
       base matrix; VM colour scale (84) carries four u16 registers. */
    {
        uint8_t mat[768]={0};memcpy(mat,d,sizeof d);w32(mat+16,sizeof mat);w32(mat+128+24,512);
        uint8_t *c=mat+512;*c++=43;*c++=0;*c++=12;
        *c++=17;wf(c,2);wf(c+4,3);wf(c+8,0);c+=12;
        *c++=18;wf(c,-1);wf(c+4,1);wf(c+8,1);c+=12;
        *c++=4;*c++=0;*c++=13;
        *c++=84;for(int i=0;i<8;i++)*c++=(uint8_t)(i&1?0:i+1);
        *c++=4;*c++=0;*c=0;
        m=gt4_model_decode(mat,sizeof mat,0);CHECK(m && m->count==2);
        const gt4_triangle *t=&m->triangles[0];
        CHECK(t->v[0].p[0]==2 && t->v[0].p[1]==3);
        CHECK(t->v[1].p[0]==2 && t->v[1].p[1]==4 && t->v[2].p[0]==1 && t->v[2].p[1]==3);
        CHECK(t->v[0].normal[0]==-1);
        CHECK(m->triangles[1].v[1].p[0]==1 && m->triangles[1].v[1].p[1]==0);
        gt4_model_free(m);
        /* rotateZ(90): positions and normals turn the same way. */
        c=mat+514;*c++=22;wf(c,90);c+=4;*c++=4;*c++=0;*c=0;
        m=gt4_model_decode(mat,sizeof mat,0);CHECK(m && m->count==1);t=&m->triangles[0];
        CHECK(fabsf(t->v[1].p[0])<1e-6f && fabsf(t->v[1].p[1]-1)<1e-6f);
        CHECK(fabsf(t->v[0].normal[0])<1e-6f && fabsf(t->v[0].normal[1]-1)<1e-6f);
        gt4_model_free(m);
    }
    w32(ordered+128+24,767);ordered[767]=36;CHECK(!gt4_model_decode(ordered,sizeof ordered,0));
    /* PRIM's per-packet ABE flag distinguishes opaque paint from reflections. */
    uint8_t tagged[768]={0};memcpy(tagged,d,sizeof d);w32(tagged+16,sizeof tagged);
    w32(tagged+208,320);tagged[212]=9;
    tagged[512]=0xc0;tagged[513]=0xc0;tagged[514]=1;tagged[515]=0x68;
    w32(tagged+520,0x301e4000);memcpy(tagged+528,d+224,128);
    m=gt4_model_decode(tagged,sizeof tagged,0);CHECK(m && m->count==1 && !m->triangles[0].blend_enabled);gt4_model_free(m);
    w32(tagged+520,0x303e4000);
    m=gt4_model_decode(tagged,sizeof tagged,0);CHECK(m && m->triangles[0].blend_enabled);
    combined=calloc(1,sizeof *combined);CHECK(gt4_model_attach(combined,m,transform,1));
    CHECK(combined->triangles[0].blend_enabled);
    /* Reflection changes winding, not which side of the wheel faces out. */
    CHECK(combined->triangles[0].v[1].p[1]==5 && combined->triangles[0].v[2].p[0]==1);
    gt4_model_free(combined);gt4_model_free(m);
    /* Daytime and released-brake branch 0 are not the fall-through defaults.
       This fixture nests the brake callback inside the daytime callback. */
    uint8_t lamps[832]={0};memcpy(lamps,"CAR4",4);w32(lamps+24,64);
    memcpy(lamps+64,d,sizeof d);w32(lamps+64+16,768);w32(lamps+64+128+24,512);
    const uint8_t lamp_commands[]={6,7,0,2,6,0,24,0,8,27,
        6,0,0,2,6,0,8,0,8,9,4,0,8,5,4,0,4,0,
        8,7,4,0,4,0,4,0,0};
    memcpy(lamps+64+512,lamp_commands,sizeof lamp_commands);
    /* both brake branches are kept: the released one tagged 1, the lit one 2 */
    m=gt4_model_decode(lamps,sizeof lamps,0);CHECK(m && m->count==3 && m->triangles[0].lamp==1 && m->triangles[1].lamp==2 && m->triangles[2].lamp==2);gt4_model_free(m);
    /* The same callback number in a generic MDLS has different semantics. */
    m=gt4_model_decode_part(lamps+64,768,0,0);CHECK(m && m->count==0);gt4_model_free(m);
    lamps[64+516]=255;lamps[64+517]=255;CHECK(!gt4_model_decode(lamps,sizeof lamps,0));
    lamps[64+516]=1;lamps[64+517]=0;CHECK(!gt4_model_decode(lamps,sizeof lamps,0));
    uint8_t car[700]={0};memcpy(car,"CAR4",4);w32(car+24,64);w32(car+28,600);memcpy(car+64,d,sizeof d);
    memcpy(car+600,"Pat0",4);car[616]=car[618]=1;w32(car+632,36);w32(car+636,240);w32(car+640,4);wf(car+644,.5f);
    m=gt4_model_decode(car,sizeof car,0);CHECK(m && m->count==1 && m->triangles[0].v[1].p[0]==.5f);gt4_model_free(m);
    /* Early shape indices are not necessarily brakes: MP5 uses them for bodywork. */
    w32(car+32,600);m=gt4_model_decode(car,sizeof car,0);CHECK(m && m->count==1);gt4_model_free(m);
    w32(car+636,510);CHECK(!gt4_model_decode(car,sizeof car,0));
    /* Historic Showroom's displaced ground pivot must stay fixed through
       every heading, without moving the authored rest placement. */
    {
        float pivot[3]={-.013428f,0,.497999f},offset[3];
        const float rest[9]={1,0,0,0,1,0,0,0,1};
        ui_model_pivot_offset(rest,rest,pivot,offset);
        CHECK(offset[0]==0 && offset[1]==0 && offset[2]==0);
        for(int deg=0;deg<=360;deg++){
            float a=deg*.01745329252f,c=cosf(a),sn=sinf(a);
            float rotation[9]={c,0,sn,0,1,0,-sn,0,c};
            ui_model_pivot_offset(rotation,rest,pivot,offset);
            for(int i=0;i<3;i++){
                float center=offset[i];for(int k=0;k<3;k++)center+=rotation[i*3+k]*pivot[k];
                CHECK(fabsf(center-pivot[i])<1e-6f);
            }
        }
    }
    puts("native model: strips, coordinates, color patches and malformed bounds passed");return 0;
}
