/* Exercise the original Photo Lab commandDevelop, with an authored film. */
#include "../../src/core/menu.c"
#include "film_fixture.h"
#include <SDL.h>
#include "plat_sdl.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"film menu %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int confirmations;
static int confirm(menu *m,const char *title,const char *message,int type,const char *yes,const char *no){(void)m;(void)title;(void)message;(void)type;(void)yes;(void)no;confirmations++;return 1;}
int main(int argc,char **argv){
 CHECK(argc>=3);gt4_save_init(argv[2]);CHECK(gt4_save_mkdir(argv[2]));char dir[1200],path[1300];snprintf(dir,sizeof dir,"%s/films",argv[2]);CHECK(gt4_save_mkdir(dir));
 snprintf(path,sizeof path,"%s/authored.ogt4film",dir);uint8_t raw[2048];size_t n=film_fixture(raw,sizeof raw);CHECK(gt4_save_write(path,raw,n));
 CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));SDL_HideWindow(plat_sdl_window());gt4_vol *v=gt4_vol_open(argv[1],NULL);CHECK(v);char error[256];menu *m=menu_open(v,"labomode",error,sizeof error);CHECK(m);
 CHECK(menu_open_labo(m,"PhotoRoot"));for(int k=0;k<10;k++)menu_tick(m);
 menu_send_input(m,MENU_ACCEPT);for(int k=0;k<10;k++)menu_tick(m);
 {ah_val manager,mode;ah_get_member(m->vm,m->project,"manager",&manager);if(ah_as_obj(manager,AO_MODULE) && ah_get_member(m->vm,manager.u.o,"_mode",&mode))fprintf(stderr,"album: %d mode %s name %s\n",album_files_count,ah_cstr(mode),album_files_count?album_files[0].name:"");}
 CHECK(album_files_count==1 && album_is_film(album_files[0].name));
 menu_set_dialog_presenter(m,confirm);
 ah_obj *selection=ah_array_new(m->vm);ah_array_push(m->vm,selection,ah_int(0));ah_set_member(m->vm,m->project,"selection_",ah_objv(selection));
 ah_obj *page=NULL;for(int k=0;k<ui_root_count(m->ui);k++)if(!strcmp(ui_root_name(m->ui,k),"PhotoRoot"))page=widget_module(m,ui_root_widget(m->ui,k));CHECK(page);
 ah_val ctx=ah_objv(m->context),result;CHECK(ah_call_member(m->vm,page,"commandDevelop",1,&ctx,&result)>=0);
 char developed[1200]={0};int took=0;
 for(int k=0;k<180 && !took;k++){
  menu_tick(m);menu_animate(m,1.f/60);
  took=menu_take_photo_film(m,developed,sizeof developed);
 }
 if(!took){ah_val value;ah_get_member(m->vm,game_object(m),"menu_result",&value);fprintf(stderr,"dispatch: confirm=%d loaded=%s next=%s result=%s root=%d\n",confirmations,film_loaded,m->next_project,ah_cstr(value),menu_active_root(m));}
 if(argc>3)took=0;CHECK(took && confirmations==1 && !strcmp(developed,path));CHECK(!menu_take_photo_film(m,developed,sizeof developed));
 CHECK(menu_open_labo(m,"PhotoRoot"));for(int k=0;k<10;k++)menu_tick(m);
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"PhotoRoot"));
 uint8_t *source=NULL;size_t bytes=0;CHECK(gt4_save_read(path,&source,&bytes,NULL)>0 && bytes==n && !memcmp(source,raw,n));free(source);
 puts("PASS original PhotoRoot film listing, commandDevelop confirmation/loading, one-shot dispatch, album return, unchanged source; negative drops dispatch.");
 menu_close(m);gt4_vol_close(v);gt4_save_shutdown();plat_video_shutdown();return 0;
}
