#include "video_settings.h"
#include "video_gl.h"
#include "plat_sdl.h"
#include "race_hud.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static const int sizes[][2]={{1280,720},{1280,960},{1600,900},{1920,1080},{2560,1440},{3840,2160}};
static void quality(const pc_video_options *v)
{video_ui_quality(v->filter,v->text/100.f);}

void video_settings_init(video_settings *s,int persist,const char *config_file)
{
    memset(s,0,sizeof *s);s->persist=persist;
    s->current.video=(plat_video_desc){1280,960,0,1,0};
    /* Classic starts with texture/glyph coverage from the disc. Optional
       PC sharpening remains available and existing preferences still load. */
    s->current.filter=0;s->current.text=0;
    if(!persist)return;
    if(config_file)snprintf(s->path,sizeof s->path,"%s",config_file);
    else {
        char *dir=SDL_GetPrefPath("OpenGT4","OpenGT4");
        if(!dir)return;
        snprintf(s->path,sizeof s->path,"%svideo.ini",dir);SDL_free(dir);
    }
    FILE *f=fopen(s->path,"r");if(!f)return;
    char line[128],key[40];int value;
    while(fgets(line,sizeof line,f))if(sscanf(line,"%39[^=]=%d",key,&value)==2) {
        if(!strcmp(key,"width") && value>=640 && value<=7680)s->current.video.width=value;
        if(!strcmp(key,"height") && value>=480 && value<=4320)s->current.video.height=value;
        if(!strcmp(key,"fullscreen") && (value==0 || value==1))s->current.video.fullscreen=value;
        if(!strcmp(key,"vsync") && (value==0 || value==1))s->current.video.vsync=value;
        if(!strcmp(key,"upscaling") && value>=0 && value<=4)s->current.video.upscale=value;
        if(!strcmp(key,"ui_filter") && value>=0 && value<=2)s->current.filter=value;
        if(!strcmp(key,"aspect_ratio") && (value==0 || value==1))s->current.aspect=value;
        if(!strcmp(key,"text_sharpness") && value>=0 && value<=100)s->current.text=value;
    }
    fclose(f);
}

static void save(video_settings *s)
{
    if(!s->persist || !s->path[0])return;
    char tmp[1040];snprintf(tmp,sizeof tmp,"%s.tmp",s->path);
    FILE *f=fopen(tmp,"w");if(!f){snprintf(s->message,sizeof s->message,"Nao foi possivel salvar as preferencias.");return;}
    const pc_video_options *v=&s->current;
    int ok=fprintf(f,"width=%d\nheight=%d\nfullscreen=%d\nvsync=%d\nupscaling=%d\nui_filter=%d\ntext_sharpness=%d\naspect_ratio=%d\n",
        v->video.width,v->video.height,v->video.fullscreen,v->video.vsync,v->video.upscale,v->filter,v->text,v->aspect)>0;
    if(fclose(f)!=0)ok=0;
#ifdef _WIN32
    /* Replace the preference file only after its complete replacement exists. */
    if(ok) {
        if(!MoveFileExA(tmp,s->path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))ok=0;
    }
#else
    if(ok && rename(tmp,s->path)!=0)ok=0;
#endif
    if(!ok)snprintf(s->message,sizeof s->message,"Preferencias aplicadas; falha ao salvar.");
}


static void text(ui_widget *w,const char *value)
{if(w){free(w->text);w->text=strdup(value);}}
static void values(ui_project *ui,ui_widget *choice,const char *const *names,int count)
{
    ui_widget *box=ui_widget_child(ui_widget_child(choice,"popup"),"vbox");
    ui_widget *original=ui_option_item(choice,0);
    if(!box || !original)return;
    ui_widget *sample=ui_widget_copy(ui,original);
    /* Keep the authored translucent background and other decorative faces. */
    for(int i=box->nkids-1;i>=0;i--)
        if(!strcmp(box->kids[i]->type,"Composite") || !strcmp(box->kids[i]->type,"Button"))ui_widget_detach(box->kids[i]);
    for(int i=0;i<count;i++) {
        ui_widget *item=ui_widget_copy(ui,sample);item->name=names[i];item->y=i*24.f;
        text(ui_widget_child(item,"label"),names[i]);ui_widget_append(box,item);
    }
}
static void read_draft(video_settings *s)
{
    pc_video_options *v=&s->draft;
    v->aspect=s->choices[0]->focus_index;
    v->video.fullscreen=s->choices[1]->focus_index;
    int size=s->choices[2]->focus_index;
    if(size>=0 && size<(int)(sizeof sizes/sizeof sizes[0])){v->video.width=sizes[size][0];v->video.height=sizes[size][1];}
    v->filter=s->choices[3]->focus_index;
    v->text=s->choices[4]->focus_index*25;
    v->video.vsync=s->choices[5]->focus_index;
    v->video.upscale=s->choices[6]->focus_index;
}
static int idle(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{(void)vm;(void)self;(void)data;(void)argc;(void)argv;*ret=ah_int(0);return 0;}
static int changed(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)vm;(void)self;(void)argc;(void)argv;video_settings *s=data;
    read_draft(s);quality(&s->draft);*ret=ah_int(2);return 0;
}
static const char *hints[]={
    "Select the original 4:3 or widescreen 16:9 display ratio.",
    "Select windowed or full-screen display.",
    "Select the window resolution. Aspect Ratio controls the image proportions.",
    "Sharp preserves image edges. Smooth softens pixels. Original uses nearest sampling.",
    "Adjust text edge clarity independently of image filtering.",
    "Synchronize presentation with the monitor refresh rate.",
    "Native renders at full resolution. FSR 1 improves 3D performance; the UI stays at full resolution."
};
static int focus(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)vm;(void)argc;(void)argv;video_settings *s=data;ui_widget *w=ah_host_data(self);
    for(int i=0;i<7;i++)if(s->choices[i]==w)ui_set_tooltips(s->page,hints[i]);
    *ret=ah_int(2);return 0;
}
static int enter(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)vm;(void)self;(void)argc;(void)argv;video_settings *s=data;
    s->draft=s->current;s->message[0]=0;
    s->choices[0]->focus_index=s->current.aspect;
    s->choices[1]->focus_index=s->current.video.fullscreen;
    int size=6;for(int i=0;i<(int)(sizeof sizes/sizeof sizes[0]);i++)if(sizes[i][0]==s->current.video.width && sizes[i][1]==s->current.video.height)size=i;
    ui_widget *custom=ui_option_item(s->choices[2],6);custom->visible=size==6;
    char resolution[64];snprintf(resolution,sizeof resolution,"%d x %d",s->current.video.width,s->current.video.height);
    text(ui_widget_child(custom,"label"),resolution);
    s->choices[2]->focus_index=size;
    s->choices[3]->focus_index=s->current.filter;
    s->choices[4]->focus_index=(s->current.text+12)/25;
    s->choices[5]->focus_index=s->current.video.vsync;
    s->choices[6]->focus_index=s->current.video.upscale;
    quality(&s->current);menu_native_focus(s->owner,s->choices[0]);
    ui_set_tooltips(s->page,hints[0]);*ret=ah_int(0);return 0;
}
static int leave(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{(void)vm;(void)self;(void)argc;(void)argv;quality(&((video_settings *)data)->current);*ret=ah_int(0);return 0;}
static int cancel(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)vm;(void)self;(void)argc;(void)argv;video_settings *s=data;
    menu_play_effect(s->owner,"cancel");quality(&s->current);menu_native_page(s->owner,s->option_root);*ret=ah_int(2);return 0;
}
static int apply(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)vm;(void)self;(void)argc;(void)argv;video_settings *s=data;read_draft(s);
    SDL_Window *window=plat_sdl_window();
    int mode_changed=s->draft.video.fullscreen!=s->current.video.fullscreen;
    int size_changed=s->draft.video.width!=s->current.video.width || s->draft.video.height!=s->current.video.height;
    if(mode_changed && SDL_SetWindowFullscreen(window,s->draft.video.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0)<0) {
        ui_set_tooltips(s->page,"The display mode could not be changed.");*ret=ah_int(2);return 0;
    }
    /* Keep user maximization and manual resizing when resolution is unchanged. */
    if(!s->draft.video.fullscreen && size_changed) {
        if(SDL_GetWindowFlags(window)&SDL_WINDOW_MAXIMIZED)SDL_RestoreWindow(window);
        SDL_SetWindowSize(window,s->draft.video.width,s->draft.video.height);
    }
    if(!video_set_vsync(s->draft.video.vsync))s->draft.video.vsync=s->current.video.vsync;
    if(!video_set_upscale(s->draft.video.upscale))s->draft.video.upscale=0;
    video_set_aspect(s->draft.aspect);race_hud_set_widescreen(s->draft.aspect);   /* D_006184F0 */
    s->current=s->draft;quality(&s->current);save(s);
    if(s->message[0])ui_set_tooltips(s->page,s->message);
    else {menu_play_effect(s->owner,"select");menu_native_page(s->owner,s->option_root);}
    *ret=ah_int(2);return 0;
}
void video_settings_attach(video_settings *s,menu *m)
{
    ui_project *ui=menu_ui(m);s->root=s->option_root=-1;
    for(int i=0;i<ui_root_count(ui);i++) {
        if(!strcmp(ui_root_name(ui,i),"ScreenRoot"))s->root=i;
        if(!strcmp(ui_root_name(ui,i),"OptionRoot"))s->option_root=i;
    }
    if(s->root<0 || s->option_root<0)return;
    s->owner=m;s->page=ui_root_widget(ui,s->root);
    ui_widget *pane=ui_widget_child(s->page,"Pane"),*row=ui_widget_child(pane,"Wide"),*buttons=ui_widget_child(pane,"Buttons");
    if(!pane || !row || !buttons)return;
    /* Reuse the authored Screen Settings pane, frame, fonts, dropdowns and
       Cancel/OK buttons. Only the platform-specific rows change. */
    pane->y=78;pane->h=278;
    ui_widget *bg=ui_widget_child(pane,"bg");if(bg)bg->h=278;
    const char *names[]={"AspectRatio","WindowMode","Resolution","UIFilter","TextSharpness","VSync","Upscaling"};
    const char *titles[]={"Aspect Ratio","Window Mode","Resolution","UI Filter","Text Clarity","VSync","Upscaling"};
    ui_widget *sample=ui_widget_copy(ui,row);
    for(int i=pane->nkids-1;i>=0;i--) {
        ui_widget *w=pane->kids[i];
        if(w!=bg && w!=buttons && strcmp(w->name,"Title"))ui_widget_detach(w);
    }
    ui_widget_detach(buttons);
    const char *ratios[]={"4:3","16:9"};
    const char *modes[]={"Windowed","Full Screen"},*resolutions[]={"1280 x 720","1280 x 960","1600 x 900","1920 x 1080","2560 x 1440","3840 x 2160","Custom"};
    const char *filters[]={"Smooth","Sharp","Original"},*clarity[]={"0%","25%","50%","75%","100%"},*toggle[]={"Off","On"};
    const char *upscalers[]={"Native","FSR Ultra Quality","FSR Quality","FSR Balanced","FSR Performance"};
    const char *const *lists[]={ratios,modes,resolutions,filters,clarity,toggle,upscalers};int counts[]={2,2,7,3,5,2,5};
    for(int i=0;i<7;i++) {
        ui_widget *w=ui_widget_copy(ui,sample);w->name=names[i];w->y=20+i*32;
        text(ui_widget_child(w,"label"),titles[i]);ui_widget_append(pane,w);
        ui_widget *choice=ui_widget_child(w,"OptionMenu");s->choices[i]=choice;
        values(ui,choice,lists[i],counts[i]);
        menu_native_method(m,choice,"onValueChanged",changed,s);
        menu_native_method(m,choice,"onFocusEnter",focus,s);
    }
    buttons->y=244;ui_widget_append(pane,buttons);
    if(!video_fsr_available())for(int i=1;i<5;i++)ui_option_item(s->choices[6],i)->insensitive=1;
    menu_native_method(m,s->page,"onLoad",idle,s);
    menu_native_method(m,s->page,"onInitialize",idle,s);
    menu_native_method(m,s->page,"enter",enter,s);
    menu_native_method(m,s->page,"onFinalize",leave,s);
    menu_native_method(m,s->page,"onCancel",cancel,s);
    menu_native_method(m,ui_widget_child(buttons,"Cancel"),"onActivate",cancel,s);
    menu_native_method(m,ui_widget_child(buttons,"OK"),"onActivate",apply,s);
}
