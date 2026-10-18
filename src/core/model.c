/* Native, bounded MDLS/CAR4 geometry conversion. Layout cross-checked with
   Nenkai's MIT PDTools (see docs/MODELS.md). Converts asset data once. */
#include "model.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned u32(const uint8_t *p){return u16(p)|u16(p+2)<<16;}
static float fl(const uint8_t *p){float f;memcpy(&f,p,4);return f;}
static int span(size_t n,size_t p,size_t s){return p<=n && s<=n-p;}
#define WORLD_FIXED (1/500.f)   /* course world "compressed" positions */
typedef struct {const uint8_t *d;size_t n;gt4_model *out;int *texbase;unsigned nt,ns,nm;int car;gt4_model_stats *stats;int nesting;float fixed;int human;
    unsigned hand_tween,hand_branch;int tween_active,wheel,wheel_table,wing,branch;float *radial_bounds;} reader;
static int append(gt4_model *m,const gt4_vertex *a,const gt4_vertex *b,const gt4_vertex *c,int texture)
{
    if(m->count>=4000000)return 0;   /* a whole course world set is ~1e6 */
    if(m->count==m->capacity){int cap=m->capacity?m->capacity*2:2048;void *p=realloc(m->triangles,(size_t)cap*sizeof *m->triangles);if(!p)return 0;m->triangles=p;m->capacity=cap;}
    if(m->skin && m->skin_capacity<m->capacity) {
        void *p=realloc(m->skin,(size_t)m->capacity*sizeof *m->skin);if(!p)return 0;
        m->skin=p;memset(m->skin+m->skin_capacity,0,(size_t)(m->capacity-m->skin_capacity)*sizeof *m->skin);m->skin_capacity=m->capacity;
    }
    if(m->skin)memset(&m->skin[m->count],0,sizeof *m->skin);
    if(m->morph && m->morph_capacity<m->capacity) {
        void *p=realloc(m->morph,(size_t)m->capacity*sizeof *m->morph);if(!p)return 0;
        m->morph=p;m->morph_capacity=m->capacity;
    }
    gt4_triangle *t=&m->triangles[m->count++];t->v[0]=*a;t->v[1]=*b;t->v[2]=*c;t->texture=texture;t->kind=1;t->blend_enabled=0;t->attachment=0;t->state=(gt4_draw_state){0,0x44,0,1,1,1,6,32,0,1,1,3,0};
    /* A newly assembled stream belongs to model 0. decode() assigns the
       authored MDLS index after each command stream (00455598); static
       attachments flatten into the destination stream, not a Vls0 list.
       Never retain the allocator's bytes here: the GPU validates this index. */
    t->source_model=0;t->wheel_blur_texture=0;t->lamp=0;
    if(m->morph)for(int k=0;k<3;k++){memcpy(m->morph[m->count-1].v[k].p,t->v[k].p,12);memcpy(m->morph[m->count-1].v[k].normal,t->v[k].normal,12);memcpy(m->morph[m->count-1].v[k].uv,t->v[k].uv,8);}return 1;
}
static int shape(reader *r,size_t a,int table);
static int commands(reader *r,size_t p,int *table);
/* Conversion failure: remembered for diagnostics (gt4model_survey, gt4course). */
#define FAIL do{if(r->stats && !r->stats->fail_line)r->stats->fail_line=__LINE__;return 0;}while(0)

static int skin_triangle(gt4_model *m,const gt4_skin_vertex *a,const gt4_skin_vertex *b,const gt4_skin_vertex *c)
{
    if(m->skin_capacity<m->capacity) {
        void *p=realloc(m->skin,(size_t)m->capacity*sizeof *m->skin);if(!p)return 0;
        m->skin=p;memset(m->skin+m->skin_capacity,0,(size_t)(m->capacity-m->skin_capacity)*sizeof *m->skin);
        m->skin_capacity=m->capacity;
    }
    gt4_skin_triangle *s=&m->skin[m->count-1];s->v[0]=*a;s->v[1]=*b;s->v[2]=*c;return 1;
}

/* 0049DA60 dispatches the block immediately after a packet; 0049DCB0
   reads quantized position/normal/weight groups, 0049D728 the float form.
   A group's four bone slots are loaded by 0049D970. This is asset decoding,
   performed once, without executing the game's EE/VU instruction stream. */
static int skin_packet(reader *r,size_t p,uint8_t flags,gt4_vertex v[256],gt4_skin_vertex s[256],int *nv)
{
    int quant=(flags&0x20)!=0;unsigned n,normals;
    unsigned slots=u16(r->d+46);
    if(!slots || slots>20)FAIL;
    float scale=1;
    if(quant) {
        if(!span(r->n,p,6))FAIL;scale=fl(r->d+p);n=r->d[p+4];normals=r->d[p+5];p+=6;
        if(!isfinite(scale) || scale<=0 || scale>1)FAIL;
    } else {
        if(!span(r->n,p,4))FAIL;n=u16(r->d+p);normals=u16(r->d+p+2);p+=4;
    }
    if(!n || n>256)FAIL;
    unsigned at=0;
    for(unsigned budget=0;budget<256;budget++) {
        unsigned header;
        if(!span(r->n,p,quant?6:8))FAIL;
        header=quant?r->d[p]:u16(r->d+p);p+=quant?2:4;
        unsigned count=header&(quant?0x7f:0x7fff),again=header&(quant?0x80:0x8000);
        uint8_t bone[4];memcpy(bone,r->d+p,4);p+=4;
        for(int k=0;k<4;k++)if(bone[k]>=slots)FAIL;
        size_t stride=quant?(normals?13:10):(normals?40:28);
        if(count>n-at || !span(r->n,p,(size_t)count*stride))FAIL;
        for(unsigned i=0;i<count;i++,p+=stride,at++) {
            gt4_vertex *dst=&v[at];gt4_skin_vertex *skin=&s[at];
            memcpy(skin->bone,bone,4);
            for(int k=0;k<3;k++) {
                dst->p[k]=quant?(int16_t)u16(r->d+p+k*2)*scale:fl(r->d+p+k*4);
                if(!isfinite(dst->p[k]) || fabsf(dst->p[k])>100000)FAIL;
                if(normals) {
                    dst->normal[k]=quant?(int8_t)r->d[p+6+k]/127.f:fl(r->d+p+12+k*4);
                    if(!isfinite(dst->normal[k]) || fabsf(dst->normal[k])>10000)FAIL;
                }
            }
            size_t w=p+(quant?(normals?9:6):(normals?24:12));float sum=0;
            for(int k=0;k<4;k++) {
                float weight=quant?(int8_t)r->d[w+k]:fl(r->d+w+k*4);
                if(!isfinite(weight) || weight<0)FAIL;
                /* VU0 0x2E8/0x460 converts signed bytes to float and multiplies
                   by I=0x3C010204 (1/127); float path 0x308 uses authored weights.
                   Do not renormalize the file and change its influences. */
                skin->weight[k]=quant?weight*.007874015718698502f:weight;sum+=skin->weight[k];
            }
            if(!isfinite(sum) || !(sum>0))FAIL;
        }
        if(!again){if(at!=n)FAIL;*nv=(int)n;return 1;}
    }
    FAIL;
}

/* Model matrix commands (pglPushMatrix..pglRotateZ) follow OpenGL: column-major
   4x4, each command multiplies the current matrix on the right, angles in
   degrees. The disc data agrees: multMatrix carries its translation in
   elements 12-14 (e.g. a steering wheel at x -0.30, y 0.63, rotateX -20.5). */
typedef struct {float m[16];} mat4;
static const mat4 identity4={{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};
static mat4 mat_mul(const mat4 *a,const mat4 *b)
{
    mat4 o;
    for(int c=0;c<4;c++)for(int r=0;r<4;r++) {
        float v=0;for(int k=0;k<4;k++)v+=a->m[k*4+r]*b->m[c*4+k];
        o.m[c*4+r]=v;
    }
    return o;
}
static mat4 mat_rotate(float degrees,float x,float y,float z)
{
    float len=sqrtf(x*x+y*y+z*z);mat4 o=identity4;
    if(!(len>1e-12f) || !isfinite(degrees))return o;
    x/=len;y/=len;z/=len;
    float a=degrees*0.01745329252f,c=cosf(a),s=sinf(a),t=1-c;
    o.m[0]=t*x*x+c;   o.m[1]=t*x*y+s*z; o.m[2]=t*x*z-s*y;
    o.m[4]=t*x*y-s*z; o.m[5]=t*y*y+c;   o.m[6]=t*y*z+s*x;
    o.m[8]=t*x*z+s*y; o.m[9]=t*y*z-s*x; o.m[10]=t*z*z+c;
    return o;
}
static int mat_is_identity(const mat4 *a){return !memcmp(a,&identity4,sizeof *a);}
/* Positions by M; normals by the inverse transpose (cofactors), renormalized;
   a mirroring matrix reverses the winding as gt4_model_attach does. */
static void transform_triangles(gt4_model *out,int first,const mat4 *M)
{
    const float *m=M->m;
    float cof[9]={m[5]*m[10]-m[6]*m[9], m[6]*m[8]-m[4]*m[10], m[4]*m[9]-m[5]*m[8],
                  m[2]*m[9]-m[1]*m[10], m[0]*m[10]-m[2]*m[8], m[1]*m[8]-m[0]*m[9],
                  m[1]*m[6]-m[2]*m[5], m[2]*m[4]-m[0]*m[6], m[0]*m[5]-m[1]*m[4]};
    float det=m[0]*cof[0]+m[4]*cof[3]+m[8]*cof[6];
    for(int i=first;i<out->count;i++) {
        gt4_triangle *t=&out->triangles[i];
        for(int v=0;v<3;v++)for(int endpoint=0;endpoint<(out->morph?2:1);endpoint++) {
            float *q=endpoint?out->morph[i].v[v].p:t->v[v].p,x=q[0],y=q[1],z=q[2];
            for(int k=0;k<3;k++)q[k]=m[k]*x+m[4+k]*y+m[8+k]*z+m[12+k];
            float *n=endpoint?out->morph[i].v[v].normal:t->v[v].normal,nx=n[0],ny=n[1],nz=n[2],l;
            for(int k=0;k<3;k++)n[k]=cof[k]*nx+cof[3+k]*ny+cof[6+k]*nz;   /* cof[j*3+k] = C(k,j) */
            l=sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
            if(l>1e-12f)for(int k=0;k<3;k++)n[k]/=det<0?-l:l;   /* cofactors / det keep the sign */
        }
        if(det<0){gt4_vertex tmp=t->v[1];t->v[1]=t->v[2];t->v[2]=tmp;
            if(out->skin){gt4_skin_vertex st=out->skin[i].v[1];out->skin[i].v[1]=out->skin[i].v[2];out->skin[i].v[2]=st;}}
        if(det<0 && out->morph){gt4_morph_vertex tmp=out->morph[i].v[1];out->morph[i].v[1]=out->morph[i].v[2];out->morph[i].v[2]=tmp;}
    }
}

/* Convert each authored draw in order, retaining its native render state. */
static int commands(reader *r,size_t p,int *table)
{
    gt4_draw_state state={0,0x44,0,1,1,1,6,32,0,1,1,3,0};
    uint8_t saved_fog[4]={0};   /* command 34's copy of the fog colour (00455708 sp+0x5C) */
    mat4 stack[32];int depth=0;stack[0]=identity4;
    int blur_table=-1;
    uint8_t attachment[32]={0};
    /* CarModel::Callback 00453698, entry 0: the car's +0x38, set each frame
       by 00390C90 -> 00451370 from 003446B8 (brake pedal +0x590 above 0).
       Branch 0 draws the released lamps, branch 1 the lit ones; both are
       kept, tagged, branch 1 read after branch 0 jumps past it to their
       join, from the state the callback was entered with (only one branch
       runs on the PS2); at the join, the state branch 0 left. */
    /* Culling is two switches of the draw context: commands 45/46 turn it
       on and off (00456170/00456180: 004A1638/004AB040 with 0Fh), 78/79
       pick the side (00456190/004561A0: 004A19B0 stores 2 or 1 at
       70002000h +0x2C8) and 64 swaps the side (00456288, no argument).
       A side picked with culling off culls nothing (Integra Type R '04,
       hond0036, picks sides and never turns it on). The scenes set 2
       before drawing (0015689C, 001D5CAC, 00273A94) and the car's mirror
       swaps it (0045390C): 2 culls back faces, 1 front faces. The HKS
       Genki Hyper Silvia (hks_0001) draws its right wheels with 79 (1)
       and goes back to 78 (2) for the body. state.cull keeps the
       renderer's meaning: 1 back, 2 front. */
    uint8_t cull_on=0,cull_side=2;
#define CULL_FACES(side) ((uint8_t)((side)==2?1:2))
    typedef struct {gt4_draw_state state;mat4 top;int depth,table,blur_table;uint8_t attachment,fog[4],cull_on,cull_side;} lamp_ctx;
    uint8_t lamp=0;size_t lamp_pressed=0,lamp_join=0;lamp_ctx lamp_entry,lamp_after;
#define LAMP_SAVE(c) ((c).cull_on=cull_on,(c).cull_side=cull_side,(c).state=state,(c).top=stack[depth],(c).depth=depth,(c).table=*table,(c).blur_table=blur_table,(c).attachment=attachment[depth],memcpy((c).fog,saved_fog,4))
#define LAMP_LOAD(c) (cull_on=(c).cull_on,cull_side=(c).cull_side,state=(c).state,depth=(c).depth,stack[depth]=(c).top,*table=(c).table,blur_table=(c).blur_table,attachment[depth]=(c).attachment,memcpy(saved_fog,(c).fog,4))
    for(int budget=0;budget<8192 && span(r->n,p,1);budget++) {
        /* 00455708 executes callback-selected jumps as control flow too.
           CAR4 daih0002, MDLS +7C1 brake callback: released +7CB nests
           callback7, whose selected target +7FD skips pressed +7EB.
           The join is reached through opcode6, not necessarily opcode8/9.
           Recognize it for every jump before tagging shared body shapes. */
        if(lamp==1 && p>lamp_pressed){lamp_join=p;LAMP_SAVE(lamp_after);LAMP_LOAD(lamp_entry);lamp=2;p=lamp_pressed;}
        if(lamp==2 && p==lamp_join){LAMP_LOAD(lamp_after);lamp=0;}   /* the join: on as after branch 0 */
        if(lamp==1 && p==lamp_pressed)lamp=0;   /* branch 0 ran into branch 1: no join to find */
        unsigned op=r->d[p++],arg=0,skip=0;
        if(r->stats) {
            r->stats->ops[op]++;
            if((op==6 || op==77) && span(r->n,p,2))r->stats->callbacks[u16(r->d+p)&255]++;
            int nargs=op==16||op==15?16:op==17||op==18?3:op==19?4:op>=20&&op<=22?1:0;
            if(nargs && r->stats->nsamples<8 && span(r->n,p,(size_t)nargs*4)) {
                float *s=r->stats->samples[r->stats->nsamples++];s[0]=(float)op;
                for(int k=0;k<nargs;k++)s[1+k]=fl(r->d+p+k*4);
            }
        }
        switch(op) {
        case 0:return 1;
        case 4:case 5:
            skip=op==4?1:2;if(!span(r->n,p,skip))FAIL;
            arg=skip==1?r->d[p]:u16(r->d+p);
            if(arg>=r->ns || *table<-1 || *table>=(int)r->nt)FAIL;
            {
                int first=r->out->count;
                if(!shape(r,u32(r->d+u32(r->d+60)+arg*4),*table))FAIL;
                for(int i=first;i<r->out->count;i++) {
                    gt4_triangle *t=&r->out->triangles[i];t->state=state;t->attachment=attachment[depth];t->lamp=lamp;
                    /* WheelContext 00453F04..00453F38: mode 1 selects the
                       adjacent authored Tex1 bank. Keep both resident; only
                       external slot 2 draws switch, never the barrel/env. */
                    if(blur_table>=0 && t->texture>=0 && r->texbase[*table]>=0 && r->texbase[blur_table]>=0) {
                        int alt=r->texbase[blur_table]+t->texture-r->texbase[*table];
                        int end=r->out->ntextures;
                        for(int bank=blur_table+1;bank<(int)r->nt;bank++)if(r->texbase[bank]>=0){end=r->texbase[bank];break;}
                        if(alt>=r->texbase[blur_table] && alt<end)t->wheel_blur_texture=alt+1;
                    }
                }
                if(!mat_is_identity(&stack[depth]))transform_triangles(r->out,first,&stack[depth]);
            }
            break;
        case 2:case 3: {
            /* RenderModel: draw another model of this set under the current
               matrix. Not met on the US disc once VM command sizes are right. */
            skip=op==2?1:2;if(!span(r->n,p,skip))FAIL;
            arg=skip==1?r->d[p]:u16(r->d+p);
            unsigned models=u16(r->d+22);size_t mo=u32(r->d+56);
            if(arg>=models || !span(r->n,mo,(size_t)(arg+1)*40) || r->nesting>=8)FAIL;
            int first=r->out->count,sub_table=*table;
            r->nesting++;
            int ok=commands(r,u32(r->d+mo+arg*40+24),&sub_table);
            r->nesting--;
            if(!ok)FAIL;
            if(!mat_is_identity(&stack[depth]))transform_triangles(r->out,first,&stack[depth]);
            break;
        }
        case 12:if(depth+1>=32)FAIL;stack[depth+1]=stack[depth];attachment[depth+1]=attachment[depth];depth++;break;
        case 13:if(depth>0)depth--;break;   /* unbalanced pop: keep the base matrix */
        case 15:case 16: {
            if(!span(r->n,p,64))FAIL;mat4 a;
            for(int k=0;k<16;k++){a.m[k]=fl(r->d+p+k*4);if(!isfinite(a.m[k]))FAIL;}
            stack[depth]=op==15?a:mat_mul(&stack[depth],&a);
            if(op==15)attachment[depth]=0; /* load replaces callback transforms */
            skip=64;break;
        }
        case 17:case 18: {
            if(!span(r->n,p,12))FAIL;mat4 a=identity4;float v[3];
            for(int k=0;k<3;k++){v[k]=fl(r->d+p+k*4);if(!isfinite(v[k]))FAIL;}
            if(op==17){a.m[12]=v[0];a.m[13]=v[1];a.m[14]=v[2];}
            else{a.m[0]=v[0];a.m[5]=v[1];a.m[10]=v[2];}
            stack[depth]=mat_mul(&stack[depth],&a);skip=12;break;
        }
        case 19: {
            if(!span(r->n,p,16))FAIL;
            mat4 a=mat_rotate(fl(r->d+p),fl(r->d+p+4),fl(r->d+p+8),fl(r->d+p+12));
            stack[depth]=mat_mul(&stack[depth],&a);skip=16;break;
        }
        case 20:case 21:case 22: {
            if(!span(r->n,p,4))FAIL;
            mat4 a=mat_rotate(fl(r->d+p),op==20,op==21,op==22);
            stack[depth]=mat_mul(&stack[depth],&a);skip=4;break;
        }
        case 43:case 44:skip=op==43?1:2;if(!span(r->n,p,skip))FAIL;*table=skip==1?r->d[p]:u16(r->d+p);blur_table=-1;break;
        case 80:case 81:
            skip=op==80?1:2;if(!span(r->n,p,skip))FAIL;
            arg=skip==1?r->d[p]:u16(r->d+p);*table=-1;blur_table=-1;
            /* 00453E68 -> 004568F0 binds the authored static rim Tex1
               to external slot 2. Slot 0 is the environment reflection.
               Do not replace the rim face with an untextured polygon. */
            if(r->wheel && arg==2){if(r->wheel_table<0 || r->wheel_table>=(int)r->nt)FAIL;*table=r->wheel_table;if(*table+1<(int)r->nt)blur_table=*table+1;}
            break;
        case 8:case 9:
            skip=op==8?1:2;if(!span(r->n,p,skip))FAIL;arg=skip==1?r->d[p]:u16(r->d+p);if(arg<skip)FAIL;p+=arg;
            continue;
        case 6:case 77:
            if(!span(r->n,p,3))FAIL;
            skip=3+r->d[p+2]*2;
            if(!span(r->n,p,skip))FAIL;
            if(r->human>=2 && op==77){
                if(u16(r->d+p)!=r->hand_branch || !r->d[p+2] || (r->human==2 && r->d[p+2]!=1))FAIL;
                unsigned branch=r->human==3?(unsigned)r->branch:0;
                if(branch>=r->d[p+2])FAIL;size_t entry=p+3+branch*2;
                if(!span(r->n,entry,u16(r->d+entry)+1))FAIL;
                p=entry+u16(r->d+entry);continue;
            }
            if(r->wheel && op==6)r->wheel_table=(int)u16(r->d+p);
            /* 00453698's jump-table entry 8 -> 00453758 rotates local Z.
               Keep the callback frame, including its authored pivot/tilt;
               rotating about the car origin would swing the whole cabin. */
            if(r->car && op==6 && u16(r->d+p)==8) {
                unsigned frame=0;
                while(frame<r->out->steering_count && memcmp(r->out->steering_frame[frame],stack[depth].m,sizeof stack[depth].m))frame++;
                if(frame==r->out->steering_count) {
                    if(frame>=GT4_MODEL_STEERING_MAX)FAIL;
                    memcpy(r->out->steering_frame[frame],stack[depth].m,sizeof stack[depth].m);r->out->steering_count++;
                }
                attachment[depth]=(uint8_t)(5+frame);
            }
            /* 00453698 callback 47 -> 00453950 reads installed wing
               flag +0x80, set by 00453488. Select authored support branch. */
            if(r->car && op==6 && u16(r->d+p)==47 && r->d[p+2]>(unsigned)r->wing){
                size_t entry=p+3+(unsigned)r->wing*2,offset=u16(r->d+entry);
                if(offset<skip-(entry-p) || !span(r->n,entry,offset+1))FAIL;
                p=entry+offset;continue;
            }
            /* Car callbacks return 0 for daytime and released brake lamps.
               The fall-through is the out-of-range/default path, not branch 0. */
            /* CarModel::Callback 00453698, +100: 11..14 enter the wheel
               frame (00454110) and return visibility. Branch 0 draws the
               authored brakes, branch 1 skips them. Keep their frame tag
               for car_model's static pose and the race's steered calipers. */
            if(r->car && op==6 && u16(r->d+p)>=11 && u16(r->d+p)<=14)
                attachment[depth]=(uint8_t)(u16(r->d+p)-10);
            if(r->car && op==6 && u16(r->d+p)==0 && r->d[p+2]>=2 && !lamp) {
                size_t entry=p+3,pressed=p+5;
                if(u16(r->d+entry)<skip-3 || u16(r->d+pressed)<skip-5 || !span(r->n,pressed,u16(r->d+pressed)+1))FAIL;
                lamp=1;lamp_pressed=pressed+u16(r->d+pressed);p=entry+u16(r->d+entry);LAMP_SAVE(lamp_entry);continue;
            }
            if(!r->car && op==6 && r->branch>=0 && r->branch<r->d[p+2]) {   /* the branch asked for */
                size_t entry=p+3+(size_t)r->branch*2;
                if(!span(r->n,entry,2) || !span(r->n,entry,u16(r->d+entry)+1))FAIL;
                p=entry+u16(r->d+entry);continue;
            }
            if(((r->car && op==6 && (u16(r->d+p)==7 || u16(r->d+p)==0 ||
               u16(r->d+p)==8 || (u16(r->d+p)>=11 && u16(r->d+p)<=14))) ||
               /* 00453028 -> 00455598 submits the rim with WheelContext
                  (vtable 006885F0), whose callback is 00453E68. Its
                  argument selects the rim Tex1 bank; it returns branch 0.
                  Default mode ctx+8=0 uses the authored static bank.
                  Mode 1's adjacent blur bank is retained above. Modes >1
                  require the separate 00453BB0 accumulation target. */
               (r->wheel && op==6) ||
               (r->human && op==77)) && r->d[p+2]) {
                size_t entry=p+3,offset=u16(r->d+entry);
                if(offset<skip-3 || !span(r->n,entry,offset+1))FAIL;
                p=entry+offset;continue;
            }
            break;
        case 7:
            if(!span(r->n,p,19))FAIL;p+=17;p+=u16(r->d+p);continue;
        case 10:if(!span(r->n,p,1))FAIL;skip=1+r->d[p]*12+2;break;
        case 45:cull_on=1;state.cull=CULL_FACES(cull_side);break;case 46:cull_on=0;state.cull=0;break;
        case 78:case 79:cull_side=op==78?2:1;state.cull=cull_on?CULL_FACES(cull_side):0;break;
        case 64:cull_side=(uint8_t)(3-cull_side);state.cull=cull_on?CULL_FACES(cull_side):0;break;
        case 25:if(!span(r->n,p,1))FAIL;state.depth_func=r->d[p];skip=1;break;
        case 47:if(!span(r->n,p,1))FAIL;state.alpha_fail=r->d[p];skip=1;break;
        case 23:state.depth_test=1;break;case 24:state.depth_test=0;break;
        case 26:state.alpha_test=1;break;case 27:state.alpha_test=0;break;
        case 29:state.destination_test=1;break;case 30:state.destination_test=0;break;
        case 37:state.depth_write=0;break;case 38:state.depth_write=1;break;
        case 31:if(!span(r->n,p,1))FAIL;state.destination_bit=r->d[p]==5;skip=1;break;
        case 28:if(!span(r->n,p,2))FAIL;state.alpha_func=r->d[p];state.alpha_ref=r->d[p+1];skip=2;break;
        case 32:if(!span(r->n,p,2))FAIL;state.blend=r->d[p];state.fixed_alpha=r->d[p+1];skip=2;break;
        case 36:if(!span(r->n,p,4))FAIL;state.color_mask=u32(r->d+p);skip=4;break;
        case 14:case 41:case 42:case 89:skip=1;break;
        /* VM_* commands name VM output registers (u16); sizes per PDTools. */
        case 76:
            if(!span(r->n,p,2))FAIL;
            if(r->human>=2){if(u16(r->d+p)!=r->hand_tween)FAIL;r->tween_active=1;}
            skip=2;break;
        case 90:case 68:case 72:case 73:case 74:case 75:skip=2;break;
        case 84:case 85:skip=8;break;
        /* 33: the fog colour (004A4050: r, g, b bytes); 34 keeps the
           current one (004A7C00), 35 puts it back */
        case 33:if(!span(r->n,p,4))FAIL;state.fog_override=1;memcpy(state.fog_rgb,r->d+p,3);skip=4;break;
        case 34:saved_fog[0]=state.fog_override;memcpy(saved_fog+1,state.fog_rgb,3);break;
        case 35:state.fog_override=saved_fog[0];memcpy(state.fog_rgb,saved_fog+1,3);break;
        case 39:case 49:case 50:case 52:case 54:skip=4;break;
        case 69:case 70:skip=6;break;
        case 40:case 51:case 55:case 82:case 83:skip=16;break;
        case 11:case 67:case 86:case 87:case 88:break;
        default:
            r->out->unsupported++;
            if(r->stats){r->stats->fail_op=(int)op;r->stats->fail_offset=p-1;}
            FAIL;
        }
        if(!span(r->n,p,skip))FAIL;p+=skip;
    }
    if(r->stats && r->stats->fail_op==-1)r->stats->fail_op=-2;
    FAIL;
#undef LAMP_SAVE
#undef LAMP_LOAD
}
static int shape(reader *r,size_t a,int table)
{
    if(!span(r->n,a,16))FAIL;
    unsigned np=u16(r->d+a+10),kind=r->d[a+8]&31;
    /* The tween VU path stores endpoint 0/1 at 00/20, 80/A0, 40/60.
       Unlike ordinary geometry, the second 32-slot block is not another
       strip. Retain it only after the hand's VM_pgluShapeTweenRatio. */
    unsigned tween_mode=r->d[a+8]>>5;
    int tween=r->human>=2 && r->tween_active && (tween_mode==2 || (r->human==3 && tween_mode==1));
    if(r->human>=2 && !tween)FAIL;
    /* 0049D328 -> 0049E548 scales the three model-matrix columns by
       shape+4, preserving translation. Compressed rigid pilot parts carry
       their own scale (helmet: 5/32767), distinct from car/course units. */
    float authored_scale=fl(r->d+a+4);
    if(!isfinite(authored_scale) || authored_scale<0 || authored_scale>1)FAIL;
    if(!span(r->n,a+16,(size_t)np*8))FAIL;
    /* Keep the authored masks and environment geometry in draw order. */

    for(unsigned j=0;j<np;j++) {
        size_t desc=a+16+j*8,p=a+(size_t)u32(r->d+desc),len=u16(r->d+desc+4)*16,end=p+len;
        if(!span(r->n,p,len))FAIL;
        unsigned flags=u16(r->d+desc+6),mat=(flags>>9)-1,tex=flags&511;
        gt4_vertex v[256]={0};gt4_skin_vertex skin[256]={0};int nv=0,blend_enabled=0;uint8_t reset[256];unsigned nr=0;
        uint32_t tween_stream[3][2]={{0}};
        int32_t row[4]={0,0,0,0};unsigned mode=0;   /* VIF STROW/STMOD state for this packet list */
        float diffuse[3]={1,1,1};size_t material=u32(r->d+64)+(size_t)mat*80;
        if(mat<r->nm && span(r->n,material,32))for(int k=0;k<3;k++){
            diffuse[k]=fl(r->d+material+16+k*4);if(!isfinite(diffuse[k]))FAIL;
        }
        for(int k=0;k<256;k++) {
            v[k].alpha=1;v[k].shine=16;
            for(int c=0;c<3;c++){v[k].rgb[c]=diffuse[c];v[k].ambient[c]=1;}
            if(mat<r->nm && span(r->n,material,80)) {
                for(int c=0;c<3;c++) {
                    float amb=fl(r->d+material+c*4),spec=fl(r->d+material+32+c*4);
                    if(!isfinite(amb) || !isfinite(spec))FAIL;
                    /* 0049E228 +14..20/+9C..AC loads and retains authored
                       float colours; 0049F038 +74..80 multiplies them by
                       light colours before vertex saturation. Pit cloth
                       and helmets have ambient 1.2..1.5, not unit clamps. */
                    v[k].ambient[c]=amb;v[k].specular[c]=spec;
                }
                float alpha=fl(r->d+material+28),shine=fl(r->d+material+64);
                if(!isfinite(alpha) || !isfinite(shine))FAIL;
                /* Diffuse alpha is not opacity unless the draw state enables it. */v[k].shine=fmaxf(1,fminf(128,shine));
            }
        }
        while(span(end,p,4)) {
            unsigned addr=u16(r->d+p),num=r->d[p+2],op=r->d[p+3]&127;p+=4;size_t bytes=0;
            if(addr==0xc0c0){bytes=12;if(!span(end,p,bytes))FAIL;blend_enabled=(u32(r->d+p+4)>>21)&1;}
            else if(op==0x30){bytes=16;if(!span(end,p,bytes))FAIL;for(int k=0;k<4;k++)row[k]=(int32_t)u32(r->d+p+k*4);}
            else if(op==0x31)bytes=16;
            else if(op==0x05)mode=addr&3;          /* STMOD: 0 plain, 1 offset by row, 2 accumulate */
            else if(op==0x20)bytes=4;
            else if((op&0x60)==0x60) {
                unsigned f=op&3,c=((op>>2)&3)+1,sz=f==0?4:f==1?2:1;num=num?num:256;
                if(f==3)FAIL;bytes=(size_t)num*c*sz;if(!span(end,p,bytes))FAIL;
                if((addr&0xbfc0)==0x8000 && c>=3) {
                    /* Positions fill VU addresses 0..63 from the unpack's own
                       address: course worlds send a packet's vertices in
                       several batches (16-bit deltas broken by 32-bit jumps),
                       all through the same row. */
                    unsigned start=addr&0x3f;
                    if(start+num>256)FAIL;
                    if(tween){if((start!=0 && start!=32) || num>32)FAIL;tween_stream[0][start/32]=num==32?0xffffffffu:(1u<<num)-1;}
                    if(start+num>(unsigned)nv)nv=(int)(start+num);
                    for(unsigned i=0;i<num;i++)for(int k=0;k<3;k++) {
                        const uint8_t *q=r->d+p+(i*c+k)*sz;float *out=&v[start+i].p[k];
                        if(addr==0xc000 && f==0 && authored_scale>0)
                            *out=(int32_t)u32(q)*authored_scale;
                        else if(addr==0xc000 && f==0 && r->fixed==WORLD_FIXED && (r->d[a+8]&128))
                            *out=(int32_t)u32(q)/65536.f;
                        else if((addr&0x4000) && f==0)*out=fl(q);
                        else {
                            /* 0x8000 unpacks (GT4 "compressed") are signed fixed point:
                               cars 1/4096; course worlds 1/500 (2 mm), stored as
                               deltas in STMOD 2, where the VIF adds each value to
                               its row register and keeps the sum there. The world
                               scale puts the road on the runway's road surface on
                               every course checked; 1/512 drifted with distance
                               from the origin (tens of metres on the Nurburgring). */
                            int32_t x=f==0?(int32_t)u32(q):f==1?(int16_t)u16(q):(int8_t)*q;
                            if(mode==2){row[k]+=x;x=row[k];}
                            else if(mode==1)x+=row[k];
                            *out=(float)x*(authored_scale>0?authored_scale:r->fixed);
                        }
                        if(!isfinite(*out) || fabsf(*out)>100000)FAIL;
                    }
                } else if((addr&0xffc0)==0x8040 && c==2 && f<=2 && !tween) {
                    /* V2-32/16/8 texture coordinates: signed
                       deltas through the VIF row like the positions (STMOD),
                       in the same 1/4096 fixed point; at 0x8040 + n.
                       0049D328, shape+0x10 packet descriptors: preserve the
                       VIF USN bit. Unsigned C040 V2-8 carries strip metadata,
                       not UVs; consuming it here removed tire triangles.
                       Signed 8040 carries the Ka bonnet's paint UVs.
                       UV batches at 8054/806F also occur in its LOD body;
                       their UV offsets must survive every packet batch. */
                    unsigned start=addr&0x3f;if(start+num>256)FAIL;
                    for(unsigned i=0;i<num;i++)for(int k=0;k<2;k++) {
                        const uint8_t *q=r->d+p+(i*2+k)*sz;
                        int32_t x=f==0?(int32_t)u32(q):f==1?(int16_t)u16(q):(int8_t)*q;
                        if(mode==2){row[k]+=x;x=row[k];}
                        else if(mode==1)x+=row[k];
                        v[start+i].uv[k]=(float)x/4096.f;
                        if(fabsf(v[start+i].uv[k])>10000)FAIL;
                    }
                } else if((addr==0xc040 || addr==0x8040 || (tween && (addr==0xc060 || addr==0x8060))) && c==2 && f<=1) {
                    if(tween){if(num>32)FAIL;tween_stream[2][(addr&0x20)!=0]=num==32?0xffffffffu:(1u<<num)-1;}
                    for(unsigned i=0;i<num;i++)for(int k=0;k<2;k++) {
                        unsigned dst=i+(tween && (addr&0x20)?32:0);if(dst>=256)FAIL;
                        const uint8_t *q=r->d+p+(i*c+k)*sz;v[dst].uv[k]=f==1?(int16_t)u16(q)/4096.f:!(addr&0x4000)?(int32_t)u32(q)/4096.f:fl(q);
                        if(!isfinite(v[dst].uv[k]) || fabsf(v[dst].uv[k])>10000)FAIL;
                    }
                } else if(((addr&0xbfc0)==0x8040 || (addr&0xbfc0)==0x8080) && c==3 &&
                          (f==0 || !(addr&0x4000))) {
                    /* Compressed cars retain authored smooth normals as
                       signed V3-8 at 8080 (lit) / 8040 (reflection). Ignoring
                       them made the renderer substitute one normal per face,
                       exposing every body triangle even in the closest view.
                       Uniform quantization scaling vanishes on normalization. */
                    unsigned start=addr&0x3f;if(start+num>256)FAIL;
                    if(tween){if((start!=0 && start!=32) || num>32)FAIL;tween_stream[1][start/32]=num==32?0xffffffffu:(1u<<num)-1;}
                    for(unsigned i=0;i<num;i++)for(int k=0;k<3;k++) {
                        const uint8_t *q=r->d+p+(i*3+k)*sz;
                        float normal=f==0?fl(q):f==1?(int16_t)u16(q)/32767.f:(int8_t)*q/127.f;
                        if(!isfinite(normal) || fabsf(normal)>10000)FAIL;
                        v[start+i].normal[k]=normal;
                    }
                } else if(addr==0xc040 && f==2) {nr=num;for(unsigned i=0;i<num;i++)reset[i]=r->d[p+i*c];}
                else if((addr==0xc080 || (tween_mode==1 && tween && addr==0xc0a0)) && c>=3 && f==2)for(unsigned i=0;i<num;i++) {
                    unsigned dst=i+(addr==0xc0a0?32:0);if(dst>=256)FAIL;
                    for(int k=0;k<3;k++)v[dst].rgb[k]*=r->d[p+i*c+k]/128.f;
                    if(c==4)v[dst].alpha=r->d[p+i*c+3]/128.f;
                }
            } else if(op==0x4a)bytes=(num?num:256)*8;
            else if(op==0x50 || op==0x51)bytes=addr*16;
            if(!span(end,p,bytes))FAIL;p=(p+bytes+3)&~(size_t)3;
        }
        if(r->radial_bounds) {
            /* 004588C0 +B0..+FC visits every shape's position stream,
               including lower LODs, before 00458AB8 deforms the tire. */
            for(int i=0;i<nv;i++) {
                float radius2=v[i].p[1]*v[i].p[1]+v[i].p[2]*v[i].p[2];
                if(radius2<r->radial_bounds[0])r->radial_bounds[0]=radius2;
                if(radius2>r->radial_bounds[1])r->radial_bounds[1]=radius2;
                float axial=fabsf(v[i].p[0]);if(axial>r->radial_bounds[2])r->radial_bounds[2]=axial;
            }
            continue;
        }
        int skinned=r->human && (r->d[a+9]&0x80);
        if(tween){if(skinned || nv<=32 || nv>64)FAIL;nv-=32;
            uint32_t mask=nv==32?0xffffffffu:(1u<<nv)-1;
            for(int s=0;s<3;s++)if((s!=1 || tween_mode!=1) && (tween_stream[s][0]!=mask || tween_stream[s][1]!=mask))FAIL;
            /* BC colour-tween packets carry positions/RGBA/UV, without
               normals (kind 25h, VU1 path 1). Their authored endpoint
               colours are equal; reject a different unsupported program. */
            if(tween_mode==1)for(int i=0;i<nv;i++)if(memcmp(v[i].rgb,v[i+32].rgb,12) || v[i].alpha!=v[i+32].alpha)FAIL;
        }
        if(skinned && !skin_packet(r,end,r->d[a+9],v,skin,&nv))FAIL;
        if(nv<3)continue;
        int texture=-1;
        if(table>=0 && table<(int)r->nt && tex>0 && r->texbase[table]>=0)texture=r->texbase[table]+(int)tex-1;
        if(texture>=r->out->ntextures)texture=-1;
        if(kind==2 && table==-1)texture=-2;
        unsigned first=0;
        if(nr<2){r->out->unsupported++;continue;}
        for(unsigned strip=1;strip<nr;strip++) {
            /* Like PDTools, stop at the packet's vertex count: course shapes
               list more strip restarts than vertices they send. */
            unsigned count=(reset[strip]+6)/3;
            if(first+count>(unsigned)nv)count=(unsigned)nv-first;
            if(count<3)break;
            for(unsigned i=0;i+2<count;i++) {
                unsigned indices[3]={first+i+(i&1?2:0),first+i+1,first+i+(i&1?0:2)};
                if(!append(r->out,&v[indices[0]],&v[indices[1]],&v[indices[2]],texture))FAIL;
                if(tween)for(int k=0;k<3;k++) {
                    const gt4_vertex *target=&v[indices[k]+32];gt4_morph_vertex *dst=&r->out->morph[r->out->count-1].v[k];
                    memcpy(dst->p,target->p,12);memcpy(dst->normal,target->normal,12);memcpy(dst->uv,target->uv,8);
                }
                if(skinned && !skin_triangle(r->out,&skin[first+i+(i&1?2:0)],&skin[first+i+1],&skin[first+i+(i&1?0:2)]))FAIL;
                r->out->triangles[r->out->count-1].kind=kind;
                r->out->triangles[r->out->count-1].blend_enabled=(uint8_t)blend_enabled;
            }
            first+=count;
        }
    }
    return 1;
}
int gt4_model_tire_bounds(const uint8_t *bytes,size_t size,float bounds[3])
{
    if(!bytes || !bounds || size<100 || memcmp(bytes,"MDLS",4))return 0;
    reader r={0};gt4_model unused={0};r.d=bytes;r.n=size;r.out=&unused;
    r.ns=u16(bytes+24);r.nm=u16(bytes+26);r.fixed=1/4096.f;r.radial_bounds=bounds;
    size_t shapes=u32(bytes+60);if(!r.ns || !span(size,shapes,(size_t)r.ns*4))return 0;
    bounds[0]=100;bounds[1]=bounds[2]=0; /* 00458910..00458930 */
    for(unsigned i=0;i<r.ns;i++)if(!shape(&r,u32(bytes+shapes+i*4),-1))return 0;
    bounds[0]=sqrtf(bounds[0]);bounds[1]=sqrtf(bounds[1]);
    return bounds[1]>bounds[0] && bounds[2]>0;
}
gt4_model *gt4_model_decode_part(const uint8_t *bytes,size_t size,int color,int model_index)
{return gt4_model_decode_stats(bytes,size,color,model_index,NULL);}
static gt4_model *decode(const uint8_t *bytes,size_t size,int color,int model_index,gt4_model_stats *stats,float fixed,int human,int wheel,int wing,int branch);
gt4_model *gt4_model_decode_stats(const uint8_t *bytes,size_t size,int color,int model_index,gt4_model_stats *stats)
{return decode(bytes,size,color,model_index,stats,1/4096.f,0,0,0,-1);}
gt4_model *gt4_model_decode_wheel_part(const uint8_t *bytes,size_t size,int model_index)
{return decode(bytes,size,0,model_index,NULL,1/4096.f,0,1,0,-1);}
gt4_model *gt4_model_decode_skin_part(const uint8_t *bytes,size_t size,int model_index,gt4_model_stats *stats)
{return decode(bytes,size,-1,model_index,stats,1/4096.f,1,0,0,-1);}
gt4_model *gt4_model_decode_hand_part(const uint8_t *bytes,size_t size,int model_index,gt4_model_stats *stats)
{return decode(bytes,size,-1,model_index,stats,1/4096.f,2,0,0,-1);}
gt4_model *gt4_model_decode_pit_walk_part(const uint8_t *bytes,size_t size,int model_index,int branch,gt4_model_stats *stats)
{return branch>=0?decode(bytes,size,-1,model_index,stats,1/4096.f,3,0,0,branch):NULL;}
/* a model whose callback picks one of its branches (race/after_fire.mdl:
   CarModel::AfterFireCallback 00452F28 returns the frame, particle time x 6) */
gt4_model *gt4_model_decode_branch(const uint8_t *bytes,size_t size,int model_index,int branch)
{return decode(bytes,size,0,model_index,NULL,1/4096.f,0,0,0,branch);}

/* HumanModel hosts 003B09C0/D0 read recorded frame +80/+84 via
   003B3FB4. The hand callback in each MDLS is the same expression:
   x = host * authored_scale; branch = (int)x; tween = x % 1.
   Validate that small program; this is not a general model VM fallback. */
static int hand_program(reader *r,size_t descriptor)
{
    size_t base=u32(r->d+92),offset=u32(r->d+descriptor+36);
    if(offset==0xffffffff || !span(r->n,base,offset) || !span(r->n,base+offset,35))return 0;
    const uint8_t *p=r->d+base+offset;
    if(p[0]!=0x20 || p[1]!=1 || p[2]!=0x12 || p[5]!=0x11 || p[10]!=0x82 || p[11]!=0x13 ||
       p[14]!=0x10 || p[17]!=0x8c || p[18]!=0x14 || p[21]!=0x10 || p[24]!=0x11 ||
       u32(p+25)!=0x3f800000 || p[29]!=0x84 || p[30]!=0x14 || p[33]!=0x23 || p[34]!=0 ||
       u16(p+12)!=2 || u16(p+15)!=2 || u16(p+22)!=2 || (u16(p+3)&0xc000)!=0x8000)return 0;
    unsigned host=u16(p+3)&0x3fff,count=u16(r->d+36);size_t hosts=u32(r->d+76);
    if(!span(r->n,hosts,(size_t)count*8))return 0;int found=0;
    for(unsigned i=0;i<count;i++)if(u16(r->d+hosts+i*8+6)==host) {
        size_t name=u32(r->d+hosts+i*8);
        found=(span(r->n,name,10) && !memcmp(r->d+name,"leftGrasp",10)) ||
              (span(r->n,name,11) && !memcmp(r->d+name,"rightGrasp",11));
    }
    if(r->human==3){
        /* BC time is argument 0 of the float host array (VM class 2).
           The declaration's +6 is the callback id (time=1), not that
           array index. 003D3E80 -> 004573B8 passes actor+68 directly. */
        found=0;if(host==0)for(unsigned i=0;i<count;i++){size_t name=u32(r->d+hosts+i*8);if(span(r->n,name,5) && !memcmp(r->d+name,"time",5))found=1;}
    }
    float scale=fl(p+6);if(!found || !isfinite(scale) || scale<=0 || scale>4 || (r->human==2 && scale>=1))return 0;
    r->hand_branch=u16(p+19);r->hand_tween=u16(p+31);
    if(r->hand_branch>=u16(r->d+32) || r->hand_tween>=u16(r->d+32))return 0;
    r->out->grasp_scale=r->human==3?1.f:scale;r->out->phase_scale=r->human==3?scale:0;
    r->out->morph=malloc(sizeof *r->out->morph);
    return r->out->morph!=NULL;
}
gt4_model *gt4_model_decode_world(const uint8_t *bytes,size_t size,gt4_model_stats *stats)
{return decode(bytes,size,-1,-1,stats,WORLD_FIXED,0,0,0,-1);}
static gt4_model *decode(const uint8_t *bytes,size_t size,int color,int model_index,gt4_model_stats *stats,float fixed,int human,int wheel,int wing,int branch)
{
    if(stats){memset(stats,0,sizeof *stats);stats->fail_op=-1;}
    if(!bytes || size<128)return NULL;size_t base=0,patch=0;
    int car=!memcmp(bytes,"CAR4",4);
    if(car){base=u32(bytes+24);patch=u32(bytes+28);}
    if(!span(size,base,128) || memcmp(bytes+base,"MDLS",4))return NULL;
    size_t n=u32(bytes+base+16);if(n<128 || !span(size,base,n))return NULL;
    uint8_t *d=malloc(n);if(!d)return NULL;memcpy(d,bytes+base,n);
    if(car && color>=0 && span(size,patch,32) && !memcmp(bytes+patch,"Pat0",4)) {
        unsigned nc=u16(bytes+patch+16),np=u16(bytes+patch+18);
        if((unsigned)color<nc && span(size,patch+32,(size_t)nc*np*4))for(unsigned i=0;i<np;i++) {
            size_t p=patch+(size_t)u32(bytes+patch+32+((size_t)color*np+i)*4);
            if(!span(size,p,8)){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto bad;}size_t off=u32(bytes+p),len=u32(bytes+p+4);
            if(!span(n,off,len)||!span(size,p+8,len)){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto bad;}memcpy(d+off,bytes+p+8,len);
        }
    }
    gt4_model *m=calloc(1,sizeof *m);if(!m){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto bad;}
    reader r={0};r.d=d;r.n=n;r.out=m;r.car=car;r.stats=stats;r.fixed=fixed;r.human=human;r.wheel=wheel;r.wing=wing;r.branch=branch;r.wheel_table=-1;r.ns=u16(d+24);r.nm=u16(d+26);r.nt=u16(d+28);
    if(r.ns>4096 || r.nt>128 || !span(n,u32(d+60),(size_t)r.ns*4)){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto fail;}
    r.texbase=malloc((r.nt?r.nt:1)*sizeof(int));
    if(!r.texbase){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto fail;}
    size_t lists=u32(d+68);if(!span(n,lists,4)){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto fail;}size_t list=u32(d+lists);
    if(!span(n,list,r.nt*4)){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto fail;}
    for(unsigned i=0;i<r.nt;i++) {
        r.texbase[i]=-1;size_t at=u32(d+list+i*4);if(!span(n,at,48)){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto fail;}
        tex1_image *images=NULL;int count=tex1_decode(d+at,n-at,&images);
        /* 0 is an empty block the authors left that way, -1 is bad data: only
           the second is worth counting as unsupported. */
        if(count>=0){if(count>0){void *q=realloc(m->textures,(size_t)(m->ntextures+count)*sizeof *m->textures);if(!q){tex1_free(images,count);{if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto fail;}}
            m->textures=q;r.texbase[i]=m->ntextures;memcpy(m->textures+m->ntextures,images,count*sizeof *images);m->ntextures+=count;free(images);}}
        else m->unsupported++;
    }
    unsigned models=u16(d+22);size_t mo=u32(d+56);if(!span(n,mo,(size_t)models*40)){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto fail;}
    if(model_index>=0 && (unsigned)model_index>=models){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto fail;}
    if(human>=2 && (model_index<0 || !hand_program(&r,mo+(size_t)model_index*40)))goto fail;
    for(unsigned i=0;i<models;i++) {
        if(model_index>=0 && i!=(unsigned)model_index)continue;
        int first=m->count,table=0;if(!commands(&r,u32(d+mo+i*40+24),&table)){if(stats && !stats->fail_line)stats->fail_line=__LINE__;goto fail;}
        for(int triangle=first;triangle<m->count;triangle++)m->triangles[triangle].source_model=(uint16_t)i;
    }
    if(human>=2 && (!m->count || !r.tween_active))goto fail;
    m->source_models=models;free(r.texbase);free(d);return m;
fail:free(r.texbase);gt4_model_free(m);
bad:free(d);return NULL;
}
gt4_model *gt4_model_decode(const uint8_t *bytes,size_t size,int color)
{return gt4_model_decode_part(bytes,size,color,bytes && size>=4 && !memcmp(bytes,"CAR4",4)?0:-1);}
void gt4_model_free(gt4_model *m){if(m){free(m->morph);free(m->skin);free(m->triangles);tex1_free(m->textures,m->ntextures);free(m);}}

gt4_model *gt4_model_steering_part(const gt4_model *m,unsigned frame)
{
    if(!m || frame>=m->steering_count)return NULL;const float *a=m->steering_frame[frame];
    /* Full affine inverse, including scale/mirroring, for the one-time split.
       Per-frame movement changes a matrix, never the vertex buffer. */
    mat4 inv=identity4;
    float cof[9]={a[5]*a[10]-a[6]*a[9],a[6]*a[8]-a[4]*a[10],a[4]*a[9]-a[5]*a[8],
                  a[2]*a[9]-a[1]*a[10],a[0]*a[10]-a[2]*a[8],a[1]*a[8]-a[0]*a[9],
                  a[1]*a[6]-a[2]*a[5],a[2]*a[4]-a[0]*a[6],a[0]*a[5]-a[1]*a[4]};
    float det=a[0]*cof[0]+a[4]*cof[3]+a[8]*cof[6];
    if(!isfinite(det) || fabsf(det)<1e-12f)return NULL;
    for(int c=0;c<3;c++)for(int r=0;r<3;r++)inv.m[c*4+r]=cof[r*3+c]/det;
    for(int r=0;r<3;r++){inv.m[12+r]=0;for(int c=0;c<3;c++)inv.m[12+r]-=inv.m[c*4+r]*a[12+c];}
    gt4_model *part=calloc(1,sizeof *part);if(!part)return NULL;
    for(int i=0;i<m->count;i++)if(m->triangles[i].attachment==5+frame) {
        const gt4_triangle *t=&m->triangles[i];
        if(!append(part,&t->v[0],&t->v[1],&t->v[2],t->texture)){gt4_model_free(part);return NULL;}
        part->triangles[part->count-1]=*t;part->triangles[part->count-1].attachment=0;
    }
    if(!part->count){gt4_model_free(part);return NULL;}transform_triangles(part,0,&inv);return part;
}
int gt4_model_steering_matrix(const gt4_model *m,unsigned frame,float angle,float matrix[16])
{
    if(!m || !matrix || frame>=m->steering_count || !isfinite(angle))return 0;
    float degrees=angle*57.295780181884766f;if(!isfinite(degrees))return 0;
    mat4 base,turn=mat_rotate(degrees,0,0,1);
    memcpy(base.m,m->steering_frame[frame],sizeof base.m);mat4 out=mat_mul(&base,&turn);
    memcpy(matrix,out.m,sizeof out.m);return 1;
}

int gt4_model_attach(gt4_model *m,gt4_model *part,const float tr[][6],int count)
{
    if(!m || !part || m->morph || part->morph)return 0;int base=m->ntextures;
    if(part->ntextures) {
        void *q=realloc(m->textures,(size_t)(base+part->ntextures)*sizeof *m->textures);
        if(!q)return 0;m->textures=q;
        memcpy(m->textures+base,part->textures,part->ntextures*sizeof *part->textures);m->ntextures+=part->ntextures;
        free(part->textures);part->textures=NULL;part->ntextures=0;
    }
    for(int j=0;j<count;j++)for(int i=0;i<part->count;i++) {
        gt4_triangle t=part->triangles[i];for(int v=0;v<3;v++)for(int k=0;k<3;k++){
            t.v[v].p[k]=t.v[v].p[k]*tr[j][k+3]+tr[j][k];
            t.v[v].normal[k]=fabsf(tr[j][k+3])>1e-8f?t.v[v].normal[k]/tr[j][k+3]:0;
        }
        if(tr[j][3]*tr[j][4]*tr[j][5]<0){gt4_vertex tmp=t.v[1];t.v[1]=t.v[2];t.v[2]=tmp;}
        if(!append(m,&t.v[0],&t.v[1],&t.v[2],t.texture<0?t.texture:t.texture+base))return 0;
        if(part->skin) {
            const gt4_skin_triangle *s=&part->skin[i];int mirror=tr[j][3]*tr[j][4]*tr[j][5]<0;
            if(!skin_triangle(m,&s->v[0],&s->v[mirror?2:1],&s->v[mirror?1:2]))return 0;
        }
        m->triangles[m->count-1].state=t.state;m->triangles[m->count-1].kind=t.kind;m->triangles[m->count-1].blend_enabled=t.blend_enabled;m->triangles[m->count-1].attachment=t.attachment;
        m->triangles[m->count-1].wheel_blur_texture=t.wheel_blur_texture?t.wheel_blur_texture+base:0;
    }
    return 1;
}

gt4_model *gt4_model_decode_car_wing(const uint8_t *bytes,size_t size,int color,int installed)
{return decode(bytes,size,color,0,NULL,1/4096.f,0,0,installed?1:0,-1);}
