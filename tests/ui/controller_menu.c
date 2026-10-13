/* Exercise the unmodified disc scripts, including draft/conflict handling. */
#include "../../src/core/menu.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"controller menu line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static ui_widget *child(ui_widget *p,const char *a,const char *b)
{return ui_widget_child(ui_widget_child(p,a),b);}
static int root_index(menu *m,const char *name)
{for(int k=0;k<ui_root_count(m->ui);k++)if(!strcmp(ui_root_name(m->ui,k),name))return k;return -1;}
int main(int argc,char **argv)
{
 CHECK(argc>=3);menu_options_storage(NULL);snprintf(option_path,sizeof option_path,"%s",argv[2]);const char *why;char error[256];
 gt4_vol *vol=gt4_vol_open(argv[1],&why);CHECK(vol);menu *m=menu_open(vol,"option",error,sizeof error);CHECK(m);
 int options=root_index(m,"OptionRoot"),key=root_index(m,"KeyconfigRoot");CHECK(options>=0 && key>=0);
 menu_enter(m,options);ah_obj *page=widget_module(m,ui_root_widget(m->ui,key));
 ah_val args[]={ah_objv(m->context),ah_objv(page)},result;
 CHECK(ah_call_member(m->vm,widget_module(m,ui_root_widget(m->ui,options)),"ds2_port_0",2,args,&result)>=0);
 menu_enter(m,key);menu_tick(m);
 ui_widget *pane=ui_widget_child(ui_root_widget(m->ui,key),"Pane");CHECK(pane);
 const char *names[]={"Steering","Accel","Brake","Reverse","Side","ShiftUp","ShiftDown","BackView","PlayView","Special","Ghost"};
 const int defaults[]={0,1,3,2,0,5,7,6,8,4,9};
 /* Original DS2.setDefault is the independent authored reference. */
 ah_val ds2;CHECK(ah_get_member(m->vm,page,"DS2",&ds2));ah_val option;
 CHECK(ah_get_member(m->vm,game_object(m),"option",&option));
 ah_val defargs[]={ah_objv(m->context),option,ah_int(0)};
 CHECK(ah_call_member(m->vm,ds2.u.o,"setDefault",3,defargs,&result)>=0);
 for(int k=0;k<11;k++)CHECK(child(pane,names[k],"OptionMenu")->focus_index==defaults[k]);
 CHECK(ah_call_member(m->vm,page,"reflect",1,args,&result)>=0);
 for(int k=0;k<11;k++){int expected=defaults[k];if(argc>3 && !k)expected++;CHECK(child(pane,names[k],"OptionMenu")->focus_index==expected);}
 gt4_pad_config before,after;menu_pad_configuration(m,0,&before);
 /* Swap pedal buttons through authored dropdowns: CROSS <-> SQUARE. */
 ui_widget *accel=child(pane,"Accel","OptionMenu"),*brake=child(pane,"Brake","OptionMenu");
 set_focus(m,accel);menu_send_input(m,MENU_RIGHT);CHECK(accel->focus_index==1 && !accel->option_open);
 if(getenv("GT4_CLOSED_OPTION_NEGATIVE"))CHECK(accel->focus_index==3);
 set_focus(m,accel);menu_send_input(m,MENU_ACCEPT);menu_send_input(m,MENU_RIGHT);menu_send_input(m,MENU_RIGHT);menu_send_input(m,MENU_ACCEPT);
 ah_val conflict;CHECK(ah_get_member(m->vm,page,"conflict",&conflict) && ah_truthy(conflict));
 /* Cancel discards the draft. Re-enter through the same authored setup. */
 CHECK(ah_call_member(m->vm,page,"onCancel",1,args,&result)>=0);menu_tick(m);
 menu_pad_configuration(m,0,&after);CHECK(!memcmp(&before,&after,sizeof before));
 CHECK(ah_call_member(m->vm,widget_module(m,ui_root_widget(m->ui,options)),"ds2_port_0",2,args,&result)>=0);
 menu_enter(m,key);menu_tick(m);CHECK(accel->focus_index==1);
 set_focus(m,accel);menu_send_input(m,MENU_ACCEPT);menu_send_input(m,MENU_RIGHT);menu_send_input(m,MENU_RIGHT);menu_send_input(m,MENU_ACCEPT);
 set_focus(m,brake);menu_send_input(m,MENU_ACCEPT);menu_send_input(m,MENU_LEFT);menu_send_input(m,MENU_LEFT);menu_send_input(m,MENU_ACCEPT);
 CHECK(ah_get_member(m->vm,page,"conflict",&conflict) && !ah_truthy(conflict));
 menu_pad_configuration(m,0,&after);CHECK(!memcmp(&before,&after,sizeof before));
 CHECK(ah_call_member(m->vm,page,"apply",1,args,&result)>=0);menu_tick(m);
 menu_pad_configuration(m,0,&after);CHECK(after.button[GT4_PAD_SQUARE]==GT4_PAD_ACCEL && after.button[GT4_PAD_CROSS]==GT4_PAD_BRAKE);
 menu_pad_configuration(NULL,0,&after);CHECK(after.button[GT4_PAD_SQUARE]==GT4_PAD_ACCEL);
 menu_close(m);menu_options_storage(argv[2]);m=menu_open(vol,"option",error,sizeof error);CHECK(m);
 menu_enter(m,options);menu_pad_configuration(m,0,&after);CHECK(after.button[GT4_PAD_CROSS]==GT4_PAD_BRAKE);
 /* 2P is independent and uses the authored DS2 defaults for its port. */
 menu_pad_configuration(m,1,&after);CHECK(after.button[GT4_PAD_CROSS]==GT4_PAD_ACCEL && after.button[GT4_PAD_R1]==-1);
 /* Runtime uses the loaded session, even with different standalone defaults. */
 gt4_save_init(NULL);CHECK(gt4_save_capture(m->vm,game_object(m),0));
 menu_options_storage(NULL);menu_pad_configuration(NULL,0,&after);
 CHECK(after.button[GT4_PAD_SQUARE]==GT4_PAD_ACCEL);
 gt4_pad_defaults(&before,0);ah_val current;CHECK(ah_get_member(m->vm,game_object(m),"option",&current));
 ah_obj *keys=ah_array_new(m->vm);for(int k=0;k<16;k++)ah_array_push(m->vm,keys,ah_int(before.button[k]));
 ah_set_member(m->vm,current.u.o,"pad_buttons_1p",ah_objv(keys));options_capture(m);
 menu_pad_configuration(NULL,0,&after);CHECK(after.button[GT4_PAD_SQUARE]==GT4_PAD_ACCEL);
 menu_save_refresh(m);menu_pad_configuration(m,0,&after);CHECK(after.button[GT4_PAD_CROSS]==GT4_PAD_BRAKE);
 gt4_save_shutdown();menu_close(m);
 /* The pad's own keys reach the scripts: Select (Delete, 0xFFFF) up to
    memcard.ad's MemcardProject.onKeyPress, which marks deleteKeyPress;
    its release clears it. */
 {menu *mc=menu_open(vol,"memcard",error,sizeof error);CHECK(mc);menu_enter(mc,0);menu_tick(mc);
  ah_val flag=ah_nil();menu_send_key(mc,0xffff,0);
  CHECK(ah_get_member(mc->vm,mc->project,"deleteKeyPress",&flag) && ah_truthy(flag));
  menu_send_key(mc,0xffff,1);CHECK(ah_get_member(mc->vm,mc->project,"deleteKeyPress",&flag) && !ah_truthy(flag));
  menu_close(mc);}
 gt4_vol_close(vol);puts("PASS original DS2 defaults/reflect, draft isolation, pedal swap, applied input and persistent reload; negative changes expected steering choice.");return 0;
}
