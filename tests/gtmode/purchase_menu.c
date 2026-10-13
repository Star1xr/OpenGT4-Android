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
static int title_fade_probe,title_fade_alpha,title_fade_frames;
static float label_min_y,label_max_y;
static int label_probe;static float label_width;static unsigned label_glyphs;
plat_texture *plat_texture_create(int w,int h,const uint8_t *p){(void)w;(void)h;(void)p;plat_texture *t=calloc(1,sizeof *t);t->w=w;t->h=h;return t;}
void plat_texture_destroy(plat_texture *t){free(t);}
void plat_draw_quad4(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4]){if(card_probe && t && t->w==640 && t->h==206){card_draws++;if(h!=card_height)card_bad=1;}(void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;}
void plat_draw_solid_quad(const float xy[4][2],const uint8_t c[4]){(void)xy;(void)c;}
void plat_draw_quad(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4]){(void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;}
void plat_draw_quad_blend(const plat_texture *t,float x,float y,float w,float h,
                           float u,float v,float a,float b,const uint8_t c[4],float factor)
{
    if(title_fade_probe && !t && x==0 && y==0 && w==640 && h==480 && !c[0] && !c[1] && !c[2])title_fade_alpha=(int)lroundf(factor*255);
    uint8_t tint[4];memcpy(tint,c,4);tint[3]=(uint8_t)(factor*255.0f+.5f);
    plat_draw_quad(t,x,y,w,h,u,v,a,b,tint);
}
void plat_set_clip(float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;}
void plat_draw_glyph(const plat_texture*t,float x,float y,float w,float h,float b,float s,float u,float v,float u1,float v1,const uint8_t c[4]){
 if(label_probe && t && c[0]==7 && c[1]==11 && c[2]==13 && c[3]>128){
  label_glyphs++;label_width+=w;label_min_y=fminf(label_min_y,y);label_max_y=fmaxf(label_max_y,y+h);
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
static int ui_regression(const char *disc){
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(disc,&err);CHECK(vol && gt4_game_load_disc(disc));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(disc);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 gt4_save_init("");char msg[256];
 for(unsigned region=0;region<3;region++){
  menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);menu_enter(m,0);ticks(m);
  script_errors=0;ah_set_log(m->vm,log_script,NULL);ah_obj *page=get(m->vm,m->project,"UsedCarRoot").u.o;
  const char *names[]={"JP","US","EU"};ah_set_member(m->vm,page,"current_region",ah_str(m->vm,names[region]));
  for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),"UsedCarRoot"))menu_enter(m,i);ticks(m);
  ui_widget *root=ui_root_widget(m->ui,menu_active_root(m));ui_widget *list=kid(kid(root,"ScrollBox"),"SelectBox");CHECK(list && list->nkids>0);
  ah_obj *stock=get(m->vm,get(m->vm,page,"UsedCarModule").u.o,"UsedCarData").u.o;
  const char *key[]={"usedcar_00_carlist","usedcar_01_carlist","usedcar_02_carlist"};
  ah_obj *reference=ah_as_obj(get(m->vm,stock,key[region]),AO_ARRAY);
  unsigned count=(unsigned)list->nkids;if(getenv("GT4_VISIBLE_MENU_NEGATIVE") && !region)count=0;
  CHECK(reference && count==(unsigned)ah_array_size(reference));CHECK(!script_errors);
  printf("PASS used region %s: %u authored weekly entries and initialized rows\n",names[region],count);menu_close(m);
 }
 menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);menu_enter(m,0);ticks(m);script_errors=0;ah_set_log(m->vm,log_script,NULL);
 menu_send_input(m,MENU_LEFT);menu_send_input(m,MENU_LEFT);menu_send_input(m,MENU_ACCEPT);ticks(m);
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"CourseEntryRoot"));
 ui_widget *list=kid(kid(ui_root_widget(m->ui,menu_active_root(m)),"CourseList"),"SelectBox");CHECK(list && list->nkids>0 && !m->preview_courses && !script_errors);
 printf("PASS course list without preview switch: %d entries\n",list->nkids);menu_close(m);
 m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);menu_enter(m,0);ticks(m);script_errors=0;ah_set_log(m->vm,log_script,NULL);
 const menu_input original_route[]={MENU_DOWN,MENU_DOWN,MENU_RIGHT,MENU_RIGHT,MENU_RIGHT,MENU_ACCEPT};
 for(unsigned i=0;i<sizeof original_route/sizeof *original_route;i++){menu_send_input(m,original_route[i]);ticks(m);}
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"CourseEntryRoot"));
 list=kid(kid(ui_root_widget(m->ui,menu_active_root(m)),"CourseList"),"SelectBox");CHECK(list && list->nkids>0 && !m->preview_courses);
 /* Original add_direction L3293/L3295 queries optional actor before creating
    it. The interpreter logs those two nil reads; neither is an exception. */
 CHECK(script_errors==2);printf("PASS original courses without preview switch: %d entries\n",list->nkids);menu_close(m);

 m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);menu_enter(m,0);ticks(m);
 ah_obj *game=game_object(m),*garage=get(m->vm,game,"garage").u.o;ah_val buy[]={ah_str(m->vm,"ka_01"),ah_int(0)},ret;
 CHECK(ah_call_member(m->vm,garage,"addNewCar",2,buy,&ret)==1);ah_val index=ah_int(0);CHECK(ah_call_member(m->vm,garage,"setRidingCar",1,&index,&ret)==1);
 const menu_input inputs[]={MENU_UP,MENU_UP,MENU_ACCEPT,MENU_RIGHT,MENU_RIGHT,MENU_ACCEPT,MENU_RIGHT,MENU_RIGHT,MENU_RIGHT,MENU_ACCEPT};
 for(unsigned i=0;i<sizeof inputs/sizeof *inputs;i++){menu_send_input(m,inputs[i]);ticks(m);}
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"EventHallRoot"));
 list=kid(kid(ui_root_widget(m->ui,menu_active_root(m)),"ScrollBox"),"SelectBox");CHECK(list && list->nkids==3);
 ui_widget *ticket=kid(list->kids[0],"Ticket");CHECK(ticket);card_height=ticket->h;
 if(getenv("GT4_CARD_SIZE_NEGATIVE"))card_height+=1;
 card_probe=1;int drawn,skipped;ui_draw_root(m->ui,menu_active_root(m),&drawn,&skipped);card_probe=0;
 CHECK(card_draws>=3 && !card_bad);printf("PASS Volkswagen cards: %u background quads use authored client height %.0f\n",card_draws,card_height);
 menu_close(m);gt4_save_shutdown();gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
static int shop_catalog_regression(const char *disc){
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(disc,&err);CHECK(vol && gt4_game_load_disc(disc));
 gt4_save_init("");char msg[256];menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);
 load_car_tables(m);ah_vm *vm=m->vm;unsigned tuner_cars=0,black_count=0,total=0;
 ah_obj *parent=ah_module_new(vm,"clear parent",NULL,NULL,NULL),*child=ah_module_new(vm,"clear child",parent,NULL,NULL);ah_val cleared;
 ah_set_member(vm,parent,"inherited",ah_int(7));ah_set_member(vm,child,"owned",ah_int(11));
 CHECK(ah_call_member(vm,child,"clearStatic",0,NULL,&cleared)==1);
 unsigned remaining=(unsigned)ah_object_count(child);if(getenv("GT4_CLEAR_STATIC_NEGATIVE"))remaining++;
 CHECK(!remaining && get(vm,parent,"inherited").u.i==7 && get(vm,child,"owned").t==AV_NIL);
 CHECK(ah_call_member(vm,child,"clearStatic",0,NULL,&cleared)==1);
 /* Asset-boundary test: cached camera equals independently loaded GPB motion,
    at eleven time samples; this is not an EE camera-matrix oracle. */
 size_t package_size=0;uint8_t *package=gt4_vol_read(vol,"/projects/gtmode/US/GTAutoRoot.gpb",&package_size);CHECK(package);
 /* Original entrance motion records: verify local pelvis offsets and
    animated root changes. Full skeletal presentation remains outside proof. */
 const char *motions[]={"image/gtmode/gtauto/GTAutoEntranceMotion.mot","image/gtmode/gtauto/GTAutoEntranceMotion2.mot",
  "image/gtmode/gtauto/carwash_motion1.mot","image/gtmode/gtauto/carwash_motion2.mot"};
 for(unsigned i=0;i<4;i++){
  size_t bytes;uint8_t *raw=gpb_load(package,package_size,motions[i],&bytes);CHECK(raw);
  gt4_mot1 *motion=gt4_mot1_open(raw,bytes);free(raw);CHECK(motion);
  float position[3]={0},rotation[3]={0},later[3]={0},later_rotation[3]={0};
  CHECK(gt4_mot1_transform(motion,"LOWER_SPINE",0,position,rotation)==3);
  if(i<2)CHECK(position[1]==0.15600000321865082f);
  CHECK(gt4_mot1_transform(motion,"PELVIS",0,position,rotation)==3);
  const char *bone_names[]={"PELVIS","LOWER_SPINE","MIDDLE_SPINE","SCAPULA","NECK_BONE","CRANIUM",
   "LCLAVICLE","LUPARM_BONE","LFOREARM_BONE","LHAND_BONE","RCLAVICLE","RUPARM_BONE","RFOREARM_BONE","RHAND_BONE",
   "LTHIGH_BONE","LSHANK_BONE","LFOOT_BONE","RTHIGH_BONE","RSHANK_BONE","RFOOT_BONE"};
  float frames[20][16];CHECK(gt4_mot1_frames(motion,0,bone_names,20,frames,gt4_sincosf)==20);
  if(getenv("GT4_MODEL_FRAME_NEGATIVE"))frames[0][12]+=1;
  for(int k=0;k<3;k++)CHECK(frames[0][12+k]==position[k]);
  for(int b=0;b<20;b++)for(int k=0;k<16;k++)CHECK(isfinite(frames[b][k]));
  /* The two entrance assets translate LOWER_SPINE by +0.156 along
     the parent's y column before applying its local rotation. */
  if(i<2)for(int k=0;k<3;k++)CHECK(fabsf(frames[1][12+k]-(frames[0][12+k]+frames[0][4+k]*.156f))<0.000001f);
  CHECK(gt4_mot1_frames(motion,gt4_mot1_end(motion),bone_names,20,frames,gt4_sincosf)==20);
  CHECK(gt4_mot1_transform(motion,"PELVIS",gt4_mot1_end(motion)*.5f,later,later_rotation)==3);
  if(getenv("GT4_MODEL_CHANNEL_NEGATIVE"))memcpy(later,position,sizeof later);
  CHECK(memcmp(position,later,sizeof position));
  CHECK(!gt4_mot1_transform(motion,"MISSING",0,position,rotation));gt4_mot1_close(motion);
 }
 puts("PASS four original entrance/wash motions: named local channels, animated pelvis and absent targets");
 const char *camera_paths[]={"image/gtmode/gtauto/gtauto_entrance_cam.mot","image/gtmode/gtauto/wheelShop_camera.mot"};
 for(unsigned k=0;k<2;k++){
  size_t bytes=0;uint8_t *data=gpb_load(package,package_size,camera_paths[k],&bytes);
  if(!data){size_t sz=0;uint8_t *pkg=gt4_vol_read(vol,"/projects/gtmode/US/WheelMakerRoot.gpb",&sz);
   if(pkg)data=gpb_load(pkg,sz,camera_paths[k],&bytes);free(pkg);}
  if(!data){size_t sz=0;uint8_t *pkg=gt4_vol_read(vol,"/projects/gtmode/US/WheelShopRoot.gpb",&sz);
   if(pkg)data=gpb_load(pkg,sz,camera_paths[k],&bytes);free(pkg);}
  CHECK(data);
  gt4_mot1 *reference=gt4_mot1_open(data,bytes);free(data);CHECK(reference);
  gt4_mot1 *cached=ui_load_camera(m->ui,0,camera_paths[k]);CHECK(cached && ui_load_camera(m->ui,0,camera_paths[k])==cached);
  for(unsigned j=0;j<11;j++){
   float at=gt4_mot1_end(reference)*(float)j/10,expected[9],actual[9],ef,af;
   CHECK(gt4_mot1_camera(reference,at,expected,&ef) && gt4_mot1_camera(cached,at,actual,&af));
   if(getenv("GT4_OPTION_CAMERA_NEGATIVE") && !k && !j)actual[0]+=1;
   CHECK(!memcmp(expected,actual,sizeof expected) && !memcmp(&ef,&af,sizeof ef));
  }gt4_mot1_close(reference);
 }
 CHECK(!ui_load_camera(m->ui,0,"image/gtmode/gtauto/not_a_camera.mot"));free(package);
 puts("PASS two original GPB option cameras, eleven samples each, cache reuse and missing asset rejection");

 const gt4_career_part_data *rules=gt4_game_part_data();CHECK(rules);
 size_t n=0;uint8_t *tuner_db=gt4_vol_read(vol,"/specdb/GT4_US2560/TUNER_LIST.dbt",&n);CHECK(tuner_db);
 for(unsigned row=0;row<car_u32(m->car_db+8);row++){
  uint8_t generic[24],tuners[10];CHECK(gt4_specdb_row_at(m->car_db,m->car_db_size,row,generic,sizeof generic)==24);
  /* SpecDB row index is not its ID: use the original row header's key. */
  uint32_t id=car_u32(m->car_db+16+row*8);
  ah_obj *car=ah_module_new(vm,"catalog test",NULL,NULL,NULL);ah_set_member(vm,car,"spec_id",ah_int(id));ah_val result;
  CHECK(nat_car_tuners(vm,car,m,0,NULL,&result)==0);ah_obj *list=ah_as_obj(result,AO_ARRAY);CHECK(list);
  unsigned expected=strcmp(rules->tuner_names[generic[16]],"nothing")!=0;
  if(expected)CHECK(!strcmp(ah_cstr(ah_array_get(list,0)),rules->tuner_names[generic[16]]));
  int bytes=gt4_specdb_row(tuner_db,n,id,tuners,sizeof tuners);
  for(int j=0;j<bytes && tuners[j];j++){CHECK(!strcmp(ah_cstr(ah_array_get(list,expected)),rules->tuner_names[tuners[j]]));expected++;}
  if(getenv("GT4_TUNER_CATALOG_NEGATIVE") && !row)expected++;
  CHECK((unsigned)ah_array_size(list)==expected);
  CHECK(nat_car_customization(vm,car,"isChangeWheelDisabled",0,NULL,&result)==0 && result.u.i==(generic[21]!=0));
  CHECK(nat_car_customization(vm,car,"isChangeWingDisabled",0,NULL,&result)==0 && result.u.i==(generic[22]!=0));tuner_cars++;
 }
 free(tuner_db);
 for(unsigned week=0;week<100;week++){
  char name[64];snprintf(name,sizeof name,"usedcar/us_used%04u",week);ah_val arg=ah_str(vm,name),ret;
  ah_obj *module=ah_module_new(vm,"weekly stock test",NULL,NULL,NULL);CHECK(nat_project_load(vm,module,m,1,&arg,&ret)==0);
  ah_obj *stock=ah_as_obj(get(vm,module,"UsedCarData"),AO_MODULE);CHECK(stock);
  unsigned special=0;
  for(unsigned region=0;region<3;region++){
   char key[64];snprintf(key,sizeof key,"usedcar_%02u_carlist",region);ah_obj *cars=ah_as_obj(get(vm,stock,key),AO_ARRAY);CHECK(cars);
   snprintf(key,sizeof key,"color_%02u_carlist",region);ah_obj *colors=ah_as_obj(get(vm,stock,key),AO_ARRAY);CHECK(colors && ah_array_size(colors)==ah_array_size(cars));
   for(int i=0;i<ah_array_size(cars);i++){
    const char *label=ah_cstr(ah_array_get(cars,i));uint32_t id;CHECK(label && gt4_specdb_label(m->car_labels,m->car_labels_size,label,&id));total++;
    if(!strcmp(label,"r390_race_98") || !strcmp(label,"gt_one_race_exxon_99") || !strcmp(label,"_787b_91") || !strcmp(label,"r92cp_92"))special++;
   }
  }
  if(week==99){if(getenv("GT4_BLACK_STOCK_NEGATIVE"))special=0;CHECK(special==4);black_count=special;}
 }
 gt4_career_vehicle *factory=gt4_career_vehicle_open(disc);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 ah_obj *garage=get(vm,game_object(m),"garage").u.o;ah_val args[]={ah_str(vm,"ka_01"),ah_int(0)},result;
 CHECK(ah_call_member(vm,garage,"addNewCar",2,args,&result)==1);ah_val index=ah_int(0);CHECK(ah_call_member(vm,garage,"setRidingCar",1,&index,&result)==1);
 ah_obj *owned=get(vm,garage,"riding_car").u.o;uint8_t before[0x4b8],actual[0x4b8];CHECK(gt4_game_car_body(vm,owned,before));unsigned changes=0;
 for(unsigned kind=0;kind<2;kind++){
  char path[128];snprintf(path,sizeof path,"/specdb/GT4_US2560/%s.idi",kind?"WING":"WHEEL");size_t size=0;uint8_t *idi=gt4_vol_read(vol,path,&size);CHECK(idi);
  for(unsigned row=0;row<car_u32(idi+4);row++){
   uint32_t id=car_u32(idi+20+row*8);const char *label=gt4_specdb_label_for_id(idi,size,id);CHECK(label);
   for(unsigned cat=0;cat<(kind?1u:4u);cat++){
    CHECK(gt4_career_blob_set(vm,owned,"full_car_ps2",before,sizeof before));ah_val choice[]={ah_str(vm,rules->wheel_categories[cat]),ah_str(vm,label)};
    CHECK(ah_call_member(vm,owned,kind?"setWing":"setTireCategoryWheel",kind?1:2,choice+(kind?1:0),&result)==1);
    CHECK(ah_call_member(vm,owned,kind?"haveWing":"haveTireCategoryWheel",kind?1:2,choice+(kind?1:0),&result)==1 && result.u.i==1);
    uint8_t expected[0x4b8];memcpy(expected,before,sizeof expected);uint64_t key=((uint64_t)(kind?30:29)<<32)|id;
    if(!strcmp(label,kind?"p000":"w000"))key=UINT64_MAX;
    if(kind)for(unsigned sheet=0;sheet<3;sheet++)memcpy(expected+sheet*0x178+0x100,&key,8);
    else memcpy(expected+0x468+cat*8,&key,8);
    CHECK(gt4_game_car_body(vm,owned,actual));if(getenv("GT4_CUSTOMIZATION_NEGATIVE") && !changes)actual[0x100]^=1;
    CHECK(!memcmp(expected,actual,sizeof actual));changes++;
   }
  }free(idi);
 }
 /* Original 0042F3C0 / 0042F408 defined fields and Adhoc calendar bridge. */
 gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears,factory);
 CHECK(gt4_career_blob_set(vm,owned,"full_car_ps2",before,sizeof before));
 CHECK(ah_call_member(vm,owned,"setupPartsBuffer",0,NULL,&result)==1);
 unsigned gear_installs=0;
 for(unsigned stage=0;stage<3;stage++){
  ah_val selection[]={ah_int(11),ah_int(stage)},key;
  CHECK(ah_call_member(vm,owned,"_getSettingPartKey",2,selection,&key)==1);
  if(key.u.i==-1)continue;
  CHECK(ah_call_member(vm,owned,"changeParts",2,selection,&result)==1);
  CHECK(gt4_game_car_body(vm,owned,actual));uint64_t fitted;memcpy(&fitted,actual+0x48,8);CHECK(fitted==(uint64_t)key.u.i);
  uint8_t ratios[24];memcpy(ratios,actual+0x110,sizeof ratios);CHECK(gt4_career_vehicle_auto_gears(factory,actual));
  if(getenv("GT4_AUTO_GEARS_NEGATIVE"))ratios[0]^=1;
  CHECK(!memcmp(ratios,actual+0x110,sizeof ratios));gear_installs++;
 }
 CHECK(gear_installs>0);printf("PASS %u actual gearbox installs and repeated native automatic ratio generation\n",gear_installs);
 gt4_game_set_auto_gears(NULL,NULL);
 ah_obj *calendar=get(vm,game_object(m),"calendar").u.o;
 unsigned event_count=(unsigned)ah_array_size(get(vm,calendar,"original_events").u.o);
 for(unsigned wing=0;wing<2;wing++){
  const char *label=wing?"p001":"brdg001w";ah_val key;
  CHECK(nat_calendar_custom_key(vm,calendar,(void *)(intptr_t)wing,1,(ah_val[]){ah_str(vm,label)},&key)==0 && key.u.i!=-1);
  for(unsigned category=0;category<(wing?1u:4u);category++){
   uint8_t record[16],expected[16];memset(record,0x93,sizeof record);memcpy(expected,record,16);
   expected[0]=wing?7:6;if(!wing){expected[1]=(uint8_t)category;expected[2]=0x23;}
   uint64_t k=(uint64_t)key.u.i;memcpy(expected+8,&k,8);
   gt4_career_calendar_custom_event(record,wing,k,category,0x123);
   CHECK(!memcmp(record,expected,16));
   ah_obj *date=get(vm,calendar,"today").u.o;
   gt4_career_calendar_data dates;CHECK(gt4_career_calendar_data_load(disc,&dates));
   int32_t prior=gt4_career_date_to_day(&dates,(int32_t)ah_array_get(date,0).u.i,(int32_t)ah_array_get(date,1).u.i,(int32_t)ah_array_get(date,2).u.i);
   ah_val choices[]={ah_str(vm,label),ah_str(vm,rules->wheel_categories[category]),ah_int(0x123)};
   CHECK(ah_call_member(vm,calendar,wing?"putBuyWingEvent":"putBuyWheelEvent",wing?1:3,choices,&result)==1);
   ah_obj *events=get(vm,calendar,"original_events").u.o;CHECK((unsigned)ah_array_size(events)==++event_count);
   ah_obj *last=ah_array_get(events,event_count-1).u.o;
   memset(expected,0,16);gt4_career_calendar_custom_event(expected,wing,k,category,0x123);
   if(getenv("GT4_CUSTOM_EVENT_NEGATIVE"))expected[8]^=1;
   for(unsigned j=0;j<16;j++)CHECK(ah_array_get(last,j).u.i==expected[j]);
   date=get(vm,calendar,"today").u.o;
   CHECK(gt4_career_date_to_day(&dates,(int32_t)ah_array_get(date,0).u.i,(int32_t)ah_array_get(date,1).u.i,(int32_t)ah_array_get(date,2).u.i)==prior+1);
  }
 }
 puts("PASS wheel/wing original event fields, reserved-byte preservation and five native day advances");
 gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);
 printf("PASS %u car tuner catalogs and wheel/wing restrictions; 100 weekly stocks, %u entries; %u special black race cars in week 99; %u wheel/wing body changes\n",tuner_cars,total,black_count,changes);
 menu_close(m);gt4_save_shutdown();gt4_vol_close(vol);return 0;
}
/* UI transaction harness: model loading is covered separately by showroom_probe. */
static int transaction_model_loader(menu *m,const ui_widget *w,void *u){(void)m;(void)u;return w && w->car_code;}
static int confirm_choice=1;
static int purchase_dialog(menu *m,const char *t,const char *v,int type,const char *y,const char *n){(void)m;(void)t;(void)v;(void)y;(void)n;return type==1?confirm_choice:1;}
static int enter_named(menu *m,const char *name){for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),name)){menu_enter(m,i);ticks(m);return 1;}return 0;}
static int customization_purchase(const char *disc){
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(disc,&err);CHECK(vol && gt4_game_load_disc(disc));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(disc);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 char directory[160];snprintf(directory,sizeof directory,"build/career-native/custom-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);
 char msg[256];menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);
 ah_obj *game=game_object(m),*garage=get(vm,game,"garage").u.o;ah_val args[]={ah_str(vm,"ka_01"),ah_int(0)},ret;
 CHECK(ah_call_member(vm,garage,"addNewCar",2,args,&ret)==1);ah_val index=ah_int(0);CHECK(ah_call_member(vm,garage,"setRidingCar",1,&index,&ret)==1);
 ah_set_member(vm,game,"money",ah_int(10000));CHECK(gt4_save_capture(vm,game,1));CHECK(gt4_save_create(1,"Customization validation"));CHECK(gt4_save_capture(vm,game,1));CHECK(gt4_save_store());menu_save_refresh(m);menu_set_dialog_presenter(m,purchase_dialog);menu_set_car_loader(m,transaction_model_loader,NULL);
 CHECK(enter_named(m,"GTAutoRoot"));
 double world[4],direction[4],ambient[4],diffuse[4],specular[4];
 ui_light_defaults(world,direction,ambient,diffuse,specular);
 if(getenv("GT4_LIGHT_DEFAULT_NEGATIVE"))diffuse[0]=0;
 for(int k=0;k<3;k++){
  float a=(float)(ambient[k]/255),d=(float)(diffuse[k]/255),s=(float)(specular[k]/255);uint32_t ab,db,sb;
  memcpy(&ab,&a,4);memcpy(&db,&d,4);memcpy(&sb,&s,4);
  CHECK(world[k]==0 && direction[k]==(k==1) && ab==0x3e999999 && db==0x3f000000 && sb==0x3f000000);
 }
 ui_widget *crew=kid(ui_root_widget(m->ui,menu_active_root(m)),"CarFace");CHECK(crew && crew->car_crew_data && crew->car_crew_motion);
 /* Record the actual original GTAutoRoot label glyphs, whose small
    alignment rectangles must not squeeze their authored 0.95 scale. */
 for(int i=0;i<60;i++)menu_animate(m,1.f/60);
 const char *labels[]={"Wash","Oil","Wheel","Wing"};
 for(int i=0;i<4;i++){
  ui_widget *label=kid(kid(ui_root_widget(m->ui,menu_active_root(m)),labels[i]),"label");CHECK(label);
  float original_w=label->w;CHECK(label->scale_x==.95f);
  memcpy(label->runtime_color,(uint8_t[]){7,11,13,255},4);label->has_runtime_color=1;
  label_probe=1;label_width=0;label_glyphs=0;ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
  CHECK(label_glyphs>5);float narrow_width=label_width;
  label->w=640;label_width=0;ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
  if(getenv("GT4_LABEL_SCALE_NEGATIVE"))narrow_width*=.5f;
  CHECK(narrow_width==label_width);label->w=original_w;label->has_runtime_color=0;label_probe=0;
 }
 CHECK(!crew->car_crew_loop && !crew->car_camera_loop);
 float duration=gt4_mot1_end(crew->car_crew_motion);ui_animate(m->ui,menu_active_root(m),duration+1);
 ui_car_prepare_draw(crew);
 CHECK(crew->car_crew_time>duration && crew->car_crew_playing);
 CHECK(gt4_service_sample(crew->car_crew_time,1,duration,gt4_mot1_loop(crew->car_crew_motion),0)==duration);
 CHECK(crew->car_camera_time==gt4_mot1_end(crew->car_camera));
 ah_val model=car_model_value(vm,widget_module(m,crew),crew),loop=ah_int(1);
 CHECK(ah_call_member(vm,model.u.o,"setModelMotionLoop",1,&loop,&ret)==1 && crew->car_camera_loop);
 float retained=crew->car_crew_time;
 CHECK(ah_call_member(vm,model.u.o,"playModelMotion",0,NULL,&ret)==1 && crew->car_crew_playing);
 if(getenv("GT4_CREW_PLAYBACK_NEGATIVE"))crew->car_crew_time=0;
 CHECK(crew->car_crew_time==retained); /* 00153E20 does not restart. */
 ui_animate(m->ui,menu_active_root(m),1.f/60);ui_car_prepare_draw(crew);
 CHECK(crew->car_crew_time>retained && crew->car_camera_playing);
 garage=get(vm,game_object(m),"garage").u.o;
 const menu_input route[]={MENU_RIGHT,MENU_RIGHT,MENU_ACCEPT,MENU_ACCEPT,MENU_ACCEPT};
 for(unsigned i=0;i<sizeof route/sizeof *route;i++){menu_send_input(m,route[i]);ticks(m);}
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"WheelShopRoot"));
 uint8_t cancellation_body[0x4b8],cancellation_after[0x4b8];
 garage=get(vm,game_object(m),"garage").u.o;CHECK(gt4_game_car_body(vm,get(vm,garage,"riding_car").u.o,cancellation_body));
 confirm_choice=0;menu_send_input(m,MENU_ACCEPT);ticks(m);CHECK(get(vm,game_object(m),"money").u.i==10000);
 CHECK(gt4_game_car_body(vm,get(vm,garage,"riding_car").u.o,cancellation_after) && !memcmp(cancellation_body,cancellation_after,sizeof cancellation_body));
 CHECK(ah_array_size(get(vm,get(vm,game_object(m),"calendar").u.o,"original_events").u.o)==0);
 confirm_choice=1;menu_send_input(m,MENU_ACCEPT);ticks(m);
 CHECK(get(vm,game_object(m),"money").u.i==9500);
 const menu_input wing_route[]={MENU_RIGHT,MENU_RIGHT,MENU_RIGHT,MENU_ACCEPT,MENU_RIGHT};
 CHECK(enter_named(m,"GTAutoRoot"));
 for(unsigned i=0;i<sizeof wing_route/sizeof *wing_route;i++){menu_send_input(m,wing_route[i]);ticks(m);}
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"WingShopRoot"));
 confirm_choice=0;menu_send_input(m,MENU_ACCEPT);ticks(m);CHECK(get(vm,game_object(m),"money").u.i==9500);
 confirm_choice=1;menu_send_input(m,MENU_ACCEPT);ticks(m);CHECK(get(vm,game_object(m),"money").u.i==8300);
 game=game_object(m);garage=get(vm,game,"garage").u.o;ah_obj *car=get(vm,garage,"riding_car").u.o;
 uint8_t body[0x4b8],restored[0x4b8];CHECK(gt4_game_car_body(vm,car,body));
 char fitted_wheel[32],fitted_wing[32];CHECK(menu_photo_parts(m,fitted_wheel,fitted_wing));
 if(getenv("GT4_PURCHASE_PART_LABEL_NEGATIVE"))fitted_wheel[0]=0;
 CHECK(!strcmp(fitted_wheel,"brdg001w") && !strcmp(fitted_wing,"p001"));
 ah_val choice=ah_str(vm,"p001");CHECK(ah_call_member(vm,car,"haveWing",1,&choice,&ret)==1 && ret.u.i==1);
 ah_val wheel_choice[]={ah_str(vm,gt4_game_part_data()->wheel_categories[0]),ah_str(vm,"brdg001w")};
 CHECK(ah_call_member(vm,car,"haveTireCategoryWheel",2,wheel_choice,&ret)==1 && ret.u.i==1);
 ah_obj *calendar=get(vm,game,"calendar").u.o;ah_obj *events=get(vm,calendar,"original_events").u.o;CHECK(ah_array_size(events)==2);
 CHECK(ah_array_get(ah_array_get(events,0).u.o,0).u.i==6 && ah_array_get(ah_array_get(events,1).u.o,0).u.i==7);
 /* GTAutoRoot.onCancel L11078..11080 returns through authored checkAutoSave. */
 menu_send_input(m,MENU_BACK);ticks(m);ticks(m);
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"TopRoot") && !gt4_save_dirty());
 menu_close(m);CHECK(gt4_save_load_slot(1));
 m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);vm=m->vm;menu_save_refresh(m);game=game_object(m);garage=get(vm,game,"garage").u.o;car=get(vm,garage,"riding_car").u.o;
 CHECK(gt4_game_car_body(vm,car,restored));if(getenv("GT4_PURCHASE_CUSTOM_NEGATIVE"))restored[0x100]^=1;
 CHECK(!memcmp(body,restored,sizeof body) && get(vm,game,"money").u.i==8300);
 CHECK(menu_photo_parts(m,fitted_wheel,fitted_wing));
 CHECK(!strcmp(fitted_wheel,"brdg001w") && !strcmp(fitted_wing,"p001"));
 CHECK(ah_array_size(get(vm,get(vm,game,"calendar").u.o,"original_events").u.o)==2);
 CHECK(enter_named(m,"TopRoot"));
 ui_widget *space=kid(kid(kid(ui_root_widget(m->ui,menu_active_root(m)),"ScrollWindow"),"clip"),"space");CHECK(space);
 unsigned notices=0;
 for(int i=0;i<space->nkids;i++){
  ui_widget *item=space->kids[i];const char *text=ah_cstr(get(vm,widget_module(m,item),"notice"));
  if(text && (strstr(text,"tune") || strstr(text,"Tune"))){ui_widget *ex=kid(item,"ex");CHECK(ex && ex->visible);notices++;fprintf(stderr,"tuning notice %s: %s\n",item->name,text);}
 }
 if(getenv("GT4_TUNING_NOTICE_NEGATIVE"))notices=0;
 CHECK(notices>0);
 puts("PASS authored wheel/wing purchases, cancellation, money, installation, authored Auto Save/reopen and active tuning notices");
 menu_close(m);gt4_save_shutdown();gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
static int special_purchase(const char *disc){
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(disc,&err);CHECK(vol && gt4_game_load_disc(disc));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(disc);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 char directory[160];snprintf(directory,sizeof directory,"build/career-native/special-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);
 char msg[256];menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);ah_vm *vm=m->vm;menu_save_capture(m);CHECK(gt4_save_create(1,"Special purchase"));menu_save_refresh(m);menu_set_dialog_presenter(m,purchase_dialog);menu_set_car_loader(m,transaction_model_loader,NULL);
 ah_obj *game=game_object(m),*garage=get(vm,game,"garage").u.o,*calendar=get(vm,game,"calendar").u.o;
 ah_set_member(vm,game,"money",ah_int(20000000));
 gt4_career_calendar_data dates;CHECK(gt4_career_calendar_data_load(disc,&dates));
 uint32_t packed=gt4_career_day_to_date(&dates,gt4_career_date_to_day(&dates,2005,4,2)+99*7);
 ah_obj *date=ah_array_new(vm);ah_array_push(vm,date,ah_int(packed&65535));ah_array_push(vm,date,ah_int((packed>>16)&255));ah_array_push(vm,date,ah_int(packed>>24));ah_set_member(vm,calendar,"today",ah_objv(date));
 CHECK(gt4_save_capture(vm,game,1) && gt4_save_store());
 const char *names[]={"r390_race_98","gt_one_race_exxon_99","_787b_91","r92cp_92"};
 int colors[4]={-1,-1,-1,-1};ah_val module_arg=ah_str(vm,"usedcar/us_used0099"),ret;
 ah_obj *module=ah_module_new(vm,"special stock",NULL,NULL,NULL);CHECK(nat_project_load(vm,module,m,1,&module_arg,&ret)==0);ah_obj *stock=get(vm,module,"UsedCarData").u.o;
 for(unsigned region=0;region<3;region++){
  char field[64];snprintf(field,sizeof field,"usedcar_%02u_carlist",region);ah_obj *cars=get(vm,stock,field).u.o;
  snprintf(field,sizeof field,"color_%02u_carlist",region);ah_obj *paint=get(vm,stock,field).u.o;
  for(int i=0;i<ah_array_size(cars);i++)for(unsigned j=0;j<4;j++)if(!strcmp(ah_cstr(ah_array_get(cars,i)),names[j])){ah_val paint_value=ah_array_get(paint,i);const char *text=ah_cstr(paint_value);colors[j]=text?atoi(text):(int)paint_value.u.i;}
 }
 ah_val sources[4]={ah_nil(),ah_nil(),ah_nil(),ah_nil()},stock_ids[4],stock_weeks[4];
 const char *regions[]={"JP","US","EU"};
 /* UsedCarRoot L5617..5635 supplies the actual deterministic mileage/seed;
    use the visible stock's garage objects rather than fabricated used inputs. */
 for(unsigned region=0;region<3;region++){
  if(m->depth)finalize_page(m,menu_active_root(m));
  ah_obj *page=get(vm,m->project,"UsedCarRoot").u.o;
  ah_set_member(vm,page,"current_region",ah_str(vm,regions[region]));CHECK(enter_named(m,"UsedCarRoot"));
  ui_widget *list=kid(kid(ui_root_widget(m->ui,menu_active_root(m)),"ScrollBox"),"SelectBox");CHECK(list && list->nkids);
  for(int row=0;row<list->nkids;row++){
   ah_obj *item=widget_module(m,list->kids[row]);ah_val source=get(vm,item,"gcar");
   const char *label=ah_cstr(get(vm,source.u.o,"car_code"));CHECK(label);
   for(unsigned i=0;i<4;i++)if(!strcmp(label,names[i])){
    sources[i]=source;stock_ids[i]=get(vm,item,"idx");stock_weeks[i]=get(vm,item,"week");
    /* C locals do not root VM objects across page ticks/collection. */
    ah_set_member(vm,ah_root(vm),names[i],source);
    uint8_t body[0x4b8];CHECK(gt4_game_car_body(vm,source.u.o,body) && body[0x18]==colors[i]);
    CHECK(!ah_num(get(vm,item,"soldout")));
   }
  }
 }
 int64_t credits=20000000;uint8_t saved[4][0x4b8];
 for(unsigned i=0;i<4;i++){
  CHECK(colors[i]>=0 && sources[i].t==AV_OBJ);ah_val label=ah_str(vm,names[i]),source=sources[i];
  ah_val price;CHECK(ah_call_member(vm,source.u.o,"getUsedPrice",0,NULL,&price)==1 && price.u.i>0);
  garage=get(vm,game_object(m),"garage").u.o;CHECK(ah_call_member(vm,garage,"addUsedCar",1,&source,&ret)==1 && ret.u.i==i);
  calendar=get(vm,game_object(m),"calendar").u.o;ah_val event[]={label,ah_int(colors[i])};CHECK(ah_call_member(vm,calendar,"putBuyUsedCarEvent",2,event,&ret)==1);
  ah_val sold[]={stock_weeks[i],stock_ids[i]};CHECK(ah_call_member(vm,get(vm,game_object(m),"used_car").u.o,"set",2,sold,&ret)==1);
  ah_obj *page=get(vm,m->project,"GetCarRoot").u.o;
  ah_val open[]={ah_objv(m->context),label,ah_int(colors[i]),source,price,ah_int(i)};CHECK(ah_call_member(vm,page,"open",6,open,&ret)==1);menu_tick(m);apply_pages(m);
  ah_val finish[]={ah_objv(m->context),ah_int(1)};CHECK(ah_call_member(vm,page,"start_phase1",2,finish,&ret)==1);ticks(m);
  credits-=price.u.i;game=game_object(m);garage=get(vm,game,"garage").u.o;CHECK(get(vm,game,"money").u.i==credits);
  CHECK(gt4_game_car_body(vm,ah_array_get(get(vm,garage,"cars").u.o,i).u.o,saved[i]));CHECK(saved[i][0x18]==colors[i]);
 }
 CHECK(gt4_save_load_slot(1));menu_save_refresh(m);game=game_object(m);garage=get(vm,game,"garage").u.o;CHECK(get(vm,game,"money").u.i==credits && ah_array_size(get(vm,garage,"cars").u.o)==4);
 for(unsigned i=0;i<4;i++){uint8_t actual[0x4b8];CHECK(gt4_game_car_body(vm,ah_array_get(get(vm,garage,"cars").u.o,i).u.o,actual));if(getenv("GT4_SPECIAL_PURCHASE_NEGATIVE") && !i)actual[0x18]^=1;CHECK(!memcmp(actual,saved[i],sizeof actual));}
 for(unsigned i=0;i<4;i++){
  ah_val sold[]={stock_weeks[i],stock_ids[i]};CHECK(ah_call_member(vm,get(vm,game,"used_car").u.o,"get",2,sold,&ret)==1 && ah_num(ret));
 }
 puts("PASS four visible black stock cars with authored mileage/paint, GetCarRoot purchase/Auto Save, credits, bodies and sold flags after reload; acquisition bridge is explicit");
 menu_close(m);gt4_save_shutdown();gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
static int purchase_specs(const char *disc){
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(disc,&err);CHECK(vol && gt4_game_load_disc(disc));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(disc);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
 gt4_save_init("");char msg[256];menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);menu_enter(m,0);ticks(m);ah_vm *vm=m->vm;
 ah_obj *page=get(vm,m->project,"UsedCarRoot").u.o;ah_set_member(vm,page,"current_region",ah_str(vm,"US"));CHECK(enter_named(m,"UsedCarRoot"));ticks(m);
 ui_widget *list=kid(kid(ui_root_widget(m->ui,menu_active_root(m)),"ScrollBox"),"SelectBox");CHECK(list && list->nkids);
 const char *methods[]={"getCatEngineType","getCatEngineCam","getCatDisplacement","getCatLength","getCatWidth","getCatHeight","getCatPs","getCatTq","getCatMs","getCurrentPs","getCurrentTq","getCurrentMs"};
 unsigned checked=0;script_errors=0;ah_set_log(vm,log_script,NULL);ah_obj *first_car=NULL;
 for(int i=0;i<list->nkids;i++){
  ah_obj *car=get(vm,widget_module(m,list->kids[i]),"gcar").u.o;CHECK(car);
  if(!i)first_car=car;
  for(unsigned j=0;j<12;j++){
   ah_val arg=ah_str(vm,j==6 || j==9?"%d%s/%s%s":j==7 || j==10?"%.2t%s/%s%s":"%d%s"),ret=ah_nil();
   CHECK(ah_call_member(vm,car,methods[j],j>=6?1:0,&arg,&ret)==1);
   const char *text=ah_cstr(ret);if(getenv("GT4_PURCHASE_SPECS_NEGATIVE") && !checked)text="nil:nil";
   CHECK(text && *text && !strstr(text,"nil"));checked++;
   if(!i)printf("%s %s: %s\n",ah_cstr(get(vm,car,"car_code")),methods[j],text);
  }
 }
 CHECK(!script_errors);printf("PASS %u populated used-car specification fields\n",checked);
 ah_obj *dialog=get(vm,m->project,"BuyCarDialog").u.o;CHECK(dialog);
 ah_set_member(vm,dialog,"catalog",ah_int(0));ah_set_member(vm,dialog,"current_gcar",ah_objv(first_car));
 CHECK(enter_named(m,"BuyCarDialog"));ticks(m);CHECK(!script_errors);
 ui_widget *box=kid(kid(ui_root_widget(m->ui,menu_active_root(m)),"Pane"),"VBox");CHECK(box);
 const char *lines[]={"Line1","Line2","Line3","Line4","Line6","Line9"};
 for(unsigned i=0;i<6;i++){ui_widget *value=kid(kid(box,lines[i]),"td");CHECK(value && value->text && *value->text && !strstr(value->text,"nil"));printf("BuyCarDialog %s: %s\n",lines[i],value->text);}
 /* CR-X fields are the original ENGINE/CHASSIS/RACINGMODIFY disc rows,
    not a substitute catalog assembled from external specifications. */
 CHECK(!strcmp(kid(kid(box,"Line1"),"td")->text,"L4:DOHC"));
 CHECK(!strcmp(kid(kid(box,"Line2"),"td")->text,"1595cc"));
 CHECK(!strcmp(kid(kid(box,"Line6"),"td")->text,"3800x1675x1270"));
 CHECK(!strcmp(kid(kid(box,"Line9"),"td")->text,"986kg"));
 puts("PASS authored BuyCarDialog initialization with used CR-X and no missing-method errors");
 menu_close(m);gt4_game_set_stats_reader(NULL,NULL);gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_save_shutdown();gt4_vol_close(vol);return 0;
}
static int garage_sell_event(menu *m,void *user){
 (void)user;const char *root=ui_root_name(m->ui,menu_active_root(m));
 if(root && !strcmp(root,"BuyCarDialog")){menu_send_input(m,MENU_RIGHT);menu_send_input(m,MENU_ACCEPT);return 1;}
 return 0;
}
static int home_garage(const char *disc){
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(disc,&err);CHECK(vol && gt4_game_load_disc(disc));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(disc);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
 char directory[160];snprintf(directory,sizeof directory,"build/career-native/home-garage-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);char msg[256];menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);menu_set_car_loader(m,transaction_model_loader,NULL);menu_enter(m,0);ticks(m);ah_vm *vm=m->vm;
 ah_obj *garage=get(vm,game_object(m),"garage").u.o;ah_val ret,arg;
 const char *cars[]={"ka_01","crx_sir_90","_120i_04"};
 for(unsigned i=0;i<3;i++){ah_val args[]={ah_str(vm,cars[i]),ah_int(0)};CHECK(ah_call_member(vm,garage,"addNewCar",2,args,&ret)==1);}
 arg=ah_int(0);CHECK(ah_call_member(vm,garage,"setRidingCar",1,&arg,&ret)==1);
 CHECK(gt4_save_capture(vm,game_object(m),1));CHECK(gt4_save_create(1,"Home Garage validation"));CHECK(gt4_save_capture(vm,game_object(m),1));CHECK(gt4_save_store());
 menu_set_dialog_presenter(m,purchase_dialog);menu_save_capture(m);CHECK(enter_named(m,"HomeRoot"));ticks(m);
 garage=get(vm,game_object(m),"garage").u.o;ah_val riding=get(vm,garage,"riding_car"),index;
 CHECK(ah_call_member(vm,riding.u.o,"getGarageIndex",0,NULL,&index)==1 && index.u.i==0);
 script_errors=0;ah_set_log(vm,log_script,NULL);
 ah_obj *home=get(vm,m->project,"HomeRoot").u.o,*button=get(vm,get(vm,home,"Buttons").u.o,"Garage").u.o;CHECK(button);
 arg=ah_objv(m->context);CHECK(ah_call_member(vm,button,"onActivate",1,&arg,&ret)==1);ticks(m);
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"GarageRoot"));CHECK(!script_errors);
 ah_obj *page=get(vm,m->project,"GarageRoot").u.o;ah_obj *list=ah_as_obj(get(vm,page,"list_"),AO_ARRAY);CHECK(list && ah_array_size(list)==3);
 unsigned count=(unsigned)ah_array_size(list);if(getenv("GT4_HOME_GARAGE_NEGATIVE"))count--;
 CHECK(count==3);
 for(unsigned i=0;i<3;i++){
  ah_obj *car=ah_array_get(list,i).u.o;CHECK(car);ah_val idx,power;
  CHECK(ah_call_member(vm,car,"getGarageIndex",0,NULL,&idx)==1 && idx.u.i==(int64_t)i);
  CHECK(ah_call_member(vm,car,"getCurrentPower",0,NULL,&power)==1 && power.u.i>0);
  CHECK(ah_cstr(get(vm,car,"short_name_no_year")) && *ah_cstr(get(vm,car,"short_name_no_year")));
 }
 for(unsigned i=0;i<3;i++)for(int field=0;field<10;field++){
  ah_val args[]={ah_objv(m->context),ah_array_get(list,i),ah_int(field)},text;
  CHECK(ah_call_member(vm,page,"getItemText",3,args,&text)==1);
  CHECK(ah_cstr(text) && *ah_cstr(text) && !strstr(ah_cstr(text),"nil"));
 }
 ah_val countries;CHECK(ah_call_member(vm,garage,"pickupCountry",0,NULL,&countries)==1 && ah_array_size(countries.u.o)==3);
 ah_val filters[]={ah_array_get(countries.u.o,0),ah_str(vm,"all"),ah_str(vm,"all")},filtered;
 CHECK(ah_call_member(vm,garage,"pickup",3,filters,&filtered)==1 && ah_array_size(filtered.u.o)==1);
 CHECK(ah_call_member(vm,garage,"pickupTuner",1,filters,&filtered)==1 && ah_array_size(filtered.u.o)==1);
 ah_obj *menu_=get(vm,get(vm,ah_root(vm),"main").u.o,"menu").u.o;
 ah_val sortargs[]={ah_objv(list),ah_int(0)};
 CHECK(ah_call_member(vm,get(vm,menu_,"MCarGarage").u.o,"sortByRideHistory",2,sortargs,&ret)==1);
 CHECK(ah_call_member(vm,ah_array_get(list,2).u.o,"getGarageIndex",0,NULL,&index)==1 && index.u.i==0);
 const char *sorts[]={"sortByPower","sortByWeight","sortByPWR","sortByDate","sortByPrice","sortByYear","sortByCountry","sortByTuner","sortByName","sortByDriveTrain","sortByColor","sortByAspiration"};
 for(unsigned i=0;i<12;i++){
  sortargs[1]=ah_int(1);CHECK(ah_call_member(vm,get(vm,menu_,"MCarGarage").u.o,sorts[i],2,sortargs,&ret)==1 && ah_array_size(list)==3);
  sortargs[1]=ah_int(0);CHECK(ah_call_member(vm,get(vm,menu_,"MCarGarage").u.o,sorts[i],2,sortargs,&ret)==1 && ah_array_size(list)==3);
 }
 sortargs[1]=ah_int(0);CHECK(ah_call_member(vm,get(vm,menu_,"MCarGarage").u.o,"sortByPower",2,sortargs,&ret)==1);
 CHECK(!strcmp(ah_cstr(get(vm,ah_array_get(list,0).u.o,"car_code")),"crx_sir_90"));
 sortargs[1]=ah_int(1);CHECK(ah_call_member(vm,get(vm,menu_,"MCarGarage").u.o,"sortByRideHistory",2,sortargs,&ret)==1);
 /* Open the CR-X, then change the selected car through authored UI. */
 CHECK(!script_errors);
 int crx_row=-1;for(int i=0;i<ah_array_size(list);i++)if(!strcmp(ah_cstr(get(vm,ah_array_get(list,i).u.o,"car_code")),"crx_sir_90"))crx_row=i;CHECK(crx_row>=0);
 ah_set_member(vm,page,"index_",ah_int(crx_row));ah_set_member(vm,page,"page_",ah_int(0));
 ah_obj *list_widget=get(vm,get(vm,page,"List").u.o,"Cars").u.o;
 ah_set_member(vm,list_widget,"index_",ah_int(crx_row));
 ah_val activate[]={ah_objv(m->context),ah_nil()};
 CHECK(ah_call_member(vm,list_widget,"onActivate",2,activate,&ret)==1);ticks(m);
 CHECK(!script_errors);CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"CarViewRoot"));
 CHECK(!script_errors);
 ah_obj *view=get(vm,m->project,"CarViewRoot").u.o,*change=get(vm,get(vm,view,"Pane").u.o,"Change").u.o;
 arg=ah_objv(m->context);CHECK(ah_call_member(vm,change,"onActivate",1,&arg,&ret)==1);ticks(m);
 CHECK(!script_errors);garage=get(vm,game_object(m),"garage").u.o;
 CHECK(get(vm,garage,"riding_index").u.i==1);
 CHECK(gt4_save_load_slot(1));menu_save_refresh(m);garage=get(vm,game_object(m),"garage").u.o;
 CHECK(get(vm,garage,"riding_index").u.i==1 && ah_array_size(get(vm,garage,"cars").u.o)==3);
 page=get(vm,m->project,"GarageRoot").u.o;ah_set_member(vm,page,"index_",ah_int(1));ah_set_member(vm,page,"page_",ah_int(0));
 list_widget=get(vm,get(vm,page,"List").u.o,"Cars").u.o;ah_set_member(vm,list_widget,"index_",ah_int(1));
 CHECK(ah_call_member(vm,list_widget,"onActivate",2,activate,&ret)==1);ticks(m);CHECK(!script_errors);
 view=get(vm,m->project,"CarViewRoot").u.o;ah_val current=get(vm,view,"current_car"),price;
 CHECK(ah_call_member(vm,current.u.o,"getUsedSellPrice",0,NULL,&price)==1 && price.u.i>0);
 int64_t credits=get(vm,game_object(m),"money").u.i;
 menu_set_event_presenter(m,garage_sell_event,NULL);
 ah_obj *assessment=get(vm,get(vm,view,"Pane").u.o,"Assessment").u.o;arg=ah_objv(m->context);
 CHECK(ah_call_member(vm,assessment,"onActivate",1,&arg,&ret)==1);ticks(m);
 CHECK(!script_errors);garage=get(vm,game_object(m),"garage").u.o;
 CHECK(get(vm,garage,"count").u.i==2 && get(vm,game_object(m),"money").u.i==credits+price.u.i);
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"GarageRoot"));
 for(unsigned i=0;i<11;i++){ah_val args[]={ah_str(vm,"ka_01"),ah_int(0)};CHECK(ah_call_member(vm,garage,"addNewCar",2,args,&ret)==1);}
 arg=ah_objv(m->context);page=get(vm,m->project,"GarageRoot").u.o;
 ah_set_member(vm,page,"garage_",ah_objv(garage));
 CHECK(ah_call_member(vm,page,"pickupCars",0,NULL,&ret)==1);CHECK(ah_array_size(get(vm,page,"list_").u.o)==13);
 ah_val empty_car=get(vm,garage,"riding_car"),empty_index;CHECK(ah_call_member(vm,empty_car.u.o,"getGarageIndex",0,NULL,&empty_index)==1);
 ah_set_member(vm,page,"page_",ah_int(1));CHECK(ah_call_member(vm,page,"updatePage",1,&arg,&ret)==1);
 ui_widget *rows=kid(kid(ui_root_widget(m->ui,menu_active_root(m)),"List"),"Cars");CHECK(rows);
 unsigned visible=0;for(int i=0;i<rows->nkids;i++)visible+=rows->kids[i]->visible!=0;
 CHECK(visible==3 && !script_errors);
 puts("PASS authored Home Garage: list, 30 fields, 13 sorts both directions, filters, CarView, Change/Auto Save/reload, sale and 13-car pagination");menu_close(m);gt4_game_set_stats_reader(NULL,NULL);gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_save_shutdown();gt4_vol_close(vol);return 0;
}
static int home_status(const char *disc){
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(disc,&err);CHECK(vol && gt4_game_load_disc(disc));char error[256];gt4_save_init("");
 menu *m=menu_open(vol,"gtmode",error,sizeof error);CHECK(m);ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);script_errors=0;
 CHECK(enter_named(m,"HomeRoot"));ticks(m);CHECK(!script_errors);
 ah_obj *game=game_object(m),*stats=profile_module(vm,game,"statistics"),*calendar=get(vm,game,"calendar").u.o;ah_val ret;
 ah_obj *positions=ah_array_new(vm);const int counts[]={8,4,2,1,1,0,0};for(unsigned i=0;i<7;i++)ah_array_push(vm,positions,ah_int(counts[i]));ah_set_member(vm,stats,"race_positions",ah_objv(positions));
 ah_set_member(vm,stats,"a_spec_distance",ah_int(1234567));ah_set_member(vm,stats,"b_spec_distance",ah_int(9876543));
 ah_set_member(vm,stats,"prize_cars",ah_int(3));ah_set_member(vm,stats,"prize_money",ah_int(12345));
 uint8_t experience[0x180];memset(experience,255,sizeof experience);CHECK(gt4_career_blob_set(vm,stats,"b_spec_experience_ps2",experience,sizeof experience));
 const char *names[]={"getBspecTotalExperience","getBspecCarExperience","getBspecCourseExperience","getBspecBattleExperience"};
 for(unsigned i=0;i<4;i++){CHECK(ah_call_member(vm,game,names[i],0,NULL,&ret)==1 && ret.u.i==(i?100:10000));}
 ah_obj *option=get(vm,game,"option").u.o;ah_set_member(vm,option,"unit_velocity",ah_str(vm,"KMH"));CHECK(ah_call_member(vm,game,"getAspecDistance",0,NULL,&ret)==1 && !strcmp(ah_cstr(ret),"1,234 km"));
 ah_set_member(vm,option,"unit_velocity",ah_str(vm,"MPH"));CHECK(ah_call_member(vm,game,"getBspecDistance",0,NULL,&ret)==1 && !strcmp(ah_cstr(ret),"6,137 miles"));
 CHECK(enter_named(m,"StatusRoot"));ticks(m);CHECK(!script_errors);ui_widget *root=ui_root_widget(m->ui,menu_active_root(m));
 const char *fields[]={"WinningPercentage","Trophy","PresentCar","PrizeMoney","AspecTravelDistance","BspecTravelDistance"};
 for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){ui_widget *w=kid(kid(root,fields[i]),"value");CHECK(w && w->text && *w->text);printf("Home status %s: %s\n",fields[i],w->text);}
 CHECK(strstr(kid(kid(root,"WinningPercentage"),"value")->text,"50.0"));
 ui_widget *percentage=kid(kid(root,"WinningPercentage"),"value");percentage->has_runtime_color=1;percentage->runtime_color[0]=7;percentage->runtime_color[1]=11;percentage->runtime_color[2]=13;percentage->runtime_color[3]=255;
 int drawn,skipped;label_probe=1;label_glyphs=0;label_width=0;ui_draw_root(m->ui,menu_active_root(m),&drawn,&skipped);unsigned escaped_glyphs=label_glyphs;float escaped_width=label_width;
 char *escaped_text=percentage->text;percentage->text=strdup("8 / 16 = 50.0%");label_glyphs=0;label_width=0;ui_draw_root(m->ui,menu_active_root(m),&drawn,&skipped);label_probe=0;CHECK(escaped_glyphs==label_glyphs && escaped_width==label_width);free(percentage->text);percentage->text=escaped_text;
 ah_val arg=ah_str(vm,"nurburgring");CHECK(ah_call_member(vm,game,"isCourseAvailable",1,&arg,&ret)==1 && ah_truthy(ret));
 ah_obj *available=get(vm,game,"available").u.o;uint64_t keys[128];CHECK(course_saved(m,available,keys));unsigned unlocked=0;for(unsigned i=0;i<128;i++)unlocked+=keys[i]!=UINT64_MAX;CHECK(unlocked>0 && unlocked<128);
 /* A course absent from boot's list remains locked until a time-present
    script invokes the original setter. Selection itself never grants it. */
 const char *locked=NULL;for(unsigned id=0;id<65536 && !locked;id++){const char *label=gt4_specdb_label_for_id(m->course_labels,m->course_labels_size,id);if(label && !gt4_career_course_available(keys,UINT64_C(0x2200000000)|id,0))locked=label;}CHECK(locked);
 arg=ah_str(vm,locked);CHECK(ah_call_member(vm,game,"isCourseAvailable",1,&arg,&ret)==1 && !ah_truthy(ret));
 ah_set_member(vm,available,"all",ah_int(1));CHECK(ah_call_member(vm,game,"isCourseAvailable",1,&arg,&ret)==1 && ah_truthy(ret));ah_set_member(vm,available,"all",ah_int(0));CHECK(ah_call_member(vm,game,"isCourseAvailable",1,&arg,&ret)==1 && !ah_truthy(ret));
 CHECK(ah_call_member(vm,game,"setCourseAvailable",1,&arg,&ret)==1);CHECK(ah_call_member(vm,game,"isCourseAvailable",1,&arg,&ret)==1 && ah_truthy(ret));
 ah_obj *date=ah_array_new(vm);ah_array_push(vm,date,ah_int(2005));ah_array_push(vm,date,ah_int(4));ah_array_push(vm,date,ah_int(11));ah_set_member(vm,calendar,"today",ah_objv(date));
 ah_obj *events=ah_array_new(vm);uint64_t event_keys[]={0,0,0,UINT64_C(0x2300000000),UINT64_C(0x2300000000),UINT64_C(0x2300000000),UINT64_C(0x1d00000000),UINT64_C(0x1e00000000),UINT64_C(0x2200000000)};char labels_expected[9][128]={{0}};
 const char *tables[]={NULL,"GENERIC_CAR","GENERIC_CAR","RACE","RACE","RACE","WHEEL","WING","COURSE"};
 for(unsigned i=1;i<9;i++){char path[96];snprintf(path,sizeof path,"/specdb/GT4_US2560/%s.idi",tables[i]);size_t bytes;uint8_t *labels=gt4_vol_read(vol,path,&bytes);CHECK(labels);const char *label=NULL;unsigned id;
  for(id=0;id<65536 && !label;id++)label=gt4_specdb_label_for_id(labels,bytes,id);CHECK(label && strlen(label)<128);strcpy(labels_expected[i],label);event_keys[i]|=id-1;free(labels);}
 for(unsigned i=0;i<9;i++){uint8_t e[16]={0};e[0]=(uint8_t)i;e[1]=0;e[2]=1;e[3]=2;uint32_t time=10150;memcpy(e+4,&time,4);memcpy(e+8,event_keys+i,8);ah_obj *record=ah_array_new(vm);for(unsigned j=0;j<16;j++)ah_array_push(vm,record,ah_int(e[j]));ah_array_push(vm,events,ah_objv(record));}ah_set_member(vm,calendar,"original_events",ah_objv(events));
 ah_obj *menu_=get(vm,get(vm,ah_root(vm),"main").u.o,"menu").u.o;ah_val instance;CHECK(ah_call(vm,get(vm,menu_,"MCalendar"),0,NULL,&instance)==0);
 const int lengths[]={1,4,4,5,5,5,4,2,6};
 for(unsigned i=0;i<9;i++){ah_val args[]={ah_int(2005),ah_int(4),ah_int(2+i)};CHECK(ah_call_member(vm,instance.u.o,"getEvent",3,args,&ret)==1);CHECK(ah_array_size(ret.u.o)==lengths[i]);CHECK(!strcmp(ah_cstr(ah_array_get(ret.u.o,0)),gt4_game_home_data()->event_names[i]));if(i)CHECK(ah_cstr(ah_array_get(ret.u.o,1)) && !strcmp(ah_cstr(ah_array_get(ret.u.o,1)),labels_expected[i]));}
 CHECK(ah_call_member(vm,instance.u.o,"getEvent",0,NULL,&ret)==1 && ret.t==AV_NIL);
 CHECK(enter_named(m,"LogRoot"));ticks(m);menu_send_input(m,MENU_LEFT);ticks(m);CHECK(!script_errors);uint64_t saved_keys[128];CHECK(course_saved(m,available,saved_keys));
 uint8_t *saved;size_t size;CHECK(gt4_state_pack(vm,game,&saved,&size));CHECK(gt4_state_replace(vm,game,saved,size));free(saved);gt4_game_bind(vm,game);
 CHECK(gt4_career_blob_get(vm,get(vm,game,"statistics").u.o,"b_spec_experience_ps2",experience,sizeof experience));
 CHECK(ah_call_member(vm,game,"getTotalRaceCount",0,NULL,&ret)==1);if(getenv("GT4_HOME_STATUS_NEGATIVE"))ret.u.i++;CHECK(ret.u.i==16);
 CHECK(ah_array_size(get(vm,get(vm,game,"calendar").u.o,"original_events").u.o)==9);
 CHECK(course_saved(m,get(vm,game,"available").u.o,keys));CHECK(!memcmp(keys,saved_keys,sizeof keys));
 printf("PASS Home/Status/Diary authored initialization, B-Spec four maxima, two distance units, seven counters, %u boot unlocks, locked/set/override, nine diary schemas, profile serialization\n",unlocked);
 menu_close(m);gt4_save_shutdown();gt4_vol_close(vol);return 0;
}
static int selection_labels(menu*m,int root,ui_widget*w,unsigned*count){
 if(!strcmp(w->type,"TextFace") && !strcmp(w->name,"info")){
  ui_widget*bg=ui_widget_child(w->parent,"bg");const char*edge=bg?mp_string(bg->src,"t_image_path"):NULL;
  if(edge && strstr(edge,"/bt_")){
   CHECK(w->y==20);free(w->text);w->text=strdup("242 cars");memcpy(w->runtime_color,(uint8_t[]){7,11,13,255},4);w->has_runtime_color=1;
   const char*name=w->name;if(getenv("GT4_SELECTION_COUNT_NEGATIVE"))w->name="label";
   label_probe=1;label_glyphs=0;label_min_y=1e9f;label_max_y=-1e9f;ui_draw_root(m->ui,root,NULL,NULL);CHECK(label_glyphs>0);
   float actual_min=label_min_y,actual_max=label_max_y;w->name=name;
   /* Independent authored rectangle reference: remove the unrelated button
      image lookup, preserving the original count TextFace's geometry. */
   const char*bg_name=bg->name;bg->name="reference_bg";label_glyphs=0;label_min_y=1e9f;label_max_y=-1e9f;ui_draw_root(m->ui,root,NULL,NULL);bg->name=bg_name;
   CHECK(label_glyphs>0 && actual_min==label_min_y && actual_max==label_max_y);
   w->has_runtime_color=0;label_probe=0;(*count)++;
  }
 }
 for(int i=0;i<w->nkids;i++)CHECK(!selection_labels(m,root,w->kids[i],count));return 0;
}
static int selection_layout(const char*disc){
 const char*err;gt4_vol*v=gt4_vol_open(disc,&err);CHECK(v);char msg[256];menu*m=menu_open(v,"arcade",msg,sizeof msg);CHECK(m);unsigned count=0;
 for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),"MakerRoot") || !strcmp(ui_root_name(m->ui,i),"HistoryRoot"))CHECK(!selection_labels(m,i,ui_root_widget(m->ui,i),&count));
 CHECK(count>=4);printf("PASS %u authored car count faces retain y=20 below button labels y=3; exact glyph-coordinate comparison\n",count);menu_close(m);gt4_vol_close(v);return 0;
}
static void title_wait(menu*m,float seconds,void*user){
 (void)user;int previous=-1;
 for(int i=0;i<(int)ceilf(seconds*60);i++){
  menu_animate(m,1.f/60);title_fade_alpha=0;ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
  if(title_fade_alpha<previous){fprintf(stderr,"title fade reversed\n");exit(1);}previous=title_fade_alpha;title_fade_frames++;
 }
}
static int title_exit(const char*disc){
 const char*err;gt4_vol*v=gt4_vol_open(disc,&err);CHECK(v);char msg[256];menu*m=menu_open(v,"arcade",msg,sizeof msg);CHECK(m);
 CHECK(enter_named(m,"TopRoot"));for(int i=0;i<60;i++)menu_animate(m,1.f/60);
 ah_obj*opt=submodule(m->vm,game_object(m),"option");ah_set_member(m->vm,opt,"demo_movie_interval",ah_int(1));
 attract_origin[0]=0;attract_left=0;attract_cancelled=0;title_fade_probe=1;title_fade_frames=0;menu_set_presenter(m,title_wait,NULL);
 menu_send_input(m,MENU_BACK);menu_tick(m);CHECK(title_fade_frames>0);
 char next[64];CHECK(menu_take_project(m,next,sizeof next) && !strcmp(next,"boot:MovieRoot"));
 if(getenv("GT4_TITLE_EXIT_NEGATIVE"))ui_fade(m->ui,0,0,0);
 title_fade_alpha=0;ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);CHECK(title_fade_alpha==255);
 for(int i=0;i<60;i++){menu_animate(m,1.f/60);title_fade_alpha=0;ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);CHECK(title_fade_alpha==255);}
 title_fade_probe=0;attract_left=0;menu_close(m);gt4_vol_close(v);puts("PASS original TopRoot Cancel: monotonic outgoing fade, movie dispatch, 60 black fields without a title flash");return 0;
}
int main(int argc,char **argv){
 if(argc==3 && !strcmp(argv[2],"--title-exit"))return title_exit(argv[1]);
 if(argc==3 && !strcmp(argv[2],"--selection-layout"))return selection_layout(argv[1]);
 if(argc==3 && !strcmp(argv[2],"--home-status"))return home_status(argv[1]);
 if(argc==3 && !strcmp(argv[2],"--home-garage"))return home_garage(argv[1]);
 if(argc==3 && !strcmp(argv[2],"--purchase-specs"))return purchase_specs(argv[1]);
 if(argc==3 && !strcmp(argv[2],"--special-purchase"))return special_purchase(argv[1]);
 if(argc==3 && !strcmp(argv[2],"--customization-purchase"))return customization_purchase(argv[1]);
 if(argc==3 && !strcmp(argv[2],"--shop-catalog"))return shop_catalog_regression(argv[1]);
 if(argc==3 && !strcmp(argv[2],"--ui-regression"))return ui_regression(argv[1]);
 if(argc!=4){fprintf(stderr,"usage: gtmode_purchase_menu ISO BEFORE-RAM AFTER-RAM\n");return 2;}
 uint8_t *before=malloc(32u*1024*1024),*after=malloc(32u*1024*1024);CHECK(before && after && load(argv[2],before,32u*1024*1024) && load(argv[3],after,32u*1024*1024));
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&err);CHECK(vol && gt4_game_load_disc(argv[1]));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 char directory[160];snprintf(directory,sizeof directory,"build/career-native/purchase-menu-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);
 char error[256];menu *m=menu_open(vol,"gtmode",error,sizeof error);CHECK(m);menu_save_capture(m);CHECK(gt4_save_create(1,"Purchase validation"));menu_save_refresh(m);ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);
 ah_obj *game=game_object(m),*garage=get(vm,game,"garage").u.o,*calendar=get(vm,game,"calendar").u.o;
 ah_set_member(vm,game,"money",ah_int((int64_t)gt4_career_money_get(before+0xa0cf58)));
 gt4_career_blob_set(vm,garage,"original_units_ps2",before+0xa18840+8,32000);
 ah_obj *history=ah_array_new(vm);for(unsigned i=0;i<1000;i++){int64_t h;memcpy(&h,before+0xa18840+0x20+i*32,8);ah_array_push(vm,history,ah_int(h));}ah_set_member(vm,garage,"original_unit_history",ah_objv(history));
 ah_obj *date=ah_array_new(vm);int32_t day;memcpy(&day,before+0xa0d178+0xb6c0,4);int fields[3];CHECK(gt4_game_decode_calendar_day(day,fields));for(unsigned i=0;i<3;i++)ah_array_push(vm,date,ah_int(fields[i]));ah_set_member(vm,calendar,"today",ah_objv(date));
 ah_obj *events=ah_array_new(vm);uint32_t count,first;memcpy(&count,before+0xa0d178+0x10,4);memcpy(&first,before+0xa0d178+0xc,4);
 for(unsigned i=0;i<count;i++){ah_obj *e=ah_array_new(vm);for(unsigned j=0;j<16;j++)ah_array_push(vm,e,ah_int(before[0xa0d178+0x18+((first+i)%2922)*16+j]));ah_array_push(vm,events,ah_objv(e));}ah_set_member(vm,calendar,"original_events",ah_objv(events));
 ah_obj *main_=get(vm,ah_root(vm),"main").u.o,*menu_=get(vm,main_,"menu").u.o;
 ah_val label=ah_str(vm,"beat_f_92"),source,ret;CHECK(ah_call(vm,get(vm,menu_,"MCarGarage"),1,&label,&source)==0);
 ah_val used[]={ah_int(123),ah_int(0),ah_int(85574576)};CHECK(ah_call_member(vm,source.u.o,"makeUsed",3,used,&ret)==1);
 ah_val price;CHECK(ah_call_member(vm,source.u.o,"getUsedPrice",0,NULL,&price)==1 && price.u.i==4857);
 CHECK(ah_call_member(vm,garage,"addUsedCar",1,&source,&ret)==1 && ret.u.i==0);
 ah_val event[]={label,ah_int(0)};CHECK(ah_call_member(vm,calendar,"putBuyUsedCarEvent",2,event,&ret)==1);
 ah_obj *page=get(vm,m->project,"GetCarRoot").u.o;CHECK(page);
 ah_val open[]={ah_objv(m->context),label,ah_int(0),source,price,ah_int(0)};
 CHECK(ah_call_member(vm,page,"open",6,open,&ret)==1);menu_tick(m);apply_pages(m);garage=get(vm,game,"garage").u.o;
 ah_val finish[]={ah_objv(m->context),ah_int(1)};CHECK(ah_call_member(vm,page,"start_phase1",2,finish,&ret)==1);
 /* Comparator control: a one-credit error must reject the transaction. */
 if(getenv("GT4_CAREER_TEST_NEGATIVE"))ah_set_member(vm,game,"money",ah_int(get(vm,game,"money").u.i+1));
 CHECK(get(vm,game,"money").u.i==1082 && get(vm,garage,"riding_index").u.i==0);
 ah_obj *car=ah_array_get(get(vm,garage,"cars").u.o,0).u.o;uint8_t unit[32],body[0x4b8];CHECK(gt4_game_car_unit(vm,car,unit) && gt4_game_car_body(vm,car,body));
 CHECK(!memcmp(unit,after+0xa18840+8,32) && !memcmp(body,after+0xa18840+0x7d10,sizeof body));
 CHECK(script_errors==0);CHECK(gt4_save_load_slot(1));menu_save_refresh(m);game=game_object(m);garage=get(vm,game,"garage").u.o;
 car=ah_array_get(get(vm,garage,"cars").u.o,0).u.o;CHECK(get(vm,game,"money").u.i==1082 && get(vm,garage,"riding_index").u.i==0);
 CHECK(gt4_game_car_unit(vm,car,unit) && gt4_game_car_body(vm,car,body) && !memcmp(unit,after+0xa18840+8,32) && !memcmp(body,after+0xa18840+0x7d10,sizeof body));
 menu_close(m);gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);gt4_save_shutdown();free(before);free(after);
 puts("PASS authored GetCarRoot purchase/selection/native Auto Save and reload; actual BEAT unit/body/money identical; zero script errors");return 0;
}
