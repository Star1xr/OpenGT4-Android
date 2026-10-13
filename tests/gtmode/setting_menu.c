/* Native application bridge + original authored Setting scripts.
   No EE runtime or generated original C is linked into this executable. */
#include "../../src/core/menu.c"
#include "career_vehicle.h"
#include "career_blob.h"
#include "mot1.h"
#include "gpb.h"
#include "physics/phys.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"setting menu line %d: %s\n",__LINE__,#x);return 1;}}while(0)
struct plat_texture{int w,h;};
static void (*setting_quad_observer)(const float xy[4][2],const uint8_t color[4]);
static int card_probe;static float card_height;static unsigned card_draws;static int card_bad;
static int label_probe;static float label_width;static unsigned label_glyphs;
plat_texture *plat_texture_create(int w,int h,const uint8_t *p){(void)w;(void)h;(void)p;plat_texture *t=calloc(1,sizeof *t);t->w=w;t->h=h;return t;}
void plat_texture_destroy(plat_texture *t){free(t);}
void plat_draw_quad4(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4]){if(card_probe && t && t->w==640 && t->h==206){card_draws++;if(h!=card_height)card_bad=1;}(void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;}
void plat_draw_solid_quad(const float xy[4][2],const uint8_t c[4]){if(setting_quad_observer)setting_quad_observer(xy,c);}
void plat_draw_quad(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4]){(void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;}
void plat_draw_quad_blend(const plat_texture *t,float x,float y,float w,float h,
                           float u,float v,float a,float b,const uint8_t c[4],float factor)
{
    uint8_t tint[4];memcpy(tint,c,4);tint[3]=(uint8_t)(factor*255.0f+.5f);
    plat_draw_quad(t,x,y,w,h,u,v,a,b,tint);
}
void plat_set_clip(float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;}
void plat_draw_glyph(const plat_texture*t,float x,float y,float w,float h,float b,float s,float u,float v,float u1,float v1,const uint8_t c[4]){
 if(label_probe && t && c[0]==7 && c[1]==11 && c[2]==13 && c[3]>128){
  label_glyphs++;label_width+=w;
 }
 (void)x;(void)y;(void)h;(void)b;(void)s;(void)v;(void)v1;
}
void plat_draw_rotated(const plat_texture*t,float x,float y,float w,float h,float cx,float cy,float a,const uint8_t c[4]){(void)t;(void)x;(void)y;(void)w;(void)h;(void)cx;(void)cy;(void)a;(void)c;}
int plat_dialog(const char *title,const char *text,int confirm){(void)title;(void)text;return confirm?1:0;}
int plat_audio_play(const int16_t*p,size_t n,int r,int c){(void)p;(void)n;(void)r;(void)c;return 1;}
static ah_val get(ah_vm *vm,ah_obj *o,const char *name){ah_val v=ah_nil();ah_get_member(vm,o,name,&v);return v;}
static unsigned errors;
static void log_script(void *user,const char *msg){(void)user;if(strstr(msg,"error:") || strstr(msg,"undefined") || strstr(msg,"call of nil") || strstr(msg,"call of module")){errors++;fprintf(stderr,"%s\n",msg);}}
static int page(menu *m,const char *name){for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(name,ui_root_name(m->ui,i))){m->depth=1;m->pages[0]=i;menu_enter(m,i);return i;}return -1;}
static ui_widget *child(ui_widget *w,const char *name){return ui_widget_child(w,name);}
static void ticks(menu *m){for(unsigned i=0;i<60;i++){menu_tick(m);menu_animate(m,1.f/60);}}
int main(int argc,char **argv){
 CHECK(argc==2);const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&err);CHECK(vol && gt4_game_load_disc(argv[1]));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
 gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears,factory);gt4_game_set_setting_reader(gt4_career_vehicle_setting,factory);
 const char *save_dir=getenv("GT4_SETTING_SAVE_DIR");gt4_save_init(save_dir?save_dir:"");char msg[256];menu *source=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(source);menu_enter(source,0);ticks(source);
 ah_vm *vm=source->vm;ah_obj *car=ah_module_new(vm,"Car",NULL,NULL,NULL);CHECK(gt4_game_construct_car(vm,car,"ka_01",0,0));
 ah_val bound,arg=ah_objv(car);CHECK(nat_bind_game_car(vm,NULL,source,1,&arg,&bound)>=0);
 ah_obj *garage=get(vm,game_object(source),"garage").u.o,*cars=ah_array_new(vm);ah_array_push(vm,cars,arg);
 ah_set_member(vm,garage,"cars",ah_objv(cars));ah_set_member(vm,garage,"riding_index",ah_int(0));menu_save_capture(source);
 if(save_dir){if(!gt4_save_active())CHECK(gt4_save_create(1,"Setting validation"));menu_save_capture(source);CHECK(gt4_save_store());}
 menu *m=menu_open(vol,"setting",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);menu_set_origin(m,"gtmode");CHECK(menu_setting_begin(m));apply_pages(m);ticks(m);
 CHECK(!errors);
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"SettingRoot"));
 vm=m->vm;car=get(vm,m->project,"current_car").u.o;CHECK(car);
 uint8_t body[0x4b8],before[0x4b8];CHECK(gt4_game_car_body(vm,car,before));
 for(int slot=0;slot<3;slot++){
  ah_set_member(vm,car,"slot",ah_int(slot));ah_val result,values[]={ah_int(0),ah_int(30+slot)};
  CHECK(ah_call_member(vm,car,"setBallastSetting",2,values,&result)>=0);
  CHECK(ah_call_member(vm,car,"getBallastSetting",1,values,&result)>=0 && result.u.i==30+slot);
 }
 CHECK(gt4_game_car_body(vm,car,body));for(int i=0;i<3;i++)CHECK(body[i*0x178+0x15b]==30+i);
 CHECK(menu_setting_end(m));menu_save_refresh(source);vm=source->vm;garage=get(vm,game_object(source),"garage").u.o;car=ah_array_get(get(vm,garage,"cars").u.o,0).u.o;
 CHECK(gt4_game_car_body(vm,car,body));if(getenv("GT4_SETTING_FLOW_NEGATIVE"))body[0x15b]^=1;for(int i=0;i<3;i++)CHECK(body[i*0x178+0x15b]==30+i);
 CHECK(menu_setting_begin(m));apply_pages(m);ticks(m);CHECK(!errors);
 car=get(m->vm,m->project,"current_car").u.o;ah_val result,part[]={ah_int(27),ah_int(1)};
 CHECK(ah_call_member(m->vm,car,"getParts",2,part,&result)>=0);CHECK(ah_call_member(m->vm,car,"changeParts",2,part,&result)>=0);CHECK(ah_call_member(m->vm,car,"setupPartsBuffer",0,NULL,&result)>=0);
 for(int i=1;i<ui_root_count(m->ui)-1;i++){page(m,ui_root_name(m->ui,i));ticks(m);CHECK(!errors);}
 int root=page(m,"BallastRoot");CHECK(root>=0);ticks(m);
 ui_widget *r=ui_root_widget(m->ui,root),*pane=child(r,"Pane"),*weight=child(child(pane,"Weight"),"slider"),*position=child(child(pane,"Position"),"slider");
 CHECK(weight && position && weight->slider_range_set && position->slider_range_set);
 CHECK(weight->slider_min==0 && weight->slider_max==200 && position->slider_min==-50 && position->slider_max==50);
 m->focus[root]=weight;int old=weight->focus_index;menu_send_input(m,MENU_RIGHT);CHECK(weight->focus_index==old+(getenv("GT4_SETTING_CONTROL_NEGATIVE")?2:1));
 m->focus[root]=position;old=position->focus_index;menu_send_input(m,MENU_LEFT);CHECK(position->focus_index==old-1);
 CHECK(ah_call_member(m->vm,widget_module(m,r),"apply",0,NULL,&result)>=0);
 car=get(m->vm,m->project,"current_car").u.o;ah_val selector=ah_int(0);
 CHECK(ah_call_member(m->vm,car,"getBallastSetting",1,&selector,&result)>=0 && result.u.i==weight->focus_index);
 selector=ah_int(1);CHECK(ah_call_member(m->vm,car,"getBallastSetting",1,&selector,&result)>=0 && result.u.i==position->focus_index);
 CHECK(!errors);
 /* Test the authored Home Triangle command in isolation: Home's display
    callbacks require rendering/calendar services outside this headless fixture. */
 int home=-1;for(int i=0;i<ui_root_count(source->ui);i++)if(!strcmp(ui_root_name(source->ui,i),"HomeRoot"))home=i;
 CHECK(home>=0);source->depth=1;source->pages[0]=home;ah_set_log(source->vm,log_script,NULL);
 menu_send_input(source,MENU_TRIANGLE);ticks(source);CHECK(!errors && !strcmp(source->next_project,"setting"));
 menu_setting_context(m,source);CHECK(nat_finish(m->vm,NULL,m,0,NULL,&result)>=0);CHECK(!strcmp(m->next_project,"gtmode:HomeRoot"));
 CHECK(menu_setting_end(m));menu_save_refresh(source);CHECK(menu_riding_car_body(source,body));
 CHECK(body[2*0x178+0x15b]==weight->focus_index && (int8_t)body[2*0x178+0x15c]==position->focus_index);
 if(save_dir){menu_save_capture(source);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(source);CHECK(menu_riding_car_body(source,before));CHECK(!memcmp(body,before,sizeof body));}
 printf("PASS authored SettingRoot, independent A/B/C ballast edits, exit synchronization, garage snapshot and cached reopen\n");
 menu_close(m);menu_close(source);gt4_vol_close(vol);gt4_save_shutdown();gt4_career_vehicle_close(factory);return 0;
}
