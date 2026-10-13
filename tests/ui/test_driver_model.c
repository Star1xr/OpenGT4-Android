#include "driver_model.h"
#include "driver_pose.h"
#include "model.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void w16(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void w32(uint8_t *p,unsigned v){w16(p,v);w16(p+2,v>>16);}
static void wf(uint8_t *p,float f){memcpy(p,&f,4);}
static void selection(void)
{
    gt4_driver_car c={0};gt4_driver_selection s;
    CHECK(gt4_driver_select(NULL,0,-1).source==GT4_DRIVER_NONE);
    for(int type=0;type<256;type++) {
        c.type=(uint8_t)type;c.crew=1;
        s=gt4_driver_select(&c,0,-1);
        int shared=type==1 || type==2 || type==5 || type==6 || type==7 || type==10 || type==14 || type==15;
        CHECK((s.source==GT4_DRIVER_SHARED)==shared);
        CHECK(s.crew_enabled==shared);
        c.builtin_offset=128;s=gt4_driver_select(&c,3,-1);
        CHECK(s.source==GT4_DRIVER_BUILTIN && !s.path && !s.crew_enabled);c.builtin_offset=0;
    }
    c.type=1;CHECK(!strcmp(gt4_driver_select(&c,0,-1).path,"/character/driver2.mdl"));
    CHECK(!strcmp(gt4_driver_select(&c,1,-1).path,"/character/driver.mdl"));
    CHECK(!strcmp(gt4_driver_select(&c,2,-1).path,"/character/driver3.mdl"));
    CHECK(!strcmp(gt4_driver_select(&c,4,5).path,"/character/driver3.mdl"));
    c.type=5;CHECK(!strcmp(gt4_driver_select(&c,0,-1).path,"/character/classic1.mdl"));
    c.type=7;CHECK(!strcmp(gt4_driver_select(&c,0,-1).path,"/character/classic2.mdl"));
    c.type=15;c.crew=1;CHECK(!strcmp(gt4_driver_select(&c,2,-1).path,"/character/driver_mtr.mdl"));
    CHECK(!strcmp(gt4_driver_select(&c,3,-1).path,"/character/driver_nike.mdl"));
    CHECK(gt4_driver_detail(0)==0 && gt4_driver_detail(32)==1 && gt4_driver_detail(33)==1);
    CHECK(gt4_driver_detail(16)==2 && gt4_driver_detail(17)==2 && gt4_driver_detail(1)==3 && gt4_driver_detail(255)==3);
}
static void car_info(void)
{
    uint8_t d[640]={0};memcpy(d,"CAR4",4);w32(d+16,64);d[66]=5;w32(d+64+80,192);
    wf(d+192+0x34,-.239f);wf(d+192+0x38,1.148f);wf(d+192+0x3c,.246f);
    gt4_driver_car c;CHECK(gt4_driver_car_read(d,sizeof d,&c));
    CHECK(c.type==5 && c.seat[0]==-.239f && c.seat[1]==1.148f && c.seat[2]==.246f);
    CHECK(!gt4_driver_car_read(d,192+0x7b,&c));
    w32(d+64+80,0xfffffff0);CHECK(!gt4_driver_car_read(d,sizeof d,&c));w32(d+64+80,192);
    wf(d+192,NAN);CHECK(!gt4_driver_car_read(d,sizeof d,&c));wf(d+192,0);
    w32(d+52,384);CHECK(!gt4_driver_car_read(d,sizeof d,&c));
    memcpy(d+384,"MDLS",4);w32(d+400,128);CHECK(gt4_driver_car_read(d,sizeof d,&c));
    CHECK(gt4_driver_select(&c,0,-1).source==GT4_DRIVER_BUILTIN);
    w32(d+400,0xffffffff);CHECK(!gt4_driver_car_read(d,sizeof d,&c));
}
static void rig(void)
{
    uint8_t d[1024]={0};memcpy(d,"MDLS",4);w32(d+16,sizeof d);w16(d+22,3);w16(d+42,2);w16(d+46,1);
    w32(d+72,128);w32(d+128+64,0);w32(d+128+68,800);memcpy(d+800,"PELVIS",7);
    for(int i=0;i<16;i++)wf(d+128+i*4,i%5==0?1.f:0.f);
    w32(d+88,224);w32(d+224,800);w16(d+228,0);w32(d+232,816);w16(d+236,1);memcpy(d+816,"SKINNING",9);
    w32(d+104,256);w32(d+256,0x80000010);for(int i=0;i<16;i++)wf(d+260+i*4,(i+1)*.1f);
    w32(d+324,0x80000102);wf(d+328,.5f);wf(d+332,.25f);
    gt4_driver_rig r;CHECK(gt4_driver_rig_read(d,sizeof d,&r));
    CHECK(r.bone[0].present && r.bone[0].slot==0 && r.bone[0].model_first==0);
    CHECK(r.skin_first==1 && r.skin_count==1 && r.head2_first==-1 && r.dimensions[15]==1.6f);
    CHECK(r.bone[0].inverse_bind[15]==1 && r.lod[1]==.25f);
    w32(d+128+64,20);CHECK(!gt4_driver_rig_read(d,sizeof d,&r));w32(d+128+64,0);
    wf(d+128,NAN);CHECK(!gt4_driver_rig_read(d,sizeof d,&r));wf(d+128,1);
    w32(d+104,0xfffffff0);CHECK(!gt4_driver_rig_read(d,sizeof d,&r));w32(d+104,256);
    w32(d+256,0x800000ff);CHECK(!gt4_driver_rig_read(d,sizeof d,&r));
    /* A reduced builtin helmet has dimensions and a rigid symbol, no binds
       or SKINNING. A skin stream without bind slots is still malformed. */
    w32(d+256,0x80000010);w16(d+46,0);CHECK(!gt4_driver_rig_read(d,sizeof d,&r));
    w16(d+42,1);CHECK(gt4_driver_rig_read(d,sizeof d,&r) && r.slots==0 && r.bone[0].model_count==1);
    w16(d+42,0);CHECK(!gt4_driver_rig_read(d,sizeof d,&r));
}
static void skin(void)
{
    uint8_t d[1024]={0};memcpy(d,"MDLS",4);w32(d+16,sizeof d);d[22]=d[24]=d[28]=d[30]=1;w16(d+46,4);
    w32(d+56,128);w32(d+60,168);w32(d+68,172);w32(d+152,180);w32(d+168,192);w32(d+172,176);w32(d+176,800);
    d[180]=4;memcpy(d+800,"Tex1",4);d[200]=1;d[201]=0xa0;w16(d+202,1);w32(d+208,32);w16(d+212,1);
    d[224]=0x40;d[225]=0xc0;d[226]=2;d[227]=0x72;d[229]=3;
    wf(d+240,1/4096.f);d[244]=3;d[245]=1;d[246]=3;d[248]=0;d[249]=1;d[250]=2;d[251]=3;
    for(int i=0;i<3;i++){uint8_t *v=d+252+i*13;w16(v,i==1?4096:0);w16(v+2,i==2?4096:0);v[8]=127;v[9]=(uint8_t)(i==0?64:i==1?127:0);v[10]=(uint8_t)(127-v[9]);}
    gt4_model *m=gt4_model_decode_skin_part(d,sizeof d,0,NULL);CHECK(m && m->count==1 && m->skin);
    CHECK(m->triangles[0].v[1].p[0]==1 && m->triangles[0].v[2].p[1]==1 && m->triangles[0].v[0].normal[2]==1);
    CHECK(m->skin[0].v[0].bone[1]==1 && fabsf(m->skin[0].v[0].weight[0]-64/127.f)<1e-6f);
    gt4_model *copy=calloc(1,sizeof *copy);const float tr[1][6]={{0,0,0,-1,1,1}};
    CHECK(gt4_model_attach(copy,m,tr,1));CHECK(copy->skin && copy->skin[0].v[2].weight[1]==m->skin[0].v[1].weight[1]);
    gt4_model_free(copy);gt4_model_free(m);
    /* VU0 preserves the authored sum, even when it differs from one. */
    d[261]=64;d[262]=32;
    m=gt4_model_decode_skin_part(d,sizeof d,0,NULL);CHECK(m && m->skin);
    CHECK(fabsf(m->skin[0].v[0].weight[0]-64/127.f)<1e-6f);
    CHECK(fabsf(m->skin[0].v[0].weight[1]-32/127.f)<1e-6f);gt4_model_free(m);
    d[261]=64;d[262]=63;
    /* A static conversion allocates no skin and cannot silently treat the
       post-packet bytes as VIF vertices. */
    m=gt4_model_decode_part(d,sizeof d,0,0);CHECK(m && m->count==0 && !m->skin);gt4_model_free(m);
    d[249]=4;CHECK(!gt4_model_decode_skin_part(d,sizeof d,0,NULL));d[249]=1;
    w16(d+46,21);CHECK(!gt4_model_decode_skin_part(d,sizeof d,0,NULL));w16(d+46,4);
    w16(d+46,0);CHECK(!gt4_model_decode_skin_part(d,sizeof d,0,NULL));w16(d+46,4);
    d[246]=4;CHECK(!gt4_model_decode_skin_part(d,sizeof d,0,NULL));d[246]=2;
    CHECK(!gt4_model_decode_skin_part(d,sizeof d,0,NULL));d[246]=3;
    wf(d+240,NAN);CHECK(!gt4_model_decode_skin_part(d,sizeof d,0,NULL));wf(d+240,1/4096.f);
    for(int k=0;k<4;k++)d[261+k]=0;CHECK(!gt4_model_decode_skin_part(d,sizeof d,0,NULL));
    /* Float skin data carries position + normal + four float weights. */
    d[201]=0x80;memset(d+240,0,180);w16(d+240,3);w16(d+242,1);w16(d+244,3);
    d[249]=1;d[250]=2;d[251]=3;
    for(int i=0;i<3;i++){uint8_t *v=d+252+i*40;wf(v,i==1?1:0);wf(v+4,i==2?1:0);wf(v+20,1);wf(v+24,.25f);wf(v+28,.75f);}
    m=gt4_model_decode_skin_part(d,sizeof d,0,NULL);CHECK(m && m->skin && m->triangles[0].v[2].p[1]==1);
    CHECK(m->skin[0].v[1].weight[1]==.75f);gt4_model_free(m);
    wf(d+276,.5f);m=gt4_model_decode_skin_part(d,sizeof d,0,NULL);CHECK(m && m->skin);
    CHECK(m->skin[0].v[0].weight[0]==.5f && m->skin[0].v[0].weight[1]==.75f);gt4_model_free(m);
    wf(d+276,-1);CHECK(!gt4_model_decode_skin_part(d,sizeof d,0,NULL));wf(d+276,.25f);
    wf(d+272,INFINITY);CHECK(!gt4_model_decode_skin_part(d,sizeof d,0,NULL));
}
static void rigid_scale(void)
{
    /* Same compressed rigid stream at two authored scales. Scaling belongs
       to each shape, before model translations; this catches oversized heads
       and coincident diffuse/specular geometry diverging in the cabin. */
    uint8_t d[512]={0};memcpy(d,"MDLS",4);w32(d+16,sizeof d);d[22]=d[24]=d[28]=d[30]=1;
    w32(d+56,128);w32(d+60,168);w32(d+68,172);w32(d+152,180);w32(d+168,192);w32(d+172,176);w32(d+176,400);
    d[180]=4;memcpy(d+400,"Tex1",4);d[200]=0xa1;w16(d+202,1);w32(d+208,32);w16(d+212,3);
    w16(d+224,0x8000);d[226]=3;d[227]=0x69;w16(d+234,1000);w16(d+242,2000);
    w16(d+248,0x8080);d[250]=3;d[251]=0x6a;
    d[253]=127;d[255]=64;d[256]=64;d[258]=(uint8_t)-64;d[260]=127;
    w16(d+264,0xc040);d[266]=2;d[267]=0x72;d[269]=3;
    wf(d+196,5/32767.f);gt4_model *m=gt4_model_decode_part(d,sizeof d,0,0);CHECK(m && m->count==1);
    CHECK(m->triangles[0].v[1].p[0]==1000*(5/32767.f) && m->triangles[0].v[2].p[1]==2000*(5/32767.f));
    CHECK(m->triangles[0].v[0].normal[1]==1 && m->triangles[0].v[1].normal[0]==64/127.f && m->triangles[0].v[2].normal[0]==-64/127.f);gt4_model_free(m);
    /* The environment/reflection stream places the same signed normals at
       8040; the following scalar strip stream at C040 must not replace them. */
    w16(d+248,0x8040);m=gt4_model_decode_part(d,sizeof d,0,0);CHECK(m && m->count==1 && m->triangles[0].v[2].normal[0]==-64/127.f);gt4_model_free(m);
    wf(d+196,1/8192.f);m=gt4_model_decode_part(d,sizeof d,0,0);CHECK(m && m->count==1 && m->triangles[0].v[1].p[0]==1000/8192.f);gt4_model_free(m);
    wf(d+196,NAN);CHECK(!gt4_model_decode_part(d,sizeof d,0,0));wf(d+196,-1);CHECK(!gt4_model_decode_part(d,sizeof d,0,0));
}
static void hand_tween(void)
{
    /* Independent tiny MDLS: two float endpoints, a VM-selected shape,
       translated/mirrored model matrix and one strip. No disc needed. */
    uint8_t d[1024]={0};memcpy(d,"MDLS",4);w32(d+16,sizeof d);w16(d+22,1);w16(d+24,1);w16(d+28,1);w16(d+32,2);w16(d+36,1);
    w32(d+56,128);w32(d+60,168);w32(d+68,172);w32(d+172,176);w32(d+176,800);memcpy(d+800,"Tex1",4);w32(d+168,384);w32(d+152,224);w32(d+164,0);
    w32(d+76,184);w32(d+184,192);w16(d+188,1);memcpy(d+192,"leftGrasp",10);w32(d+92,320);
    const uint8_t program[35]={0x20,1,0x12,0,0x80,0x11,0,0,0,0,0x82,0x13,2,0,0x10,2,0,0x8c,0x14,1,0,0x10,2,0,0x11,0,0,0x80,0x3f,0x84,0x14,0,0,0x23,0};
    memcpy(d+320,program,35);wf(d+326,.9999f);
    d[224]=16;for(int k=0;k<16;k++)wf(d+225+k*4,k%5==0?1.f:0.f);wf(d+225,-1);wf(d+273,2);
    const uint8_t commands[]={67,76,0,0,67,77,1,0,1,3,0,0,4,0,0};memcpy(d+289,commands,sizeof commands);
    d[392]=0x41;d[393]=0x40;w16(d+394,1);w32(d+400,32);w16(d+404,16);
    size_t p=416;
    const unsigned addr[6]={0xc000,0xc020,0xc080,0xc0a0,0xc040,0xc060};
    for(int stream=0;stream<6;stream++) {
        int components=stream>=4?2:3;w16(d+p,addr[stream]);d[p+2]=3;d[p+3]=components==3?0x68:0x64;p+=4;
        for(int i=0;i<3;i++)for(int k=0;k<components;k++) {
            float value=stream<2?(k==0?(float)i+stream:k==1?(float)(i==2):0):stream<4?(float)(k==(stream==2?2:1)):(float)(stream-4)*.5f;
            wf(d+p,value);p+=4;
        }
    }
    w16(d+p,0xc040);d[p+2]=2;d[p+3]=0x72;d[p+5]=3;
    gt4_model *m=gt4_model_decode_hand_part(d,sizeof d,0,NULL);CHECK(m && m->count==1 && m->morph && !m->skin);
    CHECK(m->grasp_scale==.9999f);
    CHECK(m->triangles[0].v[0].p[0]==2 && m->morph[0].v[0].p[0]==1);
    CHECK(m->triangles[0].v[1].p[0]==0 && m->morph[0].v[1].p[0]==-1); /* mirrored winding */
    CHECK(m->triangles[0].v[0].normal[2]==1 && m->morph[0].v[0].normal[1]==1);
    CHECK(m->triangles[0].v[0].uv[0]==0 && m->morph[0].v[0].uv[0]==.5f);gt4_model_free(m);
    m=gt4_model_decode_skin_part(d,sizeof d,0,NULL);CHECK(m && !m->morph);gt4_model_free(m);
    d[330]=0x81;CHECK(!gt4_model_decode_hand_part(d,sizeof d,0,NULL));d[330]=0x82;
    wf(d+326,NAN);CHECK(!gt4_model_decode_hand_part(d,sizeof d,0,NULL));wf(d+326,.9999f);
    w16(d+164,0xfff0);CHECK(!gt4_model_decode_hand_part(d,sizeof d,0,NULL));w32(d+164,0);
    d[192]='x';CHECK(!gt4_model_decode_hand_part(d,sizeof d,0,NULL));d[192]='l';
    w16(d+295,0xffff);CHECK(!gt4_model_decode_hand_part(d,sizeof d,0,NULL));w16(d+295,1);
    for(size_t n=0;n<sizeof d;n++)CHECK(!gt4_model_decode_hand_part(d,n,0,NULL));
}
static void hand_assets(const char *folder)
{
    const char *names[]={"driver","driver2","driver3","driver4","classic1","classic2","driver_nike","driver_mtr"};
    unsigned hands=0;
    for(int i=0;i<8;i++) {
        char path[512];snprintf(path,sizeof path,"%s/%s.mdl",folder,names[i]);FILE *f=fopen(path,"rb");CHECK(f);
        CHECK(!fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0 && n<10000000);rewind(f);uint8_t *d=malloc((size_t)n);CHECK(d && fread(d,1,(size_t)n,f)==(size_t)n);fclose(f);
        gt4_driver_rig rig;CHECK(gt4_driver_rig_read(d,(size_t)n,&rig));
        for(int side=0;side<2;side++) {
            unsigned b=side?GT4_RHAND:GT4_LHAND;
            if(i==7){CHECK(rig.bone[b].model_count==0);continue;}
            CHECK(rig.bone[b].model_count==1);
            if(i==6) {
                CHECK(!gt4_model_decode_hand_part(d,(size_t)n,rig.bone[b].model_first,NULL));
                gt4_model *m=gt4_model_decode_skin_part(d,(size_t)n,rig.bone[b].model_first,NULL);CHECK(m && m->count && !m->morph);gt4_model_free(m);continue;
            }
            gt4_model *m=gt4_model_decode_hand_part(d,(size_t)n,rig.bone[b].model_first,NULL);CHECK(m && m->morph && m->count>0);int moved=0;
            for(int t=0;t<m->count;t++)for(int v=0;v<3;v++)for(int k=0;k<3;k++) {
                CHECK(isfinite(m->morph[t].v[v].p[k]) && isfinite(m->morph[t].v[v].normal[k]));
                moved|=fabsf(m->triangles[t].v[v].p[k]-m->morph[t].v[v].p[k])>1e-5f;
            }
            CHECK(moved);printf("%s hand %d: %d triangles, VM scale %.9g\n",names[i],side,m->count,m->grasp_scale);gt4_model_free(m);hands++;
        }free(d);
    }CHECK(hands==12);
}
static void palette(void)
{
    gt4_driver_rig r={0};r.slots=20;float pose[20][16]={0},out[20][16];
    for(int b=0;b<20;b++) {
        r.bone[b].present=1;r.bone[b].slot=(uint8_t)(19-b);
        for(int k=0;k<16;k++)pose[b][k]=r.bone[b].inverse_bind[k]=k%5==0?1.f:0.f;
        /* Inverse-bind translation must be rotated before pose translation. */
        pose[b][0]=0;pose[b][2]=-1;pose[b][8]=1;pose[b][10]=0;pose[b][12]=(float)b;
        r.bone[b].inverse_bind[12]=2;r.bone[b].inverse_bind[14]=3;
    }
    CHECK(gt4_driver_palette(&r,pose,out));
    for(int b=0;b<20;b++)CHECK(out[19-b][12]==b+3 && out[19-b][14]==-2);
    float saved[20][16];memcpy(saved,out,sizeof out);pose[5][0]=NAN;
    CHECK(!gt4_driver_palette(&r,pose,out) && !memcmp(saved,out,sizeof out));pose[5][0]=0;
    r.bone[5].slot=20;CHECK(!gt4_driver_palette(&r,pose,out));r.bone[5].slot=19;
    CHECK(!gt4_driver_palette(&r,pose,out));
}
static void skeleton(void)
{
    float d[16];for(int i=0;i<16;i++)d[i]=1;
    gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(d,&s));
    CHECK(s.bone[GT4_PELVIS].position[1]==0 && s.bone[GT4_PELVIS].length==1);
    CHECK(s.bone[GT4_LTHIGH].position[0]==.5f && s.bone[GT4_RTHIGH].position[0]==-.5f);
    CHECK(s.bone[GT4_LFOOT].position[1]==-1 && s.bone[GT4_LFOOT].end[2]==1);
    CHECK(s.bone[GT4_SCAPULA].position[1]==3 && s.bone[GT4_NECK].position[1]==5);
    CHECK(s.bone[GT4_LHAND].end[0]==1 && s.bone[GT4_RHAND].end[0]==-1);
    CHECK(s.bone[GT4_SCAPULA].branch[0][0]==.02f && s.bone[GT4_PELVIS].branch[1][1]==1);
    float poses[20][16];CHECK(gt4_driver_skeleton_matrices(&s,poses));
    CHECK(poses[GT4_CRANIUM][13]==6 && poses[GT4_LSHANK][0]==1);
    gt4_driver_skeleton saved=s;d[4]=NAN;CHECK(!gt4_driver_skeleton_init(d,&s));CHECK(!memcmp(&s,&saved,sizeof s));
    memset(d,0,sizeof d);CHECK(gt4_driver_skeleton_init(d,&s));
    CHECK(s.bone[0].length==0 && s.bone[0].inverse_length==0);
    s.bone[3].rotation[2]=NAN;CHECK(!gt4_driver_skeleton_matrices(&s,poses));
    gt4_driver_controls controls={0};gt4_driver_control_input in={-1.570796251296997f,1,{.5f,.75f},.25f,.5f};
    CHECK(gt4_driver_controls_update(1,&in,&controls));
    CHECK(controls.steering_enabled && controls.grasp_enabled && controls.grasp[0]==.5f && controls.grasp[1]==1);
    CHECK(gt4_driver_controls_update(4,&in,&controls));
    CHECK(!controls.grasp_enabled && controls.grasp[0]==.5f && controls.grasp[1]==1 && controls.pedal[0]==.75f && controls.pedal[1]==0);
    CHECK(gt4_driver_controls_update(3,&in,&controls) && controls.grasp[0]==.25f && controls.grasp[1]==.5f);
    gt4_driver_controls saved_controls=controls;in.steering=NAN;
    CHECK(!gt4_driver_controls_update(1,&in,&controls) && !memcmp(&controls,&saved_controls,sizeof controls));
}
static void limbs(void)
{
    gt4_driver_limb_target t={0};t.target[1]=-.6f;t.target[2]=.2f;t.guide[2]=1;
    t.axis[0][1]=-1;t.axis[1][2]=1;t.length[0]=t.length[1]=.4f;t.min_bend=.1f;
    gt4_driver_segment a={0},b={0};a.end[1]=b.end[1]=-.4f;int reachable=-1;
    CHECK(gt4_driver_solve_limb(&t,&a,&b,&reachable) && reachable);
    float pose[20][16];gt4_driver_skeleton s={0};s.bone[0]=a;s.bone[1]=b;
    CHECK(gt4_driver_skeleton_matrices(&s,pose));
    float end[3];for(int k=0;k<3;k++)end[k]=b.position[k]+pose[1][4+k]*b.end[1];
    for(int k=0;k<3;k++)CHECK(fabsf(end[k]-t.target[k])<2e-6f);
    t.target[1]=-2;CHECK(gt4_driver_solve_limb(&t,&a,&b,&reachable) && !reachable);
    gt4_driver_segment saved_a=a,saved_b=b;t.length[0]=0;
    CHECK(!gt4_driver_solve_limb(&t,&a,&b,&reachable) && !reachable);
    CHECK(!memcmp(&a,&saved_a,sizeof a) && !memcmp(&b,&saved_b,sizeof b));
    t.length[0]=.4f;t.target[0]=INFINITY;CHECK(!gt4_driver_solve_limb(&t,&a,&b,&reachable));
    float d[16];for(int k=0;k<16;k++)d[k]=.2f;CHECK(gt4_driver_skeleton_init(d,&s));
    const float feet[2][3]={{.1f,-.1f,.25f},{-.1f,-.1f,.25f}};
    gt4_driver_leg_controls controls={{.5f,.5f},{.5f,.5f},{.5f,.5f},{0,0}};
    int reach[2];CHECK(gt4_driver_solve_legs(&s,feet,&controls,reach));CHECK(gt4_driver_skeleton_matrices(&s,pose));
    gt4_driver_skeleton saved=s;int saved_reach[2];memcpy(saved_reach,reach,sizeof reach);
    controls.spread[1]=NAN;CHECK(!gt4_driver_solve_legs(&s,feet,&controls,reach));
    CHECK(!memcmp(&s,&saved,sizeof s) && !memcmp(reach,saved_reach,sizeof reach));
    gt4_driver_chain_target target={0};target.target[1]=.5f;target.target[2]=.1f;
    target.guide[2]=-1;target.axis[0][1]=1;target.axis[1][2]=-1;
    gt4_driver_segment chain[3];for(int k=0;k<3;k++){chain[k]=s.bone[GT4_LOWER_SPINE+k];target.length[k]=chain[k].length;}
    CHECK(gt4_driver_solve_chain(&target,chain,&reachable));gt4_driver_segment saved_chain[3];memcpy(saved_chain,chain,sizeof chain);
    target.length[1]=NAN;CHECK(!gt4_driver_solve_chain(&target,chain,&reachable) && !memcmp(saved_chain,chain,sizeof chain));
    gt4_driver_torso_target torso={{0,0,0},{0,0,0,1},{0,.6f,-.2f},{0,0,-1}};
    gt4_driver_torso_controls upper={.2f,.1f,.05f,.5f,0};CHECK(gt4_driver_solve_torso(&s,&torso,&upper,NULL));
    CHECK(gt4_driver_skeleton_matrices(&s,pose));saved=s;upper.twist=NAN;
    CHECK(!gt4_driver_solve_torso(&s,&torso,&upper,NULL) && !memcmp(&s,&saved,sizeof s));
    CHECK(gt4_driver_skeleton_init(d,&s));
    const float side_axis[3]={1,0,0},root[3]={.1f,.8f,0},hand_point[3]={.3f,.65f,.3f};float guide[3]={0};
    CHECK(gt4_driver_elbow_guide(guide,&s.bone[GT4_LCLAVICLE],side_axis,root,hand_point,.5f,0));
    float norm=guide[0]*guide[0]+guide[1]*guide[1]+guide[2]*guide[2];CHECK(fabsf(norm-1)<2e-6f);
    float saved_guide[3];memcpy(saved_guide,guide,sizeof guide);
    CHECK(!gt4_driver_elbow_guide(guide,&s.bone[GT4_LCLAVICLE],side_axis,root,hand_point,NAN,0));
    CHECK(!memcmp(guide,saved_guide,sizeof guide));
    gt4_driver_arm_target arms={{0,0,0,1},{{.3f,.65f,.3f},{-.3f,.65f,.3f}},{{0,0,1},{0,0,-1}},{{0},{0}}};
    gt4_driver_arm_controls arm_controls={0};arm_controls.auto_clavicle=1;
    arm_controls.elbow[0]=arm_controls.elbow[1]=arm_controls.wrist_twist[0]=arm_controls.wrist_twist[1]=.5f;
    arm_controls.normal_enabled[0]=arm_controls.normal_enabled[1]=1;
    reach[0]=77;reach[1]=88;CHECK(gt4_driver_solve_arms(&s,&arms,&arm_controls,reach));
    CHECK(reach[0]==reach[1]);CHECK(gt4_driver_skeleton_matrices(&s,pose));
    for(int side=0;side<2;side++) {
        int bone=side?GT4_RHAND:GT4_LHAND;const gt4_driver_segment *h=&s.bone[bone];
        for(int k=0;k<3;k++){float endpoint=h->position[k]+pose[bone][k]*h->end[0];CHECK(fabsf(endpoint-arms.hand[side][k])<2e-4f);}
    }
    saved=s;gt4_driver_arm_controls saved_arm=arm_controls;memcpy(saved_reach,reach,sizeof reach);
    arms.hand[1][2]=NAN;CHECK(!gt4_driver_solve_arms(&s,&arms,&arm_controls,reach));
    CHECK(!memcmp(&s,&saved,sizeof s) && !memcmp(&arm_controls,&saved_arm,sizeof arm_controls) && !memcmp(reach,saved_reach,sizeof reach));
    arms.hand[1][2]=.3f;arm_controls.simple_hand_twist=1;reach[0]=77;reach[1]=88;
    CHECK(gt4_driver_solve_arms(&s,&arms,&arm_controls,reach) && reach[0]==77 && reach[1]==88);
}
static void posture(void)
{
    float d[16];for(int k=0;k<16;k++)d[k]=.2f;gt4_driver_skeleton skeleton;
    CHECK(gt4_driver_skeleton_init(d,&skeleton));gt4_driver_head_controls head={.5f,0,1,0,1,{0,1,0},{0,0,0},0};
    int reach=77;const float look[3]={0,.8f,2};CHECK(gt4_driver_solve_head(&skeleton,&head,look,&reach));
    CHECK(head.blend>=0 && head.blend<=1 && (reach==0 || reach==1));
    gt4_driver_skeleton saved=skeleton;gt4_driver_head_controls saved_head=head;int saved_reach=reach;
    head.up[1]=NAN;CHECK(!gt4_driver_solve_head(&skeleton,&head,look,&reach));CHECK(!memcmp(&skeleton,&saved,sizeof saved) && reach==saved_reach);
    head=saved_head;reach=77;CHECK(gt4_driver_solve_head(&skeleton,&head,NULL,&reach) && reach==77);
    CHECK(gt4_driver_skeleton_init(d,&skeleton));gt4_driver_posture state;gt4_driver_pose_targets targets;
    gt4_driver_posture_init(&state,&targets);CHECK(state.enabled==15 && state.head.up[1]==1 && state.arms.normal_enabled[1]);
    targets.torso.end[1]=.6f;targets.torso.end[2]=-.2f;
    for(int side=0;side<2;side++){targets.arms.hand[side][0]=side?-.3f:.3f;targets.arms.hand[side][1]=.65f;targets.arms.hand[side][2]=.3f;
        targets.feet[side][0]=side?-.1f:.1f;targets.feet[side][1]=-.1f;targets.feet[side][2]=.25f;}
    saved=skeleton;gt4_driver_posture initial=state;int statuses[5]={77,88,99,111,222};
    int old_mode=fegetround();CHECK(!fesetround(FE_TONEAREST));CHECK(gt4_driver_posture_solve(&skeleton,&state,&targets,look,statuses));CHECK(fegetround()==FE_TONEAREST);
    gt4_driver_skeleton first=skeleton;gt4_driver_posture first_state=state;int first_status[5];memcpy(first_status,statuses,sizeof statuses);
    skeleton=saved;state=initial;int original_status[5]={77,88,99,111,222};memcpy(statuses,original_status,sizeof statuses);
    CHECK(!fesetround(FE_UPWARD));CHECK(gt4_driver_posture_solve(&skeleton,&state,&targets,look,statuses));CHECK(fegetround()==FE_UPWARD);
    CHECK(!memcmp(&skeleton,&first,sizeof first) && !memcmp(&state,&first_state,sizeof state) && !memcmp(statuses,first_status,sizeof statuses));
    targets.arms.hand[1][2]=NAN;CHECK(!gt4_driver_posture_solve(&skeleton,&state,&targets,look,statuses));CHECK(fegetround()==FE_UPWARD);
    CHECK(!memcmp(&skeleton,&first,sizeof first) && !memcmp(&state,&first_state,sizeof state) && !memcmp(statuses,first_status,sizeof statuses));CHECK(!fesetround(old_mode));
}
static void vehicle_targets(void)
{
    float dimensions[16];for(int k=0;k<16;k++)dimensions[k]=.2f;gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(dimensions,&s));
    gt4_driver_geometry g;gt4_driver_geometry_init(&g);g.wheel[1]=.4f;g.wheel[2]=.25f;g.wheel_radius=.2f;
    g.feet[0][2]=g.feet[1][2]=.4f;g.shift_side=1;g.gear[0][0]=.3f;g.gear[0][1]=.2f;
    float normal[3]={0,0,-1};int mode=fegetround();CHECK(!fesetround(FE_TONEAREST));CHECK(gt4_driver_geometry_wheel(&g,normal));CHECK(fegetround()==FE_TONEAREST);
    gt4_driver_grip grip={0},first_grip;float angles[2]={-1.5f,-1.5f};int flag;
    CHECK(gt4_driver_grip_solve(&grip,&g,GT4_GRIP_WHEEL,0,angles,.3f,.5f,&flag));first_grip=grip;int first_flag=flag;
    CHECK(!fesetround(FE_UPWARD));memset(&grip,0,sizeof grip);CHECK(gt4_driver_grip_solve(&grip,&g,GT4_GRIP_WHEEL,0,angles,.3f,.5f,&flag));
    CHECK(fegetround()==FE_UPWARD && !memcmp(&grip,&first_grip,sizeof grip) && flag==first_flag);
    angles[1]=NAN;CHECK(!gt4_driver_grip_solve(&grip,&g,GT4_GRIP_WHEEL,0,angles,.3f,.5f,&flag));CHECK(!memcmp(&grip,&first_grip,sizeof grip) && flag==first_flag);
    gt4_driver_posture p;gt4_driver_pose_targets targets;gt4_driver_posture_init(&p,&targets);gt4_driver_target_controls channels;gt4_driver_targets_init(&channels);
    CHECK(gt4_driver_targets_build(&s,&p,&g,&channels,&targets));CHECK(fegetround()==FE_UPWARD);
    gt4_driver_posture first=p;gt4_driver_pose_targets first_targets=targets;
    CHECK(!fesetround(FE_TONEAREST));gt4_driver_posture_init(&p,&targets);CHECK(gt4_driver_targets_build(&s,&p,&g,&channels,&targets));
    CHECK(fegetround()==FE_TONEAREST && !memcmp(&p,&first,sizeof p) && !memcmp(&targets,&first_targets,sizeof targets));
    channels.clavicle_roll[1]=NAN;CHECK(!gt4_driver_targets_build(&s,&p,&g,&channels,&targets));
    CHECK(fegetround()==FE_TONEAREST && !memcmp(&p,&first,sizeof p) && !memcmp(&targets,&first_targets,sizeof targets));
    gt4_driver_geometry before=g;normal[2]=NAN;CHECK(!gt4_driver_geometry_wheel(&g,normal));CHECK(!memcmp(&g,&before,sizeof g));CHECK(!fesetround(mode));
}
static void race_frame(void)
{
    gt4_driver_car c={0};c.geometry_offset=1;c.type=1;c.wheel_normal[2]=-1;c.wheel_radius=.2f;
    c.wheel[0]=c.seat[0]=-.3f;c.wheel[1]=.7f;c.wheel[2]=-.25f;c.seat[1]=.3f;
    c.shift[1]=.45f;c.feet[0][0]=-.2f;c.feet[1][0]=-.4f;c.feet[0][2]=c.feet[1][2]=-.4f;
    float dimensions[16],channels[64],matrices[20][16];for(int k=0;k<16;k++)dimensions[k]=.2f;
    gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(dimensions,&s));CHECK(gt4_driver_record_init(&c,channels));
    CHECK(channels[0x58/4]==c.seat[0] && channels[0x60/4]==c.seat[2]-6.25f);
    int mode=fegetround();CHECK(!fesetround(FE_UPWARD));CHECK(gt4_driver_race_pose(&c,channels,&s,matrices));CHECK(fegetround()==FE_UPWARD);
    for(int b=0;b<20;b++)for(int k=0;k<16;k++)CHECK(isfinite(matrices[b][k]));
    gt4_driver_skeleton saved=s;float saved_matrices[20][16];memcpy(saved_matrices,matrices,sizeof matrices);
    channels[0x34/4]=NAN;CHECK(!gt4_driver_race_pose(&c,channels,&s,matrices));
    CHECK(fegetround()==FE_UPWARD && !memcmp(&s,&saved,sizeof s) && !memcmp(matrices,saved_matrices,sizeof matrices));
    c.type=10;CHECK(!gt4_driver_record_init(&c,channels));CHECK(!fesetround(mode));
}
static void animation(void)
{
    gt4_driver_car c={0};c.geometry_offset=1;c.type=1;c.wheel_normal[2]=-1;c.wheel_radius=.2f;
    c.wheel[0]=c.seat[0]=-.3f;c.wheel[1]=.7f;c.wheel[2]=-.25f;c.seat[1]=.3f;
    c.shift[0]=.1f;c.shift[1]=.45f;c.shift_normal[1]=1;c.shift_length=.15f;
    c.feet[0][0]=-.2f;c.feet[1][0]=-.4f;c.feet[0][2]=c.feet[1][2]=-.4f;
    int tr[4]={1,1,-1,6};gt4_driver_animation a;CHECK(gt4_driver_animation_init(&a,&c,tr,1));
    float dimensions[16],matrices[20][16];for(int k=0;k<16;k++)dimensions[k]=.2f;
    gt4_driver_skeleton skeleton;CHECK(gt4_driver_skeleton_init(dimensions,&skeleton));
    int mode=fegetround();CHECK(!fesetround(FE_UPWARD));
    float max_phase=0;unsigned regrips=0;
    for(int i=0;i<1200;i++) {
        gt4_driver_animation_input in={0};in.dt=1.f/60;in.gear=i<120?1:i<600?2:1;
        in.steering=i<240?4*sinf(i*.02f):0;in.throttle=i<600?1:0;in.brake=i>=600?1:0;
        CHECK(gt4_driver_animation_step(&a,&c,&in));CHECK(fegetround()==FE_UPWARD);
        CHECK(gt4_driver_race_pose(&c,a.channels,&skeleton,matrices));
        if(a.channels[0x1c/4]>max_phase)max_phase=a.channels[0x1c/4];
        regrips+=a.hand[0].state!=0 || a.hand[1].state!=0;
        for(int b=0;b<20;b++)for(int k=0;k<16;k++)CHECK(isfinite(matrices[b][k]));
    }
    CHECK(max_phase>.9f && regrips>0 && a.gear==1 && a.shift_state==0);
    CHECK(a.channels[0x1c/4]==0 && a.channels[0x14/4]==0 && a.channels[0x18/4]==1);
    gt4_driver_animation saved=a;gt4_driver_animation_input invalid={0};invalid.dt=NAN;
    CHECK(!gt4_driver_animation_step(&a,&c,&invalid));CHECK(!memcmp(&a,&saved,sizeof a) && fegetround()==FE_UPWARD);
    invalid.dt=1.f/60;invalid.steering=INFINITY;
    CHECK(!gt4_driver_animation_step(&a,&c,&invalid));CHECK(!memcmp(&a,&saved,sizeof a));
    CHECK(!fesetround(mode));
}
int main(int argc,char **argv){hand_tween();animation();race_frame();rigid_scale();vehicle_targets();posture();limbs();skeleton();selection();car_info();rig();skin();palette();if(argc==2)hand_assets(argv[1]);puts("driver metadata, skin streams and native animation: OK");return 0;}
