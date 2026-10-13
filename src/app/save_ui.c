#include "save_ui.h"
#include "save.h"
#include "save_ps2.h"
#include "ps2_media.h"
#include "plat_sdl.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif
static save_ui *screen;
static int action(ah_vm *,ah_obj *,void *,int,const ah_val *,ah_val *);
static void heading(save_ui *,ui_widget *,const char *);
static int complete_first_run(save_ui *s){
 if(!s->onboarding)return 0;s->first_run=s->onboarding=0;
 menu_request_project(s->owner,menu_option_number(s->owner,"opening_after_autoload",1)?"boot:MovieRoot":"arcade");
 return 1;
}
static void text(ui_widget *w,const char *s){if(w){free(w->text);w->text=strdup(s);}}
static void tip(save_ui *s,const char *value){ui_set_tooltips(menu_active_root(s->owner)==s->name_root?s->entry:s->page,value);}
static int none(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)vm;(void)self;(void)data;(void)argc;(void)argv;*ret=ah_int(0);return 0;}
void save_ui_name(save_ui *s,const char *name){snprintf(s->name,sizeof s->name,"%s",name?name:"");text(s->name_label,s->name);}
static void refresh(save_ui *s){menu_save_refresh(s->owner);for(int i=0;i<3;i++){int slot=s->first+i+1;ui_widget *card=s->cards[i];card->visible=slot<=GT4_SAVE_SLOTS;gt4_save_info info;int status=gt4_save_info_get(slot,&info);char title[160],details[192];snprintf(title,sizeof title,"%s",status>0?info.name:status<0?"Savegame Unavailable":"Create New Savegame");if(status>0){time_t t=(time_t)info.timestamp;struct tm *local=localtime(&t);char date[80]="";if(local)strftime(date,sizeof date,"%d/%m/%Y  %H:%M",local);snprintf(details,sizeof details,"Slot %d  |  %s%s%s",slot,slot==gt4_save_active()?"Current player  |  ":"",date,status==2?"  (backup)":"");}else snprintf(details,sizeof details,"Slot %d  |  %s",slot,status<0?"Keep this file for recovery":"Start a new player profile");text(ui_widget_child(card,"label"),title);text(ui_widget_child(card,"details"),details);}char page[64];snprintf(page,sizeof page,"%d / 3",s->first/3+1);text(s->pagination,page);}
static int enter_cards(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)vm;(void)self;(void)argc;(void)argv;save_ui *s=data;s->editing=0;SDL_StopTextInput();
 for(int i=0;i<s->page->nkids;i++){
  ui_widget *child=s->page->kids[i];
  int decoration=child->name && (!strcmp(child->name,"bg") || !strcmp(child->name,"ToolTip"));
  child->visible=decoration?1:child==s->welcome?s->first_run:!s->first_run;
 }
 if(s->first_run){heading(s,s->page,"Welcome to OpenGT4");menu_native_focus(s->owner,ui_widget_child(s->welcome,"Begin"));tip(s,"Create a player profile, import a savegame, or continue without saving.");*ret=ah_int(0);return 0;}
 heading(s,s->page,"Save Games");refresh(s);menu_native_focus(s->owner,s->cards[0]);tip(s,"Select a savegame to load, or an empty card to create a new player.");*ret=ah_int(0);return 0;}
static int enter_name(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)vm;(void)self;(void)argc;(void)argv;save_ui *s=data;s->editing=1;s->typed=0;SDL_StartTextInput();text(s->name_label,s->name);ui_widget *lower=ui_widget_child(s->keyboard,"LowerCase"),*upper=ui_widget_child(s->keyboard,"UpperCase");lower->visible=1;upper->visible=0;menu_native_focus(s->owner,ui_widget_child(lower,"key_q"));tip(s,"Enter your name using the keyboard. Select ENTER to confirm; Escape cancels.");*ret=ah_int(0);return 0;}
static void backspace(save_ui *s){size_t n=strlen(s->name);if(n){do{n--;}while(n && ((unsigned char)s->name[n]&0xc0)==0x80);s->name[n]=0;text(s->name_label,s->name);}}
static void append(save_ui *s,const char *input){size_t n=strlen(s->name),add=strlen(input);if(n+add>=sizeof s->name){tip(s,"The player name is too long.");return;}char candidate[97];snprintf(candidate,sizeof candidate,"%s%s",s->name,input);if(!n && candidate[0]>='a' && candidate[0]<='z')candidate[0]-='a'-'A';int chars=0;for(const unsigned char *p=(const unsigned char *)candidate;*p;p++)if((*p&0xc0)!=0x80)chars++;if(chars>24){tip(s,"Maximum 24 characters.");return;}save_ui_name(s,candidate);menu_play_effect(s->owner,"param");}
static void name_done(save_ui *s){if(!gt4_save_valid_name(s->name)){tip(s,"Enter a player name (1 to 24 characters).");menu_play_effect(s->owner,"disable");return;}menu_save_capture(s->owner);int ok=s->rename?gt4_save_rename(s->name):gt4_save_create(s->slot,s->name);if(!ok){tip(s,gt4_save_error());menu_play_effect(s->owner,"disable");return;}s->editing=0;SDL_StopTextInput();menu_play_effect(s->owner,"select");if(complete_first_run(s))return;if(s->enter_gtmode){s->enter_gtmode=0;menu_request_project(s->owner,"gtmode");}else menu_native_page(s->owner,s->root);}
static int key_action(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)vm;(void)self;(void)argc;(void)argv;save_ui *s=screen;const char *key=data;if(!strcmp(key,"key_enter"))name_done(s);else if(strstr(key,"backspace")){backspace(s);menu_play_effect(s->owner,"cancel");}else if(strstr(key,"caps")){ui_widget *lower=ui_widget_child(s->keyboard,"LowerCase"),*upper=ui_widget_child(s->keyboard,"UpperCase");lower->visible=!lower->visible;upper->visible=!upper->visible;menu_native_focus(s->owner,ui_widget_child(lower->visible?lower:upper,"key_q"));menu_play_effect(s->owner,"param");}else if(!strncmp(key,"key_",4) && key[4]){char c[2]={key[4],0};ui_widget *upper=ui_widget_child(s->keyboard,"UpperCase");if(upper->visible && c[0]>='a' && c[0]<='z')c[0]-=32;append(s,c);}*ret=ah_int(2);return 0;}
static int cancel_name(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)vm;(void)self;(void)argc;(void)argv;save_ui *s=data;s->editing=0;SDL_StopTextInput();menu_native_page(s->owner,s->root);menu_play_effect(s->owner,"cancel");*ret=ah_int(2);return 0;}
static void bind_keys(save_ui *s,ui_widget *w){if(w->name && !strncmp(w->name,"key_",4))menu_native_method(s->owner,w,"onActivate",key_action,(void *)w->name);for(int i=0;i<w->nkids;i++)bind_keys(s,w->kids[i]);}
static int choose_import(char path[1200],int media){
#ifdef _WIN32
 wchar_t chosen[1200]=L"";OPENFILENAMEW open={0};open.lStructSize=sizeof open;SDL_SysWMinfo info;SDL_VERSION(&info.version);if(SDL_GetWindowWMInfo(plat_sdl_window(),&info))open.hwndOwner=info.info.win.window;open.lpstrFilter=media?L"GT4 media archives\0*.psu;*.ps2\0All files\0*.*\0":L"GT4 savegames\0*.psu;*.ps2;*GAMEDATA\0All files\0*.*\0";open.lpstrFile=chosen;open.nMaxFile=1200;open.lpstrTitle=media?L"Import GT4 Media":L"Import GT4 Savegame";open.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(!GetOpenFileNameW(&open))return 0;return WideCharToMultiByte(CP_UTF8,0,chosen,-1,path,1200,NULL,NULL)!=0;
#else
 (void)path;return 0;
#endif
}
static int action(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)argc;(void)argv;(void)self;save_ui *s=screen;const char *op=data;int ok=0;
 if(!strcmp(op,"Begin")){
  s->slot=0;for(int i=1;i<=GT4_SAVE_SLOTS;i++){gt4_save_info info;if(gt4_save_info_get(i,&info)==0){s->slot=i;break;}}
  if(!s->slot){tip(s,"No empty profile slots. Select an existing player profile.");goto end;}
  s->rename=0;save_ui_name(s,"");menu_native_page(s->owner,s->name_root);menu_play_effect(s->owner,"select");goto end;
 }
 if(!strcmp(op,"Profiles")){s->first_run=0;enter_cards(vm,self,s,0,NULL,ret);goto end;}
 if(!strcmp(op,"Skip")){complete_first_run(s);menu_play_effect(s->owner,"select");goto end;}
 if(!strcmp(op,"Back") && s->first_run){complete_first_run(s);goto end;}
 if(!strcmp(op,"Back")){ah_val main,game,origin;const char *project="arcade";if(ah_get_member(vm,ah_root(vm),"main",&main) && ah_get_member(vm,main.u.o,"game",&game) && ah_get_member(vm,game.u.o,"last_menu_project",&origin) && ah_cstr(origin) && strcmp(ah_cstr(origin),"save"))project=ah_cstr(origin);menu_request_project(s->owner,project);menu_play_effect(s->owner,"cancel");goto end;}
 if(!strcmp(op,"Previous") || !strcmp(op,"Next")){s->first=(s->first+(!strcmp(op,"Next")?3:6))%9;refresh(s);menu_native_focus(s->owner,s->cards[0]);menu_play_effect(s->owner,"cursor");goto end;}
 if(!strcmp(op,"Card")){int slot=s->slot;gt4_save_info info;int status=gt4_save_info_get(slot,&info);if(status<0){tip(s,"This savegame could not be read. Its recovery files were preserved.");goto end;}if(gt4_save_dirty() && !menu_confirm(s->owner,"Save Game","Discard unsaved changes and switch player?"))goto end;if(!status){s->rename=0;menu_native_page(s->owner,s->name_root);menu_play_effect(s->owner,"select");goto end;}ok=gt4_save_load_slot(slot);}
 else if(!strcmp(op,"Save")){menu_save_capture(s->owner);ok=gt4_save_store();}
 else if(!strcmp(op,"Rename")){if(!gt4_save_active()){tip(s,"Load a player profile first.");goto end;}gt4_save_info info;gt4_save_info_get(gt4_save_active(),&info);save_ui_name(s,info.name);s->rename=1;s->slot=gt4_save_active();menu_native_page(s->owner,s->name_root);menu_play_effect(s->owner,"select");goto end;}
 else if(!strcmp(op,"Delete")){if(!gt4_save_active()){tip(s,"Load the player profile you want to remove.");goto end;}if(!menu_confirm(s->owner,"Delete Savegame","Remove the current player? A recovery copy will be kept."))goto end;ok=gt4_save_delete_slot(gt4_save_active());}
 else if(!strcmp(op,"Media")){char path[1200];if(!choose_import(path,1))goto end;gt4_media_import report;int imported=menu_import_ps2_media(s->owner,path,&report);char message[512];if(imported)snprintf(message,sizeof message,"Imported: %d replays, %d photos, %d films. Open films in Photo Lab to develop. Skipped: %d.",report.replays,report.photos,report.films,report.skipped);else snprintf(message,sizeof message,"%s",report.error);tip(s,message);menu_play_effect(s->owner,imported?"select":"disable");goto end;}
 else if(!strcmp(op,"Import")){int slot=0;for(int i=1;i<=8;i++){gt4_save_info info;if(!gt4_save_info_get(i,&info)){slot=i;break;}}if(!slot){tip(s,"All save slots are occupied.");goto end;}if(gt4_save_dirty() && !menu_confirm(s->owner,"Import Savegame","Discard unsaved changes and load the imported player?"))goto end;char path[1200];if(!choose_import(path,0))goto end;menu_save_capture(s->owner);ok=gt4_save_import_ps2(slot,path);if(ok)s->first=((slot-1)/3)*3;}
 if(ok && (!strcmp(op,"Card") || !strcmp(op,"Import")) && complete_first_run(s))goto end;
 if(ok && s->enter_gtmode && (!strcmp(op,"Card") || !strcmp(op,"Import"))){s->enter_gtmode=0;menu_play_effect(s->owner,"select");menu_request_project(s->owner,"gtmode");goto end;}
 if(ok){refresh(s);tip(s,gt4_save_error()[0]?gt4_save_error():!strcmp(op,"Save")?"Game saved successfully.":!strcmp(op,"Delete")?"Savegame removed. A recovery copy is in the save folder.":"Player loaded successfully.");menu_play_effect(s->owner,"select");}else {tip(s,gt4_save_error());menu_play_effect(s->owner,"disable");}end:*ret=ah_int(2);return 0;}
static int card_activate(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){screen->slot=screen->first+(int)(intptr_t)data+1;return action(vm,self,"Card",argc,argv,ret);}
int save_ui_event(save_ui *s,int key,const char *input){if(!s->editing)return 0;if(input && *input){s->typed=1;append(s,input);return 1;}if(key==SDLK_BACKSPACE){backspace(s);menu_play_effect(s->owner,"cancel");return 1;}if((key==SDLK_RETURN || key==SDLK_KP_ENTER) && s->typed){name_done(s);return 1;}return 0;}
/* Reuse Options' authored tile: its bands, rounded outline and shadow are baked in. */
static ui_widget *button(save_ui *s,ui_widget *parent,const char *name,const char *label,float x,float y,float width,float height,int card){ui_project *ui=menu_ui(s->owner);if(!card){ui_widget *languages=ui_root_widget(ui,1),*box=ui_widget_child(languages,"VBox"),*sample=ui_widget_child(box,"GB");ui_widget *b=ui_widget_copy(ui,sample);b->name=name;b->x=x;b->y=y;b->w=width;b->h=height;b->visible=1;for(int i=0;i<b->nkids;i++){b->kids[i]->w=width;b->kids[i]->h=height;}ui_widget *face=ui_widget_child(b,"label");face->scale_x=face->scale_y=0.7f;text(face,label);ui_text_adjust_scale(ui,face);ui_widget_append(parent,b);return b;}
 char description[2500];snprintf(description,sizeof description,
 "Composite { name string {\"%s\"} can_focus digit {1} geometry rectangle {%g %g %g %g} cursor_align digit {0} children [5] {"
 "ImageFace {name string {\"bg\"} image_path string {\"image/option/opt_window2L.png\"} slice rectangle {4 0 4 0} }"
 "ColorFace {name string {\"body\"} geometry rectangle {80 9 %g %g} color [1] {RGBA {154 154 161 255}} }"
 "ImageFace {name string {\"icon\"} geometry rectangle {16 17 58 54} image_path string {\"image/gtmode/home/status.png\"} }"
 "TextFace {name string {\"label\"} geometry rectangle {88 14 %g 30} text_color RGBA {0 0 0 255} font string {\"hm_16\"} align digit {3} scale_x digit {0.95} scale_y digit {0.95} }"
 "TextFace {name string {\"details\"} geometry rectangle {88 46 %g 22} text_color RGBA {45 45 48 255} font string {\"hm_16\"} align digit {3} scale_x digit {0.62} scale_y digit {0.62} } } }",
 name,x,y,width,height,width-84,height-14,width-104,width-104);ui_widget *b=ui_widget_from_text(ui,s->name_root,description);if(b){ui_widget *bg=ui_widget_child(b,"bg");bg->w=width;bg->h=height;bg->has_geometry=1;text(ui_widget_child(b,"label"),label);ui_widget_append(parent,b);}return b;}
static void heading(save_ui *s,ui_widget *page,const char *label){ui_widget *old=ui_widget_child(page,"Title");if(old)ui_widget_detach(old);ui_widget *w=ui_widget_from_text(menu_ui(s->owner),s->name_root,"TextFace {name string {\"Title\"} geometry rectangle {30 16 580 42} font string {\"hb_24\"} align digit {1} scale_x digit {1.1} scale_y digit {1.1} shadow digit {1.5} shadow_color RGBA {0 0 0 152} }");text(w,label);ui_widget_append(page,w);}
void save_ui_attach(save_ui *s,menu *m){memset(s,0,sizeof *s);screen=s;s->owner=m;s->root=0;s->name_root=3;ui_project *ui=menu_ui(m);s->page=ui_root_widget(ui,s->root);s->entry=ui_root_widget(ui,s->name_root);s->keyboard=ui_widget_child(s->entry,"Keyboard");s->name_label=ui_widget_child(s->keyboard,"input");s->name_label->type="TextFace";s->name_label->scale_x=s->name_label->scale_y=0.9f;heading(s,s->entry,"Create New Savegame");
 while(s->page->nkids)ui_widget_detach(s->page->kids[s->page->nkids-1]);for(int i=0;i<s->entry->nkids;i++)if(s->entry->kids[i]!=s->keyboard)ui_widget_append(s->page,ui_widget_copy(ui,s->entry->kids[i]));heading(s,s->page,"Save Games");ui_load_package(ui,"/projects/gtmode","HomeRoot");ui_load_package(ui,"/projects/option","OptionRoot");
 for(int i=0;i<3;i++){const char *names[]={"SaveCard1","SaveCard2","SaveCard3"};s->cards[i]=button(s,s->page,names[i],"Create New Savegame",116,76+i*92,408,80,1);ui_text_adjust_scale(ui,ui_widget_child(s->cards[i],"label"));menu_native_method(m,s->cards[i],"onActivate",card_activate,(void *)(intptr_t)i);}
 const char *names[]={"Previous","Next","Save","Rename","Delete","Import","Media"},*labels[]={"<",">","Save","Rename","Delete","Import Savegame","Import Media"};float xs[]={252,352,116,260,404,116,328},ys[]={352,352,382,382,382,414,414},ws[]={36,36,120,120,120,196,196};for(int i=0;i<7;i++){ui_widget *b=button(s,s->page,names[i],labels[i],xs[i],ys[i],ws[i],23,0);menu_native_method(m,b,"onActivate",action,(void *)names[i]);}
 ui_widget *exit=ui_widget_from_text(ui,s->root,"Composite {name string {\"Back\"} can_focus digit {1} geometry rectangle {567 390 58 40} children [2] {ImageFace {name string {\"bg\"} image_path string {\"image/labo/exit.png\"}} ImageFace {name string {\"bgf\"} visible_condition digit {2} visible_compare digit {2} image_path string {\"image/labo/exit_over.png\"}}}}");
 ui_widget_append(s->page,exit);menu_native_method(m,exit,"onActivate",action,"Back");
 s->pagination=ui_widget_copy(ui,s->name_label);s->pagination->type="TextFace";s->pagination->x=290;s->pagination->y=352;s->pagination->w=60;s->pagination->h=24;s->pagination->scale_x=s->pagination->scale_y=0.65f;ui_widget_append(s->page,s->pagination);
 bind_keys(s,s->keyboard);for(int i=0;i<2;i++){ui_widget *page=i?s->entry:s->page;menu_native_method(m,page,"onLoad",none,s);menu_native_method(m,page,"onInitialize",none,s);menu_native_method(m,page,"onFinalize",none,s);menu_native_method(m,page,"enter",i?enter_name:enter_cards,s);menu_native_method(m,page,"onCancel",i?cancel_name:action,i?(void *)s:(void *)"Back");}menu_enter(m,s->root);
}

void save_ui_first_run(save_ui *s){
 if(!s || !s->owner)return;s->first_run=s->onboarding=1;
 if(!s->welcome){
  ui_project *ui=menu_ui(s->owner);
  s->welcome=ui_widget_from_text(ui,s->root,"Composite {name string {\"Welcome\"} geometry rectangle {0 0 640 480} children [1] {TextFace {name string {\"Introduction\"} geometry rectangle {60 80 520 60} font string {\"hm_16\"} align digit {1} text_color RGBA {35 35 38 255} }}}");
  text(ui_widget_child(s->welcome,"Introduction"),"Your journey begins here.\nChoose how you want to start.");
  ui_widget_append(s->page,s->welcome);
  const char *names[]={"Begin","Import","Profiles","Skip"};
  const char *labels[]={"New Player","Import Savegame","Player Profiles","Play as Guest"};
  const char *icons[]={"image/gtmode/home/status.png","image/gtmode/home/diary.png","image/gtmode/home/garage.png","image/labo/exit.png"};
  const char *details[]={"Create your profile","Bring your GT4 progress","Choose a saved player","Continue without saving"};
  for(int i=0;i<4;i++){
   ui_widget *b=button(s,s->welcome,names[i],labels[i],60+(i%2)*270,172+(i/2)*124,250,102,1);
   ui_widget_set_image(ui_widget_child(b,"icon"),icons[i]);
   text(ui_widget_child(b,"details"),details[i]);
   ui_text_adjust_scale(ui,ui_widget_child(b,"label"));
   menu_native_method(s->owner,b,"onActivate",action,(void *)names[i]);
  }
 }
 ah_val result;enter_cards(NULL,NULL,s,0,NULL,&result);
}

void save_ui_new_career(save_ui *s)
{
 if(!s || !s->owner)return;
 s->first_run=s->onboarding=0;s->enter_gtmode=1;
 ah_val result;action(NULL,NULL,"Begin",0,NULL,&result);
}
