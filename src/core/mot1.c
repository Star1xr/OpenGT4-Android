#include "mot1.h"

#include <math.h>
#include <fenv.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float t, v, c1, c2; } key;
typedef struct { key *keys; int count; } curve;
/* One parameter: up to 4 components, each a curve index or a constant. */
typedef struct { char name[32]; int kind, light, n, curve[9]; float value[9]; } param;

struct gt4_mot1 {
    float end, loop;
    curve *curves; int ncurves;
    param *params; int nparams;
};

static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static float f32(const uint8_t *p) { float f; uint32_t v = u32(p); memcpy(&f, &v, 4); return f; }
static int span(size_t size, size_t at, size_t len) { return at <= size && len <= size - at; }

void gt4_mot1_close(gt4_mot1 *m)
{
    if (!m) return;
    for (int i = 0; i < m->ncurves; i++) free(m->curves[i].keys);
    free(m->curves);
    free(m->params);
    free(m);
}

/* Curve data: u32 n, then n+1 floats; after one leading word, keys of
   (time, value, control 1, control 2); the last key may stop after value. */
static int read_curve(const uint8_t *d, size_t size, size_t at, curve *c)
{
    if (!span(size, at, 4)) return 0;
    uint32_t n = u32(d + at);
    if (n > 4096 || !span(size, at + 4, (size_t)(n + 1) * 4)) return 0;
    const uint8_t *f = d + at + 4;
    if (n < 2) return 0;
    int count = (int)((n - 2) / 4 + 1);   /* key k: time at word 1+4k, value at 2+4k <= n */
    c->keys = calloc((size_t)count, sizeof *c->keys);
    if (!c->keys) return 0;
    for (int k = 0; k < count; k++) {
        size_t j = 1 + 4 * (size_t)k;
        c->keys[k].t = f32(f + 4 * j);
        c->keys[k].v = f32(f + 4 * (j + 1));
        c->keys[k].c1 = j + 2 <= n ? f32(f + 4 * (j + 2)) : c->keys[k].v;
        c->keys[k].c2 = j + 3 <= n ? f32(f + 4 * (j + 3)) : c->keys[k].v;
    }
    c->count = count;
    return 1;
}

static int add_param(gt4_mot1 *m, const param *p)
{
    param *grown = realloc(m->params, (size_t)(m->nparams + 1) * sizeof *grown);
    if (!grown) return 0;
    m->params = grown;
    m->params[m->nparams++] = *p;
    return 1;
}

gt4_mot1 *gt4_mot1_open(const uint8_t *d, size_t size)
{
    if (!d || size < 0x20 || memcmp(d, "Mot1", 4)) return NULL;
    size_t table = u32(d + 0x10);
    if (!u16(d + 0x14) || !span(size, table, 16)) return NULL;
    size_t elements = u32(d + table + 4), params_at = u32(d + table + 12), params_end = u32(d + table + 8);
    if (!u32(d + table) || !span(size, elements, 32) || params_end > size || params_at > params_end) return NULL;
    const uint8_t *e = d + elements;
    gt4_mot1 *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->end = f32(e);
    m->loop = f32(e + 4);
    uint32_t ncurves = u32(e + 0x14);
    size_t offsets = u32(e + 0x18);
    if (ncurves > 1024 || !span(size, offsets, (size_t)ncurves * 4)) goto fail;
    m->curves = calloc(ncurves ? ncurves : 1, sizeof *m->curves);
    if (!m->curves) goto fail;
    m->ncurves = (int)ncurves;
    for (uint32_t i = 0; i < ncurves; i++) {
        size_t header = u32(d + offsets + 4 * i);
        if (!span(size, header, 16) || u32(d + header) != 1) goto fail;
        if (!read_curve(d, size, u32(d + header + 12), &m->curves[i])) goto fail;
    }
    /* Parameter records until an unknown type (0x01 ends the list): 0x0A
       field of view (u8 mask, 1 component, radians), 0x0B look-at (u16
       mask, 9 components), 0x0C-0x0E light colours (light, mask, 4
       components), 0x0F light position (3 components), 0x10 named scalar
       (flag, name, 1 component). A set mask/flag bit means a u16 curve
       index, a clear one a float constant. Opcodes as dispatched by the
       motion interpreter, CORE 0x0042B440. */
    size_t target_start=0;
    for (size_t p = params_at; p < params_end;) {
        param q; memset(&q, 0, sizeof q);
        unsigned type = d[p], mask;
        size_t at;
        /* 0042B440 +0xc8..0x174 dispatches stack markers and target drawing.
           0042A660 captures the transformed frame at opcode 4: its name
           belongs to the preceding channels, not the following child. */
        if(type==0 || type==2 || type==3){q.kind=(int)type;if(!add_param(m,&q))goto fail;p++;continue;}
        if(type==1)break;
        if(type==4){
            const uint8_t *z=memchr(d+p+1,0,params_end-p-1);
            if(!z || (size_t)(z-(d+p+1))>=sizeof q.name)goto fail;
            for(size_t j=target_start;j<(size_t)m->nparams;j++) {
                param *track=&m->params[j];
                if(track->kind==5 || (track->kind>=6 && track->kind<=8) || track->kind==0x13)
                    memcpy(track->name,d+p+1,(size_t)(z-(d+p+1)));
            }
            q.kind=4;memcpy(q.name,d+p+1,(size_t)(z-(d+p+1)));
            if(!add_param(m,&q))goto fail;
            target_start=(size_t)m->nparams;p=(size_t)(z-d)+1;continue;
        }
        if(type==5 || (type>=6 && type<=9) || type==0x13){
            /* 0042B5E0 translation xyz, 0042B698..0042B7E8 axes,
               0042C178 Euler xyz -> 0048B888 quaternion. Expose channels;
               attachment hierarchy and quaternion conversion stay separate. */
            if(!span(params_end,p,2))goto fail;
            q.kind=(int)type;q.n=type==5 || type==9 || type==0x13?3:1;
            mask=d[p+1];at=p+2;
        } else if (type == 0x0A) {
            if (!span(params_end, p, 2)) break;
            q.kind = 0x0A; mask = d[p + 1]; q.n = 1; at = p + 2;
        } else if (type == 0x0B) {
            if (!span(params_end, p, 3)) break;
            q.kind = 0x0B; mask = u16(d + p + 1); q.n = 9; at = p + 3;
        } else if (type >= 0x0C && type <= 0x0F) {
            if (!span(params_end, p, 3)) break;
            q.kind = (int)type; q.light = d[p + 1]; mask = d[p + 2];
            q.n = type == 0x0F ? 3 : 4; at = p + 3;
        } else if (type == 0x10) {
            if (!span(params_end, p, 3)) break;
            mask = d[p + 1];
            const uint8_t *z = memchr(d + p + 2, 0, params_end - (p + 2));
            if (!z || (size_t)(z - (d + p + 2)) >= sizeof q.name) break;
            memcpy(q.name, d + p + 2, (size_t)(z - (d + p + 2)));
            q.kind = 0x10; q.n = 1; at = (size_t)(z - d) + 1;
        } else break;
        for (int c = 0; c < q.n; c++) {
            if (mask & (1u << c)) {
                if (!span(params_end, at, 2)) goto done;
                q.curve[c] = u16(d + at); at += 2;
                if (q.curve[c] >= m->ncurves) goto fail;
            } else {
                if (!span(params_end, at, 4)) goto done;
                q.curve[c] = -1; q.value[c] = f32(d + at); at += 4;
            }
        }
        if (!add_param(m, &q)) goto fail;
        p = at;
    }
done:
    return m;
fail:
    gt4_mot1_close(m);
    return NULL;
}

float gt4_mot1_end(const gt4_mot1 *m) { return m ? m->end : 0; }
float gt4_mot1_loop(const gt4_mot1 *m) { return m ? m->loop : 0; }

float gt4_mot1_wrap(const gt4_mot1 *m, float t)
{
    if (!m || t < 0) return 0;
    if (t < m->end) return t;
    float span_len = m->end - m->loop;
    if (span_len == 0) return m->loop;
    return m->loop + fmodf(t - m->loop, span_len);
}

static float evaluate(const curve *c, float t)
{
    if(c->count<2)return c->keys[0].v;
    /* 00486090 +0x00..0x50 / 00486388 +0x00..0xa4 select a segment,
       including the first/last outside the key range. 004861A8 +0x00..0x68
       evaluates its cubic in Horner order; it does not clamp the endpoint. */
    int k=0;while(k+2<c->count && t>c->keys[k+1].t)k++;
    const key *a=&c->keys[k],*b=&c->keys[k+1];
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    float s=(t-a->t)/(b->t-a->t),v1=(a->c1-a->c2)*3.f,v2=(a->v-a->c1)*3.f;
    float v=b->v-a->v;v=v+v1;v1=v2-v1;
    v=v*s;v=v+v1;v=v*s;v=v-v2;v=v*s;v=v+a->v;
    fesetround(saved);return v;
}

static float component(const gt4_mot1 *m, const param *p, int c, float t)
{
    return p->curve[c] >= 0 ? evaluate(&m->curves[p->curve[c]], t) : p->value[c];
}

int gt4_mot1_value(const gt4_mot1 *m, const char *name, float t, float *out)
{
    for (int i = 0; m && i < m->nparams; i++)
        if (m->params[i].kind == 0x10 && !strcmp(m->params[i].name, name)) {
            *out = component(m, &m->params[i], 0, t);
            return 1;
        }
    return 0;
}

int gt4_mot1_light(const gt4_mot1 *m, int kind, int light, float t, float out[4])
{
    for (int i = 0; m && i < m->nparams; i++) {
        const param *p = &m->params[i];
        if (p->kind != kind || p->light != light) continue;
        for (int c = 0; c < p->n; c++) out[c] = component(m, p, c, t);
        return p->n;
    }
    return 0;
}

int gt4_mot1_camera(const gt4_mot1 *m, float t, float look[9], float *fov)
{
    int found = 0;
    for (int i = 0; m && i < m->nparams; i++) {
        const param *p = &m->params[i];
        if (p->kind == 0x0B) { for (int c = 0; c < 9; c++) look[c] = component(m, p, c, t); found |= 1; }
        if (p->kind == 0x0A) { *fov = component(m, p, 0, t); found |= 2; }
    }
    return found == 3;
}

/* Named model channels as encoded by 0042B440; absent tracks preserve caller
   state, which is important for single-axis records in EntranceMotion2. */
int gt4_mot1_transform(const gt4_mot1 *m,const char *target,float t,float position[3],float euler[3]){
    if(!m || !target || !position || !euler)return 0;
    int found=0;
    for(int i=0;i<m->nparams;i++){
        const param *p=&m->params[i];if(strcmp(p->name,target))continue;
        if(p->kind==5){for(int j=0;j<3;j++)position[j]=component(m,p,j,t);found|=1;}
        /* 00686900 slots +30/+38/+40 -> 0042C890/0042C8B8/0042C8E0:
           axis rotations, radians converted to degrees for the model context. */
        else if(p->kind>=6 && p->kind<=8){euler[p->kind-6]=component(m,p,0,t);found|=2;}
        else if(p->kind==0x13){for(int j=0;j<3;j++)euler[j]=component(m,p,j,t);found|=2;}
    }
    return found;
}

static void identity(float m[16]){for(int k=0;k<16;k++)m[k]=k%5==0?1.f:0.f;}
static void multiply(float a[16],const float b[16]){
    float r[16];
    for(int c=0;c<4;c++)for(int y=0;y<4;y++){
        float v=0;for(int k=0;k<4;k++)v+=a[k*4+y]*b[c*4+k];r[c*4+y]=v;
    }
    memcpy(a,r,sizeof r);
}
static void quaternion_matrix(const float q[4],float r[16]){
    /* 0048A970 +0x08..0xb8, preserving the original column layout. */
    float x=q[0],y=q[1],z=q[2],w=q[3],xx=(x+x)*x,yy=(y+y)*y,zz=(z+z)*z;
    float xy=(x+x)*y,xz=(x+x)*z,yz=(y+y)*z,xw=(x+x)*w,yw=(y+y)*w,zw=(z+z)*w;
    identity(r);r[0]=(1.f-yy)-zz;r[1]=xy+zw;r[2]=xz-yw;
    r[4]=xy-zw;r[5]=(1.f-zz)-xx;r[6]=yz+xw;
    r[8]=xz+yw;r[9]=yz-xw;r[10]=(1.f-xx)-yy;
}
int gt4_mot1_frames(const gt4_mot1 *m,float t,const char *const *names,int count,
                    float (*out)[16],void (*sc)(float,float *,float *)){
    if(!m || !names || count<0 || !out || !sc)return -1;
    float frame[16],stack[32][16];unsigned depth=0;int found=0;identity(frame);
    for(int i=0;i<m->nparams;i++){
        const param *p=&m->params[i];
        /* 0042C818 / 0042C848 -> 004A53F8 / 004A5400: push/pop
           the current transform, not a parent guessed from bone names. */
        if(p->kind==2){if(depth==32)return -1;memcpy(stack[depth++],frame,sizeof frame);}
        else if(p->kind==3){if(!depth)return -1;memcpy(frame,stack[--depth],sizeof frame);}
        else if(p->kind==4){
            for(int n=0;n<count;n++)if(names[n] && !strcmp(names[n],p->name)){memcpy(out[n],frame,sizeof frame);found++;}
        }else if(p->kind==5 || p->kind==9){
            float local[16];identity(local);
            for(int k=0;k<3;k++)local[p->kind==5?12+k:k*5]=component(m,p,k,t);
            multiply(frame,local); /* 004A7844 translation; 004A7890 scale */
        }else if((p->kind>=6 && p->kind<=8) || p->kind==0x13){
            float q[4]={0,0,0,1},local[16];
            if(p->kind!=0x13)sc(component(m,p,0,t)*.5f,&q[p->kind-6],&q[3]);
            else{
                /* 0048B888 +0x54..0xc8; sin/cos pairs from 0057D8A0. */
                float sx,cx,sy,cy,sz,cz;
                sc(component(m,p,0,t)*.5f,&sx,&cx);sc(component(m,p,1,t)*.5f,&sy,&cy);sc(component(m,p,2,t)*.5f,&sz,&cz);
                q[0]=(sx*cy)*cz-(cx*sy)*sz;q[1]=(cx*sy)*cz+(sx*cy)*sz;
                q[2]=(cx*cy)*sz-(sx*sy)*cz;q[3]=(cx*cy)*cz+(sx*sy)*sz;
            }
            quaternion_matrix(q,local);multiply(frame,local);
        }
    }
    return depth?-1:found;
}
