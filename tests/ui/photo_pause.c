/* Actual authored racephoto movie, getter contract and preview storage binding.
   Native camera preset enumeration is not certified by this test. */
#define main purchase_fixture_main
#include "../gtmode/purchase_menu.c"
#undef main
#include "photo_pause.h"
#include "film_fixture.h"
void plat_video_set_view(float w,float h){(void)w;(void)h;}
static int saved;
static int store(void *user){(void)user;saved++;return 1;}
int main(int argc,char **argv){
 CHECK(argc>=2);gt4_save_init("");gt4_vol *v=gt4_vol_open(argv[1],NULL);CHECK(v);
 float a[17];int32_t sh[19];CHECK(gt4_photo_tables_load(argv[1],a,sh));
 gt4_photo_mode state={0};state.view_angle=35;state.aperture=7;state.shutter=0;
 state.drive_camera=2;state.drive_cameras=5;state.manual_focus=1;
 photo_pause *p=photo_pause_open(v,&state,a,sh);CHECK(p);strb_value value;
 CHECK(photo_pause_value(p,"getBlink",&value) && value.n==-1);
 CHECK(photo_pause_value(p,"getCurCam",&value) && !strcmp(value.s,"3"));
 CHECK(photo_pause_value(p,"getNCam",&value) && !strcmp(value.s,"5"));
 CHECK(photo_pause_value(p,"getZoom",&value) && !strcmp(value.s,"f = 35mm"));
 CHECK(photo_pause_value(p,"getMF",&value));if(argc>2)value.n=0;CHECK(value.n==1);
 photo_pause_help(p,2);CHECK(photo_pause_value(p,"getBlink",&value) && value.n==2);
 photo_pause_draw(p,1.0/60,1);CHECK(photo_pause_value(p,"isWide",&value) && value.n==1);
 photo_pause_close(p);
 char error[256];menu *m=menu_open(v,"photo_save",error,sizeof error);CHECK(m);
 menu_set_photo_saver(m,store,NULL);int root=-1;
 for(int k=0;k<ui_root_count(m->ui);k++)if(!strcmp(ui_root_name(m->ui,k),"PreviewRoot"))root=k;
 CHECK(root>=0);ui_widget *w=ui_widget_child(ui_root_widget(m->ui,root),"PR");CHECK(w);
 ah_obj *object=widget_module(m,w);ah_val ret;
 CHECK(ah_call_member(m->vm,object,"outUSBStorage",0,NULL,&ret)>=0);
 CHECK(ah_cstr(ret) && !strcmp(ah_cstr(ret),"OK") && saved==1);
 menu_close(m);
 gt4_save_shutdown();gt4_save_init("build/photo-pause-audit/labo-fixture");
 uint8_t film[2048];size_t film_size=film_fixture(film,sizeof film);CHECK(film_size);
 CHECK(gt4_save_mkdir("build/photo-pause-audit/labo-fixture/films"));
 CHECK(gt4_save_write("build/photo-pause-audit/labo-fixture/films/test.ogt4film",film,film_size));
 m=menu_open(v,"labomode",error,sizeof error);CHECK(m);ah_obj *manager=ah_module_new(m->vm,"film_manager",NULL,NULL,NULL);
 album_manager(m->vm,manager,"MODE_LOAD_PHOTO_FILM");album_manager_files(m,manager,"MODE_LOAD_PHOTO_FILM");
 ah_val count;CHECK(ah_get_member(m->vm,manager,"count",&count) && ah_num(count)==1);
 ah_obj *file=album_file_object(m,"test.ogt4film");ah_val kind;CHECK(ah_get_member(m->vm,file,"objectName",&kind) && !strcmp(ah_cstr(kind),"FilePhotoFilm"));
 CHECK(ah_call_member(m->vm,file,"load",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"OK"));
 ah_set_member(m->vm,game_object(m),"menu_result",ah_str(m->vm,"develop"));strcpy(m->next_project,"photo_save");char path[1200];
 CHECK(menu_take_photo_film(m,path,sizeof path) && strstr(path,"/films/test.ogt4film"));CHECK(!menu_take_photo_film(m,path,sizeof path));
 menu_close(m);gt4_vol_close(v);puts("PASS racephoto authored movie/getters and PhotoSave storage binding");return 0;
}
