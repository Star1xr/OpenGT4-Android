/* The Logger (logger.mproject, logger.ad): MLoggerControl (constructor
   0011EA90, methods 0011F1E0), its LoggerFace (001BBC50: control +0xA4,
   progress +0xA8, grab +0xAC; update 001BC138, keys 001BC2C0, draw
   001BC1E0 -> 00120CF0) and the analysis (00122FA8). Included by menu.c. */

/* The last race's two tracks, given by the race's owner. A: the original
   analysis replays the race (00122FA8 +0x6C..0x1E0) and samples its
   entries 0 and 1; here the race samples them live as it runs. */
static struct {
    int valid;
    gt4_logger_sampler track[2];
    int present_b;
    uint32_t time[2];
    char car[2][32];
    int power[2];
    char course[32], mode[32];
    int subnumber;
} logger_source;

void menu_set_logger_source(const menu_logger_source *s)
{
    memset(&logger_source,0,sizeof logger_source);
    if(!s || !s->track[0] || !s->track[0]->count)return;
    logger_source.valid=1;
    logger_source.track[0]=*s->track[0];
    logger_source.present_b=s->track[1] && s->track[1]->count;
    logger_source.track[1]=logger_source.present_b?*s->track[1]:*s->track[0];
    for(int i=0;i<2;i++){
        logger_source.time[i]=s->time[i];logger_source.power[i]=s->power[i];
        snprintf(logger_source.car[i],sizeof logger_source.car[i],"%s",s->car[i]);
    }
    snprintf(logger_source.course,sizeof logger_source.course,"%s",s->course);
    snprintf(logger_source.mode,sizeof logger_source.mode,"%s",s->mode);
    logger_source.subnumber=s->subnumber;
}
static int logger_can(void)
{
    /* MQuickWork.CanLogger: a race to analyse, the licence not
       disabling it (0011D400 +0x4c..0x70, IB-6/IB-7). A: its getter's
       own conditions not read */
    return logger_source.valid && quick_race.can_replay && !(quick_race.license && quick_race.license_disable_logger);
}

static struct {
    gt4_logger_view view;
    ah_obj *control;
    int analyzing;            /* control +0x2A4: analyze_start until the finish event */
    int replaying;            /* the race replayed for it, a step a frame */
    float progress;           /* 00122CC0: +0x2AC / +0x2B0 */
    ui_widget *time_w[2], *average_w[2], *speed_w[2], *map_w;   /* +0x8C, +0x94, +0x9C, +0x80 */
    float sticks[4];          /* left x, y, right x, y (-1..1) */
} logger;

void menu_set_sticks(menu *m,const float axes[4])
{
    (void)m;for(int i=0;i<4;i++)logger.sticks[i]=fmaxf(-1,fminf(1,axes[i]));
}

static int logger_index(int argc,const ah_val *argv)
{
    int i=argc && ah_is_num(argv[0])?(int)ah_num(argv[0]):-1;
    return i==0 || i==1?i:-1;
}
static int logger_present(int i){return logger_source.valid && (i==0 || logger_source.present_b);}
static ui_widget *logger_widget_arg(int argc,const ah_val *argv,int at)
{
    ah_obj *o=argc>at?ah_as_obj(argv[at],AO_MODULE):NULL;
    return o && ah_host_of(o)==&widget_host?ah_host_data(o):NULL;
}

/* 00120238: the entry's car name, empty when it is not in the race */
static int nat_logger_car(ah_vm *vm,ah_obj *self,void *d,int argc,const ah_val *argv,ah_val *ret)
{
    (void)self;menu *m=d;int i=logger_index(argc,argv);
    const char *name=i>=0 && logger_present(i)?car_name(m,logger_source.car[i]):NULL;
    *ret=ah_str(vm,name?name:"");return 0;
}
/* 00120330: its power as the option shows it, "%d%s" (D_0068DD48), or
   "----" (D_0068DD40) with none */
static int nat_logger_power(ah_vm *vm,ah_obj *self,void *d,int argc,const ah_val *argv,ah_val *ret)
{
    (void)self;(void)d;int i=logger_index(argc,argv);char text[32]="";
    if(i>=0 && logger_present(i)){
        static const char *const names[]={"PS","HP","BHP","CH","kW","CV","PF"};
        int unit=power_unit(ah_user(vm));float ps=(float)logger_source.power[i],v=ps;
        v=power_display(ps,unit);
        if(ps>0)snprintf(text,sizeof text,"%d%s",(int32_t)(v+0.5f),names[unit]);
        else snprintf(text,sizeof text,"----");
    }
    *ret=ah_str(vm,text);return 0;
}
/* 0011EDB8 / 0011EE40 / 0011EEC8 / 0011F180: the widgets it writes */
static int nat_logger_widget(ah_vm *vm,ah_obj *self,void *d,int argc,const ah_val *argv,ah_val *ret)
{
    (void)vm;(void)self;const char *op=d;*ret=ah_nil();
    if(!strcmp(op,"setCourseMap")){logger.map_w=logger_widget_arg(argc,argv,0);return 0;}
    int i=logger_index(argc,argv);if(i<0)return 0;
    ui_widget *w=logger_widget_arg(argc,argv,1);
    if(!strcmp(op,"setTotalTime"))logger.time_w[i]=w;
    else if(!strcmp(op,"setSpeedAverage"))logger.average_w[i]=w;
    else logger.speed_w[i]=w;
    return 0;
}
/* The race's owner replays the last race for the analysis (00122FA8 on
   its thread): begin, a step a frame (1 done, 0 more, -1 failed, with the
   progress), end (ok 0: stopped; it gives the new source when ok) */
static struct { void *user; int (*begin)(void *); int (*step)(void *,float *); void (*end)(void *,int); } logger_analyzer;
void menu_set_logger_analyzer(void *user,int (*begin)(void *),int (*step)(void *,float *),void (*end)(void *,int))
{
    logger_analyzer.user=user;logger_analyzer.begin=begin;logger_analyzer.step=step;logger_analyzer.end=end;
}
static void logger_analyzed(void)
{
    if(!logger_source.valid){logger.view.analyzed=0;return;}
    gt4_logger_view_analyze(&logger.view,&logger_source.track[0],&logger_source.track[1],logger_source.present_b,
                            logger_source.time[0],logger_source.time[1]);
}
/* analyze_start (0011EF50 -> the thread 00123568) / analyze_stop (+0x2A0:
   the thread leaves its loop; +0x2A4 cleared, nothing analysed) */
static int nat_logger_analyze(ah_vm *vm,ah_obj *self,void *d,int argc,const ah_val *argv,ah_val *ret)
{
    (void)self;(void)argc;(void)argv;*ret=ah_nil();menu *m=ah_user(vm);
    if(d){
        if(logger.analyzing && logger.replaying && logger_analyzer.end)logger_analyzer.end(logger_analyzer.user,0);
        if(logger.analyzing)logger.view.analyzed=0;
        logger.analyzing=logger.replaying=0;return 0;
    }
    logger.view.analyzed=0;logger.progress=0;
    logger.replaying=logger_analyzer.begin && logger_analyzer.begin(logger_analyzer.user);
    /* without a race to replay: the samples the race took as it ran (A) */
    if(!logger.replaying)logger_analyzed();
    logger.analyzing=1;
    if(m && m->present_progress)m->present_progress(m,2,NULL,0,0);
    return 0;
}
static int nat_logger_control(ah_vm *vm,ah_obj *self,void *d,int argc,const ah_val *argv,ah_val *ret)
{
    (void)self;(void)argc;(void)argv;menu *m=d;
    ah_obj *o=ah_module_new(vm,"MLoggerControl",NULL,NULL,NULL);
    memset(&logger.view,0,sizeof logger.view);logger.analyzing=0;
    memset(logger.time_w,0,sizeof logger.time_w);memset(logger.average_w,0,sizeof logger.average_w);
    memset(logger.speed_w,0,sizeof logger.speed_w);logger.map_w=NULL;
    /* 0011EA90 +0x88..: D_006244D8, the option's speed unit (0 km/h) */
    gt4_logger_view_init(&logger.view,!strcmp(menu_option_text(m,"unit_velocity","MPH"),"KMPH"));
    ah_set_member(vm,o,"getCarName",ah_native(vm,"getCarName",nat_logger_car,m));
    ah_set_member(vm,o,"getPower",ah_native(vm,"getPower",nat_logger_power,m));
    static const char *const widgets[]={"setTotalTime","setSpeedAverage","setSpeedCurrent","setCourseMap"};
    for(int i=0;i<4;i++)ah_set_member(vm,o,widgets[i],ah_native(vm,widgets[i],nat_logger_widget,(void *)widgets[i]));
    ah_set_member(vm,o,"analyze_start",ah_native(vm,"analyze_start",nat_logger_analyze,NULL));
    ah_set_member(vm,o,"analyze_stop",ah_native(vm,"analyze_stop",nat_logger_analyze,(void *)1));
    ah_set_member(vm,o,"mode",ah_int(0));
    ah_set_member(vm,o,"load_track",ah_int(-1));
    ah_set_member(vm,o,"course_label",ah_str(vm,logger_source.course));
    ah_set_member(vm,o,"race_mode_label",ah_str(vm,logger_source.mode));
    ah_set_member(vm,o,"race_mode_subnumber",ah_int(logger_source.valid?logger_source.subnumber:-1));
    /* GameData -0x5CA8..-0x5CA4 (00123640..001236A0) */
    static const char *const shown[]={"display_accel","display_brake","display_speed","display_TrackA","display_TrackB"};
    for(int i=0;i<5;i++)ah_set_member(vm,o,shown[i],ah_int(1));
    logger.control=o;
    *ret=ah_objv(o);return 0;
}

static ui_widget *logger_find(ui_widget *w,const char *type)
{
    if(!w)return NULL;
    if(!strcmp(w->type,type))return w;
    for(int i=0;i<w->nkids;i++){ui_widget *f=logger_find(w->kids[i],type);if(f)return f;}
    return NULL;
}
static ui_widget *logger_face(menu *m)
{
    for(int r=0;r<ui_root_count(m->ui);r++){ui_widget *w=logger_find(ui_root_widget(m->ui,r),"LoggerFace");if(w)return w;}
    return NULL;
}
static int logger_member(menu *m,ah_obj *o,const char *name)
{
    ah_val v;return o && ah_get_member(m->vm,o,name,&v) && ah_truthy(v);
}
static void logger_text(ui_widget *w,const char *text)
{
    if(w && (!w->text || strcmp(w->text,text)))pc_label(w,text);
}

/* 001BC138 / 001206A0: the frame's work. */
static void logger_tick(menu *m)
{
    if(!m || !logger.control || strcmp(ah_module_name(m->project),"LoggerProject"))return;
    ui_widget *face=logger_face(m);ah_obj *fo=face?face->script:NULL;
    gt4_logger_view *v=&logger.view;
    v->show_accel=logger_member(m,logger.control,"display_accel");
    v->show_brake=logger_member(m,logger.control,"display_brake");
    v->show_speed=logger_member(m,logger.control,"display_speed");
    v->show_track[0]=logger_member(m,logger.control,"display_TrackA");
    v->show_track[1]=logger_member(m,logger.control,"display_TrackB");
    if(logger.analyzing){
        /* 001BC138: the dialog's bar follows 00122CC0 while the thread runs */
        int done=1;
        if(logger.replaying){
            float p=logger.progress;
            done=logger_analyzer.step(logger_analyzer.user,&p);
            logger.progress=p;
            if(done){
                logger_analyzer.end(logger_analyzer.user,done>0);   /* the new source when done */
                logger.replaying=0;
                if(done>0)logger_analyzed();else logger.view.analyzed=0;
                done=1;
            }
        }else logger.progress=1;
        ah_val progress;
        if(fo && ah_get_member(m->vm,fo,"progress",&progress) && progress.t==AV_OBJ)
            ah_set_member(m->vm,ah_as_obj(progress,AO_MODULE),"value",ah_float(logger.progress));
        if(m->present_progress)m->present_progress(m,2,NULL,0,logger.progress);
        if(done){
            logger.analyzing=0;
            /* 00123324: onLoggerAnalyzeFinish when the thread ends */
            if(fo){ah_val ctx=ah_objv(m->context),result;ah_call_member(m->vm,fo,"onLoggerAnalyzeFinish",1,&ctx,&result);apply_pages(m);}
        }
    } else if(fo && v->analyzed && logger_member(m,fo,"grab") && m->depth<=1)
        gt4_logger_view_input(v,logger.sticks[0],logger.sticks[1],logger.sticks[2],logger.sticks[3]);
    /* 001206A0 +0x368..0x570: each present entry's total time (0042E478)
       and its speed at the cursor ("%3d%s", D_0068DD50) */
    for(int i=0;i<2;i++){
        if(!logger_present(i))continue;
        char text[64];uint32_t t=v->analyzed?v->total_time[i]:0;
        if(t/3600000u)snprintf(text,sizeof text,"%u:%02u'%02u.%03u",t/3600000u,t/60000u%60,t/1000u%60,t%1000);
        else snprintf(text,sizeof text,"%u'%02u.%03u",t/60000u%60,t/1000u%60,t%1000);
        logger_text(logger.time_w[i],text);
        float kmh=v->analyzed?gt4_logger_view_speed(v,i):0;
        int speed=(int)(v->metric?kmh:kmh*0.6213712f);
        snprintf(text,sizeof text,"%3d%s",speed,v->metric?"km/h":"mph");
        logger_text(logger.speed_w[i],text);
    }
}

/* 001BC2C0: with the graph grabbed, Circle / Square (Escape, Insert) ask
   for the Logger's menu, Cross / Triangle (Return, KP_Enter) its options */
static int logger_key(menu *m,ui_widget *w,int keysym,int release)
{
    if(release || !w || strcmp(w->type,"LoggerFace") || !w->script || !logger_member(m,w->script,"grab"))return 0;
    const char *method=keysym==0xFF1B || keysym==0xFF63?"onLoggerMenu":keysym==0xFF0D || keysym==0xFF8D?"onLoggerPopup":NULL;
    if(!method)return keysym>=0xFF51 && keysym<=0xFF54;   /* the arrows stay with the graph */
    ah_val ctx=ah_objv(m->context),result;
    ah_call_member(m->vm,w->script,method,1,&ctx,&result);
    return 1;
}

/* ---- the course map (mRaceCourseMapFacePS2, 00128B68) ---- */

/* the course's GTCM (header +0x84): the picture (+0x28), its size, the
   course point at its middle, scale and turn (as the race's mini map),
   and the road's edges (+0x2C pairs at +0x30: left x, z, right x, z),
   evenly spaced over the lap */
static struct {
    char course[32];
    plat_texture *tex;
    int tw, th, w, h;
    float cx, cy, scale, angle;
    float (*edges)[4];
    int nedges;
    plat_texture *icon;int iw,ih;   /* +0xCC: the race display's point_target (0011FF98) */
} logger_map;

static uint32_t logger_le32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static float logger_lef(const uint8_t *p){uint32_t u=logger_le32(p);float f;memcpy(&f,&u,4);return f;}
static void logger_map_load(menu *m)
{
    if(!strcmp(logger_map.course,logger_source.course))return;
    if(logger_map.tex)plat_texture_destroy(logger_map.tex);
    if(logger_map.icon)plat_texture_destroy(logger_map.icon);
    free(logger_map.edges);
    memset(&logger_map,0,sizeof logger_map);
    {   /* 0011FF98 -> 003A1E10: the display's image/point_target */
        size_t n=0,in=0;uint8_t *pkg=gt4_vol_read(m->vol,"/race_display/gt4/US/display.gpb",&n);
        uint8_t *png=pkg?gpb_load(pkg,n,"image/point_target.png",&in):NULL;tex1_image *im=NULL;
        if(png && tex1_decode(png,in,&im)>0){logger_map.icon=plat_texture_create(im[0].width,im[0].height,im[0].rgba);logger_map.iw=im[0].width;logger_map.ih=im[0].height;tex1_free(im,1);}
        free(png);free(pkg);
    }
    snprintf(logger_map.course,sizeof logger_map.course,"%s",logger_source.course);
    if(!logger_source.course[0])return;
    char path[64];snprintf(path,sizeof path,"/crs/%s",logger_source.course);
    size_t size=0;uint8_t *d=gt4_vol_read(m->vol,path,&size);
    uint32_t at=d && size>=0x100?logger_le32(d+0x84):0;
    if(at && (size_t)at+0x80<size && !memcmp(d+at,"GTCM",4)){
        const uint8_t *g=d+at;size_t left=size-at;
        logger_map.w=(int)logger_le32(g+0x10);logger_map.h=(int)logger_le32(g+0x14);
        logger_map.cx=logger_lef(g+0x18);logger_map.cy=logger_lef(g+0x1c);
        logger_map.scale=logger_lef(g+0x20);logger_map.angle=logger_lef(g+0x24);
        uint32_t tex=logger_le32(g+0x28),n=logger_le32(g+0x2c),edges=logger_le32(g+0x30);
        tex1_image *im=NULL;
        if(tex<left && tex1_decode(g+tex,left-tex,&im)>0){
            logger_map.tex=plat_texture_create(im[0].width,im[0].height,im[0].rgba);
            logger_map.tw=im[0].width;logger_map.th=im[0].height;tex1_free(im,1);
        }
        if(n>1 && n<0x10000 && edges<left && (size_t)n*16<=left-edges && (logger_map.edges=malloc((size_t)n*sizeof *logger_map.edges))){
            for(uint32_t i=0;i<n;i++)for(int k=0;k<4;k++)logger_map.edges[i][k]=logger_lef(g+edges+i*16+k*4);
            logger_map.nedges=(int)n;
        }
    }
    free(d);
}
/* 003BEBA0: a course point on the map, about its middle */
static void logger_map_point(float x,float z,float *mx,float *my)
{
    float dx=x-logger_map.cx,dz=z-logger_map.cy,a=logger_map.angle*3.14159265f/180;
    float s=sinf(a)*logger_map.scale,c=cosf(a)*logger_map.scale;
    *mx=dx*c-dz*s;*my=dx*s+dz*c;
}
/* the edges at distance d of the lap (length L), interpolated */
static void logger_map_edges(float d,float L,float out[4])
{
    int n=logger_map.nedges-1;float f=d*(float)n/L;int i=(int)f;
    if(i<0)i=0;if(i>=n)i=n-1;f-=(float)i;
    for(int k=0;k<4;k++)out[k]=logger_map.edges[i][k]+(logger_map.edges[i+1][k]-logger_map.edges[i][k])*f;
}
/* 001292E0: the road between a and b (0 <= a <= b <= L) filled */
static void logger_map_span(float ox,float oy,float a,float b,float L,const uint8_t col[4])
{
    int n=logger_map.nedges-1;
    float prev[4];logger_map_edges(a,L,prev);
    for(int i=(int)(a*(float)n/L)+1;;i++){
        float d=(float)i*L/(float)n,next[4];
        if(d>=b || i>n)logger_map_edges(b,L,next);else memcpy(next,logger_map.edges[i],sizeof next);
        float q[4][2];
        logger_map_point(prev[0],prev[1],&q[0][0],&q[0][1]);logger_map_point(prev[2],prev[3],&q[1][0],&q[1][1]);
        logger_map_point(next[0],next[1],&q[2][0],&q[2][1]);logger_map_point(next[2],next[3],&q[3][0],&q[3][1]);
        for(int k=0;k<4;k++){q[k][0]+=ox;q[k][1]+=oy;}
        plat_draw_solid_quad(q,col);
        if(d>=b || i>n)break;
        memcpy(prev,next,sizeof prev);
    }
}
static int logger_draw_map(menu *m,const ui_widget *w,float x,float y,float opacity)
{
    logger_map_load(m);
    if(!logger_map.tex)return 0;
    double course[4]={255,255,255,203},sector[4]={220,176,47,178};
    mp_numbers(w->src,"course_color",course,4);mp_numbers(w->src,"sector_color",sector,4);
    uint8_t cc[4],sc[4];
    for(int k=0;k<4;k++){cc[k]=(uint8_t)course[k];sc[k]=(uint8_t)sector[k];}
    cc[3]=(uint8_t)(cc[3]*opacity);sc[3]=(uint8_t)(sc[3]*opacity);
    /* the picture at its own size from the widget's place, the course
       point at its middle */
    float ox=x+logger_map.w*0.5f,oy=y+logger_map.h*0.5f;
    plat_draw_quad(logger_map.tex,ox-logger_map.tw*0.5f,oy-logger_map.th*0.5f,(float)logger_map.tw,(float)logger_map.th,0,0,1,1,cc);
    const gt4_logger_view *v=&logger.view;
    float L=logger_source.track[0].course_length;
    if(!v->analyzed || !(L>0) || logger_map.nedges<2)return 1;
    /* 00128EC4..00128F74: the span within one lap, the whole lap when it
       is longer, in two pieces across the line */
    if(w->script && logger_member(m,w->script,"display_span")){
        float a,b;gt4_logger_view_span(v,&a,&b,NULL);
        if(b-a>=L){a=0;b=L;}
        else{while(a<0){a+=L;b+=L;}while(a>=L){a-=L;b-=L;}}
        if(b<=L)logger_map_span(ox,oy,a,b,L,sc);
        else{logger_map_span(ox,oy,a,L,L,sc);logger_map_span(ox,oy,0,b-L,L,sc);}
    }
    /* 00129010: the cursor's point, the display's point_target centred on it */
    if(w->script && logger_member(m,w->script,"display_main_point")){
        float p;gt4_logger_view_span(v,NULL,NULL,&p);
        p=fmodf(p,L);if(p<0)p+=L;
        float e[4],mx,my;logger_map_edges(p,L,e);
        logger_map_point((e[0]+e[2])*0.5f,(e[1]+e[3])*0.5f,&mx,&my);
        const uint8_t white[4]={255,255,255,(uint8_t)(255*opacity)};
        if(logger_map.icon)plat_draw_quad(logger_map.icon,ox+mx-logger_map.iw*0.5f,oy+my-logger_map.ih*0.5f,(float)logger_map.iw,(float)logger_map.ih,0,0,1,1,white);
    }
    return 1;
}

/* ---- drawing (00120CF0) through the platform's 2D calls ---- */
typedef struct { menu *m; float x, y, w, h; } logger_canvas;
static void logger_line(void *user,float x0,float y0,float x1,float y1,float width,const uint8_t rgba[4])
{
    (void)user;float dx=x1-x0,dy=y1-y0,len=sqrtf(dx*dx+dy*dy);
    if(!(len>0))return;
    float nx=-dy/len*width*.5f,ny=dx/len*width*.5f;
    float q[4][2]={{x0+nx,y0+ny},{x1+nx,y1+ny},{x0-nx,y0-ny},{x1-nx,y1-ny}};
    plat_draw_solid_quad(q,rgba);
}
static void logger_quad(void *user,const float xy[4][2],const uint8_t rgba[4][4])
{
    (void)user;plat_draw_solid_quad(xy,rgba[0]);
}
static void logger_string(void *user,const char *face,float x,float y,float scale,float right,const uint8_t rgba[4],const char *s)
{
    logger_canvas *c=user;ui_draw_string(c->m->ui,face,x,y,scale,right,rgba,s);
}
static float logger_string_height(void *user,const char *face,float scale)
{
    logger_canvas *c=user;return ui_string_height(c->m->ui,face,scale);
}
static int logger_draw(void *user,const ui_widget *w,float x,float y,float wd,float h,float opacity)
{
    menu *m=user;
    if(!m || !w || opacity<=0)return 0;
    if(!strcmp(w->type,"RaceCourseMapFace"))return logger_draw_map(m,w,x,y,opacity);
    if(!logger.control || !logger.view.analyzed)return 1;
    logger_canvas c={m,x,y,wd,h};
    gt4_logger_draw_ops ops={&c,logger_line,logger_quad,logger_string,logger_string_height};
    plat_set_clip(x,y,wd,h);
    gt4_logger_view_draw(&logger.view,x,y,wd,h,opacity,&ops);
    /* 00121188..001212D8: the graph is drawn in its own target, pasted
       opaque, then again one line lower at FIX 1/3 (004A2808 0x64) and
       one line higher at FIX 1/4: the target is the picture just drawn */
    static plat_texture *target;
    target=ui_capture_region(m->ui,target,x,y,wd,h);
    if(target){
        const uint8_t white[4]={255,255,255,255};
        plat_draw_quad_blend(target,x,y+1,wd,h,0,1,1,0,white,(float)(int)(128.f/3.f)/128.f);
        plat_draw_quad_blend(target,x,y-1,wd,h,0,1,1,0,white,(float)(int)(128.f*0.25f)/128.f);
    }
    plat_set_clip(0,0,0,0);
    return 1;
}

/* main.game.setReturnToLastMenuProject (logger.ad exit / load_mode): the
   project that opened this one is the next */
static int logger_load_request=-1;
int menu_take_logger_load(void){int t=logger_load_request;logger_load_request=-1;return t;}
static int nat_return_last(ah_vm *vm,ah_obj *self,void *d,int argc,const ah_val *argv,ah_val *ret)
{
    (void)self;(void)argc;(void)argv;menu *m=d;ah_val last,track;
    /* 0010FEE8 +0x174: D_00618714 (the control's load_track) >= 0 loads a
       best lap for that track and comes back to the Logger */
    if(logger.control && !strcmp(ah_module_name(m->project),"LoggerProject") &&
       ah_get_member(vm,logger.control,"load_track",&track) && ah_is_num(track))
        logger_load_request=(int)ah_num(track)==0 || (int)ah_num(track)==1?(int)ah_num(track):-1;
    if(ah_get_member(vm,game_object(m),"last_menu_project",&last) && ah_cstr(last) && *ah_cstr(last))
        ah_set_member(vm,game_object(m),"next_menu_project",last);
    *ret=ah_nil();return 0;
}
