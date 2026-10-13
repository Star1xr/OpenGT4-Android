/* P: optional local gallery authoring through the game's existing Photo Travel
   renderer. Explicit ISO command, separate from the asset-free launcher.
   Native scene poses/placements remain supplied by CRPM/PhotoMode. */
#include "launcher.h"
#include "photo_travel.h"
#include "save.h"
#include "plat_sdl.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
static void frames(menu *m,int count)
{
    for(int i=0;i<count;i++){
        menu_tick(m);menu_animate(m,1.f/60);plat_video_begin_frame();
        ui_draw_root(menu_ui(m),menu_active_root(m),NULL,NULL);plat_video_end_frame();
    }
}
int launcher_capture_gallery(const char *disc,const char *folder)
{
    int ok=0;menu *m=NULL;photo_travel *photo=NULL;gt4_vol *vol=NULL;
    if(plat_video_init(&(plat_video_desc){1280,960,0,0,0}))return 1;
    SDL_HideWindow(plat_sdl_window());plat_video_set_view(640,480);
    gt4_save_init(folder);const char *why;char error[256];vol=gt4_vol_open(disc,&why);if(!vol)goto done;
    m=menu_open(vol,"photo_shoot",error,sizeof error);if(!m)goto done;
    photo=photo_travel_open(disc);if(!photo)goto done;int shoot=-1;
    for(int i=0;i<ui_root_count(menu_ui(m));i++)if(!strcmp(ui_root_name(menu_ui(m),i),"ShootRoot"))shoot=i;
    if(shoot<0)goto done;
    const char *scenes[]={"ss_brooklyn","ss_gion","ss_fremont","ss_nanzenji"};
    const char *cars[]={"ford_gt_05","ka_01","ford_gt_05","ka_01"};
    for(int i=0;i<4;i++){
        if(!photo_travel_load(photo,m,scenes[i],cars[i],1,NULL,NULL))goto done;
        if(!menu_page_depth(m))menu_enter(m,shoot);else menu_native_page(m,shoot);menu_tick(m);frames(m,60);photo_travel_shoot(photo);frames(m,60);
        menu_send_input(m,MENU_ACCEPT);frames(m,2);
        const char *file=photo_travel_last_saved(photo);if(!*file)goto done;
        printf("launcher photograph: %s\n",file);
        menu_send_input(m,MENU_BACK);frames(m,60);
    }
    ok=1;
done:
    photo_travel_close(photo);if(m)menu_close(m);gt4_vol_close(vol);gt4_save_shutdown();plat_video_shutdown();
    return ok?0:1;
}
