/* Native application bridge + original authored acquisition scripts.
   No EE runtime or generated original C is linked into this executable. */
#include "../../src/core/menu.c"
#include "career_vehicle.h"
#include "career_blob.h"
#include "mot1.h"
#include "gpb.h"
#include "physics/phys.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"purchase menu line %d: %s\n",__LINE__,#x);return 1;}}while(0)
struct plat_texture{int w,h;};
static int card_probe;static float card_height;static unsigned card_draws;static int card_bad;
static int label_probe;static float label_width;static unsigned label_glyphs;
plat_texture *plat_texture_create(int w,int h,const uint8_t *p){(void)w;(void)h;(void)p;plat_texture *t=calloc(1,sizeof *t);t->w=w;t->h=h;return t;}
void plat_texture_destroy(plat_texture *t){free(t);}
void plat_draw_quad4(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4]){if(card_probe && t && t->w==640 && t->h==206){card_draws++;if(h!=card_height)card_bad=1;}(void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;}
void plat_draw_solid_quad(const float xy[4][2],const uint8_t c[4]){(void)xy;(void)c;}
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
static int load(const char*path,uint8_t*out,size_t n){FILE*f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n;fclose(f);return ok;}
static unsigned script_errors;
static void log_script(void *user,const char *msg){(void)user;if(strstr(msg,"error:") || strstr(msg,"undefined") || strstr(msg,"call of nil") || strstr(msg,"call of module")){script_errors++;fprintf(stderr,"%s\n",msg);}}

static void ticks(menu *m){for(unsigned i=0;i<60;i++){menu_tick(m);menu_animate(m,1.f/60);}}
static ui_widget *kid(ui_widget *w,const char *name){return ui_widget_child(w,name);}
static int transaction_model_loader(menu *m,const ui_widget *w,void *u){(void)m;(void)u;return w && w->car_code;}
static int confirm_choice=1;
static int purchase_dialog(menu *m,const char *t,const char *v,int type,const char *y,const char *n){(void)m;(void)t;(void)v;(void)y;(void)n;return type==1?confirm_choice:1;}
static int enter_named(menu *m,const char *name){for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),name)){menu_enter(m,i);ticks(m);return 1;}return 0;}
#include "model.h"
#include <time.h>
static float waited,max_wait;static unsigned missing_sounds;
static void service_log(void *user,const char *msg){log_script(user,msg);if(strstr(msg,"unsupported sound")){missing_sounds++;fprintf(stderr,"%s\n",msg);}}
static void service_wait(menu *m,float seconds,void *user){
 (void)user;waited+=seconds;if(seconds>max_wait)max_wait=seconds;
 for(float left=seconds;left>0;){float dt=fminf(left,1.f/60);menu_animate(m,dt);
  ui_widget *root=ui_root_widget(m->ui,menu_active_root(m));ui_car_prepare_draw(kid(root,"CarFace"));left-=dt;}
}
static int service_case(const char *disc,const char *code,const char *page,const char *button){
 const char *error=NULL;gt4_vol *vol=gt4_vol_open(disc,&error);CHECK(vol && gt4_game_load_disc(disc));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(disc);CHECK(factory);
 gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);gt4_game_set_service_reader(gt4_career_vehicle_service_geometry,factory);
 char directory[240];snprintf(directory,sizeof directory,"build/gtauto-functional/save-%s-%s-%llu-%llu",code,page,(unsigned long long)time(NULL),(unsigned long long)clock());
 const char *save=(!strcmp(code,"ka_01") && !strcmp(page,"CarWashRoot"))?getenv("GT4_SERVICE_SAVE"):NULL;gt4_save_init(save?save:directory);char msg[256];menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);menu_enter(m,0);ticks(m);ah_vm *vm=m->vm;
 ah_obj *game=game_object(m),*garage=get(vm,game,"garage").u.o;ah_val args[]={ah_str(vm,code),ah_int(0)},ret;
 CHECK(ah_call_member(vm,garage,"addNewCar",2,args,&ret)==1);ah_val index=ah_int(0);CHECK(ah_call_member(vm,garage,"setRidingCar",1,&index,&ret)==1);
 ah_set_member(vm,game,"money",ah_int(10000));CHECK(gt4_save_capture(vm,game,1));CHECK(gt4_save_create(1,"GT Auto validation"));CHECK(gt4_save_capture(vm,game,1) && gt4_save_store());menu_save_refresh(m);menu_set_car_loader(m,transaction_model_loader,NULL);menu_set_dialog_presenter(m,purchase_dialog);menu_set_presenter(m,service_wait,NULL);
 script_errors=missing_sounds=0;ah_set_log(vm,service_log,NULL);CHECK(enter_named(m,page));
 ui_widget *root=ui_root_widget(m->ui,menu_active_root(m)),*face=kid(root,"CarFace");CHECK(face && face->car_model_count==2);
 CHECK(face->car_motion_count==(!strcmp(page,"CarWashRoot") || !strcmp(code,"lancer_evo7_gta_02")?4:2));
 CHECK(face->car_service_geometry_valid);
 CHECK(face->car_additional_scale==.949999988079071f);
 for(int i=0;i<face->car_model_count;i++){const uint8_t *d=face->car_models[i].data;gt4_model *mesh=d[46] || d[47]?gt4_model_decode_skin_part(d,face->car_models[i].size,0,NULL):gt4_model_decode_part(d,face->car_models[i].size,-1,0);CHECK(mesh && mesh->count>0);gt4_model_free(mesh);}
 /* Back before activation only navigates away; no service or charge. */
 menu_send_input(m,MENU_BACK);ticks(m);CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"GTAutoRoot") && get(vm,game,"money").u.i==10000);
 CHECK(enter_named(m,page));root=ui_root_widget(m->ui,menu_active_root(m));face=kid(root,"CarFace");CHECK(face);
 garage=get(vm,game,"garage").u.o;ah_obj *car=get(vm,garage,"riding_car").u.o;uint8_t body[0x4b8];CHECK(gt4_game_car_body(vm,car,body));
 float worn=24000000,visual=.7f;memcpy(body+0x4a0,&worn,4);memcpy(body+0x4a4,&visual,4);memcpy(body+0x4a8,&worn,4);
 gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body);ah_obj *state=ah_array_new(vm);for(unsigned i=0;i<32;i++)ah_array_push(vm,state,ah_int(body[0x498+i]));ah_set_member(vm,car,"oil_state",ah_objv(state));
 uint8_t expected[0x4b8];memcpy(expected,body,sizeof body);gt4_career_oil_data oil;CHECK(gt4_career_oil_data_load(disc,&oil));
 if(!strcmp(page,"CarWashRoot"))gt4_career_set_dirtiness(expected+0x498,0);
 else gt4_career_replace_oil(expected+0x498,oil.params,oil.settings);
 CHECK(!script_errors && !missing_sounds);waited=max_wait=0;
 ah_val context=ah_objv(m->context);ah_obj *action=widget_module(m,kid(root,button));CHECK(action);
 /* The original calls sureEnoughMoney, without a confirmation dialog.
    Insufficient funds must preserve the complete car and balance. */
 uint8_t untouched[0x4b8];memcpy(untouched,body,sizeof body);
 ah_set_member(vm,game,"money",ah_int(49));confirm_choice=1;CHECK(ah_call_member(vm,action,"onActivate",1,&context,&ret)>=0);CHECK(get(vm,game,"money").u.i==49);
 CHECK(gt4_game_car_body(vm,car,body) && !memcmp(body,untouched,sizeof body));waited=max_wait=0;
 ah_set_member(vm,game,"money",ah_int(10000));CHECK(ah_call_member(vm,action,"onActivate",1,&context,&ret)>=0);CHECK(!script_errors && !missing_sounds);
 int64_t money=get(vm,game,"money").u.i;if(getenv("GT4_GTAUTO_TRANSACTION_NEGATIVE"))money++;
 printf("service money %lld time %.6f max %.6f\n",(long long)money,waited,max_wait);
 CHECK(money==9950 && waited>8 && (!strcmp(page,"CarWashRoot") || max_wait>2));
 for(int i=0;i<face->car_motion_count;i++)CHECK(!face->car_motions[i].loop);
 CHECK(get(vm,widget_module(m,root),"done").u.i==1);
 CHECK(gt4_game_car_body(vm,car,body));for(unsigned i=0;i<sizeof body;i++)if(body[i]!=expected[i])fprintf(stderr,"body %x %02x/%02x\n",i,body[i],expected[i]);CHECK(!memcmp(body,expected,sizeof body));
 uint8_t *packed=NULL;size_t packed_size=0;CHECK(gt4_state_pack(vm,game,&packed,&packed_size));
 ah_obj *restored=ah_module_new(vm,"restored service",NULL,NULL,NULL);CHECK(gt4_state_replace(vm,restored,packed,packed_size));gt4_game_bind(vm,restored);
 CHECK(get(vm,restored,"money").u.i==9950);ah_obj *restored_car=get(vm,get(vm,restored,"garage").u.o,"riding_car").u.o;
 CHECK(gt4_game_car_body(vm,restored_car,body) && !memcmp(body,expected,sizeof body));free(packed);
 ui_widget *copy=ui_widget_copy(m->ui,face);CHECK(copy && copy->car_models!=face->car_models && copy->car_motions!=face->car_motions);
 CHECK(!memcmp(copy->car_models,face->car_models,(size_t)face->car_model_count*sizeof *face->car_models));
 printf("PASS %s %s: two models, %d independent tracks, service charged 50, %.3f s authored waits, done, saved full body and deep clone\n",code,page,face->car_motion_count,waited);
 menu_send_input(m,MENU_BACK);ticks(m);CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"GTAutoRoot"));
 menu_send_input(m,MENU_BACK);ticks(m);ticks(m);CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"TopRoot") && !gt4_save_dirty());
 menu_close(m);CHECK(gt4_save_load_slot(1));m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);menu_save_refresh(m);vm=m->vm;game=game_object(m);garage=get(vm,game,"garage").u.o;car=get(vm,garage,"riding_car").u.o;
 CHECK(get(vm,game,"money").u.i==9950 && gt4_game_car_body(vm,car,body) && !memcmp(body,expected,sizeof body));puts("PASS authored Back/Auto Save and profile reopen preserve service state and money");
 menu_close(m);gt4_save_shutdown();gt4_game_set_service_reader(NULL,NULL);gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
int main(int argc,char **argv){if(argc!=2)return 2;return service_case(argv[1],"ka_01","CarWashRoot","Wash") || service_case(argv[1],"ka_01","OilChangeRoot","Change") || service_case(argv[1],"beat_f_92","OilChangeRoot","Change") || service_case(argv[1],"lancer_evo7_gta_02","OilChangeRoot","Change");}
