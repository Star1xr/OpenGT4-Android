#include "logger_view.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Track colours D_00618770 (A) / D_00618780 (B), accelerator D_00618730,
   brake D_00618750 (r, g, b, a). */
static const float track_rgb[2][4]={{0.809f,0.29f,0.295f,1.f},{0.f,0.f,0.f,0.f}};
static float track_color[2][4];
static void colors_init(void){
    static int done;if(done)return;done=1;
    /* D_00618780: the second row of the label table */
    const float b[4]={0.305f,0.443f,0.803f,1.f};
    memcpy(track_color[0],track_rgb[0],sizeof track_color[0]);memcpy(track_color[1],b,sizeof b);
}
static const float accel_rgb[3]={0.25f,1.f,0.15f},brake_rgb[3]={1.f,0.25f,0.15f};

void gt4_logger_view_init(gt4_logger_view *v,int metric)
{
    memset(v,0,sizeof *v);
    v->metric=metric;                         /* 0011EB18..: D_006244D8 */
    v->grid=metric?25.f:25.146f;
    v->speed_scale=metric?1.f:1.609344f;
    v->height=1.f;v->speed_top=800.f;v->speed_top_i=800;
    v->zoom=v->vzoom=v->width=1.f;
    v->show_track[0]=v->show_track[1]=v->show_speed=v->show_accel=v->show_brake=1;
}

static float length_of(const gt4_logger_view *v,int i){return (float)((int32_t)v->count[i]-1)*v->spacing;}

void gt4_logger_view_analyze(gt4_logger_view *v,const gt4_logger_sampler *a,const gt4_logger_sampler *b,
                             int present_b,uint32_t time_a,uint32_t time_b)
{
    if(!a){v->analyzed=0;return;}
    if(!b)b=a;
    /* 00123204..0012323C: both counts become the shorter one */
    uint32_t n=a->count<b->count?a->count:b->count;
    v->points[0]=a->points;v->points[1]=b->points;v->count[0]=v->count[1]=n;
    v->spacing=a->spacing;v->present[0]=1;v->present[1]=present_b && b!=a;
    v->total_time[0]=time_a;v->total_time[1]=time_b;
    v->start=fminf(a->origin,b->origin);                  /* 0x2F8 */
    float length=fminf(length_of(v,0),length_of(v,1));
    float top_speed=fmaxf(a->maximum_speed,b->maximum_speed);
    float speed=fminf(800.f,top_speed*3.6f+100.f);
    v->speed_top_i=(int)speed;v->speed_top=(float)v->speed_top_i;   /* 0x30C, 0x308 */
    v->end=length+v->start;                                         /* 0x2FC */
    v->vzoom=fmaxf(1.f,(float)(int)(top_speed*9.f/250.f));          /* 0x2E8 */
    /* 00122F1C..: the whole length in the window. A: the original runs
       this on its thread after the widget has drawn (+0x2EC its width);
       here the first draw after it does the fit */
    v->length=v->end-v->start;
    v->zoom=fminf(fmaxf(v->width/v->length,0.4f),10.f);
    v->center=v->length*0.5f;
    v->fit=1;
    v->analyzed=1;
}

static float total_height(const gt4_logger_view *v)
{
    /* 001233E0 */
    int shown=(v->present[0] && v->show_track[0])+(v->present[1] && v->show_track[1]);
    float bands=0;
    if(shown && (v->show_accel || v->show_brake))bands=(float)shown*50.f+4.f;
    return bands+10.f+(v->speed_top/v->vzoom+75.f);
}

static float stick(float a){float p=powf(fabsf(a),1.5f);return a<0?-p:p;}

void gt4_logger_view_input(gt4_logger_view *v,float lx,float ly,float rx,float ry)
{
    float sx=stick(lx),sy=stick(ly);
    /* 001207C0..0012083C: zoom by the right stick, log10 paced */
    float z=v->zoom+rx*(1.f/32.f)*(log10f(v->zoom)+1.2f);
    if(z<0.4f)z=0.4f;else if(10.f<z)z=10.f;
    v->zoom=z;
    float vz=v->vzoom+ry*(1.f/32.f)*(log10f(v->vzoom)+1.2f);
    if(vz<0.5f)vz=0.5f;else if(4.f<vz)vz=4.f;
    v->vzoom=vz;
    /* 001208BC..001209B0: the cursor stops at either end, then wraps
       on a new push (+0x2C4/+0x2C8: 1 stopped, 2 released) */
    float c=v->center+(sx+sx)/z;
    if(c<0.f){
        if(v->wrap_left>=0){
            if(v->wrap_left<2){c=0.f;v->wrap_left=1;}
            else if(v->wrap_left==2){v->wrap_left=0;c=v->length;}
        }
    }else if(v->length<c){
        if(v->wrap_right>=0){
            if(v->wrap_right<2){c=v->length;v->wrap_right=1;}
            else if(v->wrap_right==2){v->wrap_left=0;c=0.f;}   /* 00120950 clears +0x2C4 */
        }
    }else if(sx==0.f){
        if(v->wrap_left==1)v->wrap_left=2;
        if(v->wrap_right==1)v->wrap_right=2;
    }else v->wrap_left=v->wrap_right=0;
    v->center=c;
    float top=fmaxf(total_height(v)-v->height,0.f);
    v->scroll=fminf(fmaxf(v->scroll-(sy+sy),0.f),top);
}

void gt4_logger_view_span(const gt4_logger_view *v,float *start,float *end,float *point)
{
    /* 00120518/00120528 and the map's point at +0x2CC + +0x2F8 */
    if(point)*point=v->center+v->start;
    if(start)*start=fmaxf(v->left,0.f)+v->start;
    if(end)*end=fminf(v->right,v->end-v->start)+v->start;
}

static uint32_t index_at(const gt4_logger_view *v,int i,float d)
{
    float length=length_of(v,i);
    return (uint32_t)(fminf(fmaxf(d,0.f),length)/v->spacing);
}
float gt4_logger_view_speed(const gt4_logger_view *v,int i)
{
    /* 0011DBE8 * 3.6 */
    if(!v->analyzed || !v->count[i])return 0.f;
    return (float)(v->points[i][index_at(v,i,v->center)].speed_gear>>3)/40.f*3.6f;
}

/* ---- drawing: world (distance, height up from the bottom) to screen */
typedef struct { gt4_logger_view *v; const gt4_logger_draw_ops *ops; float x, y, alpha; } canvas;
static float sx_(const canvas *c,float d){return c->x+(d-c->v->left)*c->v->zoom;}
static float sy_(const canvas *c,float h){return c->y+(c->v->bottom-h);}
static void rgba(const float rgb[3],float a,float alpha,uint8_t out[4])
{
    for(int i=0;i<3;i++){float k=rgb[i]*255.f;out[i]=(uint8_t)(k<0?0:k>255?255:k);}
    float k=a*alpha*255.f;out[3]=(uint8_t)(k<0?0:k>255?255:k);
}
static void gray(float g,float alpha,uint8_t out[4]){const float c[3]={g,g,g};rgba(c,1.f,alpha,out);}
static void wline(const canvas *c,float d0,float h0,float d1,float h1,float width,const uint8_t col[4])
{c->ops->line(c->ops->user,sx_(c,d0),sy_(c,h0),sx_(c,d1),sy_(c,h1),width,col);}
static void text(const canvas *c,const char *face,float fx,float fy,float scale,float right,const float rgb[3],float a,const char *s)
{uint8_t col[4];rgba(rgb,a,c->alpha,col);c->ops->text(c->ops->user,face,c->x+fx,c->y+fy,scale,right,col,s);}
static float height_of(const canvas *c,const char *face,float scale){return c->ops->text_height?c->ops->text_height(c->ops->user,face,scale):20.f*scale;}

/* 00121C00: the grid of a band: minor distance lines, and the band's
   levels every 25 of `count` (white at 0, mid every 100) */
static void grid(canvas *c,float y0,float h,int count)
{
    gt4_logger_view *v=c->v;uint8_t col[4];
    int i0=(int)(v->left/v->grid)-1,i1=(int)(v->right/v->grid)+1;
    gray(0.25f,c->alpha,col);
    for(int i=i0;i<=i1;i++)if(i&3){float d=(float)i*v->grid;wline(c,d,y0,d,y0+h,1.f,col);}
    for(int j=0;j<=count;j+=25){
        float yy=y0+h*(float)j/(float)count*v->speed_scale;
        gray(j%100?0.25f:j?0.5f:1.f,c->alpha,col);
        wline(c,v->left,yy,v->right,yy,1.f,col);
    }
}
/* 00121340: major distance lines over the whole height, labelled */
static void distance_axis(canvas *c,float y)
{
    gt4_logger_view *v=c->v;uint8_t col[4];const float white[3]={1,1,1};
    float step=v->grid*4.f,hgt=height_of(c,"hc_20",0.5f)-1.f;
    int i0=(int)(v->left/step)-1,i1=(int)(v->right/step)+1;
    for(int i=i0;i<=i1;i++){
        int metres=(int)((float)i*v->grid*4.f);
        gray(0.5f,c->alpha,col);
        wline(c,(float)metres,v->bottom,(float)metres,v->top,1.f,col);
        float fx=((float)metres-v->left)*v->zoom,fy=v->top_edge-(y-hgt);
        char label[32];label[0]=0;
        if(v->metric)snprintf(label,sizeof label,"%dm",metres);
        else if(!(i&15))snprintf(label,sizeof label,"%dml",metres/1600);
        if(*label)text(c,"hc_20",fx,fy,0.5f,0,white,1.f,label);
    }
}
/* 00122A50: the band's big A / B */
static void band_letter(canvas *c,float y,int track)
{
    char s[2]={track?'B':'A',0};
    text(c,"hb_24",64.f,c->v->top_edge-(y+12.5f),2.f,0,track_color[track],0.4f,s);
}
/* 001218C0: a band's filled curve (accelerator or brake of a track) */
static void band_curve(canvas *c,int track,int brake,float base,float height)
{
    gt4_logger_view *v=c->v;
    if(!v->show_track[track] || !v->present[track] || !v->count[track])return;
    float sp=v->spacing;int count=(int)v->count[track];
    int i0=(int)(v->left*(1.f/sp))-1,i1=(int)(v->right*(1.f/sp))+1;
    if(i0<0)i0=0;if(i1>=count)i1=count-1;
    if(v->right<(float)i0*sp || (float)i1*sp<v->left)return;
    int stride=1;while((i1-i0)/stride>1000)stride++;
    const float *rgb=brake?brake_rgb:accel_rgb;
    float px=0,py=0;int have=0;
    for(int i=i0;i<=i1;i+=stride){
        const gt4_logger_point *p=v->points[track]+i;
        float val=(brake?p->brake:p->accelerator)/255.f;
        float xd=(float)i*sp,top=base+val*height;
        if(have){
            /* (Cs - Cd) * 0.5 + Cd: half the colour over what is there */
            float xy[4][2]={{sx_(c,px),sy_(c,py)},{sx_(c,xd),sy_(c,top)},{sx_(c,px),sy_(c,base)},{sx_(c,xd),sy_(c,base)}};
            uint8_t col[4][4];for(int k=0;k<4;k++)rgba(rgb,0.5f,c->alpha,col[k]);
            c->ops->quad(c->ops->user,xy,col);
        }
        px=xd;py=top;have=1;
    }
}
/* 00121688: the band's line and its 0% / 100% */
static void band_labels(canvas *c,float y,int a,int b)
{
    gt4_logger_view *v=c->v;uint8_t col[4];const float white[3]={1,1,1};
    gray(1.f,c->alpha,col);wline(c,v->left,y,v->right,y,1.f,col);
    float f=0;
    if(a)f=b?y-4.f:y+4.f;else if(b)f=y-8.f;
    float label=(a||b)?f+1.f:0.f;
    text(c,"hc_20",16.f,v->top_edge-label,0.4f,0,white,1.f,"0%");
    if(a)text(c,"hc_20",16.f,v->top_edge-(y+50.f-8.f-1.f),0.4f,0,white,1.f,"100%");
    if(b)text(c,"hc_20",16.f,v->top_edge-(y-50.f+4.f+1.f),0.4f,0,white,1.f,"100%");
}
/* 00121590 */
static void band_tracks(canvas *c,float y,int track)
{
    float h=track?-50.f:50.f;
    if(c->v->show_accel)band_curve(c,track,0,y,h);
    if(c->v->show_brake)band_curve(c,track,1,y,h);
}
/* 00122830: the speed axis' labels every 100 */
static void speed_axis(canvas *c,float y)
{
    gt4_logger_view *v=c->v;const float white[3]={1,1,1};
    float hgt=height_of(c,"hc_20",0.7f);
    float x=v->width-16.f-128.f;
    for(int s=0;(float)s<=800.f;s+=100){
        float yy=y+v->speed_top*(float)s/(float)v->speed_top_i/v->vzoom*v->speed_scale;
        char label[32];snprintf(label,sizeof label,v->metric?"%dkm/h":"%dmph",s);
        text(c,"hc_20",x,v->top_edge-yy-hgt,0.7f,128.f,white,1.f,label);
    }
}
/* 001226C8 / 00121DD0: a track's speed line (two pixels, the five
   offset passes) */
static void speed_curve(canvas *c,float y,int track)
{
    gt4_logger_view *v=c->v;
    if(!v->show_speed || !v->show_track[track] || !v->present[track] || !v->count[track])return;
    float sp=v->spacing,scale=v->speed_top/(float)v->speed_top_i/v->vzoom;int count=(int)v->count[track];
    int i0=(int)(v->left*(1.f/sp))-1,i1=(int)(v->right*(1.f/sp))+1;
    if(i0<0)i0=0;if(i1>=count)i1=count-1;
    if(v->right<(float)i0*sp || (float)i1*sp<v->left)return;
    int stride=1;while((i1-i0)/stride>1000)stride++;
    uint8_t col[4];rgba(track_color[track],1.f,c->alpha,col);
    float px=0,py=0;int have=0;
    for(int i=i0;i<=i1;i+=stride){
        float kmh=(float)(v->points[track][i].speed_gear>>3)/40.f*3.6f;
        float xd=(float)i*sp,yy=y+kmh*scale;
        if(have)wline(c,px,py,xd,yy,2.f,col);
        px=xd;py=yy;have=1;
    }
}
/* 001205D8: the label's leader, (x, y) moved to its end */
static void leader(canvas *c,float *x,float *y,float dx,float dy,const uint8_t col[4])
{
    float x1=*x+dx*15.f,y1=*y+dy*30.f;
    wline(c,*x,*y,x1,y1,1.f,col);
    float x2=x1+dx*30.f;wline(c,x1,y1,x2,y1,1.f,col);
    *x=x2;*y=y1;
}
static void speed_label(char *s,size_t n,const gt4_logger_view *v,float kmh)
{
    int t=v->metric?(int)(kmh*10.f):(int)(kmh*0.6213712f*10.f);
    snprintf(s,n,"%s%d.%d%s",t<0?"-":"",abs(t)/10,abs(t)%10,v->metric?"km/h":"mph");
}
/* 00121FF0: the cursor, its distance and each track's speed */
static void cursor(canvas *c,float y,int a,int b)
{
    gt4_logger_view *v=c->v;
    float center=(v->left+v->right)*0.5f,inv=1.f/v->zoom,base=y<v->top?v->top:y;
    const float orange[3]={0.9f,0.5f,0.1f},light[3]={0.9f,0.9f,0.9f};uint8_t col[4];
    rgba(orange,1.f,c->alpha,col);
    float end=v->end-v->start;wline(c,end,v->bottom,end,v->top,1.f,col);
    char s[48];int t;
    if(v->metric){t=(int)(center*10.f);snprintf(s,sizeof s,"%d.%dm",t/10,abs(t)%10);}
    else {t=(int)(center*0.000621371f*1000.f);snprintf(s,sizeof s,"%d.%dml",t/10,abs(t)%10);}
    text(c,"hc_20",v->width*0.5f+15.f,v->top_edge-(base+4.f+2.f),0.7f,0,light,1.f,s);
    float scale=v->speed_top/(float)v->speed_top_i/v->vzoom;
    float sa=v->analyzed?gt4_logger_view_speed(v,0):0.f,sb=v->analyzed?gt4_logger_view_speed(v,1):0.f;
    float dxa=inv,dya=-1.f,dyb=-1.f;
    if(a){
        if(b){if(sb<sa)dya=1.f;else dyb=1.f;dxa=-inv;}
        float px=center,py=sa*scale+y;
        if(py-30.f<v->top)dya=1.f;
        rgba(track_color[0],1.f,c->alpha,col);leader(c,&px,&py,dxa,dya,col);
        speed_label(s,sizeof s,v,sa);
        float fy=v->top_edge-py,hgt=height_of(c,"hc_20",0.8f);
        if(dxa<0)text(c,"hc_20",v->width*0.5f-128.f-45.f,fy-hgt,0.8f,128.f,track_color[0],1.f,s);
        else text(c,"hc_20",v->width*0.5f+45.f,fy,0.8f,0,track_color[0],1.f,s);
    }
    if(b){
        float px=center,py=sb*scale+y;
        if(py-30.f<v->top)dyb=1.f;
        rgba(track_color[1],1.f,c->alpha,col);leader(c,&px,&py,inv,dyb,col);
        speed_label(s,sizeof s,v,sb);
        text(c,"hc_20",v->width*0.5f+45.f,v->top_edge-py,0.8f,0,track_color[1],1.f,s);
    }
    const float red[3]={1,0,0};uint8_t r[4][4];for(int k=0;k<4;k++)rgba(red,1.f,c->alpha,r[k]);
    float bar[4][2]={{sx_(c,center-inv),sy_(c,v->bottom)},{sx_(c,center+inv),sy_(c,v->bottom)},
                     {sx_(c,center-inv),sy_(c,v->top)},{sx_(c,center+inv),sy_(c,v->top)}};
    c->ops->quad(c->ops->user,bar,r);
    /* 00120540: the marker under it */
    float hw=8.f*inv,y0=base+11.f,y1=base+1.f;
    float tri[4][2]={{sx_(c,center-hw),sy_(c,y0)},{sx_(c,center+hw),sy_(c,y0)},{sx_(c,center),sy_(c,y1)},{sx_(c,center),sy_(c,y1)}};
    c->ops->quad(c->ops->user,tri,r);
}

void gt4_logger_view_draw(gt4_logger_view *v,float x,float y,float w,float h,float alpha,const gt4_logger_draw_ops *ops)
{
    colors_init();
    /* 00120D60..00120DE0: the window */
    int wi=(int)(w+0.5f),hi=(int)(h+0.5f);
    float fw=(float)wi,fh=(float)hi;
    v->width=fw;v->height=fh;v->top=v->scroll;
    v->top_edge=v->bottom=v->scroll+fh;
    v->left=v->center-fw*0.5f/v->zoom;
    if(v->fit){v->fit=0;v->zoom=fminf(fmaxf(fw/v->length,0.4f),10.f);}
    v->left=v->center-fw*0.5f/v->zoom;
    v->right=v->left+fw/v->zoom;
    canvas c={v,ops,x,y,alpha};
    int a=v->present[0] && v->show_track[0],b=v->present[1] && v->show_track[1];
    int bands=(a+b) && (v->show_accel || v->show_brake);
    float line=0,next=0;
    if(bands){
        next=4.f;
        if(b){band_letter(&c,next,1);grid(&c,next,50.f,100);next=54.f;}
        line=next;
        if(a){band_letter(&c,line,0);grid(&c,line,50.f,100);next=line+50.f;}
    }
    float speed=next+10.f;
    grid(&c,speed,875.f/v->vzoom,875);
    distance_axis(&c,speed);
    if(bands){
        band_labels(&c,line,a,b);
        if(a)band_tracks(&c,line,0);
        if(b)band_tracks(&c,line,1);
    }
    speed_axis(&c,speed);
    if(b)speed_curve(&c,speed,1);
    if(a)speed_curve(&c,speed,0);
    cursor(&c,speed,a,b);
}
