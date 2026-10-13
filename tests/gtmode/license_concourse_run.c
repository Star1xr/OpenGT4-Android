/* Compare the native director with 00409258; placement, actor loading and
   camera loading are recorded boundaries, not replacement logic. */
#include "ee.h"
#include "license_concourse.h"
#include <string.h>
#include <stdlib.h>
ee_fn recomp_lookup(uint32_t);
#define OBJ 0x1800000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"concourse line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned actors,places,cameras;
static char actor_names[3][48],camera_name[48];
static void put(ee_ctx*c,uint32_t a,uint32_t v){memcpy(c->ram+a,&v,4);}
static void actor(ee_ctx*c){unsigned i=c->r[5].uw[0];if(i<3){strcpy(actor_names[i],(char*)c->ram+c->r[6].uw[0]);actors++;}}
static void camera(ee_ctx*c){strcpy(camera_name,(char*)c->ram+c->r[6].uw[0]);cameras++;}
static void place(ee_ctx*c){(void)c;places++;}
static void player(ee_ctx*c){c->r[2].uw[0]=0x1900000;}
static void noop(ee_ctx*c){(void)c;}
static ee_fn boundary(uint32_t p){
 switch(p){case 0x408268:return actor;case 0x408a78:return camera;case 0x408db8:return place;
 case 0x34c190:return player;case 0x408ee0:return noop;default:return NULL;}
}
static ee_fn lookup(uint32_t p){ee_fn f=boundary(p);return f?f:recomp_lookup(p);}
static int call(ee_ctx*c,uint32_t pc,float dt){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=OBJ;c->r[29].uw[0]=0x1ef0000;c->fpr[12].f=dt;
 jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"original %08x %s\n",c->end_pc,c->end_detail);return 0;}
 recomp_lookup(pc)(c);return 1;
}
int main(int argc,char**argv){
 CHECK(argc==2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);
 e.lookup=lookup;e.hook=boundary;unsigned cases=0;
 for(int enabled=0;enabled<=1;enabled++)for(int outcome=0;outcome<=2;outcome++)for(unsigned cycle=0;cycle<3;cycle++){
  gt4_license_concourse n;gt4_license_concourse_begin(&n,enabled?2:0,cycle);
  if(outcome)gt4_license_concourse_finish(&n,outcome==1);
  memset(e.ram+OBJ,0,0x100);put(&e,OBJ+0x14,n.outcome);put(&e,OBJ+0x18,n.enabled);
  put(&e,OBJ+0x1c,n.placement_ready);put(&e,OBJ+0x74,cycle);put(&e,OBJ+0x3c,1);
  for(unsigned tick=0;tick<800;tick++){
   actors=places=cameras=0;float dt=tick%3==0?1.f/120:tick%3==1?1.f/60:1.f/30;
   gt4_license_concourse_step(&n,dt);
   /* 00409168 calls the phase switch only for enabled, completed sessions. */
   if(n.enabled && n.outcome)CHECK(call(&e,0x409258,dt));
   if(getenv("GT4_CONCOURSE_NEGATIVE") && tick==200)n.elapsed+=.125f;
   CHECK(!memcmp(e.ram+OBJ+0x20,&n.elapsed,4));CHECK(!memcmp(e.ram+OBJ+0x24,&n.phase,4));
   CHECK(!memcmp(e.ram+OBJ+0x28,&n.previous,4));CHECK(!memcmp(e.ram+OBJ+0x1c,&n.placement_ready,4));
   CHECK(!memcmp(e.ram+OBJ+0x74,&n.camera_cycle,4));
   CHECK(actors==((n.changed&GT4_CONCOURSE_ACTORS)?3u:0u));
   CHECK(cameras==((n.changed&GT4_CONCOURSE_CAMERA)?1u:0u));CHECK(places==((n.changed&GT4_CONCOURSE_PLACE)?1u:0u));
   if(actors)for(unsigned j=0;j<3;j++)CHECK(!strcmp(actor_names[j],gt4_license_concourse_actor(&n,j)));
   if(cameras)CHECK(!strcmp(camera_name,gt4_license_concourse_camera(&n)));
   CHECK(call(&e,0x4096f8,0));CHECK(e.r[2].uw[0]==(unsigned)gt4_license_concourse_has_camera(&n));
   CHECK(call(&e,0x409860,0));CHECK(e.r[2].uw[0]==(unsigned)gt4_license_concourse_cut(&n));cases++;
  }
 }
 ee_free(&e);printf("PASS %u original LicenseConcourse phases/timers/actor requests/camera cycles; assets and drawing are boundaries.\n",cases);return 0;
}
