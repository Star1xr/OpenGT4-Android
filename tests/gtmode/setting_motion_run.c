/* Setting's authored MMoveActors, with each PC frame delivery compared
   against original 002C1BB0. This verifies panel motion and phase; it
   does not certify asynchronous loader latency or GS image composition. */
#define main setting_menu_fixture_main
#include "setting_menu.c"
#undef main
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
static int original_move(ee_ctx *c,float from,float target,float ratio,float *to){
 uint8_t *p=c->ram+0x1800000;memcpy(p+0x40,&from,4);memcpy(p+0x2c,&target,4);memcpy(p+0x50,&ratio,4);
 memset(c->r,0,sizeof c->r);c->r[16].uw[0]=0x1800000;c->r[4].uw[0]=0x1800028;c->r[29].uw[0]=0x1ef0000;
 jmp_buf jump;c->exit_jmp=&jump;c->end_reason=0;
 if(!setjmp(jump)){recomp_lookup(0x2c1bb0)(c);return 0;}
 if(c->end_reason!=EE_END_STOP_PC)return 0;memcpy(to,p+0x40,4);return 1;
}
int main(int argc,char **argv){
 CHECK(argc==3 && gt4_game_load_disc(argv[2]));ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0);c.lookup=recomp_lookup;
 const char *why=NULL;gt4_vol *vol=gt4_vol_open(argv[2],&why);CHECK(vol);
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[2]);CHECK(factory);
 gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
 gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears,factory);gt4_game_set_setting_reader(gt4_career_vehicle_setting,factory);
 gt4_save_init("");char msg[256];menu *source=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(source);menu_enter(source,0);ticks(source);
 ah_vm *vm=source->vm;ah_obj *car=ah_module_new(vm,"Car",NULL,NULL,NULL);CHECK(gt4_game_construct_car(vm,car,"ka_01",0,0));
 ah_val value=ah_objv(car),ret;CHECK(nat_bind_game_car(vm,NULL,source,1,&value,&ret)>=0);
 ah_obj *garage=get(vm,game_object(source),"garage").u.o,*cars=ah_array_new(vm);ah_array_push(vm,cars,value);
 ah_set_member(vm,garage,"cars",ah_objv(cars));ah_set_member(vm,garage,"riding_index",ah_int(0));menu_save_capture(source);
 menu *m=menu_open(vol,"setting",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);menu_set_origin(m,"gtmode");CHECK(menu_setting_begin(m));apply_pages(m);ticks(m);
 /* NOS ownership is granted only to this fixture so its authored panel
    is reachable. A stock car without NOS must not open that panel. */
 car=get(m->vm,m->project,"current_car").u.o;ah_val nos[]={ah_int(27),ah_int(1)};
 CHECK(ah_call_member(m->vm,car,"getParts",2,nos,&ret)>=0);
 CHECK(ah_call_member(m->vm,car,"changeParts",2,nos,&ret)>=0);
 CHECK(ah_call_member(m->vm,car,"setupPartsBuffer",0,NULL,&ret)>=0);
 unsigned panels=0,samples=0;
 for(int root=0;root<ui_root_count(m->ui);root++){
  const char *name=ui_root_name(m->ui,root);if(!strcmp(name,"ErrorRoot"))continue;
  page(m,name);ticks(m);CHECK(!errors);
  for(menu_actor *a=m->actors;a;a=a->next){
   ui_widget *w=ah_host_data(a->target);if(a->kind!=1 || !w || w->root!=root)continue;
   ah_obj *actor=get(m->vm,a->target,"actor").u.o;CHECK(actor && ah_host_data(actor)==a);
   /* Ratios/destinations are set by the disc script, not the oracle. */
   /* setting.ad L282: normal panes exit at 640+w. The detailed engine
      popup uses 640 alone at L3695; retain the two authored destinations. */
   float exit_x=!strcmp(name,"EngineCurbePopup")?640.f:640.f+w->w;
   CHECK(a->ratio==.20000000298023224f && a->lo[0]==exit_x && a->lo[1]==a->hi[1]);panels++;
   for(int rate=60;rate<=240;rate*=2)for(int direction=0;direction<2;direction++){
    ah_set_member(m->vm,actor,"out",ah_bool(direction==0));CHECK(ah_call_member(m->vm,actor,"reset",0,NULL,&ret)>=0);
    float expected[3];memcpy(expected,a->value,sizeof expected);
    for(int frame=0;frame<90;frame++){
     for(int axis=0;axis<3;axis++)CHECK(original_move(&c,expected[axis],a->out?a->lo[axis]:a->hi[axis],a->ratio,&expected[axis]));
     for(int delivery=0;delivery<rate/60;delivery++)menu_animate(m,1.f/rate);
     if(getenv("GT4_SETTING_MOTION_NEGATIVE") && frame==17)w->x+=1;
     float native[3]={w->x,w->y,w->magnify};
     if(memcmp(expected,native,sizeof expected))fprintf(stderr,"%s rate=%d direction=%d frame=%d x=%.9g expected=%.9g\n",name,rate,direction,frame,w->x,expected[0]);
     CHECK(!memcmp(expected,native,sizeof expected));samples+=3;
    }
   }
  }
 }
 CHECK(panels>0 && !errors);printf("PASS %u authored Setting moving panels, %u original axis updates, 60/120/240 Hz delivery and both directions, float bytes identical.\n",panels,samples);
 menu_close(m);menu_close(source);gt4_save_shutdown();gt4_career_vehicle_close(factory);gt4_vol_close(vol);ee_free(&c);return 0;
}
