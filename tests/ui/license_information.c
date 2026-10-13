#define main pregrid_fixture_main
#define plat_draw_glyph fixture_draw_glyph
#define plat_set_clip fixture_set_clip
#define plat_draw_quad4 fixture_draw_quad4
#include "pregrid_menu.c"
#undef main
#undef plat_draw_glyph
#undef plat_set_clip
#undef plat_draw_quad4
void plat_draw_quad4(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4])
{(void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;quads++;}
static float ink_bottom,clip_bottom;
static unsigned information_glyphs;
static float memory_float(FILE *f,long offset)
{float value;CHECK(!fseek(f,offset,SEEK_SET));CHECK(fread(&value,4,1,f)==1);return value;}
void plat_set_clip(float x,float y,float w,float h){(void)x;(void)w;clip_bottom=h>0?y+h:480;}
void plat_draw_glyph(const plat_texture*t,float x,float y,float w,float h,float b,float s,float u,float v,float u1,float v1,const uint8_t c[4])
{
    fixture_draw_glyph(t,x,y,w,h,b,s,u,v,u1,v1,c);
    if(c[0]==18 && c[1]==18 && c[2]==40){information_glyphs++;ink_bottom=fmaxf(ink_bottom,y+h);CHECK(y+h<=clip_bottom+.001f);}
}
/* Execute the disc's quick-license.ad L252..266 and L413..443; do not
   introduce a separate timer/scroll speed. Paragraph rasterization is A. */
int main(int argc,char **argv)
{
    CHECK(argc>=2);gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol && gt4_game_load_disc(argv[1]));
    menu_quick_race q={0};q.license=1;q.entries=1;q.gold_time=10150;q.silver_time=10400;q.bronze_time=11300;
    strcpy(q.race,"l0b0001");strcpy(q.course,"testcourse100");strcpy(q.grid[0].car,"demio_99");menu_set_quick_race(&q);
    const char *groups[]={"l0b","l0a","lib","lia","l0s"};unsigned paragraphs=0;
    for(int group=0;group<5;group++)for(int exam=1;exam<=16;exam++){
    snprintf(q.race,sizeof q.race,"%s%04d",groups[group],exam);menu_set_quick_race(&q);
    char error[256];menu *m=menu_open(vol,"quick-license",error,sizeof error);CHECK(m);ah_set_log(m->vm,log_script,NULL);
    int root=-1;for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),"LicenseRoot"))root=i;CHECK(root>=0);
    menu_enter(m,root);advance(m);
    ui_widget *scroll=child(child(ui_root_widget(m->ui,root),"Composite_title_bar"),"ScrollWindow");
    ui_widget *clip=child(scroll,"clip"),*space=child(clip,"space"),*label=child(child(space,"Info"),"label");
    ah_val value;CHECK(ah_get_member(m->vm,widget_module(m,space),"actor",&value));
    menu_actor *actor=ah_host_data(value.u.o);CHECK(actor && actor->kind==1);
    float size[2];ui_text_size(m->ui,label,size);
    if(!paragraphs && argc>2 && strncmp(argv[2],"--",2)){
        /* User's B-1 menu slot05: relative TextFace 01e34740, parent
           Composite 01e34670. Compare allocation, not the unproved measure. */
        FILE *f=fopen(argv[2],"rb");CHECK(f);
        float relative_h=memory_float(f,0x1e34740+0x40),parent_h=memory_float(f,0x1e34670+0x40);
        CHECK(relative_h==1 && parent_h>clip->h);fclose(f);
        ui_widget *info=child(space,"Info");float saved=info->h;ah_val unused;
        info->h=parent_h;CHECK(!nat_update_geometry(m->vm,widget_module(m,scroll),m,0,NULL,&unused));
        CHECK(label->h==relative_h*parent_h);
        info->h=saved;CHECK(!nat_update_geometry(m->vm,widget_module(m,scroll),m,0,NULL,&unused));
        puts("PASS B-1 PCSX2 relative paragraph height allocation, zero tolerance.");
    }
    CHECK(fabsf(label->h-space->h)<.001f);
    menu_send_input(m,MENU_DOWN);
    CHECK(actor->lo[1]<0);float target=actor->lo[1],before=space->y;menu_animate(m,1/60.f);
    CHECK(space->y<before && space->y>target);advance(m);CHECK(fabsf(space->y-target)<.001f);
    menu_send_input(m,MENU_UP);advance(m);CHECK(fabsf(space->y)<.001f);
    for(int i=0;i<100;i++)menu_send_input(m,MENU_DOWN);advance(m);advance(m);
    if(argc>2 && !strcmp(argv[2],"--negative"))space->y=0;
    if(fabsf(space->y+(label->h-clip->h))>=.001f)fprintf(stderr,"%s: y %.9g label %.9g clip %.9g target %.9g\n",q.race,space->y,label->h,clip->h,actor->lo[1]);
    CHECK(space->y<0 && fabsf(space->y+(label->h-clip->h))<.001f);
    ui_widget *vbar=child(scroll,"vbar"),*pinch=child(vbar,"pinch");
    CHECK(fabsf(pinch->y+pinch->h-clip->h)<.001f);
    information_glyphs=0;ink_bottom=0;ui_draw_root(m->ui,root,NULL,NULL);CHECK(information_glyphs);
    /* The disc handler can be used repeatedly, not just on the first entry. */
    for(int cycle=0;cycle<3;cycle++){
        for(int i=0;i<100;i++)menu_send_input(m,MENU_UP);advance(m);advance(m);CHECK(fabsf(space->y)<.001f);
        for(int i=0;i<100;i++)menu_send_input(m,MENU_DOWN);advance(m);advance(m);
        CHECK(fabsf(space->y+(label->h-clip->h))<.001f);
    }
    /* Production caches the VM across Exit/Options/Retry and new exams.
       Reinitializing without onFinalize left Info at its enlarged height,
       making the authored h > Info.h comparison hide the bar next time. */
    for(int visit=0;visit<3;visit++){
        if(visit!=1)menu_quick_event_context(m,q.race);
        CHECK(menu_enter_quick_start(m));advance(m);
        if(argc>2 && !strcmp(argv[2],"--reentry-negative"))vbar->visible=0;
        if(!vbar->visible)fprintf(stderr,"%s reentry %d: scrollbar hidden, Info h %g\n",q.race,visit,child(space,"Info")->h);
        CHECK(vbar->visible && fabsf(space->y)<.001f);
        menu_send_input(m,MENU_DOWN);advance(m);CHECK(space->y<0);
    }
    if(!paragraphs){
        const char *next[]={"lib0001","l0b0008","l0b0001","lib0001"};
        for(int i=0;i<4;i++){
            snprintf(q.race,sizeof q.race,"%s",next[i]);menu_set_quick_race(&q);
            menu_quick_event_context(m,q.race);CHECK(menu_enter_quick_start(m));advance(m);
            CHECK(vbar->visible && fabsf(space->y)<.001f && fabsf(label->h-space->h)<.001f);
            menu_send_input(m,MENU_DOWN);advance(m);CHECK(space->y<0);
        }
    }
    CHECK(!errors);menu_close(m);paragraphs++;
    }
    gt4_vol_close(vol);printf("PASS %u original licence Information paragraphs, scroll and complete last-line ink.\n",paragraphs);return 0;
}
