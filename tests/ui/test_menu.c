/* White-box host bridge tests with synthetic screens and native callbacks.
   Including menu.c lets these tests attach callbacks without a disc/script. */
#include "../../src/core/menu.c"
#include "../../src/app/menu_movie_layer.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static const char fixture[] =
"Project { name string {\"Fixture\"} children [2] {"
"RootWindow { name string {\"Home\"} bgm string {\"MAP\"} children [1] {"
"SelectBox { name string {\"List\"} spacing digit {6} children [3] {"
"Button { name string {\"One\"} }"
"Button { name string {\"Disabled\"} insensitive digit {1} }"
"Button { name string {\"Two\"} } } } } }"
"RootWindow { name string {\"Details\"} bgm string {\"keep\"} children [1] {"
"Button { name string {\"Back\"} } } } } }";

static const char packed_fixture[]=
"Project { name string {\"Packed\"} children [1] { RootWindow { name string {\"Root\"} children [1] {"
"MBox { name string {\"Grid\"} packing digit {1} geometry rectangle {0 0 360 84} minimum_height digit {84} children [4] {"
"FBox { name string {\"A\"} packing digit {1} pack_expand_x digit {1} pack_expand_y digit {1} pack_fill_x digit {1} pack_fill_y digit {1} minimum_width digit {100} minimum_height digit {34} pad_left digit {10} pad_top digit {2} children [2] { ColorFace { name string {\"BG\"} color [1] { RGBA {255 255 255 102} } } Composite { is_face digit {0} packing digit {1} geometry rectangle {0 0 80 20} } } }"
"FBox { name string {\"B\"} packing digit {1} pack_expand_x digit {1} pack_expand_y digit {1} pack_fill_x digit {1} pack_fill_y digit {1} minimum_width digit {200} minimum_height digit {34} pad_left digit {10} pad_top digit {2} children [2] { ColorFace { name string {\"BG\"} } Composite { is_face digit {0} packing digit {1} geometry rectangle {0 0 80 20} } } }"
"FBox { name string {\"C\"} packing digit {1} pack_expand_x digit {1} pack_expand_y digit {1} pack_fill_x digit {1} pack_fill_y digit {1} minimum_width digit {100} minimum_height digit {34} }"
"FBox { name string {\"D\"} packing digit {1} pack_expand_x digit {1} pack_expand_y digit {1} pack_fill_x digit {1} pack_fill_y digit {1} minimum_width digit {200} minimum_height digit {34} }"
"} } } } } }";

static uint8_t part_list_fixture[40],part_db_fixture[48];
static int part_fixture_enabled,cursor_fixture_enabled,tooltip_fixture_enabled;
static unsigned fixture_reads;
static int cursor_texture;
static int cursor_draws;static float cursor_draw_x,cursor_draw_y,cursor_draw_angle;
static void fixture_put32(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
uint8_t *gt4_vol_read(gt4_vol *v, const char *path, size_t *size)
{
    (void)v;fixture_reads++;
    if(tooltip_fixture_enabled && strstr(path,"/rtext.rt2")) {
        uint8_t *t=calloc(1,160); *size=160;
        memcpy(t,"RT03",4);fixture_put32(t+12,1);
        fixture_put32(t+16,40);fixture_put32(t+20,1);fixture_put32(t+24,32);
        fixture_put32(t+32,64);fixture_put32(t+36,96);
        strcpy((char *)t+40,"Fixture::Home");strcpy((char *)t+64,"TOOLTIP|One");
        strcpy((char *)t+96,"Home description");return t;
    }
    if(cursor_fixture_enabled && (!strcmp(path,"/image/cursor/cursor.png") || !strcmp(path,"/image/cursor/load.png"))){
        uint8_t *t=calloc(1,164);*size=164;
        memcpy(t,"Tex1",4);t[20]=t[22]=1;
        fixture_put32(t+24,48);fixture_put32(t+28,88);
        uint64_t tex0=(1ull<<14)|(2ull<<26)|(2ull<<30);
        fixture_put32(t+48,(uint32_t)tex0);fixture_put32(t+52,(uint32_t)(tex0>>32));
        fixture_put32(t+88,100);t[94]=1;t[96]=t[98]=4;
        for(int i=0;i<16;i++){t[100+i*4]=strstr(path,"load.png")?20:255;t[103+i*4]=128;}
        return t;
    }
    if(part_fixture_enabled && (strstr(path,"/CARS/") || strstr(path,"/TEST_PART.dbt"))){
        const uint8_t *bytes=strstr(path,"/CARS/")?part_list_fixture:part_db_fixture;
        *size=strstr(path,"/CARS/")?sizeof part_list_fixture:sizeof part_db_fixture;
        uint8_t *out=malloc(*size);memcpy(out,bytes,*size);return out;
    }
    if (!strstr(path, ".mproject")) { *size = 0; return NULL; }
    if(strstr(path,"/packed/")){*size=sizeof packed_fixture-1;uint8_t *out=malloc(*size);memcpy(out,packed_fixture,*size);return out;}
    *size = sizeof fixture - 1;
    uint8_t *out = malloc(*size); memcpy(out, fixture, *size); return out;
}
/* Synthetic volume: no external CORE or ISO files exist in this fixture.
   Keep the volume object out of the link so its real reader cannot consume
   the fixture's sentinel pointer. */
const char *gt4_vol_source(const gt4_vol *v){(void)v;return NULL;}
uint8_t *gt4_disc_read_root(const char *path,const char *name,size_t *size){(void)path;(void)name;*size=0;return NULL;}
uint8_t *gt4_disc_read_file(const char *path,const char *name,size_t *size){(void)path;(void)name;*size=0;return NULL;}
struct plat_texture { int dummy; };
plat_texture *plat_texture_create(int w, int h, const uint8_t *p) { (void)w; (void)h; static plat_texture texture[2]={{0},{1}};return cursor_fixture_enabled?&texture[p[0]==20]:NULL; }
void plat_texture_destroy(plat_texture *t) { (void)t; }
void plat_draw_quad4(const plat_texture *t, float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4])
{ (void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c; }
static int fade_alpha;
static int capture_calls,retained_draws;static float retained_w,retained_alpha,retained_u,retained_v;
static plat_texture retained_fixture;
void plat_draw_solid_quad(const float xy[4][2],const uint8_t c[4]){(void)xy;(void)c;}
void plat_draw_quad(const plat_texture *t, float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4])
{ (void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b; fade_alpha=c[3];if(t==&retained_fixture){retained_draws++;retained_w=w;retained_alpha=c[3];retained_u=u;retained_v=v;} }
void plat_draw_quad_blend(const plat_texture *t,float x,float y,float w,float h,
                           float u,float v,float a,float b,const uint8_t c[4],float factor)
{
    uint8_t tint[4];memcpy(tint,c,4);tint[3]=(uint8_t)(factor*255.0f+.5f);
    plat_draw_quad(t,x,y,w,h,u,v,a,b,tint);
}
void plat_set_clip(float x,float y,float w,float h) { (void)x;(void)y;(void)w;(void)h; }
static plat_texture *capture_fixture(plat_texture *t,float x,float y,float w,float h)
{(void)t;CHECK(x==0 && y==0 && w==640 && h==480);capture_calls++;return &retained_fixture;}
static int dialog_result;
int plat_dialog(const char *title, const char *text, int confirm)
{ CHECK(title && !strcmp(text, "Synthetic question")); CHECK(confirm == 1); return dialog_result; }

static int activated, initialized, finalized, changed, focused, left, deferred;
static int on_count(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{ (void)vm;(void)self;(void)argc;(void)argv; (*(int *)d)++; *ret = ah_int(0); return 0; }
static int on_open(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;(void)d;
    menu *m = ah_user(vm); ah_val w;
    CHECK(argc == 3);
    CHECK(ah_get_member(vm, ah_as_obj(argv[1], AO_MODULE), "widget", &w));
    ui_widget *target = ah_host_data(ah_as_obj(w, AO_MODULE));
    CHECK(!strcmp(target->name, "Two")); /* original target survived bubbling */
    activated++;
    ah_val page = maybe_module(m, ui_root_widget(m->ui, 1)), result;
    CHECK(nat_page(vm, NULL, (void *)1, 1, &page, &result) == 0);
    CHECK(menu_active_root(m) == 0); /* deferred until callback returns */
    *ret = ah_int(2); return 0;
}
static int on_close(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;(void)d;(void)argc;(void)argv;
    menu *m = ah_user(vm);
    ah_val page = maybe_module(m, ui_root_widget(m->ui, 1)), result;
    CHECK(nat_page(vm, NULL, (void *)3, 1, &page, &result) == 0);
    *ret = ah_int(2); return 0;
}
static int queued(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{ (void)vm;(void)self;(void)d; CHECK(argc == 1 && !strcmp(ah_cstr(argv[0]), "survives GC")); deferred++; *ret = ah_nil(); return 0; }
static void hook(menu *m, ui_widget *w, const char *name, ah_native_fn fn, void *data)
{ ah_set_member(m->vm, widget_module(m, w), name, ah_native(m->vm, name, fn, data)); }

static int modal_frames, modal_inner_done;
static int modal_present(menu *m,void *user)
{
    (void)user;ah_val result;modal_frames++;
    CHECK(ah_exchange_instruction_budget(m->vm,17)==1000000);
    if(m->event_depth==2){
        CHECK(!modal_inner_done);
        CHECK(nat_event_loop(m->vm,NULL,(void *)1,0,NULL,&result)==0);
    }else if(!modal_inner_done){
        CHECK(m->event_depth==1 && m->event_loops[0]);
        CHECK(nat_event_loop(m->vm,NULL,NULL,0,NULL,&result)==0);
        CHECK(m->event_depth==1 && m->event_loops[0]);
        modal_inner_done=1;
    }else CHECK(nat_event_loop(m->vm,NULL,(void *)1,0,NULL,&result)==0);
    return 1;
}

static int purchase_popup_frames;
static void purchase_popup_wait(menu *m,float seconds,void *user)
{
    (void)user;CHECK(menu_active_root(m)==1 && m->pending_kind==0);
    CHECK(m->focus[1] && !strcmp(m->focus[1]->name,"Back"));
    ui_widget *pane=ui_root_widget(m->ui,1);
    CHECK(pane->y==480); /* incoming actor has not already completed */
    for(int i=0;i<(int)lroundf(seconds*60);i++) {
        menu_animate(m,1.f/60);ui_draw_root(m->ui,1,NULL,NULL);
        purchase_popup_frames++;
    }
    CHECK(pane->y>0 && pane->y<1);
}

static int loading_presented;
static float loading_x,loading_y;
static void loading_wait(menu *m,float seconds,void *user)
{
    (void)user;CHECK(loading_presented?(seconds>=28.f/60 && seconds<=58.f/60):seconds==2.f/60);
    CHECK(ui_root_widget(m->ui,menu_active_root(m))->visible==(loading_presented==0));
    int frames=(int)lroundf(seconds*60);
    for(int frame=0;frame<frames;frame++) {
        menu_animate(m,1.f/60);ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
        CHECK(cursor_texture==1);
        if(!loading_presented){loading_x=cursor_draw_x;loading_y=cursor_draw_y;}
        else CHECK(cursor_draw_x==loading_x && cursor_draw_y==loading_y);
        loading_presented++;
    }
}

static int loaded_car_faces;
static int fixture_car_loader(menu *m,const ui_widget *w,void *user)
{(void)m;(void)user;CHECK(!strcmp(w->car_code,"fixture_car"));loaded_car_faces++;return 1;}

static float requested_music_fade=-1;
static void record_music_fade(float seconds){requested_music_fade=seconds;}
static unsigned prepared_music_requests;
static void record_music_preparation(menu *m,void *user)
{(void)user;CHECK(menu_music_name(m) && *menu_music_name(m));prepared_music_requests++;}

static int movie_layer_calls;
static int movie_layer_probe(void *user,const char *name,float x,float y,float w,float h,float opacity,int interlace,int repeat,int audio)
{
    (void)user;(void)x;(void)y;(void)interlace;(void)repeat;
    CHECK(!strcmp(name,"start_game") && w==640 && h==448 && opacity==1 && audio==0);
    movie_layer_calls++;return movie_layer_calls>=3;
}
static int consumed_key;
static int consume_button_key(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)self;(void)data;CHECK(argc==2);ah_val key;
    CHECK(ah_get_member(vm,argv[1].u.o,"keysym",&key));consumed_key=(int)ah_num(key);
    *ret=ah_int(1);return 0;
}

int main(void)
{
    {
        char error[256];menu *p=menu_open(NULL,"fixture",error,sizeof error);CHECK(p);
        menu_enter(p,0);menu_tick(p);ui_widget *home=ui_root_widget(p->ui,0);
        int accepted=0,cancelled=0;
        hook(p,home,"onActivate",on_count,&accepted);hook(p,home,"onCancel",on_count,&cancelled);
        menu_send_input(p,MENU_ACCEPT);menu_send_input(p,MENU_CIRCLE);
        CHECK(accepted==2 && cancelled==0);
        menu_send_input(p,MENU_TRIANGLE);menu_send_input(p,MENU_SQUARE);
        CHECK(accepted==2 && cancelled==2);
        if(getenv("GT4_MENU_BUTTON_NEGATIVE"))CHECK(cancelled==3);
        /* Specific onKeyPress handlers retain precedence and distinct keys. */
        hook(p,home,"onKeyPress",consume_button_key,NULL);
        menu_send_input(p,MENU_CIRCLE);CHECK(consumed_key==0xff1b && accepted==2);
        menu_send_input(p,MENU_SQUARE);CHECK(consumed_key==0xff63 && cancelled==2);
        menu_send_input(p,MENU_TRIANGLE);CHECK(consumed_key==0xff8d && cancelled==2);
        ui_widget *movie=ui_widget_from_text(p->ui,0,"MovieFace { name string {\"Movie\"} }");CHECK(movie);
        movie->movie_name=strdup("start_game");ui_widget_append(home,movie);
        p->pages[0]=0;p->pages[1]=1;p->depth=2;
        movie_layer_calls=0;
        /* Delayed decode: returning from courses must keep pumping the
           underlying movie on ALL frames, even after its name matches. */
        for(int frame=0;frame<6;frame++){
            if(!getenv("GT4_MOVIE_LAYER_NEGATIVE") || frame==0)
                menu_movie_layer_draw(p,movie_layer_probe,NULL);
        }
        CHECK(movie_layer_calls==6);
        ui_widget *own=ui_widget_from_text(p->ui,1,"MovieFace { name string {\"Own\"} }");CHECK(own);
        own->movie_name=strdup("category");ui_widget_append(ui_root_widget(p->ui,1),own);
        menu_movie_layer_draw(p,movie_layer_probe,NULL);CHECK(movie_layer_calls==6);
        own->visible=0;menu_movie_layer_draw(p,movie_layer_probe,NULL);CHECK(movie_layer_calls==7);
        /* A synchronous modal must retain the inherited background too. */
        if(!getenv("GT4_DIALOG_BACKGROUND_NEGATIVE"))
            menu_dialog_background_draw(p,movie_layer_probe,NULL);
        CHECK(movie_layer_calls==8);
        p->depth=1;menu_movie_layer_draw(p,movie_layer_probe,NULL);CHECK(movie_layer_calls==8);
        menu_close(p);
        if(getenv("GT4_DIALOG_BACKGROUND_ONLY")){puts("modal background layers: passed");return 0;}
    }

    /* resetToolTip refreshes the existing focus on initial entry and return;
       remove still clears it, and reset restarts an unchanged description. */
    {
        tooltip_fixture_enabled=1;char error[256];
        menu *p=menu_open(NULL,"fixture",error,sizeof error);CHECK(p);
        ui_widget *root=ui_root_widget(p->ui,0);
        ui_widget *tip=ui_widget_from_text(p->ui,0,"ToolTipFace { name string {\"Help\"} }");CHECK(tip);
        ui_widget_append(root,tip);menu_enter(p,0);menu_tick(p);
        CHECK(p->focus[0] && !strcmp(p->focus[0]->name,"One"));
        ui_set_tooltips(root,NULL);ah_val result;
        CHECK(ah_call_member(p->vm,widget_module(p,root),"resetToolTip",0,NULL,&result)>0);
        CHECK(tip->text && !strcmp(tip->text,"Home description"));
        tip->text_scroll=9;
        CHECK(ah_call_member(p->vm,widget_module(p,root),"resetToolTip",0,NULL,&result)>0);
        CHECK(tip->text_scroll==0);
        CHECK(ah_call_member(p->vm,widget_module(p,tip),"remove",0,NULL,&result)>0 && !tip->text);
        menu_enter(p,1);menu_tick(p);menu_enter(p,0);menu_tick(p);
        CHECK(ah_call_member(p->vm,widget_module(p,root),"resetToolTip",0,NULL,&result)>0);
        CHECK(tip->text && !strcmp(tip->text,"Home description"));
        set_focus(p,NULL);
        CHECK(ah_call_member(p->vm,widget_module(p,root),"resetToolTip",0,NULL,&result)>0 && !tip->text);
        menu_close(p);tooltip_fixture_enabled=0;
    }

    /* Purchase -> confirmation must commit pushPage before Sync animates
       its entrance, without resetting the context cursor at the page change. */
    {
        cursor_fixture_enabled=1;char error[256];
        menu *p=menu_open(NULL,"fixture",error,sizeof error);CHECK(p);
        menu_enter(p,0);menu_tick(p);ui_draw_root(p->ui,0,NULL,NULL);
        for(int i=0;i<60;i++){menu_animate(p,1.f/60);ui_draw_root(p->ui,0,NULL,NULL);}
        float x=cursor_draw_x,y=cursor_draw_y;
        ui_widget *pane=ui_root_widget(p->ui,1);pane->y=0;
        ah_val args[]={maybe_module(p,pane),ah_float(pane->x),ah_float(480)},actor,result;
        CHECK(nat_actor(p->vm,NULL,"MMoveActor",3,args,&actor)==0);
        ah_set_member(p->vm,actor.u.o,"out",ah_int(1));
        CHECK(ah_call_member(p->vm,actor.u.o,"warp",0,NULL,&result)>0);
        ah_val page=maybe_module(p,pane);
        CHECK(nat_page(p->vm,NULL,(void *)1,1,&page,&result)==0);
        CHECK(menu_active_root(p)==0);
        ah_set_member(p->vm,actor.u.o,"out",ah_int(0));
        menu_set_presenter(p,purchase_popup_wait,NULL);
        ah_val seconds=ah_float(.5);
        CHECK(nat_sync(p->vm,NULL,p,1,&seconds,&result)==0);
        CHECK(purchase_popup_frames==30);
        CHECK(cursor_draw_x!=x || cursor_draw_y!=y);
        p->pending_kind=3;p->pending_page=1;apply_pages(p);
        float last_x=cursor_draw_x,last_y=cursor_draw_y;
        ui_draw_root(p->ui,0,NULL,NULL);
        CHECK(cursor_draw_x==last_x && cursor_draw_y==last_y);
        menu_animate(p,1.f/60);ui_draw_root(p->ui,0,NULL,NULL);
        CHECK(cursor_draw_x!=last_x || cursor_draw_y!=last_y);
        menu_close(p);cursor_fixture_enabled=0;
    }

    /* A modal's first paint must not perform cold resource reads, including
       hidden choices, and preparation must not alter focus or geometry. */
    {
        cursor_fixture_enabled=1;char error[256];
        ui_project *p=ui_project_load(NULL,"fixture",error,sizeof error);CHECK(p);
        ui_widget *root=ui_root_widget(p,1);root->name="ConfirmDialog";
        ui_widget *content=ui_widget_from_text(p,1,
            "Composite { visible digit {0} geometry rectangle {20 40 80 40} children [2] {"
            "TextFace { text string {\"OK\"} }"
            "ImageFace { image_path string {\"image/cursor/cursor.png\"} } } }");CHECK(content);
        ui_widget_append(root,content);
        ui_prepare_dialogs(p,NULL,NULL);
        CHECK(content->visible==0 && content->x==20 && content->y==40 && !content->focused);
        content->visible=1;unsigned reads=fixture_reads;
        ui_draw_root(p,1,NULL,NULL);CHECK(fixture_reads==reads);
        ui_project_free(p);cursor_fixture_enabled=0;
    }

    /* Vertical lists must not leave the cursor on the preceding card, and
       moving offsets must update the target before the next paint pass. */
    {
        cursor_fixture_enabled=1;char error[256];
        ui_project *p=ui_project_load(NULL,"fixture",error,sizeof error);CHECK(p);
        ui_widget *root=ui_root_widget(p,0);root->kids[0]->visible=0;
        ui_widget *list=ui_widget_from_text(p,0,
            "SelectBox { geometry rectangle {40 100 300 160} direction digit {1} alignment digit {1} spacing digit {10} children [3] {"
            "Button { geometry rectangle {0 0 200 100} cursor_align digit {8} }"
            "Button { geometry rectangle {0 0 200 100} cursor_align digit {8} }"
            "Button { geometry rectangle {0 0 200 100} cursor_align digit {8} } } }");CHECK(list);
        ui_widget_append(root,list);list->focused=1;ui_cursor_select(p,list);
        ui_animate(p,0,1.f/60);ui_draw_root(p,0,NULL,NULL);
        for(int i=0;i<120;i++){ui_animate(p,0,1.f/60);ui_draw_root(p,0,NULL,NULL);}
        CHECK(fabsf(cursor_draw_x-140)<.1f && fabsf(cursor_draw_y-150)<.1f);
        list->focus_index=2;ui_draw_root(p,0,NULL,NULL);
        float x=cursor_draw_x,y=cursor_draw_y,angle=cursor_draw_angle;
        ui_animate(p,0,1.f/60);
        gt4_cursor_step(&x,&y,&angle,140,150+220+list->strip_offset,-22.5f,1);
        ui_draw_root(p,0,NULL,NULL);
        CHECK(fabsf(cursor_draw_y-y)<.01f);
        for(int i=0;i<120;i++){ui_animate(p,0,1.f/60);ui_draw_root(p,0,NULL,NULL);}
        CHECK(fabsf(cursor_draw_y-210)<.1f);
        list->focus_index=0;ui_draw_root(p,0,NULL,NULL);
        for(int i=0;i<120;i++){ui_animate(p,0,1.f/60);ui_draw_root(p,0,NULL,NULL);}
        CHECK(fabsf(cursor_draw_y-150)<.1f);
        ui_project_free(p);cursor_fixture_enabled=0;
    }

    /* Losing focus during a transition retains the cursor, under the fade;
       new-page focus resumes the existing chase instead of resetting it. */
    {
        cursor_fixture_enabled=1;char error[256];
        menu *p=menu_open(NULL,"fixture",error,sizeof error);CHECK(p);menu_enter(p,0);menu_tick(p);
        ui_draw_root(p->ui,0,NULL,NULL);CHECK(cursor_draws>0);
        for(int frame=0;frame<120;frame++)menu_animate(p,1.f/60);
        ui_draw_root(p->ui,0,NULL,NULL);
        float x=cursor_draw_x,y=cursor_draw_y;
        /* A focused outgoing button can still have moving layout actors.
           Its updates must not move the retained transition cursor. */
        ui_fade(p->ui,0,1,.5f);
        ui_widget *outgoing=p->focus[0];CHECK(outgoing);
        outgoing->x+=100;outgoing->y+=100;
        ui_draw_root(p->ui,0,NULL,NULL);menu_animate(p,.1f);
        ui_draw_root(p->ui,0,NULL,NULL);
        CHECK(cursor_draw_x==x && cursor_draw_y==y);
        /* The loading icon survives the deferred default-icon call while
           startPage prepares its destination, then restores on reveal. */
        ah_val icon_args[2]={ah_objv(p->context),ah_str(p->vm,"load")},icon_result;
        CHECK(nat_cursor_icon(p->vm,NULL,NULL,2,icon_args,&icon_result)==0);
        ui_draw_root(p->ui,0,NULL,NULL);CHECK(cursor_texture==1);
        CHECK(nat_cursor_icon(p->vm,NULL,NULL,1,icon_args,&icon_result)==0);
        ui_draw_root(p->ui,0,NULL,NULL);CHECK(cursor_texture==1);
        set_focus(p,NULL);
        ui_root_widget(p->ui,0)->kids[0]->visible=0;
        ui_fade(p->ui,0,1,.5f);cursor_draws=0;
        ui_draw_root(p->ui,0,NULL,NULL);CHECK(cursor_draws==1);
        menu_animate(p,.1f);ui_draw_root(p->ui,0,NULL,NULL);
        CHECK(cursor_draw_x==x && cursor_draw_y==y && fade_alpha>0 && fade_alpha<255);
        for(int i=0;i<5;i++)menu_animate(p,.1f);
        cursor_draws=0;ui_draw_root(p->ui,0,NULL,NULL);
        CHECK(cursor_draws==1 && fade_alpha==255 && cursor_draw_x==x && cursor_draw_y==y && cursor_texture==1);
        /* An unpainted/hidden source must not reinitialize a cached cursor. */
        menu *unpainted=menu_open(NULL,"fixture",error,sizeof error);CHECK(unpainted);
        ui_cursor_transfer(p->ui,unpainted->ui);
        cursor_draws=0;ui_draw_root(p->ui,0,NULL,NULL);
        CHECK(cursor_draws==1 && cursor_draw_x==x && cursor_draw_y==y);
        menu_close(unpainted);
        menu *next=menu_open(NULL,"fixture",error,sizeof error);CHECK(next);
        ui_cursor_transfer(next->ui,p->ui);menu_enter(next,1);menu_tick(next);
        ui_widget *destination=ui_root_widget(next->ui,1)->kids[0];
        destination->x=500;destination->y=400;destination->w=40;destination->h=30;
        destination->has_geometry=1;set_focus(next,destination);
        ui_fade(next->ui,1,0,.5f);ui_draw_root(next->ui,1,NULL,NULL);
        CHECK(cursor_draw_x==x && cursor_draw_y==y && cursor_texture==0);
        menu_animate(next,.1f);ui_draw_root(next->ui,1,NULL,NULL);
        CHECK(cursor_draw_x!=x || cursor_draw_y!=y);
        /* Persistence is transition-specific; it cannot leak into an idle
           page with no focus once the fade is cleared. */
        ui_fade(p->ui,0,0,0);cursor_draws=0;ui_draw_root(p->ui,0,NULL,NULL);
        CHECK(cursor_draws==0);
        /* The fixed original update clock gives the same displayed cursor
           at 30/60/120 Hz, including the delayed tilt after arrival. */
        float reference[3]={0};
        for(int rate=30;rate<=120;rate*=2) {
            menu *clock=menu_open(NULL,"fixture",error,sizeof error);CHECK(clock);
            ui_cursor_transfer(clock->ui,p->ui);menu_enter(clock,1);menu_tick(clock);
            ui_widget *target=ui_root_widget(clock->ui,1)->kids[0];
            target->x=500;target->y=400;target->w=40;target->h=30;
            target->has_geometry=1;set_focus(clock,target);
            ui_draw_root(clock->ui,1,NULL,NULL);
            float initial_angle=cursor_draw_angle;
            menu_animate(clock,1.0f/(float)rate);ui_draw_root(clock->ui,1,NULL,NULL);
            CHECK(cursor_draw_angle==initial_angle);
            for(int tick=1;tick<rate;tick++) {
                menu_animate(clock,1.0f/(float)rate);ui_draw_root(clock->ui,1,NULL,NULL);
            }
            float actual[3]={cursor_draw_x,cursor_draw_y,cursor_draw_angle};
            if(rate==30)memcpy(reference,actual,sizeof reference);
            else CHECK(!memcmp(reference,actual,sizeof reference));
            menu_close(clock);
        }
        /* A moving popup updates its cursor target in the same frame:
           the target is NOT the panel position from the preceding draw. */
        ui_widget *panel=ui_root_widget(next->ui,1),*button=panel->kids[0];
        set_focus(next,button);ui_draw_root(next->ui,1,NULL,NULL);
        float before_x=cursor_draw_x,before_y=cursor_draw_y,angle=cursor_draw_angle;
        float old_y=panel->y;panel->y+=24;
        float expected_x=before_x,expected_y=before_y;
        float raw_x=button->x+panel->x+button->w*.5f;
        float raw_y=button->y+panel->y+button->h*.5f;
        gt4_cursor_step(&expected_x,&expected_y,&angle,fminf(640,fmaxf(0,raw_x)),fminf(480,fmaxf(0,raw_y)),-22.5f,1);
        ui_animate(next->ui,1,1.0f/60);ui_draw_root(next->ui,1,NULL,NULL);
        CHECK(cursor_draw_x==expected_x && cursor_draw_y==expected_y);
        /* Closing holds the cursor on its choice through either exit,
           including the target used when the underlying page is restored. */
        ui_cursor_hold(next->ui,1);
        for(int direction=0;direction<2;direction++) {
            panel->y=direction?600:-600;
            float ex=cursor_draw_x,ey=cursor_draw_y,ea=cursor_draw_angle;
            (void)ea;
            ui_animate(next->ui,1,1.0f/60);ui_draw_root(next->ui,1,NULL,NULL);
            CHECK(cursor_draw_x==ex && cursor_draw_y==ey);
        }
        /* A purchase strip is hidden before the incoming pane is shown.
           The context cursor must remain painted at its held screen point. */
        button->visible=0;cursor_draws=0;
        float held_x=cursor_draw_x,held_y=cursor_draw_y;
        ui_draw_root(next->ui,1,NULL,NULL);CHECK(cursor_draws==1);
        CHECK(cursor_draw_x==held_x && cursor_draw_y==held_y);
        button->visible=1;
        ui_cursor_hold(next->ui,0);
        panel->opacity=0;cursor_draws=0;ui_draw_root(next->ui,1,NULL,NULL);
        CHECK(cursor_draws==1); /* cursor is outside the fading page */
        panel->opacity=1;panel->y=old_y;
        /* Interleave moving-panel actors and cursor ticks at every refresh
           rate. Batching all panel ticks before cursor ticks fails this. */
        float popup_reference[3]={0};
        for(int rate=30;rate<=120;rate*=2) {
            menu *moving=menu_open(NULL,"fixture",error,sizeof error);CHECK(moving);
            menu_enter(moving,1);menu_tick(moving);ui_cursor_transfer(moving->ui,p->ui);
            ui_widget *moving_panel=ui_root_widget(moving->ui,1),*moving_button=moving_panel->kids[0];
            moving_panel->y=480;moving_button->x=500;moving_button->y=0;
            moving_button->w=40;moving_button->h=30;moving_button->has_geometry=1;
            set_focus(moving,moving_button);ui_draw_root(moving->ui,1,NULL,NULL);
            ah_val args[3]={maybe_module(moving,moving_panel),ah_float(moving_panel->x),ah_float(0)},actor;
            CHECK(nat_actor(moving->vm,NULL,"MMoveActor",3,args,&actor)==0);
            ah_set_member(moving->vm,actor.u.o,"out",ah_int(1));
            for(int frame=0;frame<rate/2;frame++) {
                menu_animate(moving,1.0f/(float)rate);ui_draw_root(moving->ui,1,NULL,NULL);
            }
            float result[3]={cursor_draw_x,cursor_draw_y,moving_panel->y};
            if(rate==30)memcpy(popup_reference,result,sizeof result);
            else CHECK(!memcmp(popup_reference,result,sizeof result));
            menu_close(moving);
        }
        /* Authored modal actors hold the selected screen point through both
           exit directions; closePage releases it for the underlying focus. */
        for(int direction=0;direction<2;direction++) {
            menu *modal=menu_open(NULL,"fixture",error,sizeof error);CHECK(modal);
            menu_enter(modal,0);menu_tick(modal);
            modal->pages[modal->depth++]=1;menu_enter(modal,1);menu_tick(modal);
            ui_widget *pane=ui_root_widget(modal->ui,1),*choice=pane->kids[0];
            pane->y=100;choice->x=200;choice->y=150;choice->w=40;choice->h=30;
            choice->has_geometry=1;set_focus(modal,choice);
            ui_draw_root(modal->ui,1,NULL,NULL);
            for(int tick=0;tick<60;tick++){menu_animate(modal,1.f/60);ui_draw_root(modal->ui,1,NULL,NULL);}
            float x=cursor_draw_x,y=cursor_draw_y;
            ah_val args[3]={maybe_module(modal,pane),ah_float(pane->x),ah_float(480)},actor;
            CHECK(nat_actor(modal->vm,NULL,"MMoveActor",3,args,&actor)==0);
            float goal=direction?480:-200;
            ah_set_member(modal->vm,actor.u.o,"destinationY",ah_float(goal));
            ah_set_member(modal->vm,actor.u.o,"out",ah_int(1));
            for(int tick=0;tick<30;tick++) {
                menu_animate(modal,1.f/60);ui_draw_root(modal->ui,1,NULL,NULL);
                CHECK(cursor_draw_x==x && cursor_draw_y==y);
            }
            CHECK(fabsf(pane->y-goal)<1);
            modal->pending_kind=3;modal->pending_page=1;apply_pages(modal);
            ui_draw_root(modal->ui,0,NULL,NULL);menu_animate(modal,1.f/60);
            ui_draw_root(modal->ui,0,NULL,NULL);
            CHECK(cursor_draw_x!=x || cursor_draw_y!=y);
            menu_close(modal);
        }
        /* The title screen hides the arrow through context.cursor_visible;
           focus and coordinates survive so the next visible page can chase. */
        ui_draw_root(next->ui,1,NULL,NULL);
        float hidden_x=cursor_draw_x,hidden_y=cursor_draw_y;
        ah_set_member(next->vm,next->context,"cursor_visible",ah_int(0));
        menu_animate(next,0);cursor_draws=0;ui_draw_root(next->ui,1,NULL,NULL);
        CHECK(cursor_draws==0);
        ah_set_member(next->vm,next->context,"cursor_visible",ah_int(1));
        menu_animate(next,0);cursor_draws=0;ui_draw_root(next->ui,1,NULL,NULL);
        CHECK(cursor_draws==1 && cursor_draw_x==hidden_x && cursor_draw_y==hidden_y);
        /* An inactive horizontal list must not claim the context cursor. */
        ui_widget *inactive=ui_widget_copy(next->ui,panel);
        inactive->type="ListBox";inactive->name="InactiveStrip";
        inactive->x=10;inactive->y=10;inactive->w=600;inactive->h=50;
        inactive->focused=0;inactive->kids[0]->focused=0;
        ui_widget_append(panel,inactive);
        set_focus(next,button);ui_draw_root(next->ui,1,NULL,NULL);
        float sx=cursor_draw_x,sy=cursor_draw_y,sa=cursor_draw_angle;
        gt4_cursor_step(&sx,&sy,&sa,raw_x,button->y+panel->y+button->h*.5f,-22.5f,1);
        menu_animate(next,1.f/60);ui_draw_root(next->ui,1,NULL,NULL);
        CHECK(cursor_draw_x==sx && cursor_draw_y==sy);
        /* Cached page changes still present the loading icon during the
           original two Sync(2) teardown barriers, before resetting it. */
        menu *loading=menu_open(NULL,"fixture",error,sizeof error);CHECK(loading);
        menu_enter(loading,0);menu_tick(loading);ui_draw_root(loading->ui,0,NULL,NULL);
        ui_fade(loading->ui,0,1,.5f);ui_cursor_icon(loading->ui,"load");
        loading->loading_seed=1;
        float first=menu_loading_duration(loading);int varied=0;
        for(int i=0;i<128;i++) {
            float duration=menu_loading_duration(loading);
            CHECK(duration>=.5f && duration<=1);varied|=duration!=first;
        }
        CHECK(varied);
        loading->present_wait=loading_wait;
        ah_val page_arg=maybe_module(loading,ui_root_widget(loading->ui,1)),page_result;
        loading_presented=0;
        CHECK(nat_page(loading->vm,NULL,(void *)2,1,&page_arg,&page_result)==0);
        CHECK(loading_presented>=30 && loading_presented<=60 && menu_active_root(loading)==0);
        ui_cursor_icon(loading->ui,NULL);apply_pages(loading);
        ui_fade(loading->ui,1,0,.5f);ui_draw_root(loading->ui,1,NULL,NULL);
        CHECK(menu_active_root(loading)==1 && cursor_texture==0);
        menu_close(loading);
        /* Outgoing presentation holds its current position, even if it was
           still chasing the last button. Reveal continues without a snap. */
        menu *flight=menu_open(NULL,"fixture",error,sizeof error);CHECK(flight);
        menu_enter(flight,1);menu_tick(flight);
        ui_widget *flight_button=ui_root_widget(flight->ui,1)->kids[0];
        flight_button->x=500;flight_button->y=400;flight_button->w=40;flight_button->h=30;
        flight_button->has_geometry=1;set_focus(flight,flight_button);
        ui_draw_root(flight->ui,1,NULL,NULL);
        float fx=cursor_draw_x,fy=cursor_draw_y,fa=cursor_draw_angle;
        ui_fade(flight->ui,0,1,.5f);menu_animate(flight,1.f/60);
        ui_draw_root(flight->ui,1,NULL,NULL);
        CHECK(cursor_draw_x==fx && cursor_draw_y==fy);
        ui_fade(flight->ui,1,0,.5f);ui_draw_root(flight->ui,1,NULL,NULL);
        CHECK(cursor_draw_x==fx && cursor_draw_y==fy);
        gt4_cursor_step(&fx,&fy,&fa,520,415,-22.5f,1);
        menu_animate(flight,1.f/60);ui_draw_root(flight->ui,1,NULL,NULL);
        CHECK(cursor_draw_x==fx && cursor_draw_y==fy);
        CHECK(cursor_draw_x!=520 || cursor_draw_y!=415);
        menu_close(flight);

        menu_close(next);menu_close(p);cursor_fixture_enabled=0;
    }
    /* BGM belongs to authored pages and native keepMusic overrides. A popup
       marked keep must not restart the song, and re-entry restores its group. */
    {
        char error[256];menu *p=menu_open(NULL,"fixture",error,sizeof error);CHECK(p);
        menu_set_audio_preparer(p,record_music_preparation,NULL);
        menu_enter(p,0);CHECK(!strcmp(menu_music_name(p),"map"));
        CHECK(prepared_music_requests>0);unsigned before=prepared_music_requests;
        menu_enter(p,1);CHECK(!strcmp(menu_music_name(p),"map"));
        CHECK(prepared_music_requests==before);
        ah_val song=ah_str(p->vm,"POWERSPEED"),result;
        CHECK(nat_music(p->vm,NULL,NULL,1,&song,&result)==0);
        CHECK(!strcmp(menu_music_name(p),"powerspeed"));
        menu_set_music_fader(p,record_music_fade);
        ah_val fade_period=ah_float(.3f);
        CHECK(nat_music(p->vm,NULL,(void *)1,1,&fade_period,&result)==0);
        CHECK(fabsf(requested_music_fade-.3f)<1e-6);

        CHECK(!*menu_music_name(p));
        menu_enter(p,0);CHECK(!strcmp(menu_music_name(p),"map"));
        menu_close(p);
    }
    /* Native colour transitions complete on strict crossing, not equality.
       In particular a one-frame period requires two native updates. */
    {
        char error[256];menu *p=menu_open(NULL,"fixture",error,sizeof error);CHECK(p);menu_enter(p,0);
        ui_fade(p->ui,0,1,1.0f/60);
        menu_animate(p,1.0f/60);CHECK(ui_fade_remaining(p->ui)>0);
        menu_animate(p,1.0f/60);CHECK(ui_fade_remaining(p->ui)==0);
        ui_fade(p->ui,1,0,1.0f/60);
        menu_animate(p,1.0f/60);CHECK(ui_fade_remaining(p->ui)>0);
        menu_animate(p,1.0f/60);CHECK(ui_fade_remaining(p->ui)==0);
        menu_close(p);
    }
    /* Cross and block calls retain a render surface and never fall back to
       a colour fade. Sync and page commit preserve the original phases. */
    {
        char error[256];menu *p=menu_open(NULL,"fixture",error,sizeof error);CHECK(p);menu_enter(p,0);
        ui_set_capture_renderer(p->ui,capture_fixture);
        for(int kind=1;kind<=2;kind++) {
            ah_val transition,out,arg=ah_objv(p->context);
            CHECK(nat_transition(p->vm,NULL,kind==1?"MCrossTransition":"MBlockTransition",0,NULL,&transition)==0);
            ah_set_member(p->vm,p->context,"transition",transition);
            capture_calls=retained_draws=0;
            CHECK(ah_call_member(p->vm,transition.u.o,"panOutIn",1,&out,&arg)==1);
            CHECK(capture_calls==1 && p->pending_fade_in<0);
            gt4_transition_motion *motion=ui_transition_state(p->ui);
            CHECK(motion->active && motion->stage==1 && ui_fade_remaining(p->ui)==0);
            /* The outgoing frame stays resident through both native loading
               barriers, including when the departed root is hidden. */
            if(kind==1){
                int captures=capture_calls;ui_widget*page=ui_root_widget(p->ui,0);page->visible=0;
                for(int frame=0;frame<60;frame++){
                    menu_animate(p,1.f/60);retained_draws=0;ui_draw_root(p->ui,0,NULL,NULL);
                    if(getenv("GT4_TRANSITION_HOLD_NEGATIVE") && !frame)capture_calls++;
                    CHECK(capture_calls==captures && retained_draws==1 && retained_alpha==255);
                }page->visible=1;
            }
            /* panIn waits for RootWindow preparation; ready is not implied
               by calling panIn (0028A4F0 -> 0024D1D0). */
            apply_pages(p);CHECK(motion->stage==2 && p->pending_fade_in==0);
            ui_draw_root(p->ui,0,NULL,NULL);CHECK(retained_draws>0);
            if(kind==2)CHECK(retained_draws==32*24 && retained_w==20 && retained_alpha==255);
            else {
                CHECK(retained_alpha==128); /* PS2 feedback ratio defaults to .5. */
                CHECK(retained_u==0 && retained_v==1);
                int count=capture_calls;ui_draw_root(p->ui,0,NULL,NULL);
                CHECK(capture_calls==count); /* no extra feedback at >60 Hz */
            }
            for(int frame=0;frame<240;frame++)menu_animate(p,1.0f/60);
            CHECK(!motion->active && motion->stage==3 && ui_transition_remaining(p->ui)==0);
            p->initializing++;
            CHECK(ah_call_member(p->vm,transition.u.o,"panIn",1,&out,&arg)==1);
            CHECK(motion->stage==1 && p->pending_fade_in<0);
            for(int frame=0;frame<12;frame++)menu_animate(p,1.f/60);
            CHECK(motion->stage==1 && motion->value==1);
            p->initializing--;apply_pages(p);CHECK(motion->stage==2);

        }
        menu_close(p);
    }
    /* Render a packed regulation grid repeatedly: decorative faces must
       not become size requests and grow the layout every frame. */
    {
        char error[128];ui_project *p=ui_project_load(NULL,"packed",error,sizeof error);CHECK(p);
        ui_widget *grid=ui_widget_child(ui_root_widget(p,0),"Grid");CHECK(grid && grid->nkids==4);
        ui_draw_root(p,0,NULL,NULL);
        float width=grid->w,height=grid->h,first=grid->kids[0]->w,row=grid->kids[2]->y;
        CHECK(width==360 && height==84 && grid->kids[1]->x==first && row>0);
        for(int i=0;i<120;i++){ui_animate(p,0,1.f/60);ui_draw_root(p,0,NULL,NULL);}
        CHECK(grid->w==width && grid->h==height && grid->kids[0]->w==first && grid->kids[2]->y==row);
        CHECK(grid->kids[0]->kids[0]->w==first && grid->kids[0]->kids[1]->x==10);
        /* Popup hover and committed-choice marker belong to different rows.
           Closing the popup must hide its marker in the closed value face. */
        ui_widget *choice=ui_widget_from_text(p,0,
            "OptionMenu { name string {\"Choice\"} geometry rectangle {20 20 100 24} children [1] { Composite { name string {\"popup\"} children [1] { VBox { name string {\"vbox\"} children [3] {"
            "ColorFace { name string {\"bg\"} color [1] { RGBA {191 191 191 210} } }"
            "Composite { name string {\"One\"} geometry rectangle {0 0 100 24} children [2] { ColorFace { name string {\"bgf\"} visible_compare digit {2} visible_condition digit {2} } ImageFace { name string {\"dot\"} visible_compare digit {1} visible_condition digit {1} } } }"
            "Composite { name string {\"Two\"} geometry rectangle {0 24 100 24} children [2] { ColorFace { name string {\"bgf\"} visible_compare digit {2} visible_condition digit {2} } ImageFace { name string {\"dot\"} visible_compare digit {1} visible_condition digit {1} } } }"
            "} } } } } }");CHECK(choice);
        ui_widget_append(ui_root_widget(p,0),choice);choice->focused=1;choice->option_open=1;choice->option_pending=1;
        ui_widget *one=ui_option_item(choice,0),*two=ui_option_item(choice,1);CHECK(one && two);
        ui_draw_root(p,0,NULL,NULL);unsigned tick=ui_draw_tick(p);
        CHECK(ui_widget_child(one,"bgf")->drawn_tick!=tick && ui_widget_child(two,"bgf")->drawn_tick==tick);
        CHECK(ui_widget_child(one,"dot")->drawn_tick==tick && ui_widget_child(two,"dot")->drawn_tick!=tick);
        choice->option_open=0;ui_draw_root(p,0,NULL,NULL);
        CHECK(ui_widget_child(one,"dot")->drawn_tick!=ui_draw_tick(p));
        ui_project_free(p);
    }
    /* A short synthetic system note retains its dry attack and gains a
       delayed, decaying stereo tail instead of an immediate doubled outline. */
    {
        uint8_t bank[512]={0};memcpy(bank,"INST",4);memcpy(bank+0x2c,"SShd",4);
        bank[16]=32;bank[20]=0;bank[21]=1;bank[0x3c]=0x60;bank[0x44]=0x80;
        bank[0x82]=8;bank[0x8a]=16;
        const uint8_t notes[]={0xa0,48,127,0,0,0xff,0x2f};memcpy(bank+0x90,notes,sizeof notes);
        bank[0xa2]=8;bank[0xa8]=255;bank[0xa9]=127;bank[0xae]=bank[0xaf]=48;
        bank[0xb2]=48;bank[0xb4]=0x80;bank[0xbc]=127;bank[0xbd]=64;
        bank[0x100]=4;memset(bank+0x102,0x77,14);bank[0x110]=4;bank[0x111]=1;
        int16_t *pcm=NULL;size_t frames=0;
        CHECK(gt4_sound_render(bank,sizeof bank,0,&pcm,&frames) && frames>38000);
        CHECK(pcm[0]>0);
        for(size_t f=100;f<1400;f++)CHECK(pcm[f*2]==0 && pcm[f*2+1]==0);
        double early=0,late=0;int stereo=0;
        for(size_t f=1400;f<12000;f++){early+=abs(pcm[f*2])+abs(pcm[f*2+1]);stereo|=pcm[f*2]!=pcm[f*2+1];}
        for(size_t f=24000;f<frames;f++)late+=abs(pcm[f*2])+abs(pcm[f*2+1]);
        CHECK(early>0 && late<early && stereo);free(pcm);
    }
    /* Authored routing can differ from visual placement. Regions use right/
       bottom bounds; shared scrolling must not affect the chosen direction. */
    {
        char error[128];
        const char *source="Button { navigate_source region {0 0 20 20} }";
        const char *aligned="Button { navigate_target region {-90 40 -70 60} }";
        const char *diagonal="Button { navigate_target region {60 0 80 20} }";
        mp_node *s=mp_parse(source,strlen(source),error,sizeof error);
        mp_node *a=mp_parse(aligned,strlen(aligned),error,sizeof error);
        mp_node *d=mp_parse(diagonal,strlen(diagonal),error,sizeof error);
        CHECK(s && a && d);
        ui_widget parent={0},from={0},target={0},nearby={0};
        from.src=s;target.src=a;nearby.src=d;
        from.w=target.w=nearby.w=from.h=target.h=nearby.h=20;
        from.parent=target.parent=nearby.parent=&parent;
        target.x=100;nearby.y=30;
        float rect[4];navigation_region(&target,"navigate_target",rect);
        CHECK(rect[0]==10 && rect[1]==40 && rect[2]==30 && rect[3]==60);
        float score=navigation_score(&from,&target,MENU_DOWN);
        CHECK(score<navigation_score(&from,&nearby,MENU_DOWN));
        CHECK(navigation_score(&from,&target,MENU_UP)>=1e30f);
        parent.x=-203;parent.y=-419;
        CHECK(fabsf(score-navigation_score(&from,&target,MENU_DOWN))<.001f);
        CHECK(navigation_score(&from,&nearby,MENU_DOWN)>=1e30f);
        /* Real map bounds: Netherlands UP must not jump across to City Courses. */
        const char *net="Button { navigate_source region {-128 16 -64 80} }";
        const char *city="Button { navigate_target region {-368 -16 240 48} }";
        mp_node *net_node=mp_parse(net,strlen(net),error,sizeof error);
        mp_node *city_node=mp_parse(city,strlen(city),error,sizeof error);
        CHECK(net_node && city_node);
        ui_widget net_widget={0},city_widget={0};
        net_widget.src=net_node;net_widget.x=480;net_widget.y=144;
        city_widget.src=city_node;city_widget.x=1072;city_widget.y=80;
        CHECK(navigation_score(&net_widget,&city_widget,MENU_UP)>=1e30f);
        mp_free(net_node);mp_free(city_node);
        from.src=NULL; /* menus without regions keep geometric navigation */
        CHECK(navigation_score(&from,&nearby,MENU_DOWN)<navigation_score(&from,&target,MENU_DOWN));
        mp_free(s);mp_free(a);mp_free(d);
    }
    /* GT Mode's render context must share page state and retain its transition
       across GC. Timed fades cover the complete composed UI. */
    {
        char error[256];menu *p=menu_open(NULL,"fixture",error,sizeof error);CHECK(p);
        menu_enter(p,0);
        ah_val arg=ah_int(1),out,transition;
        CHECK(ah_call_member(p->vm,p->context,"createRenderContext",1,&arg,&out)==1);
        arg=ah_int(0);
        CHECK(ah_call_member(p->vm,p->context,"getRenderContext",1,&arg,&out)==1);
        CHECK(out.u.o==p->context);
        CHECK(nat_transition(p->vm,NULL,"MColorTransition",0,NULL,&transition)==0);
        ah_set_member(p->vm,p->context,"transition",transition);ah_gc(p->vm);
        CHECK(ah_call_member(p->vm,transition.u.o,"panOut",1,&out,&arg)==1);
        menu_animate(p,.1f);ui_draw_root(p->ui,0,NULL,NULL);CHECK(fade_alpha>=101 && fade_alpha<=103);
        for(int i=0;i<4;i++)menu_animate(p,.1f);
        ui_draw_root(p->ui,0,NULL,NULL);CHECK(fade_alpha==255);
        /* arcade.ad _start_page L52..55: completed panOut/syncOut must
           remain black while native autodemo dispatch changes projects. */
        ah_obj*option=submodule(p->vm,game_object(p),"option");
        ah_set_member(p->vm,option,"demo_movie_interval",ah_int(1));
        demo_movie_count=0;attract_left=4;attract_next(p);
        CHECK(!strcmp(p->next_project,"boot:MovieRoot"));
        if(getenv("GT4_ATTRACT_BLACK_NEGATIVE"))ui_fade(p->ui,0,0,0);
        fade_alpha=0;ui_draw_root(p->ui,0,NULL,NULL);CHECK(fade_alpha==255);
        attract_left=0;p->next_project[0]=0;
        CHECK(ah_call_member(p->vm,transition.u.o,"panIn",1,&out,&arg)==1);
        for(int i=0;i<6;i++)menu_animate(p,.1f);
        fade_alpha=0;ui_draw_root(p->ui,0,NULL,NULL);CHECK(fade_alpha==0);
        CHECK(ui_fade_remaining(p->ui)==0);

        /* Native settings return must share the authored fade and cursor
           loading path, while a second request cannot restart that fade. */
        ah_obj *original_project=p->project;
        p->project=ah_module_new(p->vm,"OptionProject",NULL,NULL,NULL);
        menu_enter(p,0);
        CHECK(p->pending_fade_in==.25f);
        apply_pages(p);
        for(int i=0;i<20;i++)menu_animate(p,1.f/60);
        CHECK(ui_fade_remaining(p->ui)==0);
        menu_native_page(p,1);
        CHECK(p->pending_kind==2 && p->pending_page==1);
        CHECK(p->pending_fade_in==.25f);
        menu_animate(p,1.f/60);
        float remaining=ui_fade_remaining(p->ui);
        menu_native_page(p,1);
        CHECK(ui_fade_remaining(p->ui)==remaining);
        apply_pages(p);
        CHECK(menu_active_root(p)==1 && p->pending_fade_in==0);
        CHECK(ui_fade_remaining(p->ui)>0);
        for(int i=0;i<20;i++)menu_animate(p,1.f/60);
        CHECK(ui_fade_remaining(p->ui)==0);
        p->project=original_project;
        menu_enter(p,0);

        ui_widget *root=ui_root_widget(p->ui,0),*other=ui_root_widget(p->ui,1);
        ui_widget *progress=ui_widget_copy(p->ui,root->kids[0]->kids[0]);
        progress->type="ProgressFace";
        /* Era-card insertion must preserve the decoration behind the cards,
           including when moving a child that is already attached. */
        ui_widget *space=ui_widget_from_text(p->ui,0,"Composite { name string {\"space\"} }");
        ui_widget *surface=ui_widget_from_text(p->ui,0,"Composite { name string {\"Surface\"} }");
        ui_widget *era=ui_widget_from_text(p->ui,0,"Composite { name string {\"era\"} }");
        CHECK(space && surface && era);ui_widget_append(root,space);ui_widget_append(space,surface);
        ah_val insertion[2]={maybe_module(p,era),maybe_module(p,surface)};
        CHECK(ah_call_member(p->vm,widget_module(p,space),"insertChild",2,insertion,&arg)>0);
        CHECK(space->nkids==2 && space->kids[0]==era && space->kids[1]==surface && era->root==root->root);
        CHECK(ah_call_member(p->vm,widget_module(p,space),"insertChild",2,insertion,&arg)>0);
        CHECK(space->nkids==2 && space->kids[0]==era && space->kids[1]==surface);
        ah_val object=maybe_module(p,progress),pr;
        CHECK(ah_call_member(p->vm,widget_module(p,root),"appendChild",1,&object,&arg)==1);
        CHECK(progress->parent==root);
        CHECK(nat_progress(p->vm,NULL,NULL,0,NULL,&pr)==0);
        ah_set_member(p->vm,object.u.o,"progress",pr);ah_gc(p->vm);
        ah_set_member(p->vm,pr.u.o,"value",ah_float(.3));menu_animate(p,.01f);
        CHECK(fabsf(progress->progress-.3f)<.0001f);
        ah_set_member(p->vm,pr.u.o,"value",ah_float(1.5));menu_animate(p,.01f);
        CHECK(progress->progress==1);
        ah_val args[2]={ah_objv(p->context),object};
        CHECK(ah_call_member(p->vm,widget_module(p,other),"removeChild",2,args,&arg)==1);
        CHECK(progress->parent==root); /* cannot remove another page's child */
        CHECK(ah_call_member(p->vm,widget_module(p,root),"removeChild",2,args,&arg)==1);
        CHECK(!progress->parent);
        CHECK(ah_call_member(p->vm,widget_module(p,other),"appendChild",2,args,&arg)==1);
        CHECK(progress->parent==other && progress->root==other->root);
        CHECK(progress->resource_root==root->resource_root);
        CHECK(ah_call_member(p->vm,transition.u.o,"panOutIn",1,&out,&arg)==1);
        p->pending_kind=2;p->pending_page=0; /* no page change */
        apply_pages(p);CHECK(p->pending_fade_in==0);
        for(int i=0;i<6;i++)menu_animate(p,.1f);
        fade_alpha=0;ui_draw_root(p->ui,0,NULL,NULL);CHECK(fade_alpha==0);
        ah_set_member(p->vm,transition.u.o,"period",ah_float(0));
        CHECK(ah_call_member(p->vm,transition.u.o,"panOutIn",1,&out,&arg)==1);
        fade_alpha=0;ui_draw_root(p->ui,0,NULL,NULL);CHECK(fade_alpha==0);
        ui_widget *landmark=ui_widget_copy(p->ui,root->kids[0]->kids[0]);
        landmark->type="ModelFace";ui_widget_append(root,landmark);
        int entered_legacy=0,left_legacy=0;
        hook(p,landmark,"onEnter",on_count,&entered_legacy);
        hook(p,landmark,"onLeave",on_count,&left_legacy);
        set_focus(p,landmark);CHECK(entered_legacy==1);
        set_focus(p,root->kids[0]->kids[0]);CHECK(left_legacy==1);
        set_focus(p,landmark);CHECK(entered_legacy==2);
        ah_obj *velocity=ah_array_new(p->vm);
        ah_array_push(p->vm,velocity,ah_float(0));ah_array_push(p->vm,velocity,ah_float(2));ah_array_push(p->vm,velocity,ah_float(0));
        ah_set_member(p->vm,widget_module(p,landmark),"rotate_v",ah_objv(velocity));
        ah_set_member(p->vm,widget_module(p,landmark),"target_mode",ah_int(0));
        const int rates[]={30,60,120,144};
        for(int r=0;r<4;r++) {
            landmark->model_angles[1]=0;
            for(int i=0;i<rates[r];i++)menu_animate(p,1.f/rates[r]);
            CHECK(fabsf(landmark->model_angles[1]-120)<.01f);
        }
        ah_set_member(p->vm,widget_module(p,landmark),"target_mode",ah_int(1));
        for(int i=0;i<120;i++)menu_animate(p,1.f/60);
        CHECK(fabsf(landmark->model_angles[1])<.01f);
        menu_close(p);
    }
    char err[256]; menu *m = menu_open(NULL, "fixture", err, sizeof err); CHECK(m);
    ui_widget *home = ui_root_widget(m->ui, 0), *details = ui_root_widget(m->ui, 1);
    ui_widget *list = ui_widget_child(home, "List"), *one = list->kids[0], *two = list->kids[2];
    {
        ah_obj *object=widget_module(m,list);ah_val result,index=ah_int(2);
        CHECK(ah_call_member(m->vm,object,"countChild",0,NULL,&result)==1 && result.u.i==3);
        CHECK(ah_call_member(m->vm,object,"getChild",1,&index,&result)==1 && ah_host_data(result.u.o)==two);
        index=ah_int(-1);CHECK(ah_call_member(m->vm,object,"getChild",1,&index,&result)==1 && result.t==AV_NIL);
        ui_widget *image=ui_widget_from_text(m->ui,0,"ImageFace { name string {\"banner\"} }");CHECK(image);
        ah_val path=ah_str(m->vm,"piece/event/test.img");
        CHECK(ah_call_member(m->vm,widget_module(m,image),"setImagePath",1,&path,&result)==1);
        CHECK(image->image_path && !strcmp(image->image_path,"piece/event/test.img"));
        ah_val speed;CHECK(ah_get_member(m->vm,m->context,"cursor_speed_ratio",&speed) && ah_num(speed)==1);
    }
    /* Original chase defaults, integer phase, stop/restart and VM ownership. */
    {
        menu *p=menu_open(NULL,"fixture",err,sizeof err);CHECK(p);
        ui_widget *one=ui_widget_child(ui_root_widget(p->ui,0),"List")->kids[0];
        menu_enter(p,0);
        one->x=106;one->y=-28;
        ah_val arg=ah_objv(widget_module(p,one)),actor,result;
        CHECK(nat_actor(p->vm,NULL,"MChaseActor",1,&arg,&actor)==0);
        ah_pin(p->vm,actor);ah_gc(p->vm);
        menu_animate(p,1.f/60);CHECK(one->x==106 && one->y==-28);
        const int rates[]={30,60,120,144};
        for(int r=0;r<4;r++) {
            CHECK(ah_call_member(p->vm,actor.u.o,"doStart",0,NULL,&result)==1);
            for(int i=0;i<rates[r]/2;i++)menu_animate(p,1.f/rates[r]);
            CHECK(one->x==106 && fabsf(one->y-(-28-16*fabsf(sinf(240*3.141592502593994f/180))))<.001f);
            CHECK(ah_call_member(p->vm,actor.u.o,"doStop",0,NULL,&result)==1);
            CHECK(one->x==106 && one->y==-28);
        }
        CHECK(ah_call_member(p->vm,actor.u.o,"doStart",0,NULL,&result)==1);
        menu_animate(p,1.f/60);
        CHECK(fabsf(one->y-(-28-16*sinf(8*3.141592502593994f/180)))<.001f);
        CHECK(ah_call_member(p->vm,actor.u.o,"doStop",0,NULL,&result)==1);
        ah_unpin(p->vm,actor);ah_gc(p->vm);one->x=one->y=0;
        menu_close(p);
    }
    /* UI selection survives GC, nested pages refer to earlier categories,
       and cancellation returns the exact selections without touching saves. */
    {
        ah_obj *game=game_object(m);ah_val result,a=ah_str(m->vm,"italy"),b=ah_str(m->vm,"alfaromeo"),depth=ah_int(1);
        CHECK(ah_call_member(m->vm,game,"menuPush",1,&a,&result)==1);
        CHECK(ah_call_member(m->vm,game,"menuPush",1,&b,&result)==1);ah_gc(m->vm);
        CHECK(ah_call_member(m->vm,game,"menuRef",1,&depth,&result)==1 && !strcmp(ah_cstr(result),"italy"));
        CHECK(ah_call_member(m->vm,game,"menuPop",0,NULL,&result)==1 && !strcmp(ah_cstr(result),"alfaromeo"));
        CHECK(ah_call_member(m->vm,game,"menuPop",0,NULL,&result)==1 && !strcmp(ah_cstr(result),"italy"));
        CHECK(ah_call_member(m->vm,game,"menuPop",0,NULL,&result)==1 && result.t==AV_NIL);
        CHECK(ah_call_member(m->vm,game,"menuRef",0,NULL,&result)==1 && result.t==AV_NIL);
        ah_val sentence=ah_str(m->vm,"{1}: Enter {0}, race on {1}."),arguments[]={ah_str(m->vm,"Sunday Cup"),ah_str(m->vm,"Autumn Ring")};
        CHECK(ah_call_member(m->vm,sentence.u.o,"build",2,arguments,&result)==1);
        CHECK(!strcmp(ah_cstr(result),"Autumn Ring: Enter Sunday Cup, race on Autumn Ring."));
        ah_val string=ah_str(m->vm,"l0b0016"),range[2]={ah_int(3),ah_int(4)},number;
        CHECK(ah_call_member(m->vm,string.u.o,"substr",2,range,&result)==1 && !strcmp(ah_cstr(result),"0016"));
        CHECK(ah_call_member(m->vm,result.u.o,"toInt",0,NULL,&number)==1 && ah_num(number)==16);
        ah_val absent_grade=ah_str(m->vm,"NONE");CHECK(ah_call_member(m->vm,absent_grade.u.o,"toInt",0,NULL,&number)==1 && number.u.i==0);
        ah_val prefix_grade=ah_str(m->vm,"  -12trailing");CHECK(ah_call_member(m->vm,prefix_grade.u.o,"toInt",0,NULL,&number)==1 && number.u.i==-12);
        range[0]=ah_int(100);CHECK(ah_call_member(m->vm,string.u.o,"substr",1,range,&result)==1 && !strcmp(ah_cstr(result),""));
        range[0]=ah_int(-1);CHECK(ah_call_member(m->vm,string.u.o,"substr",1,range,&result)<0);
        ah_val main_,menu_,type,color;ah_get_member(m->vm,ah_root(m->vm),"main",&main_);ah_get_member(m->vm,main_.u.o,"menu",&menu_);
        CHECK(ah_get_member(m->vm,menu_.u.o,"MColorObject",&type));
        ah_val rgb[4]={ah_float(.2),ah_float(.4),ah_float(.6),ah_float(.5)};
        CHECK(ah_call(m->vm,type,4,rgb,&color)==0);
        ah_set_member(m->vm,widget_module(m,one),"color",color);CHECK(one->has_runtime_color && one->runtime_color[0]==51 && one->runtime_color[3]==128);
        ah_val white[3]={ah_float(1),ah_float(1),ah_float(1)},target,blend;
        CHECK(ah_call(m->vm,type,3,white,&target)==0);
        ah_val mix[3]={color,target,ah_float(.5)};
        CHECK(ah_call_member(m->vm,type.u.o,"interpolate",3,mix,&blend)==1);
        ah_set_member(m->vm,widget_module(m,one),"shadow_color",blend);
        CHECK(one->has_runtime_shadow && one->runtime_shadow[0]==153 && one->runtime_shadow[3]==191);
        ah_set_member(m->vm,widget_module(m,one),"test_color",color);ah_gc(m->vm);
        CHECK(ah_get_member(m->vm,color.u.o,"g",&result) && fabs(ah_num(result)-.4)<.00001);
    }
    hook(m, home, "onActivate", on_open, NULL);
    hook(m, details, "onCancel", on_close, NULL);
    hook(m, details, "onInitialize", on_count, &initialized);
    hook(m, details, "onFinalize", on_count, &finalized);
    hook(m, list, "onValueChanged", on_count, &changed);
    hook(m, one, "onFocusEnter", on_count, &focused);
    hook(m, one, "onFocusLeave", on_count, &left);
    const int refresh[]={30,60,120,144};
    for(int rate=0;rate<4;rate++) {
        m->idle_frames=0;m->tick_remainder=0;
        for(int i=0;i<refresh[rate];i++)menu_advance(m,1.0/refresh[rate]);
        CHECK(m->idle_frames==60);
    }
    m->idle_frames=0;m->tick_remainder=0;
    /* Actors advance on original frames, reverse continuously, and keep their widget
       alive across GC. Fade and move controls come from the original scripts. */
    ah_val actor_args[3]={ah_objv(widget_module(m,one)),ah_float(1),ah_float(0.2)}, actor_v, ignored;
    CHECK(nat_actor(m->vm,NULL,"MFadeActor",3,actor_args,&actor_v)==0);
    ah_pin(m->vm,actor_v);
    ah_set_member(m->vm,actor_v.u.o,"period",ah_float(0.1));
    ah_set_member(m->vm,actor_v.u.o,"out",ah_int(1));
    CHECK(ah_call_member(m->vm,actor_v.u.o,"warp",0,NULL,&ignored)==1);
    CHECK(fabsf(one->opacity-0.2f)<0.001f);
    m->pages[0]=0; m->depth=1;
    ah_set_member(m->vm,actor_v.u.o,"out",ah_int(0));
    menu_animate(m,0.05f); CHECK(fabsf(one->opacity-0.6f)<0.001f);
    ah_gc(m->vm);
    menu_animate(m,0.05f); CHECK(fabsf(one->opacity-1)<0.001f);
    ah_set_member(m->vm,actor_v.u.o,"out",ah_int(1));
    menu_animate(m,0.025f); CHECK(fabsf(one->opacity-(1.0f-0.8f/6.0f))<0.001f);
    ah_call_member(m->vm,actor_v.u.o,"stop",0,NULL,&ignored);
    menu_animate(m,0.05f); CHECK(fabsf(one->opacity-(1.0f-0.8f/6.0f))<0.001f);
    ah_set_member(m->vm,actor_v.u.o,"over",ah_float(.2));
    ah_set_member(m->vm,actor_v.u.o,"repeat",ah_int(1));
    ah_set_member(m->vm,actor_v.u.o,"out",ah_int(0));
    ah_call_member(m->vm,actor_v.u.o,"warp",0,NULL,&ignored);
    menu_animate(m,.1f);CHECK(one->opacity==1);
    menu_animate(m,.1f);CHECK(one->opacity==1);
    ah_unpin(m->vm,actor_v); for(int i=0;i<25000;i++) ah_array_new(m->vm); ah_gc(m->vm); CHECK(m->actors==NULL);
    one->opacity=1;
    actor_args[1]=ah_float(100); actor_args[2]=ah_float(50);
    CHECK(nat_actor(m->vm,NULL,"MMoveActor",3,actor_args,&actor_v)==0);
    ah_pin(m->vm,actor_v);
    ah_set_member(m->vm,actor_v.u.o,"out",ah_int(1));
    menu_animate(m,1.0f/60); CHECK(fabsf(one->x-30)<0.01f && fabsf(one->y-15)<0.01f);
    ah_call_member(m->vm,actor_v.u.o,"warp",0,NULL,&ignored); CHECK(one->x==100 && one->y==50);
    ah_unpin(m->vm,actor_v); ah_gc(m->vm); one->x=one->y=0;
    /* Nested modal confirmation may close its own loop without waking the
       suspended parent. The parent resumes only after its separate close. */
    menu_set_event_presenter(m,modal_present,NULL);
    unsigned previous_budget=ah_exchange_instruction_budget(m->vm,73);
    CHECK(nat_event_loop(m->vm,NULL,NULL,0,NULL,&ignored)==0);
    CHECK(modal_frames==3 && modal_inner_done && m->event_depth==0);
    CHECK(ah_exchange_instruction_budget(m->vm,previous_budget)==73);
    menu_set_event_presenter(m,NULL,NULL);
    menu_enter(m, 0); menu_tick(m);
    CHECK(!strcmp(menu_focus_name(m), "One") && one->focused && focused == 1);
    /* Stored modal focus must regain its visual state without firing a
       second enter callback, including when a parent page is uncovered. */
    one->focused=0;menu_tick(m);
    CHECK(one->focused && focused==1);
    menu_send_input(m, MENU_DOWN);
    CHECK(!strcmp(menu_focus_name(m), "Two") && list->focus_index == 2 && left == 1 && changed == 1);
    menu_send_input(m, MENU_ACCEPT);
    CHECK(menu_active_root(m) == 1 && initialized == 1 && activated == 1);
    menu_send_input(m, MENU_BACK);
    CHECK(menu_active_root(m) == 0 && finalized == 1 && !strcmp(menu_focus_name(m), "Two") && two->focused);
    /* SelectBox stops at its ends (CORE 0x002D7468/0x002D7528). */
    menu_send_input(m, MENU_DOWN); CHECK(!strcmp(menu_focus_name(m), "Two"));
    menu_send_input(m, MENU_UP); CHECK(!strcmp(menu_focus_name(m), "One"));
    menu_send_input(m, MENU_UP); CHECK(!strcmp(menu_focus_name(m), "One"));
    menu_send_input(m, MENU_DOWN); CHECK(!strcmp(menu_focus_name(m), "Two"));
    for (int i = 0; i < 12; i++) { menu_send_input(m, MENU_ACCEPT); menu_tick(m); menu_send_input(m, MENU_BACK); }
    CHECK(initialized == 13 && finalized == 13 && list->nkids == 3);
    two->visible = 0; menu_tick(m); CHECK(!strcmp(menu_focus_name(m), "One")); two->visible = 1;

    /* Dropdown indices count values, not decorative faces; unavailable values
       are skipped and programmatic changes do not dispatch input callbacks. */
    ui_widget *choice=ui_widget_copy(m->ui,one);choice->type="OptionMenu";choice->name="Choice";
    ui_widget *popup=ui_widget_copy(m->ui,one);popup->type="Composite";popup->name="popup";
    ui_widget *box=ui_widget_copy(m->ui,one);box->type="VBox";box->name="vbox";
    ui_widget *decoration=ui_widget_copy(m->ui,one);decoration->type="ColorFace";
    ui_widget *a=ui_widget_copy(m->ui,one),*b=ui_widget_copy(m->ui,one),*c=ui_widget_copy(m->ui,one);
    a->name="A";b->name="B";c->name="C";b->insensitive=1;
    ui_widget_append(home,choice);ui_widget_append(choice,popup);ui_widget_append(popup,box);
    ui_widget_append(box,decoration);ui_widget_append(box,a);ui_widget_append(box,b);ui_widget_append(box,c);
    CHECK(ui_option_count(choice)==3 && ui_option_item(choice,1)==b);
    int option_changes=0;hook(m,choice,"onValueChanged",on_count,&option_changes);
    set_focus(m,choice);menu_send_input(m,MENU_RIGHT);
    CHECK(choice->focus_index==0 && !option_changes && !choice->option_open);
    set_focus(m,choice);menu_send_input(m,MENU_ACCEPT);menu_send_input(m,MENU_RIGHT);
    CHECK(choice->option_pending==2 && !choice->focus_index && !option_changes);
    menu_send_input(m,MENU_ACCEPT);
    CHECK(choice->focus_index==2 && option_changes==1 && m->focus[0]==choice);
    menu_send_input(m,MENU_ACCEPT);menu_send_input(m,MENU_LEFT);menu_send_input(m,MENU_ACCEPT);
    CHECK(choice->focus_index==0 && option_changes==2);
    c->visible=0;menu_send_input(m,MENU_ACCEPT);CHECK(choice->option_open && choice->focus_index==0 && option_changes==2);
    menu_send_input(m,MENU_DOWN);CHECK(choice->option_pending==0);
    c->visible=1;menu_send_input(m,MENU_DOWN);
    CHECK(choice->option_pending==2 && choice->focus_index==0 && option_changes==2);
    menu_send_input(m,MENU_BACK);CHECK(!choice->option_open && choice->focus_index==0 && option_changes==2);
    menu_send_input(m,MENU_ACCEPT);menu_send_input(m,MENU_DOWN);menu_send_input(m,MENU_ACCEPT);
    CHECK(!choice->option_open && choice->focus_index==2 && option_changes==3);
    ah_val index_args[2]={ah_objv(m->context),ah_int(999)},index_result;
    CHECK(ah_call_member(m->vm,widget_module(m,choice),"setIndex",2,index_args,&index_result)==1);
    CHECK(choice->focus_index==2 && option_changes==3);
    ah_set_member(m->vm,widget_module(m,choice),"index",ah_int(-10));CHECK(choice->focus_index==0);
    choice->insensitive=1;menu_tick(m);CHECK(m->focus[0]!=choice);

    /* Sliders are one focus target, clamp authored ranges, and roll back an
       expanded edit on Cancel before the page commits preferences. */
    ui_widget *slider=ui_widget_copy(m->ui,one);slider->type="SliderBar";slider->name="Slider";
    const char *slider_source="SliderBar { range [2] { digit {-5} digit {5} } }";
    char slider_error[128];mp_node *slider_node=mp_parse(slider_source,strlen(slider_source),slider_error,sizeof slider_error);
    CHECK(slider_node);slider->src=slider_node;
    ui_widget_append(home,slider);
    int slider_changes=0;hook(m,slider,"onValueChanged",on_count,&slider_changes);
    set_focus(m,slider);slider->focus_index=0;
    slider->focus_index=-5;menu_send_input(m,MENU_LEFT);CHECK(slider->focus_index==-5 && slider_changes==0);slider->focus_index=0;
    menu_send_input(m,MENU_RIGHT);CHECK(slider->focus_index==1 && slider_changes==1);
    menu_send_input(m,MENU_ACCEPT);CHECK(slider->option_open);
    menu_send_input(m,MENU_RIGHT);CHECK(slider->focus_index==2);
    menu_send_input(m,MENU_BACK);CHECK(!slider->option_open && slider->focus_index==1 && slider_changes==3);
    slider->visible=0;

    /* Fitting content must never slide horizontally when focusing its edge. */
    ui_widget *clip=ui_widget_copy(m->ui,one),*space=ui_widget_copy(m->ui,one),*edge=ui_widget_copy(m->ui,one);
    clip->name="clip";space->name="space";clip->w=space->w=500;clip->h=space->h=100;
    edge->x=400;edge->w=100;edge->y=0;edge->h=100;
    ui_widget_append(home,clip);ui_widget_append(clip,space);ui_widget_append(space,edge);
    scroll_to(edge,1);CHECK(space->x==0 && space->scroll_tx==0);
    clip->visible=0;

    /* Original option values survive VM reload; uncommitted edits do not. */
    menu_options_storage(NULL);
    ah_val options;CHECK(ah_get_member(m->vm,game_object(m),"option",&options));
    ah_obj *option_obj=ah_as_obj(options,AO_MODULE);
    ah_set_member(m->vm,option_obj,"menu_se_volume_raw",ah_int(80));options_capture(m);
    ah_set_member(m->vm,option_obj,"menu_se_volume_raw",ah_int(32));options_restore(m);
    CHECK(menu_option_number(m,"menu_se_volume_raw",-1)==80);
    ah_val save_result;CHECK(nat_need_save(m->vm,NULL,m,0,NULL,&save_result)==0);
    ah_set_member(m->vm,option_obj,"menu_se_volume_raw",ah_int(72));apply_pages(m);
    ah_set_member(m->vm,option_obj,"menu_se_volume_raw",ah_int(16));options_restore(m);
    CHECK(menu_option_number(m,"menu_se_volume_raw",-1)==72);
    menu_options_storage(NULL);

    /* Focus transfers retain the selected child and stay inside the carousel. */
    ui_widget *other=ui_widget_copy(m->ui,list);other->name="OtherList";
    ui_widget_append(home,other);
    list->focus_index=2;
    CHECK(ah_call_member(m->vm,widget_module(m,list),"callFocus",0,NULL,&index_result)==1);
    CHECK(m->focus[0]==two);
    CHECK(ah_call_member(m->vm,widget_module(m,other),"outFocus",0,NULL,&index_result)==1);
    CHECK(m->focus[0]==two);
    CHECK(ah_call_member(m->vm,widget_module(m,home),"getFocusedWidget",0,NULL,&index_result)==1);
    CHECK(ah_host_data(index_result.u.o)==two);
    menu_send_input(m,MENU_RIGHT);CHECK(m->focus[0]==two); /* last item: stays */
    CHECK(ah_call_member(m->vm,widget_module(m,list),"outFocus",0,NULL,&index_result)==1);
    CHECK(!m->focus[0]);
    other->visible=0;
    /* Script-selected FlashFace assets own their paths and restart cleanly. */
    ui_widget *flash=ui_widget_copy(m->ui,one);flash->type="FlashFace";flash->name="Animation";
    ui_widget_append(home,flash);
    ah_val path=ah_str(m->vm,"synthetic/background.strb");flash->flash_time=7;
    CHECK(ah_call_member(m->vm,widget_module(m,flash),"setFlashPath",1,&path,&index_result)==1);
    CHECK(!strcmp(flash->flash_path,"synthetic/background.strb") && flash->flash_time==0);
    ui_widget *flash_copy=ui_widget_copy(m->ui,flash);
    CHECK(flash_copy->flash_path!=flash->flash_path && !strcmp(flash_copy->flash_path,flash->flash_path));
    flash->flash_time=12;
    CHECK(ah_call_member(m->vm,widget_module(m,flash),"reset",0,NULL,&index_result)==1 && flash->flash_time==0);
    flash->visible=0;
    ah_val fmt=ah_str(m->vm,"%02d/%d %s %%"),fa[3]={ah_int(3),ah_int(12),ah_str(m->vm,"courses")};
    CHECK(ah_call_member(m->vm,fmt.u.o,"format",3,fa,&index_result)==1);
    CHECK(!strcmp(ah_cstr(index_result),"03/12 courses %"));
    CHECK(ah_call_member(m->vm,fmt.u.o,"format",0,NULL,&index_result)<0);
    ah_val metres=ah_float(20832);
    CHECK(nat_course_units(m->vm,NULL,NULL,1,&metres,&index_result)==0);
    CHECK(!strcmp(ah_cstr(index_result),"20.83 km"));

    ah_val args[2] = {ah_native(m->vm, "queued", queued, NULL), ah_str(m->vm, "survives GC")}, ev, result;
    CHECK(nat_event_new(m->vm, NULL, NULL, 2, args, &ev) == 0);
    CHECK(nat_push_event(m->vm, NULL, m, 1, &ev, &result) == 0);
    ah_gc(m->vm); menu_tick(m); CHECK(deferred == 1 && !m->nevents);
    ah_val dialog[4] = {ah_objv(m->context), ah_int(1), ah_str(m->vm, "Synthetic question"), ah_str(m->vm, "Test")};
    dialog_result = 0; CHECK(nat_dialog_open(m->vm, NULL, NULL, 4, dialog, &result) == 0 && !ah_truthy(result));
    dialog_result = 1; CHECK(nat_dialog_open(m->vm, NULL, NULL, 4, dialog, &result) == 0 && ah_truthy(result));
    ah_set_member(m->vm, game_object(m), "next_menu_project", ah_str(m->vm, "option"));
    CHECK(nat_finish(m->vm, NULL, m, 0, NULL, &result) == 0);
    char name[64]; CHECK(menu_take_project(m, name, sizeof name) && !strcmp(name, "option"));
    CHECK(!menu_take_project(m, name, sizeof name));
    m->idle_frames=123;menu_send_input(m,MENU_UP);CHECK(m->idle_frames==0);
    ah_val option_values;CHECK(ah_get_member(m->vm,game_object(m),"option",&option_values));
    ah_set_member(m->vm,ah_as_obj(option_values,AO_MODULE),"demo_movie_interval",ah_int(2));
    ah_set_member(m->vm,game_object(m),"next_sequence",ah_str(m->vm,"autodemo"));
    /* No disc catalog exists in this fixture: abandon a tape safely.
       The real 13-tape scheduler is compared with the EE in outside_menu. */
    CHECK(nat_finish(m->vm,NULL,m,0,NULL,&result)==0 && menu_take_project(m,name,sizeof name));
    CHECK(!menu_attract_active() && !strcmp(name,attract_origin));
    demo_movie_count=0;
    CHECK(nat_finish(m->vm,NULL,m,0,NULL,&result)==0 && menu_take_project(m,name,sizeof name));
    CHECK(!strcmp(name,"boot:MovieRoot") && menu_attract_active());
    menu_attract_finish(m,1);CHECK(menu_take_project(m,name,sizeof name) && !menu_attract_active());
    demo_movie_count=1;CHECK(nat_demo_reset(m->vm,NULL,m,0,NULL,&result)==0 && demo_movie_count==0);
    ah_set_member(m->vm, game_object(m), "next_sequence", ah_str(m->vm, "race"));
    CHECK(nat_finish(m->vm, NULL, m, 0, NULL, &result) < 0);
    CHECK(!menu_take_project(m, name, sizeof name));
    ah_val translated[2] = {maybe_module(m, home), ah_str(m->vm, "Already translated")};
    CHECK(nat_translate(m->vm, NULL, m, 2, translated, &result) == 0);
    CHECK(!strcmp(ah_cstr(result), "Already translated"));
    /* Runtime list templates are distinct copies; navigation calls the script. */
    ui_widget *cars=ui_widget_copy(m->ui,list);cars->type="ListBox";cars->name="Cars";
    ui_widget_append(home,cars);while(cars->nkids)ui_widget_detach(cars->kids[cars->nkids-1]);
    ah_val template_args[2]={ah_objv(m->context),maybe_module(m,one)};
    CHECK(ah_call_member(m->vm,widget_module(m,cars),"setItemTemplate",2,template_args,&result)==1);
    ah_val three=ah_int(3);
    CHECK(ah_call_member(m->vm,widget_module(m,cars),"setItemCount",1,&three,&result)==1);
    CHECK(cars->nkids==3 && cars->kids[0]!=cars->kids[1] && cars->kids[0]!=one);
    for(int i=0;i<3;i++)cars->kids[i]->w=100;
    /* Cards that all fit stay centred as a row: the offset is the row's
       middle (312 / 2) whatever the focus. */
    float box_w=cars->w;cars->w=1000;cars->focus_index=2;cars->strip_ready=0;ui_animate(m->ui,0,1.f/60);
    CHECK(fabsf(cars->strip_offset-156)<.1f);
    /* Wider than the box: the strip follows the focused card. */
    cars->w=150;
    cars->focus_index=0;cars->strip_ready=0;ui_animate(m->ui,0,1.f/60);
    float first_offset=cars->strip_offset;cars->focus_index=1;ui_animate(m->ui,0,1.f/60);
    CHECK(cars->strip_offset>first_offset && cars->strip_offset<156);
    for(int i=0;i<60;i++)ui_animate(m->ui,0,1.f/60);
    CHECK(fabsf(cars->strip_offset-156)<.1f);
    cars->w=box_w;
    ah_val second=ah_int(1);
    CHECK(ah_call_member(m->vm,widget_module(m,cars),"getItemWidget",1,&second,&result)==1);
    CHECK(ah_host_data(result.u.o)==cars->kids[1]);
    int car_changes=0;hook(m,cars,"onValueChanged",on_count,&car_changes);
    cars->focus_index=0;set_focus(m,cars);menu_send_input(m,MENU_LEFT);
    CHECK(cars->focus_index==2 && car_changes==1);menu_send_input(m,MENU_RIGHT);
    CHECK(cars->focus_index==0 && car_changes==2);
    ah_set_member(m->vm,widget_module(m,cars->kids[0]),"name",ah_str(m->vm,"new_name"));
    CHECK(!strcmp(cars->kids[0]->name,"new_name"));
    ah_code *nested=calloc(1,sizeof *nested);nested->source=strdup("synthetic module");nested->count=1;
    nested->ins=calloc(1,sizeof *nested->ins);nested->ins[0].op=AH_MODULE_DEFINE;
    nested->ins[0].npath=1;nested->ins[0].path=calloc(1,sizeof(char *));nested->ins[0].path[0]=strdup("Catalog");
    CHECK(ah_run_module(m->vm,nested,m->project)==0);
    CHECK(ah_get_member(m->vm,m->project,"Catalog",&result));
    CHECK(!ah_get_member(m->vm,ah_root(m->vm),"Catalog",&result));

    /* Rotating selection arcs take the requested direction across 0 degrees. */
    ah_val rot_args[3]={maybe_module(m,one),ah_float(0),ah_int(1)},rot;
    CHECK(nat_actor(m->vm,NULL,"MRotateActor",3,rot_args,&rot)==0);
    ah_set_member(m->vm,widget_module(m,one),"rotation_test",rot);
    ah_set_member(m->vm,rot.u.o,"destination",ah_float(90));
    menu_animate(m,1.f/60);CHECK(one->rotation>0 && one->rotation<90);
    for(int i=0;i<80;i++)menu_animate(m,1.f/60);
    CHECK(fabsf(one->rotation-90)<.01f);
    ah_set_member(m->vm,rot.u.o,"destination",ah_float(0));
    menu_animate(m,1.f/60);CHECK(one->rotation>90 && one->rotation<360);

    /* A script that waits forever must release control; the next call works. */
    ah_code *loop = calloc(1, sizeof *loop);
    loop->source = strdup("synthetic loop"); loop->count = 1;
    loop->ins = calloc(1, sizeof *loop->ins); loop->ins[0].op = AH_JUMP;
    CHECK(ah_run_script(m->vm, loop) < 0);
    ah_code *ok = calloc(1, sizeof *ok);
    ok->source = strdup("synthetic recovery"); ok->count = 1;
    ok->ins = calloc(1, sizeof *ok->ins); ok->ins[0].op = AH_NOP;
    CHECK(ah_run_script(m->vm, ok) == 0);
    /* Imported numeric keys resolve through the loaded disc table and price
       methods stay bound to the owning car. This tests bridge routing only. */
    m->car_load_attempted=1;m->car_labels_size=32;m->car_labels=calloc(1,32);
    memcpy(m->car_labels,"GTID",4);uint32_t count=1,key=42;
    memcpy(m->car_labels+4,&count,4);memcpy(m->car_labels+20,&key,4);
    m->car_labels[24]=6;memcpy(m->car_labels+26,"track",6);
    m->car_db_size=48;m->car_db=calloc(1,48);memcpy(m->car_db,"GTDB",4);
    memcpy(m->car_db+8,&count,4);uint32_t width=24,base_price=40000;
    memcpy(m->car_db+12,&width,4);memcpy(m->car_db+16,&key,4);memcpy(m->car_db+32,&base_price,4);
    /* Original model-year getter uses signed lh, including the sentinel -1. */
    ah_obj *metadata_type=ah_module_new(m->vm,"MCarData",NULL,NULL,NULL);
    ah_set_member(m->vm,metadata_type,"GetModelYear",ah_native(m->vm,"GetModelYear",nat_car_metadata,"GetModelYear"));
    ah_set_member(m->vm,metadata_type,"GetOpenModelType",ah_native(m->vm,"GetOpenModelType",nat_car_metadata,"GetOpenModelType"));
    ah_val metadata_arg=ah_str(m->vm,"track"),metadata_value;
    uint8_t *generic_row=m->car_db+24;
    /* Purchase page calls these installed natives before loading CarFace. */
    ah_val main_value,main_menu,registered_car;
    CHECK(ah_get_member(m->vm,ah_root(m->vm),"main",&main_value));
    CHECK(ah_get_member(m->vm,main_value.u.o,"menu",&main_menu));
    CHECK(ah_get_member(m->vm,main_menu.u.o,"MCarData",&registered_car));
    CHECK(ah_call_member(m->vm,registered_car.u.o,"GetPrice",1,&metadata_arg,&metadata_value)>0 && metadata_value.u.i==40000);
    ah_val formatted;
    CHECK(nat_money_string(m->vm,NULL,NULL,1,&metadata_value,&formatted)==0 && !strcmp(ah_cstr(formatted),"40,000"));
    uint32_t price_cases[]={0,1,0x7fffffffu,0x80000000u,0xffffffffu};
    for(unsigned i=0;i<5;i++){
        memcpy(generic_row+8,&price_cases[i],4);int32_t expected;memcpy(&expected,&price_cases[i],4);
        CHECK(ah_call_member(m->vm,registered_car.u.o,"GetPrice",1,&metadata_arg,&metadata_value)>0 && metadata_value.u.i==expected);
    }
    memcpy(generic_row+8,&base_price,4);
    for(unsigned i=0;i<256;i++){
        generic_row[18]=(uint8_t)i;
        CHECK(ah_call_member(m->vm,registered_car.u.o,"NotForSell",1,&metadata_arg,&metadata_value)>0 && metadata_value.u.i==(i&1u));
    }
    generic_row[18]=0;

    generic_row[12]=0xff;generic_row[13]=0xff;generic_row[20]=0xff;
    CHECK(ah_call_member(m->vm,metadata_type,"GetModelYear",1,&metadata_arg,&metadata_value)>0 && metadata_value.u.i==-1);
    CHECK(ah_call_member(m->vm,metadata_type,"GetOpenModelType",1,&metadata_arg,&metadata_value)>0 && metadata_value.u.i==255);
    generic_row[12]=0;generic_row[13]=0x80;
    CHECK(ah_call_member(m->vm,metadata_type,"GetModelYear",1,&metadata_arg,&metadata_value)>0 && metadata_value.u.i==-32768);
    generic_row[12]=0xda;generic_row[13]=7;
    CHECK(ah_call_member(m->vm,metadata_type,"GetModelYear",1,&metadata_arg,&metadata_value)>0 && metadata_value.u.i==2010);
    CHECK(nat_calendar_car_key(m->vm,NULL,m,1,&metadata_arg,&metadata_value)==0 && metadata_value.u.i==42);
    gt4_career_price_data price_data={0};uint32_t threshold=UINT32_MAX,factor=0x3f000000;
    memcpy(price_data.mileage_factors,&threshold,4);memcpy(price_data.mileage_factors+4,&factor,4);gt4_game_set_price_data(&price_data);
    ah_obj *owned=ah_module_new(m->vm,"car",NULL,NULL,NULL);ah_set_member(m->vm,owned,"code",ah_int(42));ah_set_member(m->vm,owned,"table",ah_int(0));ah_set_member(m->vm,owned,"odometer",ah_int(1000000));
    ah_val owned_arg=ah_objv(owned),owned_result;
    CHECK(nat_bind_game_car(m->vm,NULL,m,1,&owned_arg,&owned_result)==0);
    CHECK(ah_get_member(m->vm,owned,"car_code",&owned_result) && !strcmp(ah_cstr(owned_result),"track"));
    CHECK(ah_call_member(m->vm,owned,"getUsedPrice",0,NULL,&owned_result)>0 && owned_result.u.i==20000);
    CHECK(ah_call_member(m->vm,owned,"getUsedSellPrice",0,NULL,&owned_result)>0 && owned_result.u.i==5000);
    /* Bind a disc label to its original imported record ordinal. */
    m->race_load_attempted=1;m->race_labels_size=35;m->race_labels=calloc(1,35);
    memcpy(m->race_labels,"GTID",4);memcpy(m->race_labels+4,&count,4);memcpy(m->race_labels+20,&key,4);
    m->race_labels[24]=8;memcpy(m->race_labels+26,"l0b0001",8);
    m->race_db_size=24;m->race_db=calloc(1,24);memcpy(m->race_db,"GTDB",4);
    memcpy(m->race_db+8,&count,4);memcpy(m->race_db+16,&key,4);
    ah_obj *career=ah_module_new(m->vm,"career",NULL,NULL,NULL);gt4_game_bind(m->vm,career);
    ah_val rr;CHECK(ah_get_member(m->vm,career,"race_record",&rr));
    ah_obj *records=ah_array_new(m->vm),*imported_record=ah_module_new(m->vm,"record",NULL,NULL,NULL);
    ah_set_member(m->vm,imported_record,"original_record",ah_int(0x12340991));ah_array_push(m->vm,records,ah_objv(imported_record));
    ah_set_member(m->vm,rr.u.o,"records",ah_objv(records));
    ah_val label=ah_str(m->vm,"l0b0001");
    CHECK(ah_call_member(m->vm,rr.u.o,"getUnit",1,&label,&owned_result)>0);CHECK(owned_result.u.o!=imported_record);
    ah_set_member(m->vm,rr.u.o,"_getRecordIndex",ah_native(m->vm,"_getRecordIndex",nat_race_record_index,m));
CHECK(ah_call_member(m->vm,rr.u.o,"getUnit",1,&label,&owned_result)>0);
    CHECK(owned_result.u.o==imported_record);
    CHECK(ah_get_member(m->vm,owned_result.u.o,"original_record",&rr) && rr.u.i==0x12340991);
    /* Actual bound quote bridge reads the requested CARS list and namespace
       table, distinguishes stages, and retains the original missing sentinel. */
    gt4_career_part_data part_rules={0};part_rules.price_branches[0]=0x44a338;
    strcpy(part_rules.table_names[1],"TEST_PART");gt4_game_set_part_data(&part_rules);
    part_list_fixture[0]=2;part_list_fixture[8]=7;part_list_fixture[12]=1;
    part_list_fixture[24]=8;part_list_fixture[28]=1;part_list_fixture[32]=1;
    memcpy(part_db_fixture,"GTDB",4);part_db_fixture[8]=1;part_db_fixture[12]=24;
    part_db_fixture[16]=7;part_db_fixture[24]=0x34;part_db_fixture[25]=0x12;
    part_fixture_enabled=1;ah_val part_args[]={ah_int(1),ah_int(0)};
    CHECK(ah_call_member(m->vm,owned,"getPartsPrice",2,part_args,&owned_result)>0 && owned_result.u.i==0x1234);
    part_args[1]=ah_int(1);CHECK(ah_call_member(m->vm,owned,"getPartsPrice",2,part_args,&owned_result)>0 && owned_result.u.i==-1);
    part_args[1]=ah_int(INT64_C(0x100000000));CHECK(ah_call_member(m->vm,owned,"getPartsPrice",2,part_args,&owned_result)>0 && owned_result.u.i==-1);
    ah_set_member(m->vm,metadata_type,"GetCategory",ah_native(m->vm,"GetCategory",nat_car_metadata,"GetCategory"));
    strcpy(part_rules.category_names[2],"fixture-category");strcpy(part_rules.category_fallback,"fixture-missing");
    gt4_game_set_part_data(&part_rules);generic_row[17]=202;
    CHECK(ah_call_member(m->vm,metadata_type,"GetCategory",1,&metadata_arg,&metadata_value)>0 && !strcmp(ah_cstr(metadata_value),"fixture-category"));
    generic_row[17]=99;
    CHECK(ah_call_member(m->vm,metadata_type,"GetCategory",1,&metadata_arg,&metadata_value)>0 && !strcmp(ah_cstr(metadata_value),"fixture-missing"));
    part_fixture_enabled=0;gt4_game_set_part_data(NULL);
    /* A callable native MRaceData must bind getters to its instance, keep
       the decoded row alive across GC, and use the boxed/row offset split. */
    free(m->race_db);m->race_db_size=24+151;m->race_db=calloc(1,m->race_db_size);
    memcpy(m->race_db,"GTDB",4);memcpy(m->race_db+8,&count,4);memcpy(m->race_db+16,&key,4);
    uint32_t race_width=151;memcpy(m->race_db+12,&race_width,4);uint8_t *row=m->race_db+24;
    row[0x20]=25;row[0x2c]=0xff;row[0x2d]=0xff;
    row[0x38]=0x80;row[0x39]=0xff;row[0x3a]=0x34;row[0x3b]=0x12;
    row[0x45]=3;row[0x92]=42;row[0x93]=7;row[0x94]=100;row[0x95]=45;row[0x96]=8;
    row[0x3e]=6;row[0x18]=0x34;row[0x19]=0x12;row[0x43]=9;
    ah_obj *race_type=ah_native_class(m->vm,"MRaceData",nat_race_init,m);
    ah_val instance;CHECK(ah_call(m->vm,ah_objv(race_type),1,&label,&instance)==0);
    ah_pin(m->vm,instance);ah_gc(m->vm);
    const char *getter[]={"getNeedLicense","getNeedPs","getNeedPrice","getLimitedWeight",
        "getLimitedLength","getNeedCountryID","getNumberOfLaps","getLimitTime","getPrizeGC"};
    const int expected[]={3,420,70000,1000,4500,8,9,0x1234,-100};
    for(unsigned i=0;i<sizeof expected/sizeof expected[0];i++){
        CHECK(ah_call_member(m->vm,instance.u.o,getter[i],0,NULL,&owned_result)>0);
        CHECK(owned_result.u.i==expected[i]);
    }
    ah_val rank=ah_int(0);CHECK(ah_call_member(m->vm,instance.u.o,"getPrize",1,&rank,&owned_result)>0 && owned_result.u.i==2500);
    rank=ah_int(-1);CHECK(ah_call_member(m->vm,instance.u.o,"getPrize",1,&rank,&owned_result)>0 && owned_result.u.i==0);
    CHECK(ah_call_member(m->vm,instance.u.o,"getNeedYearRange",0,NULL,&owned_result)>0);
    CHECK(ah_array_get(owned_result.u.o,0).u.i==-128 && ah_array_get(owned_result.u.o,1).u.i==0x1234);
    /* Constructor fallback is broader than static available(label). */
    free(m->race_labels);m->race_labels_size=36;m->race_labels=calloc(1,36);
    memcpy(m->race_labels,"GTID",4);memcpy(m->race_labels+4,&count,4);memcpy(m->race_labels+20,&key,4);
    m->race_labels[24]=10;memcpy(m->race_labels+26,"usl0b0001",10);
    gt4_career_race_lookup_data lookup={0};memcpy(lookup.prefixes[0],"us",3);memcpy(lookup.prefixes[1],"jp",3);
    lookup.driver_points[0]=10;lookup.driver_points[1]=6;lookup.driver_points[5]=255;
    gt4_game_set_race_lookup_data(&lookup);
    CHECK(ah_get_member(m->vm,instance.u.o,"entry_num",&owned_result) && owned_result.u.i==6);
    rank=ah_int(0);CHECK(ah_call_member(m->vm,instance.u.o,"getAdditionalDriverPoints",1,&rank,&owned_result)>0 && owned_result.u.i==10);
    rank=ah_int(5);CHECK(ah_call_member(m->vm,instance.u.o,"getAdditionalDriverPoints",1,&rank,&owned_result)>0 && owned_result.u.i==-1);
    ah_val fallback;label=ah_str(m->vm,"l0b0001");
    CHECK(nat_race_available(m->vm,NULL,m,1,&label,&fallback)==0 && !ah_truthy(fallback));
    CHECK(ah_call(m->vm,ah_objv(race_type),1,&label,&fallback)==0);
    CHECK(ah_call_member(m->vm,fallback.u.o,"getNeedPs",0,NULL,&owned_result)>0 && owned_result.u.i==420);
    gt4_game_set_race_lookup_data(NULL);
    ah_unpin(m->vm,instance);
    gt4_game_set_price_data(NULL);
    /* The authored course initializer must survive syncLoading and apply
       CarModel.ry / warp, rather than treating the model as nil. */
    ui_widget *car_face=ui_widget_from_text(m->ui,0,"CarFace { name string {\"CarFace\"} geometry rectangle {200 45 500 375} }");CHECK(car_face);
    car_face->car_code=strdup("fixture_car");ah_obj *car_module=widget_module(m,car_face);
    menu_set_car_loader(m,fixture_car_loader,NULL);
    CHECK(ah_call_member(m->vm,car_module,"syncLoading",0,NULL,&owned_result)>0 && loaded_car_faces==1);
    ah_val model;CHECK(ah_get_member(m->vm,car_module,"car_model",&model) && ah_as_obj(model,AO_MODULE)!=NULL);
    ah_set_member(m->vm,model.u.o,"ry",ah_float(120));
    CHECK(ah_call_member(m->vm,model.u.o,"warp",0,NULL,&owned_result)>0);
    CHECK(car_face->car_pose_set && car_face->model_angles[1]==120);
    ah_set_member(m->vm,model.u.o,"tz",ah_float(-2));
    CHECK(car_face->car_position[2]==-2);
    /* The disc sets motion=1 then ry=120 and warp on both event/license
       previews. An explicit pose must continue turning at the native rate. */
    ui_widget_append(ui_root_widget(m->ui,0),car_face);
    car_face->visible=1;
    ah_set_member(m->vm,model.u.o,"motion",ah_int(1));
    ui_animate(m->ui,0,1.f/60);
    CHECK(car_face->model_angles[1]>120 && car_face->model_angles[1]<120.03f);
    CHECK(car_face->car_spin>1.24f && car_face->car_spin<1.25f);
    for(int i=0;i<59;i++)ui_animate(m->ui,0,1.f/60);
    CHECK(car_face->model_angles[1]>134 && car_face->model_angles[1]<135);
    float angle=car_face->model_angles[1];
    ah_set_member(m->vm,model.u.o,"motion",ah_int(0));ui_animate(m->ui,0,1);
    CHECK(car_face->model_angles[1]==angle);
    ah_set_member(m->vm,model.u.o,"motion",ah_int(2));ui_animate(m->ui,0,1);
    CHECK(car_face->model_angles[1]<angle && car_face->model_angles[1]>119.99f); /* mode 2 eases back to authored yaw */
    ah_set_member(m->vm,model.u.o,"motion",ah_int(1));
    ah_set_member(m->vm,model.u.o,"ry",ah_float(120));
    CHECK(ah_call_member(m->vm,model.u.o,"warp",0,NULL,&owned_result)>0);
    CHECK(car_face->car_spin==0 && car_face->car_spin_ticks==0);
    ui_animate(m->ui,0,1.f/120);CHECK(car_face->model_angles[1]==120);
    ui_animate(m->ui,0,1.f/120);CHECK(car_face->model_angles[1]>120);
    menu_close(m); puts("native menu navigation: passed"); return 0;
}

int plat_audio_play(const int16_t *p,size_t n,int r,int c) { (void)p; (void)n; (void)r; (void)c; return 1; }

void plat_draw_glyph(const plat_texture *t,float x,float y,float w,float h,float b,float s,float u,float v,float u1,float v1,const uint8_t c[4]) { (void)t; (void)x; (void)y; (void)w; (void)h; (void)b; (void)s; (void)u; (void)v; (void)u1; (void)v1; (void)c; }

void plat_draw_rotated(const plat_texture *t,float x,float y,float w,float h,float cx,float cy,float a,const uint8_t c[4]) {(void)x;(void)y;(void)w;(void)h;(void)a;(void)c;cursor_texture=t->dummy;cursor_draws++;cursor_draw_x=cx;cursor_draw_y=cy;cursor_draw_angle=a;}
