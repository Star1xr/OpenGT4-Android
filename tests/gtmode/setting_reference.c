/* User's PCSX2 Setting slots: original Honda BEAT Version F body and
   MGraphFace memory. Disc/savestate data remain local; no physics edits. */
#define main setting_menu_fixture_main
#include "setting_menu.c"
#undef main
#include "career_blob.h"
static float drawn[9][4][2];static unsigned lines;
static void observe(const float xy[4][2],const uint8_t c[4]){
 if(c[0]==78 && c[1]==125 && c[2]==160 && lines<9)memcpy(drawn[lines++],xy,sizeof drawn[0]);
}
static uint32_t word(const uint8_t *p){uint32_t x;memcpy(&x,p,4);return x;}
static float real(const uint8_t *p){float x;memcpy(&x,p,4);return x;}
static uint8_t *read_ram(const char *path){FILE *f=fopen(path,"rb");if(!f)return NULL;uint8_t *p=malloc(32*1024*1024);if(!p || fread(p,1,32*1024*1024,f)!=32*1024*1024){free(p);p=NULL;}fclose(f);return p;}
#ifdef SETTING_DRAW_ORACLE
#include "setting_graph_draw.h"
#endif
int main(int argc,char **argv){
 CHECK(argc==3 && gt4_game_load_disc(argv[1]));uint8_t *ram=read_ram(argv[2]);CHECK(ram);
 /* Original supplied slots contain one Honda body with the same car ID
    in all three 178-byte sheets. No guessed garage values. */
 uint8_t *body=NULL,*original_graph=NULL;unsigned bodies=0,graphs=0;
 for(unsigned p=0;p+0x4b8<32*1024*1024;p+=4){
  if(word(ram+p)==0x254 && word(ram+p+0x178)==0x254 && word(ram+p+0x2f0)==0x254 && word(ram+p+0x488)<3){body=ram+p;bodies++;}
  /* 0029E9D0: vtable +4, type B8, width BC, count C0, points C4. */
  if(word(ram+p+4)==0x66c5f8 && word(ram+p+0xb8)==1 && word(ram+p+0xc0)==10){original_graph=ram+p;graphs++;}
 }
 CHECK(bodies==1 && graphs==1 && real(original_graph+0xbc)==2.f);
 const char *why=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&why);CHECK(vol);
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
 gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears,factory);gt4_game_set_setting_reader(gt4_career_vehicle_setting,factory);
 gt4_save_init(getenv("GT4_SETTING_SAVE_DIR")?getenv("GT4_SETTING_SAVE_DIR"):"");
 char msg[256];menu *source=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(source);menu_enter(source,0);ticks(source);
 ah_vm *vm=source->vm;ah_obj *car=ah_module_new(vm,"Car",NULL,NULL,NULL);CHECK(gt4_game_construct_car(vm,car,"beat_f_92",0,0));
 CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",body,0x4b8));
 ah_val value=ah_objv(car),ret;CHECK(nat_bind_game_car(vm,NULL,source,1,&value,&ret)>=0);
 ah_obj *garage=get(vm,game_object(source),"garage").u.o,*cars=ah_array_new(vm);ah_array_push(vm,cars,value);ah_set_member(vm,garage,"cars",ah_objv(cars));ah_set_member(vm,garage,"riding_index",ah_int(0));menu_save_capture(source);
 if(getenv("GT4_SETTING_SAVE_DIR")){if(!gt4_save_active())CHECK(gt4_save_create(1,"Setting reference"));menu_save_capture(source);CHECK(gt4_save_store());}
 menu *m=menu_open(vol,"setting",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);menu_set_origin(m,"gtmode");CHECK(menu_setting_begin(m));apply_pages(m);ticks(m);
 int main_root=menu_active_root(m);ui_widget *main_page=ui_root_widget(m->ui,main_root);
 ui_widget *space=child(child(child(main_page,"ScrollWindow"),"clip"),"space");
 ui_widget *option=child(child(child(space,"VBox0"),"MUFFLER"),"OptionMenu");CHECK(option);
 ui_widget *item=ui_option_item(option,option->focus_index);CHECK(item && item->insensitive);
 ui_widget *label=child(item,"label"),*labeli=child(item,"labeli");CHECK(label && labeli);
 int d=0,s=0;ui_draw_root(m->ui,main_root,&d,&s);CHECK(s==0);
 /* The supplied original shows Standard in the normal closed face.
    Popup ownership remains disabled; the display clone is independent
    (002C8198/002C9B68). Check visibility, not just stored mask values. */
 if(getenv("GT4_SETTING_CLOSED_NEGATIVE"))labeli->drawn_tick=label->drawn_tick;
 CHECK(label->drawn_tick>0 && label->drawn_tick!=labeli->drawn_tick);
 int root=page(m,"GearRoot");CHECK(root>=0);ticks(m);CHECK(!errors);
 ui_widget *pane=child(ui_root_widget(m->ui,root),"Pane"),*box=child(pane,"HBox"),*column=child(box,"VBox");
 ui_widget *gf=child(child(child(box,"Graph"),"Clip"),"gf");CHECK(gf && gf->graph_count==10);
 for(int i=0;i<10;i++)for(int j=0;j<2;j++)CHECK(fabsf(gf->graph_points[i][j]-real(original_graph+0xc4+i*8+j*4))<0.00002f);
 for(int i=1;i<=8;i++){char name[16];if(i==8)snprintf(name,sizeof name,"GearF");else snprintf(name,sizeof name,"Gear%d",i);
  ui_widget *s=child(child(column,name),"slider");CHECK(s && s->insensitive && !s->option_open);CHECK(child(s,"label") && !strcmp(child(s,"label")->text,"--"));
 }
 setting_quad_observer=observe;ui_draw_root(m->ui,root,&d,&s);CHECK(s==0 && lines==5);
 if(getenv("GT4_SETTING_REFERENCE_NEGATIVE"))drawn[0][2][0]+=1;
#ifdef SETTING_DRAW_ORACLE
 CHECK(setting_graph_draw(ram,original_graph,gf));
#endif
 /* Compare submitted endpoint centres with captured original points,
    using the rectangle authored in setting.mproject (no pixel fitting).
    Convex TL/TR/BL/BR ordering is the platform drawing API contract.
    This proves the adapter, not PS2 stroke coverage/antialias pixels. */
 for(unsigned i=0;i<lines;i++){
  float x=(drawn[i][0][0]+drawn[i][2][0])*.5f,y=(drawn[i][0][1]+drawn[i][2][1])*.5f;
  CHECK(fabsf(x-(76.f+real(original_graph+0xc4+i*16)*210.f))<0.0001f);
  CHECK(fabsf(y-(128.f+(1-real(original_graph+0xc8+i*16))*147.42f))<0.0001f);
  float dx=drawn[i][0][0]-drawn[i][2][0],dy=drawn[i][0][1]-drawn[i][2][1];CHECK(fabsf(sqrtf(dx*dx+dy*dy)-real(original_graph+0xbc))<0.0001f);
  CHECK(fabsf(drawn[i][1][0]-drawn[i][3][0]-dx)<0.0001f && fabsf(drawn[i][1][1]-drawn[i][3][1]-dy)<0.0001f);
 }
 printf("PASS PCSX2 Honda BEAT: 10 captured gear endpoints, 5 independent strokes, original width, 8 disabled fields and zero skipped widgets.\n");
 menu_close(m);menu_close(source);gt4_save_shutdown();gt4_career_vehicle_close(factory);gt4_vol_close(vol);free(ram);return 0;
}
