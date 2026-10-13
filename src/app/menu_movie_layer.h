/* Shared menu movie layer selection. Movies are authored by the page stack,
   not by whichever decoder happened to finish most recently. */
#ifndef OPENGT4_MENU_MOVIE_LAYER_H
#define OPENGT4_MENU_MOVIE_LAYER_H
#include "menu.h"
#include <string.h>
static const ui_widget *menu_page_movie(const ui_widget *w)
{
    if(!w || !w->visible)return NULL;
    if(!strcmp(w->type,"MovieFace") && w->movie_name && *w->movie_name)return w;
    for(int i=w->nkids-1;i>=0;i--){const ui_widget *f=menu_page_movie(w->kids[i]);if(f)return f;}
    return NULL;
}
static int menu_page_owns_background(const ui_widget *w)
{
    if(!w || !w->visible)return 0;
    if(!strcmp(w->type,"CarFace") || (!strcmp(w->type,"MovieFace") && w->movie_name && *w->movie_name))return 1;
    for(int i=0;i<w->nkids;i++)if(menu_page_owns_background(w->kids[i]))return 1;
    return 0;
}
static void menu_movie_layer_draw(menu *m,ui_movie_draw_fn draw,void *user)
{
    ui_project *p=menu_ui(m);
    if(!draw || menu_page_owns_background(ui_root_widget(p,menu_active_root(m))))return;
    const ui_widget *lower=NULL;
    for(int i=0;i+1<menu_page_depth(m);i++){
        const ui_widget *f=menu_page_movie(ui_root_widget(p,menu_page_at(m,i)));
        if(f)lower=f;
    }
    if(lower)draw(user,lower->movie_name,0,0,640,448,1,
        (int)mp_number(lower->src,"interlace",1),(int)mp_number(lower->src,"repeat",1),lower->movie_audio_track);
}
/* cursor.adc ConfirmDialog.open keeps the caller page beneath the modal;
   /projects/cursor/cursor.adc open L232 and ProgressDialog L404 pushPage.
   Recompose the same lower layers before drawing the caller without focus. */
static void menu_dialog_background_draw(menu *m,ui_movie_draw_fn draw,void *user)
{
    ui_project *p=menu_ui(m);int depth=menu_page_depth(m);
    const char *name=ui_root_name(p,menu_active_root(m));
    if(depth>1 && name && (strstr(name,"Popup") || strstr(name,"Dialog")))
        ui_draw_under(p,menu_page_at(m,depth-2));
    else menu_movie_layer_draw(m,draw,user);
    ui_draw_under(p,menu_active_root(m));
}
#endif
