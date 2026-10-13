/* Scalar original prop dynamics; explicit VU matrix boundaries because
   the diagnostic EE executor does not carry VU registers. C tolerance. */
#include "ee.h"
#include "physics/course_objects.h"
#include "physics/car.h"
#include <math.h>
#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"prop oracle %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define OBJ 0x1800000u
#define DESC 0x1810000u
#define HULL 0x1820000u
static float matrix[9];static int collisions;static const char *phase;static unsigned trial;
static float getf(const uint8_t *p){float f;memcpy(&f,p,4);return f;}
static void putf(uint8_t *p,float f){memcpy(p,&f,4);}
static void noop(ee_ctx *e){e->r[2].uw[0]=0;}
static void surface(ee_ctx *e){e->r[2].uw[0]=1;}
static void collision(ee_ctx *e){collisions++;}
static void sincos_hook(ee_ctx *e)
{putf(e->ram+e->r[4].uw[0],sinf(e->fpr[12].f));putf(e->ram+e->r[5].uw[0],cosf(e->fpr[12].f));}
static void rotate_hook(ee_ctx *e,int inverse)
{
    float p[3],r[3];memcpy(p,e->ram+e->r[5].uw[0],12);
    for(int j=0;j<3;j++)r[j]=inverse?
        (matrix[j]*p[0]+matrix[3+j]*p[1])+matrix[6+j]*p[2]:
        (matrix[j*3]*p[0]+matrix[j*3+1]*p[1])+matrix[j*3+2]*p[2];
    memcpy(e->ram+e->r[4].uw[0],r,12);
}
static void rotate_forward(ee_ctx *e){rotate_hook(e,0);}
static void rotate_inverse(ee_ctx *e){rotate_hook(e,1);}
static void supports(ee_ctx *e){/* Only used by the count-zero integration fixture. */}
static ee_fn hook(uint32_t a)
{
    switch(a){case 0x57d8a0:return sincos_hook;case 0x3e21d8:return rotate_forward;
    case 0x3e2220:return rotate_inverse;case 0x3e3290:case 0x3e3458:return supports;
    case 0x3e29f8:return surface;case 0x3e3c48:case 0x5fe3a0:case 0x395a38:case 0x343bf0:return noop;
    case 0x350530:return collision;default:return NULL;}
}
static ee_fn lookup(uint32_t a){ee_fn h=hook(a);return h?h:recomp_lookup(a);}
/* Only the four VU register copies/transposes inside these scalar routines
   are elided; the independent matrix boundary above supplies their result.
   An unexpected original dependency still fails. */
void prop_oracle_boundary(ee_ctx *e,uint32_t target,uint32_t pc)
{
    ee_fn h=hook(target);if(h){h(e);return;}
    if((pc>=0x3e388c && pc<=0x3e390c) || (pc>=0x3e4050 && pc<=0x3e40b0))return;
    ee_unrecompiled(e,target,pc);
}
void prop_oracle_instruction(ee_ctx *e,uint32_t pc,const char *what)
{
    if((pc>=0x3e38c0 && pc<=0x3e390c) || (pc>=0x3e4054 && pc<=0x3e40a0))return;
    ee_unsupported(e,pc,what);
}
static void invoke(ee_ctx *e,uint32_t pc)
{jmp_buf j;e->exit_jmp=&j;e->r[29].uw[0]=0x1ef0000;if(setjmp(j)){fprintf(stderr,"original root %08x failed: %s\n",pc,e->end_detail);exit(1);}CHECK(recomp_lookup(pc));recomp_lookup(pc)(e);}
static void state(ee_ctx *e,const gt4_course_gadget *g)
{
    memset(e->ram+OBJ,0,96);memcpy(e->ram+DESC+32,g->descriptor,160);ee_w32(e,0x6d6054,DESC);
    memcpy(e->ram+OBJ+4,g->angles,12);memcpy(e->ram+OBJ+16,g->pos,12);
    memcpy(e->ram+OBJ+28,g->velocity,12);memcpy(e->ram+OBJ+40,g->angular,12);
    memcpy(e->ram+OBJ+52,g->normal,12);putf(e->ram+OBJ+64,g->plane);
    memcpy(matrix,g->rot,36);ee_w32(e,0x6d6050,HULL);ee_w32(e,HULL+4,0);
    e->r[4].uw[0]=OBJ;
}
static void compare(const gt4_course_gadget *g,const ee_ctx *e)
{
    const float *v[4]={g->angles,g->pos,g->velocity,g->angular};unsigned offsets[4]={4,16,28,40};
    for(int j=0;j<4;j++)for(int k=0;k<3;k++){
        float expected=getf(e->ram+OBJ+offsets[j]+k*4),actual=v[j][k];
        if(fabsf(actual-expected)>3e-5f*fmaxf(1,fabsf(expected))){
            fprintf(stderr,"%s trial %u block %d component %d native %.9g original %.9g\n",phase,trial,j,k,actual,expected);
            exit(1);}
    }
}
int main(int argc,char **argv)
{
    CHECK(argc==2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);
    e.lookup=lookup;e.hook=hook;gt4_phys_mem=e.ram;
    fenv_t env;fegetenv(&env);gt4_phys_fpu_enter();
    uint8_t descriptor[160]={0};descriptor[3]=1;
    putf(descriptor+4,.3f);putf(descriptor+8,.6f);putf(descriptor+12,.2f);putf(descriptor+0x94,2);
    putf(descriptor+16,.12f);putf(descriptor+20,-.2f);putf(descriptor+24,.08f);
    unsigned cases=0;
    for(unsigned i=0;i<256;i++){
        trial=i;phase="contact";
        gt4_course_gadget g={0};g.descriptor=descriptor;g.active=1;
        g.angles[0]=.001f*(i%30);g.angles[1]=.013f*(i%15);g.angles[2]=.01f*(i%20);
        for(int k=0;k<3;k++){g.pos[k]=.2f*(float)(i%7);g.velocity[k]=.2f*(float)((int)(i%13)-6);g.angular[k]=.01f*(float)((int)(i%11)-5);}
        gt4_course_gadget_rotation(g.angles,g.rot);g.normal[1]=1;
        descriptor[2]=(uint8_t)(i&1);state(&e,&g);
        float planes[5][4]={{1,0,0,-1},{-1,0,0,-1},{0,0,1,-1},{0,0,-1,-1},{0,1,0,-2}};
        float pos[3]={.1f,.3f,-.2f},vel[3]={5,.1f,-2},point[3];
        for(int k=0;k<3;k++)point[k]=(g.rot[k*3]*getf(descriptor+16)+g.rot[k*3+1]*getf(descriptor+20))+g.rot[k*3+2]*getf(descriptor+24)+g.pos[k];
        memcpy(e.ram+0x621d28,planes,sizeof planes);memcpy(e.ram+0x1830000,point,12);
        putf(e.ram+HULL+0x454,pos[0]);putf(e.ram+HULL+0x458,-pos[2]);putf(e.ram+HULL+0x45c,pos[1]);
        putf(e.ram+HULL+0x460,vel[0]);putf(e.ram+HULL+0x464,-vel[2]);putf(e.ram+HULL+0x468,vel[1]);putf(e.ram+HULL+0x480,.1f);
        e.r[5].uw[0]=e.r[6].uw[0]=0;e.r[7].uw[0]=0x1830000;e.r[8].uw[0]=HULL;invoke(&e,0x3e3ed0);
        float impulse=gt4_course_object_contact(&g,0,planes,pos,vel,.1f);
        if(getenv("GT4_PROP_CONTACT_NEGATIVE"))g.velocity[0]+=1;
        compare(&g,&e);CHECK(fabsf(impulse-e.fpr[0].f)<3e-5f*fmaxf(1,fabsf(impulse)));cases++;
        phase="ground";g.velocity[1]=-2;state(&e,&g);memcpy(e.ram+0x1830000,g.normal,12);
        e.r[5].uw[0]=0;e.r[6].uw[0]=0x1830000;invoke(&e,0x3e3840);
        int grounded=gt4_course_object_ground(&g,0,g.normal);CHECK(grounded==(int)e.r[2].uw[0]);
        if(getenv("GT4_PROP_GROUND_NEGATIVE"))g.angular[0]+=1;
        compare(&g,&e);CHECK(fabsf(g.impact-getf(e.ram+OBJ+84))<3e-5f*fmaxf(1,fabsf(g.impact)));cases++;
        /* Count zero excludes ground/car queries; tests only the complete
           three-stage integration, including signed angular coupling. */
        phase="integration";descriptor[3]=0;state(&e,&g);e.fpr[12].f=gt4_f32(0x3c888888);invoke(&e,0x3e2b28);
        gt4_course_object_integrate(&g,gt4_f32(0x3c888888));
        if(getenv("GT4_PROP_MOTION_NEGATIVE"))g.pos[0]+=1;
        compare(&g,&e);descriptor[3]=1;cases++;
    }
    for(unsigned age=0;age<256;age++){
        gt4_course_gadget g={0};g.active=1;g.age=(uint8_t)age;
        e.r[4].uw[0]=age;invoke(&e,0x3e20a8);
        int visible=gt4_course_gadget_visible(&g);
        if(getenv("GT4_PROP_BLINK_NEGATIVE"))visible^=1;
        CHECK(visible==(int)e.r[2].uw[0]);
    }
    phase="wall";
    for(unsigned i=0;i<256;i++){
        trial=i;gt4_course_gadget g={0};g.active=1;g.descriptor=descriptor;
        g.angles[1]=.01f*(i%15);gt4_course_gadget_rotation(g.angles,g.rot);
        putf(descriptor+0x90,.5f);g.pos[0]=.02f*(float)((int)(i%99)-49);g.pos[1]=.2f;g.pos[2]=.01f*(i%70);
        g.velocity[2]=2;g.angular[0]=.03f*(i%4);state(&e,&g);
        const uint32_t wall=0x1840000;memset(e.ram+wall,0,32);putf(e.ram+wall,-1);putf(e.ram+wall+16,1);
        ee_w16(&e,wall+14,(uint16_t)((i%8==0)?2:0));
        float previous[3]={g.pos[0],g.pos[1],g.pos[2]-.4f},points[8][3]={{0}};
        for(int k=0;k<3;k++)points[0][k]=(g.rot[k*3]*getf(descriptor+16)+g.rot[k*3+1]*getf(descriptor+20))+g.rot[k*3+2]*getf(descriptor+24)+g.pos[k];
        memcpy(e.ram+0x1830000,previous,12);memcpy(e.ram+0x1850000,points,sizeof points);
        e.r[5].uw[0]=wall;e.r[6].uw[0]=0x1830000;e.r[7].uw[0]=0x1850000;invoke(&e,0x3e44e8);
        int hit=gt4_course_object_wall(&g,wall,previous,points);
        if(getenv("GT4_PROP_WALL_NEGATIVE"))g.pos[2]+=1;
        compare(&g,&e);CHECK(hit==(int)e.r[2].uw[0]);cases++;
    }
    fesetenv(&env);gt4_phys_mem=NULL;ee_free(&e);
    printf("PASS %u original cone contact/ground/integration/wall cases, 256 blink ages; relative tolerance 3e-5, explicit VU boundaries.\n",cases);return 0;
}
