#include "ui_motion.h"
#include <math.h>
#include <string.h>

/* 0024A250: conversion uses the original update clock. */
static int ticks(const gt4_tooltip_motion *m, float seconds)
{
    if(m->half_rate) seconds=seconds*0.5f;
    return (int)(seconds*60.0f);
}
/* 0024A2A8, 0057B088: keep the float operation order. */
static float step(const gt4_tooltip_motion *m, float value)
{ return m->half_rate?(value+value)/60.0f:value/60.0f; }
static float chase(float value,float target,float ratio)
{ float difference=target-value;difference=difference*ratio;return value+difference; }

void gt4_tooltip_start(gt4_tooltip_motion *m,float width,float height,
                       float text_width,float font_size,int half_rate)
{
    memset(m,0,sizeof *m);
    m->width=width;m->height=height;m->text_width=text_width;
    m->font_size=font_size>0?font_size:8.0f;m->speed=10.0f;
    m->half_rate=half_rate;m->fits=text_width<=width*0.949999988079071f;
    m->target_x=m->fits?(width-text_width)*0.5f:width*0.04999999701976776f;
    m->target_y=height;m->x=m->target_x;m->y=height;
    /* Default animated mode (00248DD0). State 0 computes this same setup
       through font/layout calls; their native PC equivalents run in draw. */
    m->wait=ticks(m,0.0f);m->state=1;m->ready=1;
}

void gt4_tooltip_step(gt4_tooltip_motion *m)
{
    if(!m || !m->ready)return;
    switch(m->state) {
    case 1:
        if(--m->wait<0)m->state=2;
        break;
    case 2:
        if(m->target_y>0)m->target_y=m->target_y-step(m,10000.0f);
        if(m->target_y<=0) {
            m->target_y=0;
            if(m->fits)m->state=100;
            else {
                float pause=m->width*0.3999999761581421f;
                pause=pause/m->font_size;pause=pause/m->speed;
                m->wait=ticks(m,pause);m->state=3;
            }
        }
        break;
    case 3:
        if(--m->wait<0)m->state=10;
        break;
    case 4: {
        float movement=step(m,m->font_size*m->speed);
        if(m->half_rate)movement=movement*0.5f;
        m->target_x=m->target_x-movement;
        if(m->target_x<-(m->text_width-m->width)) {
            float pause=m->width*0.75f;
            pause=pause/m->font_size;pause=pause/m->speed;
            m->wait=ticks(m,pause);m->state=5;
        }
        break;
    }
    case 5:
        if(--m->wait<0)m->state=6;
        break;
    case 6:
        if(m->target_y<m->height)m->target_y=m->target_y+step(m,10000.0f);
        if(m->height<=m->target_y) {
            m->target_y=m->height;m->target_x=m->width*0.04999999701976776f;
            m->x=m->target_x;m->y=m->target_y;
            m->wait=ticks(m,1.0f);m->state=1;
        }
        break;
    case 10:
        m->target_x=m->target_x-step(m,m->font_size*m->speed);
        if(m->target_x<-m->text_width) {m->wait=ticks(m,1.0f);m->state=11;}
        break;
    case 11:
        if(--m->wait<0) {
            m->target_y=m->height;m->target_x=m->width*0.04999999701976776f;
            m->x=m->target_x;m->y=m->target_y;
            m->wait=ticks(m,1.0f);m->state=1;
        }
        break;
    default:break;
    }
    m->x=chase(m->x,m->target_x,0.09999999403953552f);
    m->y=chase(m->y,m->target_y,0.29999998211860657f);
}

void gt4_tooltip_advance(gt4_tooltip_motion *m,float seconds)
{
    if(!m || !m->ready || !isfinite(seconds) || seconds<=0)return;
    m->ticks+=(double)seconds*60.0;
    while(m->ticks>=1.0-1e-6) {m->ticks-=1;gt4_tooltip_step(m);}
}

void gt4_cursor_step(float *x,float *y,float *angle,
                     float target_x,float target_y,float target_angle,float speed)
{
    /* func_0022FFD0, 002303AC: test the differences before moving. */
    float dx=fabsf(target_x-*x),dy=fabsf(target_y-*y);
    float ratio=speed*0.14999999105930328f;
    *x=dx<1.0f?target_x:chase(*x,target_x,ratio);
    *y=dy<1.0f?target_y:chase(*y,target_y,ratio);
    if(dx<1.0f && dy<1.0f) {
        float da=fabsf(target_angle-*angle);
        *angle=da<1.0f?target_angle:chase(*angle,target_angle,ratio);
    }
}

void gt4_cursor_update(float *x,float *y,float *angle,float target_x,float target_y,
                       float target_angle,float speed,int32_t paused,
                       int32_t follow,int32_t active)
{
    /* func_0022FFD0: signed 32-bit flags are tested against zero. */
    if(paused!=0 || follow==0 || active==0)return;
    gt4_cursor_step(x,y,angle,target_x,target_y,target_angle,speed);
}

float gt4_move_value(float value,float target,float ratio)
{ return chase(value,target,ratio); }
float gt4_fade_value(float value,float target,float step)
{
    /* func_0020C478: crossing is strict, not the elapsed-time fraction. */
    float next=value+step;
    if(step<0?next<target:target<next)next=target;
    return next;
}

int gt4_color_step(float *value,float target,float step)
{
    /* func_00287348, 002873B0..00287404; equality holds another frame. */
    float next=step<0?*value-(-step):*value+step;
    int complete=step<0?next<target:target<next;
    *value=complete?target:next;
    return complete;
}

uint8_t gt4_gs_alpha(float value)
{
    /* func_004A2808: multiply, clamp, truncate, store byte. */
    float scaled=value*128.0f;
    scaled=fmaxf(scaled,0.0f);scaled=fminf(scaled,255.0f);
    return (uint8_t)(int32_t)scaled;
}

/* func_00579330 / func_00578598: private transition RNG, unsigned wrap. */
static float transition_random(uint32_t *seed)
{
    uint32_t n=*seed*17u+17u;*seed=n;
    n^=(n<<16)|(n>>16);
    if(!n)return 0;
    unsigned leading=0;for(uint32_t bit=0x80000000u;!(n&bit);bit>>=1)leading++;
    /* plzcw counts redundant sign bits, i.e. clz-1 for positive words. */
    int shift=(n&0x80000000u)?-1:(int)leading-1;
    int exponent=125-shift;
    uint32_t bits=((uint32_t)exponent<<23)|((n<<((unsigned)-exponent&31))>>9);
    float value;memcpy(&value,&bits,4);return value;
}
float gt4_cross_scale(float value)
{
    /* func_0028A5B8, 0028A628..0028A64C. */
    float scale=value*value;scale=scale+1.0f;
    if(2.0f<scale)scale=2.0f;if(scale<1.0f)scale=1.0f;return scale;
}
void gt4_block_setup(gt4_transition_motion *m)
{
    /* func_0027AEE0: rows first, then columns, one RNG sample per tile. */
    float width=640.0f/(float)m->columns,height=480.0f/(float)m->rows;
    for(int y=0;y<m->rows;y++)for(int x=0;x<m->columns;x++) {
        gt4_transition_tile *t=&m->tiles[y*m->columns+x];
        float upper=m->range+1.0f,random=transition_random(&m->random);
        float alpha=(upper-1.0f)*random;alpha=alpha+1.0f;
        float slope=m->slope>=0?((float)x/(float)m->columns)*m->slope:
            ((float)(m->columns-x)/(float)m->columns)*(-m->slope);
        t->alpha=alpha+slope;t->x=(float)x*width;t->y=(float)y*height;
        t->source_x=t->x;t->source_y=t->y;t->w=width;t->h=height;
    }
}
int gt4_block_step(gt4_transition_tile *tile,float step)
{
    /* func_0027A1B8: leave the negative tail until the next frame. */
    if(0<tile->alpha){tile->alpha=tile->alpha-step;return 1;}return 0;
}
void gt4_transition_init(gt4_transition_motion *m,int kind)
{
    memset(m,0,sizeof *m);m->kind=kind;m->stage=3;
    /* func_00289888 / func_0027A4D0 / func_0027A658. */
    m->type=1;m->with_black=1;m->black_opacity=0.09999999403953552f;
    m->magnify_ratio=.5f;m->range=.125f;m->slope=2;
    m->columns=32;m->rows=24;m->random=0x12345678u;
    m->step=kind==2?0.13333332538604736f:0.03333333134651184f;
}
void gt4_transition_start(gt4_transition_motion *m,int operation,float period)
{
    m->frame=0;m->active=period>0;m->hold=0;m->mode=operation;m->stage=operation==1?1:0;
    m->value=operation==0?0:1;
    if(period>0)m->step=1.0f/(period*60.0f);
    if(m->kind==2)gt4_block_setup(m);
    /* Cross out/out-in resets opacity; panIn preserves the selected value. */
    if(m->kind==1 && operation!=1)m->black_opacity=0.09999999403953552f;
}
void gt4_transition_release(gt4_transition_motion *m)
{
    /* func_0024D328, called after RootWindow preparation at 00235094. */
    if(m->stage!=1)return;
    if(m->mode==0){m->active=0;m->stage=3;}
    else if(m->mode==1 || m->mode==2)m->stage=2;
}
void gt4_transition_step(gt4_transition_motion *m)
{
    if(!m->active)return;
    m->frame++;
    if(m->stage==0){m->stage=1;return;}
    if(m->stage!=2)return;
    if(m->kind==2) {
        int moving=0;
        for(int i=0;i<m->columns*m->rows;i++)moving|=gt4_block_step(&m->tiles[i],m->step);
        if(!moving){m->stage=3;m->active=0;}
    } else {
        /* func_0028A6B0: a second strictly-negative frame completes it. */
        m->value=m->value-m->step;
        if(m->value<0){m->value=0;if(m->hold)m->active=0;else m->hold=1;}
        if(!m->active)m->stage=3;
    }
}

/* func_0010AE40. Preserve div.s then add.s and the sign-dependent clamp. */
int gt4_loading_fade_step(float *period,float *value,float seconds)
{
    if(*period==0)return 0;
    float increment=seconds / *period;
    *value=*value+increment;
    if(*period>0){
        if(*value>=1){*period=0;*value=1;return 1;}
    }else if(*value<=0){*period=0;*value=0;return 1;}
    return 0;
}


void gt4_car_turn_step(float *angle,float *velocity)
{
    const float dt=0x1.111110p-6f; /* CORE 0x3C888888 */
    float v=*velocity+80.0f*dt;
    v=v-(v*4.0f)*dt;
    *velocity=v;
    *angle=fmodf(*angle+v*dt,360.0f);
    if(*angle<0)*angle+=360.0f;
}

void ui_model_pivot_offset(const float rotation[9],const float rest[9],const float pivot[3],float offset[3])
{
    for(int i=0;i<3;i++){
        float before=0,after=0;
        for(int k=0;k<3;k++){before+=rest[i*3+k]*pivot[k];after+=rotation[i*3+k]*pivot[k];}
        offset[i]=before-after;
    }
}
