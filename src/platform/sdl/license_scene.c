/* LicenseConcourse 00408490: the three people share /pit/license_001.
   Native meshes/textures remain resident; only poses/palettes change. */
#include "license_scene.h"
#include "driver_model.h"
#include "license_image.h"
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct license_scene {gt4_license_motion *motion;gt4_driver_rig rig,shadow_rig;r3d_mesh *skin,*head,*pen,*board,*shadow,*gun,*fuel,*lollipop,*cable,*bake[4],*walk[4],*squat;float walk_scale[2];};
static const char *const names[22]={
 "PELVIS","LTHIGH_BONE","LSHANK_BONE","LFOOT_BONE","RTHIGH_BONE","RSHANK_BONE","RFOOT_BONE",
 "LOWER_SPINE","MIDDLE_SPINE","SCAPULA","LUPARM_BONE","LFOREARM_BONE","RUPARM_BONE","RFOREARM_BONE",
 "CRANIUM","LCLAVICLE","RCLAVICLE","LHAND_BONE","RHAND_BONE","NECK_BONE","pen","board"};
static unsigned h(const uint8_t*d){return d[0]|(unsigned)d[1]<<8;}
static unsigned w(const uint8_t*d){return h(d)|h(d+2)<<16;}
static int span(size_t n,unsigned at,size_t bytes){return at<=n && bytes<=n-at;}
static const char *label(const uint8_t*d,size_t n,unsigned at){return at<n && memchr(d+at,0,n-at)?(const char*)d+at:NULL;}
static int symbol(const uint8_t*d,size_t n,const char *name)
{unsigned at=w(d+88),count=h(d+42);if(!span(n,at,(size_t)count*8))return -1;for(unsigned i=0;i<count;i++){const char*s=label(d,n,w(d+at+i*8));if(!s)return -1;if(!strcmp(s,name)){int index=h(d+at+i*8+4);return index<h(d+22)?index:-1;}}return -1;}
static int bindings(const uint8_t*d,size_t n,gt4_driver_rig *rig)
{
 /* 0042A660 uses generic MDLS binds; workman00 has no HumanModel
    dimension block, so the pilot-specific rig reader is inapplicable. */
 unsigned at=w(d+72),count=h(d+46),seen=0;if(count!=20 || !span(n,at,count*80))return 0;
 memset(rig,0,sizeof *rig);rig->slots=count;
 for(unsigned i=0;i<count;i++){
  const uint8_t*r=d+at+i*80;const char*s=label(d,n,w(r+68));unsigned slot=w(r+64);if(!s || slot>=20 || (seen&(1u<<slot)))return 0;
  unsigned bone;for(bone=0;bone<20;bone++)if(!strcmp(s,names[bone]))break;if(bone==20)return 0;
  rig->bone[bone].present=1;rig->bone[bone].slot=(uint8_t)slot;memcpy(rig->bone[bone].inverse_bind,r,64);seen|=1u<<slot;
 }return seen==0xfffff;
}
static r3d_mesh *mesh(const uint8_t *data,size_t size,int index,const r3d_mesh *shared)
{if(index<0)return NULL;gt4_model *m=gt4_model_decode_skin_part(data,size,index,NULL);if(!m)return NULL;
 if(getenv("OPENGT4_LICENSE_SCENE_LOG")){float lo=INFINITY,hi=-INFINITY;for(int i=0;i<m->count;i++)for(int j=0;j<3;j++){lo=fminf(lo,m->triangles[i].v[j].p[1]);hi=fmaxf(hi,m->triangles[i].v[j].p[1]);}fprintf(stderr,"licence mesh: model %d skin %d triangles %d y %.6f..%.6f\n",index,m->skin!=NULL,m->count,lo,hi);}
 r3d_mesh *gpu=shared?r3d_mesh_create_shared(m,shared):r3d_mesh_create(m);gt4_model_free(m);return gpu;}
license_scene *license_scene_open(gt4_license_motion *motion)
{
 if(!motion)return NULL;size_t size;const uint8_t *data=gt4_license_motion_model(motion,0,&size);if(!data)return NULL;
 license_scene*s=calloc(1,sizeof *s);if(!s)return NULL;s->motion=motion;
 if(!bindings(data,size,&s->rig))goto fail;
 s->skin=mesh(data,size,symbol(data,size,"SKINNING"),NULL);if(!s->skin)goto fail;
 s->head=mesh(data,size,symbol(data,size,"CRANIUM"),s->skin);
 /* /character/workman00.mdl symbols CRANIUM=1, pen=2, board=3,
    selected by the bank's authored markers (0042A660 / 004548A8). */
 s->pen=mesh(data,size,symbol(data,size,"pen"),s->skin);s->board=mesh(data,size,symbol(data,size,"board"),s->skin);
 if(!s->head || !s->pen || !s->board)goto fail;
 const uint8_t *shadow=gt4_license_motion_model(motion,1,&size);
 if(!shadow || !bindings(shadow,size,&s->shadow_rig))goto fail;
 s->shadow=mesh(shadow,size,symbol(shadow,size,"SKINNING"),NULL);if(!s->shadow)goto fail;
 fprintf(stderr,"licence scene: resident pitman01 shadow, %u bind slots\n",s->shadow_rig.slots);
 fprintf(stderr,"licence scene: resident staff skin/head/pen/board, %u bind slots\n",s->rig.slots);return s;
fail:fprintf(stderr,"licence scene: staff MDLS decode failed\n");license_scene_close(s);return NULL;
}
license_scene *license_scene_open_pit(gt4_license_motion *motion){
 /* 003CC7B0 / 003CC8E8: authored pitman skin, rigid head and tools.
    Decode/upload once for all six entries, all roles share textures. */
 if(!motion)return NULL;size_t size;const uint8_t*d=gt4_license_motion_model(motion,0,&size);if(!d)return NULL;
 license_scene*s=calloc(1,sizeof *s);if(!s)return NULL;s->motion=motion;
 if(!bindings(d,size,&s->rig))goto fail;
 s->skin=mesh(d,size,symbol(d,size,"SKINNING"),NULL);if(!s->skin)goto fail;
 s->head=mesh(d,size,symbol(d,size,"CRANIUM"),s->skin);
 s->gun=mesh(d,size,symbol(d,size,"gun"),s->skin);s->fuel=mesh(d,size,symbol(d,size,"fuel"),s->skin);
 s->lollipop=mesh(d,size,symbol(d,size,"LoliPop"),s->skin);
 static const char*const baked[]={"BAKE_FUEL","BAKE_LOLI","BAKE_GUN","BAKE_GUN2"};
 for(unsigned i=0;i<4;i++){s->bake[i]=mesh(d,size,symbol(d,size,baked[i]),s->skin);if(!s->bake[i])goto fail;}
 /* 003D5528 / BC_WALK and BC_SQUAT: all time-VM branches and their
    authored tween endpoints stay resident, sharing the pit texture table. */
 for(unsigned i=0;i<5;i++){
  int index=symbol(d,size,i<4?"BC_WALK":"BC_SQUAT");gt4_model *part=gt4_model_decode_pit_walk_part(d,size,index,i<4?(int)i:0,NULL);if(!part)goto fail;
  s->walk_scale[i<4?0:1]=part->phase_scale;r3d_mesh *gpu=r3d_mesh_create_shared(part,s->skin);gt4_model_free(part);if(!gpu)goto fail;
  if(i<4)s->walk[i]=gpu;else s->squat=gpu;
 }
 d=gt4_license_motion_model(motion,1,&size);if(!d || !bindings(d,size,&s->shadow_rig))goto fail;
 s->shadow=mesh(d,size,symbol(d,size,"SKINNING"),NULL);
 if(!s->head || !s->gun || !s->fuel || !s->lollipop || !s->shadow)goto fail;
 /* 003CA168: crossed untextured strips, ALPHA 0x44, black alpha .8.
    Keep one fixed-size GPU buffer; each wheel updates positions only. */
 gt4_triangle triangles[92]={0};gt4_model cable={0};cable.count=92;cable.triangles=triangles;
 for(unsigned i=0;i<92;i++){gt4_triangle*t=triangles+i;t->texture=-1;t->blend_enabled=1;t->state.blend=0x44;t->state.color_mask=0xffffffff;t->state.depth_test=t->state.depth_write=1;t->state.depth_func=2;
  for(unsigned v=0;v<3;v++){t->v[v].alpha=gt4_f32(0x3f4ccccc);t->v[v].normal[1]=1;}}
 s->cable=r3d_mesh_create(&cable);if(!s->cable)goto fail;
 return s;
fail:fprintf(stderr,"pit crew MDLS decode failed\n");license_scene_close(s);return NULL;
}
void license_scene_close(license_scene*s){if(s){r3d_mesh_destroy(s->skin);r3d_mesh_destroy(s->head);r3d_mesh_destroy(s->pen);r3d_mesh_destroy(s->board);r3d_mesh_destroy(s->shadow);r3d_mesh_destroy(s->gun);r3d_mesh_destroy(s->fuel);r3d_mesh_destroy(s->lollipop);r3d_mesh_destroy(s->cable);for(unsigned i=0;i<4;i++){r3d_mesh_destroy(s->bake[i]);r3d_mesh_destroy(s->walk[i]);}r3d_mesh_destroy(s->squat);free(s);}}
static r3d_view attach(const r3d_view *root,const float m[16])
{r3d_view v=*root;for(unsigned r=0;r<3;r++){v.pos[r]=root->pos[r];for(unsigned k=0;k<3;k++)v.pos[r]+=root->rot[r*3+k]*m[12+k];for(unsigned c=0;c<3;c++){v.rot[r*3+c]=0;for(unsigned k=0;k<3;k++)v.rot[r*3+c]+=root->rot[r*3+k]*m[c*4+k];}}return v;}
/* 003E6C60 -> 00409B68: authored per-surface light list. The nine
   candidates are sorted stably by attenuation and the first four survive.
   A boundary: host sqrt/pow/cos and the native VU lighting rasterizer. */
static float ef(const uint8_t *p){float v;memcpy(&v,p,4);return v;}
void license_scene_lights(const uint8_t *env,uint32_t mesh,const gt4_ray_hit *hit,
                         const float pos[3],r3d_lights *out)
{
 memset(out,0,sizeof *out);float sun=hit->u,level=hit->v;
 uint32_t list=0;int custom=GT4_U16(mesh+0x40)!=0;
 if(custom && hit->leaf>=0){
  uint32_t tri=GT4_U32(mesh+0x78)+(uint16_t)hit->tri*12;
  uint32_t vertex=GT4_U32(mesh+0x74)+GT4_U16(tri)*16;
  int index=(int16_t)GT4_U16(vertex+12);
  if(index>=0)list=GT4_U32(mesh+0x6c)+(unsigned)index*2;
 }
 if(custom && !list){sun=1;level=0;}
 if(list){
  struct candidate {uint32_t record;float weight,dir[3];} candidates[9];
  unsigned count=GT4_U16(mesh+0x50);if(count>9)count=9;
  for(unsigned i=0;i<count;i++){
   struct candidate *c=candidates+i;
   c->record=GT4_U32(mesh+0x68)+(int16_t)GT4_U16(list+i*2)*32;
   uint32_t r=c->record;const uint8_t *source=env+GT4_U32(r)*128;
   memcpy(c->dir,gt4_ptr(r+0x10),12);c->weight=1;
   if(GT4_F32(r+0x1c)!=0){
    for(int j=0;j<3;j++)c->dir[j]-=pos[j];
    float d2=(c->dir[0]*c->dir[0]+c->dir[1]*c->dir[1])+c->dir[2]*c->dir[2];
    float inverse=1.f/sqrtf(d2),distance=d2*inverse;
    for(int j=0;j<3;j++)c->dir[j]*=inverse;
    float denominator=(ef(source+0x124)+ef(source+0x128)*distance)+ef(source+0x12c)*d2;
    c->weight=1.f/denominator;
    if(ef(source+0x120)!=180){
     float dot=(c->dir[0]*GT4_F32(r+4)+c->dir[1]*GT4_F32(r+8))+c->dir[2]*GT4_F32(r+12);dot=fmaxf(dot,0);
     /* 004A5ED8 -> 004A7350 -> VU0 0x58: cosine of authored degrees. */
     float threshold=cosf(ef(source+0x120)*gt4_f32(0x3c8efa35));
     c->weight=threshold<=dot?c->weight*powf(dot,ef(source+0x11c)):0;
    }
   }
  }
  for(unsigned n=count;n>1;n--)for(unsigned i=0;i<n-1;i++)if(candidates[i].weight<candidates[i+1].weight){
   struct candidate c=candidates[i];candidates[i]=candidates[i+1];candidates[i+1]=c;
  }
  out->count=count>4?4:(int)count;
  for(int i=0;i<out->count;i++){
   struct candidate *c=candidates+i;const uint8_t *source=env+GT4_U32(c->record)*128;
   float k=GT4_F32(c->record+0x1c)!=0?c->weight:1;
   memcpy(out->direction[i],c->dir,12);
   for(int j=0;j<3;j++){out->ambient[i][j]=ef(source+0xe0+j*4)*k;out->diffuse[i][j]=ef(source+0xf0+j*4)*k;out->specular[i][j]=ef(source+0x100+j*4)*k;}
  }
 }else{
  /* 0040A1D0 -> 00398210 / 00397FF8. Native frame light units
     undo Env +38 exposure; vertex_limit preserves the GS saturation. */
  const unsigned fades[3][2]={{0xac8,0xacc},{0xab4,0xabc},{0xac0,0xac4}};
  for(unsigned i=0;i<4;i++)if(w(env+0x10)&(1u<<i)){
   int k=out->count++;const uint8_t *r=env+0xd0+i*128;memcpy(out->direction[k],r,12);
   float *colors[3]={out->ambient[k],out->diffuse[k],out->specular[k]};
   for(int type=0;type<3;type++)gt4_license_light_color((const float *)(r+16+type*16),sun,1-ef(env+fades[type][0]),level,1-ef(env+fades[type][1]),1,colors[type]);
  }
 }
 /* 0040A1D0 +0x20..0x54: global ambient is independent of lights. */
 if(!out->count)out->count=1;
 for(int j=0;j<3;j++)out->ambient[0][j]+=ef(env+0xef0+j*4);
 out->vertex_limit=ef(env+0x38)*255.f/128;
}
void license_scene_draw(license_scene*s,const r3d_view*view,const float place[3][4],uint32_t course,const gt4_course *asset,const gt4_ray_hit surfaces[3])
{
 if(!s || !view || !place || !course)return;
 /* The physics loader owns only RNW4 and .ad; it intentionally has no
    Env0 pointer. Borrow the resident full course from race_view instead. */
 if(!asset || asset->size<0x84)return;
 unsigned env_offset=w(asset->data+0x80);
 if(!env_offset || !span(asset->size,env_offset,0xf00) || memcmp(asset->data+env_offset,"Env0",4))return;
 const uint8_t *env=asset->data+env_offset;
 uint32_t collision=GT4_U32(GT4_U32(course+4)+0xdc);if(!collision)return;
 fenv_t saved;fegetenv(&saved);gt4_phys_fpu_enter();
 /* 00398758 -> 00398438: exact degree multiplier sequence; libm boundary. */
 float A=(-ef(env+0xa70)*gt4_f32(0x40490fda))/180;
 float B=(-ef(env+0xa74)*gt4_f32(0x40490fda))/180,k=ef(env+0xa78);
 float way[3]={sinf(B)*cosf(A)*k,-sinf(A)*k,cosf(B)*cosf(A)*k};
 for(unsigned actor=0;actor<3;actor++){
  float poses[22][16],palette[20][16],shadow_palette[20][16];unsigned count=actor?20:22;
  if(gt4_license_motion_frames(s->motion,actor,names,count,poses)!=(int)count || !gt4_driver_palette(&s->rig,poses,palette))continue;
  r3d_view root=*view;memcpy(root.pos,place[actor],12);float sn=sinf(place[actor][3]),cs=cosf(place[actor][3]);
  float rot[9]={cs,0,sn,0,1,0,-sn,0,cs};memcpy(root.rot,rot,36);
  /* 00408490 +0x134..0x210: authored shadow bank, T * P * Ry,
     using its own inverse binds, normal and projection depth bias .01.
     00397ED8 clears the global shadow RGBA before staff drawing; the
     authored bank supplies its color/alpha (no invented contact ellipse). */
  if(gt4_driver_palette(&s->shadow_rig,poses,shadow_palette)){
   float projection[16];gt4_license_shadow_projection(way,surfaces[actor].normal,projection);
   r3d_view shadow=root;shadow.unlit=1;shadow.shadow=1;shadow.shadow_alpha=1;
   memset(shadow.shadow_rgb,0,sizeof shadow.shadow_rgb);shadow.depth_bias=gt4_f32(0x3c23d70a);
   shadow.skin_palette=shadow_palette;
   for(int r=0;r<3;r++)for(int c=0;c<3;c++){shadow.rot[r*3+c]=0;for(int j=0;j<3;j++)shadow.rot[r*3+c]+=projection[j*4+r]*rot[j*3+c];}
   r3d_draw(s->shadow,&shadow);
  }
  r3d_lights light;license_scene_lights(env,collision,surfaces+actor,place[actor],&light);
  root.skin_palette=palette;r3d_draw_lit(s->skin,&root,&light);
  root.skin_palette=NULL;r3d_view head=attach(&root,poses[14]);r3d_draw_lit(s->head,&head,&light);
  if(!actor){r3d_view pen=attach(&root,poses[20]),board=attach(&root,poses[21]);r3d_draw_lit(s->pen,&pen,&light);r3d_draw_lit(s->board,&board,&light);}
 }r3d_end();fesetenv(&saved);
}
void license_scene_draw_pit(license_scene*s,const r3d_view*view,const gt4_pit_crew*c,uint32_t course,const gt4_course*asset,int watched){
 if(!s || !view || !c || !c->motion || !c->placed || !asset || asset->size<0x84 || !course)return;
 unsigned offset=w(asset->data+0x80);if(!offset || !span(asset->size,offset,0xf00) || memcmp(asset->data+offset,"Env0",4))return;
 const uint8_t*env=asset->data+offset;uint32_t collision=GT4_U32(GT4_U32(course+4)+0xdc);if(!collision)return;
 fenv_t saved;fegetenv(&saved);gt4_phys_fpu_enter();
 float A=(-ef(env+0xa70)*gt4_f32(0x40490fda))/180,B=(-ef(env+0xa74)*gt4_f32(0x40490fda))/180,k=ef(env+0xa78);
 float way[3]={sinf(B)*cosf(A)*k,-sinf(A)*k,cosf(B)*cosf(A)*k};
 float sn=sinf(c->root[3]),cs=cosf(c->root[3]);
 for(unsigned role=0;watched && c->stage && role<6;role++)if(c->active[role]){
  unsigned cars=GT4_U32(GT4_U32(c->car+4)+0xf8a0);int model=gt4_pit_actor_model(c,role,cars);
  if(model==2)continue; /* 003D1BE0 +1DD8: drawn below by PitmenData. */
  const char *markers[23];for(unsigned i=0;i<20;i++)markers[i]=names[i];markers[20]=role<4?"gun":role==4?"fuel":"LoliPop";
  float poses[23][16],palette[20][16],shadow_palette[20][16];
  if(model<3 && (gt4_license_motion_frames_blend(c->motion,role,role<4?c->blend[role]:0,markers,21,poses)!=21 || !gt4_driver_palette(&s->rig,poses,palette)))continue;
  /* 003D1BE0 +1E60..1EB0: box root, heading, actor translation and
     Mot1 frames. +1EB0 applies the original 3F70A3D7 uniform scale. */
  r3d_view root=*view;float angle=c->root[3]+(c->place[role][3]+gt4_pit_actor_model_heading(model)),si=sinf(angle),co=cosf(angle);
  root.pos[0]=(c->root[0]+cs*c->place[role][0])+sn*c->place[role][2];
  root.pos[1]=c->root[1]+c->place[role][1];root.pos[2]=(c->root[2]-sn*c->place[role][0])+cs*c->place[role][2];
  float scale=gt4_f32(0x3f70a3d7),rot[9]={co*scale,0,si*scale,0,scale,0,-si*scale,0,co*scale};memcpy(root.rot,rot,36);
  gt4_ray_hit hit={0};float ray[3]={root.pos[0],root.pos[1]+1,root.pos[2]};
  if(!gt4_course_ray_down(collision,&hit,ray))continue;
  if(role==5)root.pos[1]=hit.pos[1]; /* 003CCC28 grounds the lollipop role. */
  r3d_lights light;license_scene_lights(env,collision,&hit,root.pos,&light);
  r3d_view tool=root;
  if(model>=3){
   /* 003D1BE0 +2030..2110: rigid authored models use the actor root;
      no skeletal palette or extra head/tool draw in this branch. */
   const char*name=gt4_pit_actor_model_name(model);int index=name && !strcmp(name,"BAKE_FUEL")?0:name && !strcmp(name,"BAKE_LOLI")?1:name && !strcmp(name,"BAKE_GUN")?2:name && !strcmp(name,"BAKE_GUN2")?3:-1;
   root.skin_palette=NULL;if(index>=0)r3d_draw_lit(s->bake[index],&root,&light);
   float marker[16]={0},end[3];marker[0]=marker[5]=marker[10]=marker[15]=1;gt4_pit_actor_cable_end(model,end);memcpy(marker+12,end,12);tool=attach(&root,marker);
  }else{
  if(gt4_driver_palette(&s->shadow_rig,poses,shadow_palette)){
   float projection[16];gt4_license_shadow_projection(way,hit.normal,projection);r3d_view shadow=root;
   shadow.unlit=shadow.shadow=1;shadow.shadow_alpha=1;memset(shadow.shadow_rgb,0,sizeof shadow.shadow_rgb);
   shadow.depth_bias=gt4_f32(0x3c23d70a);shadow.skin_palette=shadow_palette;
   for(int r=0;r<3;r++)for(int col=0;col<3;col++){shadow.rot[r*3+col]=0;for(int j=0;j<3;j++)shadow.rot[r*3+col]+=projection[j*4+r]*rot[j*3+col];}
   r3d_draw(s->shadow,&shadow);
  }
  root.skin_palette=palette;r3d_draw_lit(s->skin,&root,&light);root.skin_palette=NULL;
  r3d_view head=attach(&root,poses[14]);tool=attach(&root,poses[20]);r3d_draw_lit(s->head,&head,&light);
  r3d_draw_lit(role<4?s->gun:role==4?s->fuel:s->lollipop,&tool,&light);
  }
  if(role<4){float positions[276][3];gt4_pit_cable_vertices(c,role,tool.pos,positions);
   if(r3d_mesh_update_positions(s->cable,positions,276,gt4_f32(0x3f4ccccc))){r3d_view cable=*view;memset(cable.pos,0,sizeof cable.pos);memset(cable.rot,0,sizeof cable.rot);cable.rot[0]=cable.rot[4]=cable.rot[8]=1;cable.unlit=1;cable.skin_palette=NULL;r3d_draw(s->cable,&cable);}}
 }
 /* 003D1AA8 draws the other boxes, 003D1BE0 delegates model 2 in the
    watched box. 003D51B0 suppresses boxes below projected size .025. */
 unsigned cars=GT4_U32(GT4_U32(c->car+4)+0xf8a0);
 float depth=0;for(unsigned k=0;k<3;k++)depth+=(c->box[k]-view->cam[k])*view->f[k];
 if(depth!=0 && fabsf(view->focal_y/(224.f*depth))>gt4_f32(0x3ccccccc))for(unsigned role=0;role<4;role++){
  const gt4_pit_walker*a=c->walking+role;if(!a->enabled || (watched && gt4_pit_actor_model(c,role,cars)!=2))continue;
  unsigned branch;float tween;if(!gt4_pit_walk_sample(a->phase,s->walk_scale[a->model!=0],&branch,&tween) || branch>=4)continue;
  r3d_mesh*gpu=a->model?s->squat:s->walk[branch];if(!gpu)continue;
  /* Both Y rotations use 42654CA3, the original degree conversion. */
  float box_angle=(c->box[3]*gt4_f32(0x42654ca3)*gt4_f32(0x40490fda))/180.f;
  float actor_angle=(a->yaw*gt4_f32(0x42654ca3)*gt4_f32(0x40490fda))/180.f;
  float si=sinf(box_angle),co=cosf(box_angle),ai=sinf(actor_angle),ac=cosf(actor_angle);r3d_view root=*view;
  root.pos[0]=(co*a->position[0]+si*a->position[1])+c->box[0];root.pos[1]=c->box[1];root.pos[2]=(-si*a->position[0]+co*a->position[1])+c->box[2];
  memset(root.rot,0,36);root.rot[0]=root.rot[8]=co*ac-si*ai;root.rot[2]=co*ai+si*ac;root.rot[6]=-root.rot[2];root.rot[4]=1;
  gt4_pit_walker lit=*a;
  for(unsigned endpoint=0;endpoint<2;endpoint++){
   const float*p=endpoint?a->anchor[1].point:a->waiting;float point[3]={c->box[0]+cs*p[0]+sn*p[1],c->box[1]+1,c->box[2]-sn*p[0]+cs*p[1]};gt4_ray_hit surface={0};
   /* 003CCD8C..B4: a missing surface supplies (1,1). */
   lit.light[endpoint*2]=lit.light[endpoint*2+1]=1;
   if(gt4_course_ray_down(collision,&surface,point)){lit.light[endpoint*2]=surface.u;lit.light[endpoint*2+1]=surface.v;}
  }
  float light=gt4_pit_walk_light(&lit);if(!isfinite(light))continue;
  /* 003D51B0 +140..188: 004A60E0 multiply, 004A6100 addition. */
  root.authored_color=root.unlit=1;root.skin_palette=NULL;root.grasp=tween;
  for(unsigned k=0;k<3;k++){root.color_mul[k]=light*gt4_f32(0x3dcccccc);root.color_add[k]=gt4_f32(0x3da1cac0);}
  r3d_draw(gpu,&root);
 }
 r3d_end();fesetenv(&saved);
}
