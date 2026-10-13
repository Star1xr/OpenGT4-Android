#include "course.h"

#include <math.h>
#include <fenv.h>
#include <string.h>
#include <stdlib.h>

static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static float f32(const uint8_t *p) { float f; memcpy(&f, p, 4); return f; }
static int span(size_t n, size_t off, size_t len) { return off <= n && len <= n - off; }

/* Course header: 0x04 world model set, 0xdc runway (GT4Runway.bt). Runway
   header: 0x14 length, 0x3a grid count, 0x46/0x48 road vertex/triangle
   counts, 0x5c grids, 0x74 road vertices (16 bytes), 0x78 road triangles
   (12 bytes: three u16 vertex indices, 2 bytes, u32 flags). Runway offsets
   are relative to the runway. */
int gt4_course_open(gt4_course *c, const uint8_t *d, size_t n)
{
    if (!c) return 0;
    memset(c, 0, sizeof *c);
    if (!d || !span(n, 0, 0x100)) return 0;
    size_t world = u32(d + 0x04), runway = u32(d + 0xdc);
    if (!span(n, world, 128) || memcmp(d + world, "MDLS", 4)) return 0;
    if (!span(n, runway, 0xa0) || memcmp(d + runway, "RNW4", 4)) return 0;
    const uint8_t *r = d + runway;
    c->data = d;
    c->size = n;
    c->world = world;
    c->runway = runway;
    c->length = f32(r + 0x14);
    c->grid_count = u16(r + 0x3a);
    c->road_vertex_count = u16(r + 0x46);
    c->road_tri_count = u16(r + 0x48);
    c->grids = runway + u32(r + 0x5c);
    c->road_vertices = runway + u32(r + 0x74);
    c->road_tris = runway + u32(r + 0x78);
    if (!span(n, c->grids, (size_t)c->grid_count * 16)
        || !span(n, c->road_vertices, (size_t)c->road_vertex_count * 16)
        || !span(n, c->road_tris, (size_t)c->road_tri_count * 12)) {
        memset(c, 0, sizeof *c);
        return 0;
    }
    for (int i = 0; i < c->road_tri_count; i++)
        for (int k = 0; k < 3; k++)
            if (u16(d + c->road_tris + (size_t)i * 12 + k * 2) >= c->road_vertex_count) { memset(c, 0, sizeof *c); return 0; }
    if (!isfinite(c->length) || c->length < 0) goto invalid;
    for (int i=0;i<c->grid_count;i++)
        for(int k=0;k<4;k++)if(!isfinite(f32(d+c->grids+(size_t)i*16+k*4)))goto invalid;
    for (int i=0;i<c->road_vertex_count;i++)
        for(int k=0;k<3;k++)if(!isfinite(f32(d+c->road_vertices+(size_t)i*16+k*4)))goto invalid;
    return 1;
invalid:
    memset(c,0,sizeof *c);return 0;
}

gt4_grid gt4_course_grid(const gt4_course *c, int i)
{
    gt4_grid g = { { 0, 0, 0 }, 0 };
    if (!c || !c->data || i < 0 || i >= c->grid_count) return g;
    const uint8_t *p = c->data + c->grids + (size_t)i * 16;
    for (int k = 0; k < 3; k++) g.pos[k] = f32(p + k * 4);
    g.heading = f32(p + 12);
    return g;
}

int gt4_course_height(const gt4_course *c, float x, float z, float y_limit, float *y)
{
    int found = 0;
    if(!c || !c->data || !y || !isfinite(x) || !isfinite(z) || !isfinite(y_limit))return 0;
    for (int i = 0; i < c->road_tri_count; i++) {
        const uint8_t *t = c->data + c->road_tris + (size_t)i * 12;
        float v[3][3];
        for (int k = 0; k < 3; k++) {
            const uint8_t *p = c->data + c->road_vertices + (size_t)u16(t + k * 2) * 16;
            for (int j = 0; j < 3; j++) v[k][j] = f32(p + j * 4);
        }
        /* barycentric test in the ground plane (x, z) */
        float d = (v[1][2] - v[2][2]) * (v[0][0] - v[2][0]) + (v[2][0] - v[1][0]) * (v[0][2] - v[2][2]);
        if (fabsf(d) < 1e-9f) continue;
        float a = ((v[1][2] - v[2][2]) * (x - v[2][0]) + (v[2][0] - v[1][0]) * (z - v[2][2])) / d;
        float b = ((v[2][2] - v[0][2]) * (x - v[2][0]) + (v[0][0] - v[2][0]) * (z - v[2][2])) / d;
        float g = 1 - a - b;
        if (a < 0 || b < 0 || g < 0) continue;
        float h = a * v[0][1] + b * v[1][1] + g * v[2][1];
        if (h <= y_limit && (!found || h > *y)) { *y = h; found = 1; }
    }
    return found;
}

enum { SURFACE_BUCKETS=8192 };
typedef struct {int tri,next,x,z;} surface_node;
struct gt4_course_surface {gt4_course course;int heads[SURFACE_BUCKETS],large;surface_node *nodes;int count,capacity;};
static unsigned surface_hash(int x,int z)
{return ((uint32_t)x*73856093u^(uint32_t)z*19349663u)&(SURFACE_BUCKETS-1);}
static void surface_vertices(const gt4_course *c,int i,float v[3][3])
{
    const uint8_t *t=c->data+c->road_tris+(size_t)i*12;
    for(int j=0;j<3;j++)for(int k=0;k<3;k++)
        v[j][k]=f32(c->data+c->road_vertices+(size_t)u16(t+j*2)*16+k*4);
}
static int surface_insert(gt4_course_surface *s,int tri,int x,int z,int large)
{
    if(s->count==s->capacity) {
        int cap=s->capacity?s->capacity*2:1024;
        void *p=realloc(s->nodes,(size_t)cap*sizeof *s->nodes);if(!p)return 0;
        s->nodes=p;s->capacity=cap;
    }
    int *head=large?&s->large:&s->heads[surface_hash(x,z)];
    s->nodes[s->count]=(surface_node){tri,*head,x,z};*head=s->count++;return 1;
}
gt4_course_surface *gt4_course_surface_create(const gt4_course *c)
{
    if(!c || !c->data)return NULL;
    gt4_course_surface *s=calloc(1,sizeof *s);if(!s)return NULL;
    s->course=*c;s->large=-1;for(int i=0;i<SURFACE_BUCKETS;i++)s->heads[i]=-1;
    for(int i=0;i<c->road_tri_count;i++) {
        float v[3][3];surface_vertices(c,i,v);
        float lx=fminf(v[0][0],fminf(v[1][0],v[2][0])),hx=fmaxf(v[0][0],fmaxf(v[1][0],v[2][0]));
        float lz=fminf(v[0][2],fminf(v[1][2],v[2][2])),hz=fmaxf(v[0][2],fmaxf(v[1][2],v[2][2]));
        if(fabsf(lx)>1e7f || fabsf(hx)>1e7f || fabsf(lz)>1e7f || fabsf(hz)>1e7f)goto large;
        int x0=(int)floorf(lx/16),x1=(int)floorf(hx/16),z0=(int)floorf(lz/16),z1=(int)floorf(hz/16);
        if((int64_t)(x1-x0+1)*(z1-z0+1)>64)goto large;
        for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++)if(!surface_insert(s,i,x,z,0))goto fail;
        continue;
large:  if(!surface_insert(s,i,0,0,1))goto fail;
    }
    return s;
fail:gt4_course_surface_free(s);return NULL;
}
void gt4_course_surface_free(gt4_course_surface *s){if(s){free(s->nodes);free(s);}}
int gt4_course_surface_at(const gt4_course_surface *s,float x,float z,float ceiling,float *y,float normal[3])
{return gt4_course_surface_triangle(s,x,z,ceiling,y,normal,NULL);}
int gt4_course_surface_triangle(const gt4_course_surface *s,float x,float z,float ceiling,float *y,float normal[3],int *triangle)
{
    if(!s || !y || !isfinite(x) || !isfinite(z) || !isfinite(ceiling) || fabsf(x)>1e7f || fabsf(z)>1e7f)return 0;
    int cx=(int)floorf(x/16),cz=(int)floorf(z/16),found=0;
    for(int chain=0;chain<2;chain++)for(int at=chain?s->large:s->heads[surface_hash(cx,cz)];at>=0;at=s->nodes[at].next) {
        const surface_node *node=&s->nodes[at];if(!chain && (node->x!=cx || node->z!=cz))continue;
        float v[3][3];surface_vertices(&s->course,node->tri,v);
        float d=(v[1][2]-v[2][2])*(v[0][0]-v[2][0])+(v[2][0]-v[1][0])*(v[0][2]-v[2][2]);
        if(!isfinite(d) || fabsf(d)<1e-9f)continue;
        float a=((v[1][2]-v[2][2])*(x-v[2][0])+(v[2][0]-v[1][0])*(z-v[2][2]))/d;
        float b=((v[2][2]-v[0][2])*(x-v[2][0])+(v[0][0]-v[2][0])*(z-v[2][2]))/d,g=1-a-b;
        if(a<0 || b<0 || g<0)continue;
        float h=a*v[0][1]+b*v[1][1]+g*v[2][1];if(!isfinite(h) || h>ceiling || (found && h<=*y))continue;
        *y=h;found=1;if(triangle)*triangle=node->tri;
        if(normal) {
            float e[3],f[3];for(int k=0;k<3;k++){e[k]=v[1][k]-v[0][k];f[k]=v[2][k]-v[0][k];}
            normal[0]=e[1]*f[2]-e[2]*f[1];normal[1]=e[2]*f[0]-e[0]*f[2];normal[2]=e[0]*f[1]-e[1]*f[0];
            float len=hypotf(hypotf(normal[0],normal[1]),normal[2]);
            if(!isfinite(len) || len<1e-12f){normal[0]=normal[2]=0;normal[1]=1;}
            else {if(normal[1]<0)len=-len;for(int k=0;k<3;k++)normal[k]/=len;}
        }
    }
    return found;
}

int gt4_course_vision_open(gt4_course_vision *v,const uint8_t *d,size_t n,unsigned models)
{
 if(!v)return 0;memset(v,0,sizeof *v);
 if(!d || n<0x38 || memcmp(d,"Vls0",4) || !models)return 0;
 unsigned nx=u16(d+0x10),nz=u16(d+0x12),count=u32(d+0x30);size_t grid=u32(d+0x14),nodes=u32(d+0x34);
 if(!nx || !nz || !count || !span(n,grid,(size_t)nx*nz*4) || !span(n,nodes,(size_t)count*16))return 0;
 for(unsigned i=0;i<6;i++)if(!isfinite(f32(d+0x18+i*4)))return 0;
 for(unsigned i=0;i<count;i++) {
  const uint8_t *node=d+nodes+i*16;size_t list=u32(node);
  for(unsigned k=1;k<4;k++)if(!isfinite(f32(node+k*4)))return 0;
  if(!span(n,list,2))return 0;unsigned entries=u16(d+list);
  if(!span(n,list+2,(size_t)entries*2))return 0;
  for(unsigned k=0;k<entries;k++)if(u16(d+list+2+k*2)>=models)return 0;
 }
 /* 00458008 +30..+88 relocates the null-terminated grid candidate arrays;
    +A8..+CC relocates the node's model list. Native buffers stay immutable. */
 for(unsigned cell=0;cell<nx*nz;cell++) {
  size_t candidates=u32(d+grid+cell*4);if(!candidates)return 0;
  unsigned seen=0;for(;;candidates+=4) {
   if(!span(n,candidates,4))return 0;size_t node=u32(d+candidates);if(!node)break;
   if(node<nodes || node-nodes>=(size_t)count*16 || (node-nodes)%16 || ++seen>count)return 0;
  }
  if(!seen)return 0;
 }
 v->data=d;v->size=n;v->models=models;return 1;
}
const uint8_t *gt4_course_vision_select(const gt4_course_vision *v,const float p[3],unsigned *index)
{
 if(!v || !v->data || !p || !isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2]))return NULL;
 const uint8_t *d=v->data;unsigned nx=u16(d+16),nz=u16(d+18);fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 /* 004580F0 +00..+88: (position-origin)*cell_count*reciprocal_extent,
    cvt.w.s truncation, then clamp to grid. Preserve scalar operation order. */
 float x=p[0]-f32(d+24),z=p[2]-f32(d+28);x=x*(float)nx;z=z*(float)nz;x=x*f32(d+40);z=z*f32(d+44);
 int ix=x<=0?0:x>=(float)(nx-1)?(int)nx-1:(int)x,iz=z<=0?0:z>=(float)(nz-1)?(int)nz-1:(int)z;
 size_t candidates=u32(d+u32(d+20)+((size_t)iz*nx+ix)*4),best=u32(d+candidates);
 /* 004580F0 +90..+F8: first candidate wins ties; choose nearest in XYZ.
    D_006AB064 is FLT_MAX. No distance threshold or LOD tuning is added. */
 float distance=3.4028234663852886e38f;
 for(;u32(d+candidates);candidates+=4) {
  size_t at=u32(d+candidates);float dx=f32(d+at+4)-p[0],dy=f32(d+at+8)-p[1],dz=f32(d+at+12)-p[2];
  dx=dx*dx;dy=dy*dy;dz=dz*dz;float sum=dx+dy;sum=sum+dz;
  if(sum<distance){distance=sum;best=at;}
 }
 fesetenv(&env);if(index)*index=(unsigned)((best-u32(d+52))/16);return d+u32(d+best);
}
