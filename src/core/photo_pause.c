/* PhotoPause 003C4640 +30..40 uses this exact movie. Its help and labels
   are authored assets, never rebuilt from Photo Travel widgets. */
#include "photo_pause.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
struct photo_pause {strb_player *movie;gt4_rtext text;uint8_t *text_data;gt4_photo_mode *state;float apertures[17];int32_t shutters[19];int blink,wide;};
static int named(const char *a,const char *b){while(*a && *b){if(toupper((unsigned char)*a++)!=toupper((unsigned char)*b++))return 0;}return !*a && !*b;}
int photo_pause_value(photo_pause *p,const char *name,strb_value *ret){
 memset(ret,0,sizeof *ret);if(!p || !p->state)return 0;gt4_photo_mode *s=p->state;ret->t=STRB_STR;
 /* 003C40C0 +60..+2D8 formats the four camera readouts. Float
    formatting remains A until compared to EE printf rounding. */
 if(named(name,"getZoom"))snprintf(ret->s,sizeof ret->s,"f = %dmm",(int)(s->view_angle+.5f));
 else if(named(name,"getIris")){int n=(int)s->aperture;n=n<0?0:n>16?16:n;snprintf(ret->s,sizeof ret->s,"F %.1f",floorf((p->apertures[n]+.05f)*10)/10);}
 else if(named(name,"getShutter")){int n=s->shutter<0?0:s->shutter>18?18:s->shutter;float t=gt4_photo_shutter_seconds(p->shutters[n]);snprintf(ret->s,sizeof ret->s,"1/%d''",t>0?(int)(1/t+.5f):0);}
 else if(named(name,"getExposure")){float ev=s->brightness*.125f;snprintf(ret->s,sizeof ret->s,"%c%.1f EV",ev<0?'-':'+',floorf((fabsf(ev)+.05f)*10)/10);}
 else if(named(name,"getCurCam"))snprintf(ret->s,sizeof ret->s,"%d",s->drive_camera+1);
 else if(named(name,"getNCam"))snprintf(ret->s,sizeof ret->s,"%d",s->drive_cameras);
 else {ret->t=STRB_NUM;
  if(named(name,"getBlink"))ret->n=p->blink;
  else if(named(name,"getFilter"))ret->n=s->white_balance<0?0:s->white_balance>0?2:1;
  else if(named(name,"getMF"))ret->n=s->manual_focus;
  else if(named(name,"isPortrait"))ret->n=s->roll==90 || s->roll==-90;
  else if(named(name,"isWide"))ret->n=p->wide;
  else if(named(name,"isFocusTrigger")){ret->n=s->focus_requested;}
  else return 0;
 }return 1;
}
static int call(void *user,const char *name,const strb_value *args,int count,strb_value *ret){(void)args;(void)count;return photo_pause_value(user,name,ret);}
photo_pause *photo_pause_open(gt4_vol *vol,gt4_photo_mode *s,const float a[17],const int32_t sh[19]){
 photo_pause *p=calloc(1,sizeof *p);if(!p)return NULL;p->state=s;p->blink=-1;memcpy(p->apertures,a,sizeof p->apertures);memcpy(p->shutters,sh,sizeof p->shutters);
 strb_host host={0};host.call=call;host.user=p;host.palette=1;
 size_t length=0;p->text_data=gt4_vol_read(vol,"/rtext/raceUS.rt2",&length);
 int text_ok=p->text_data && gt4_rtext_open(&p->text,p->text_data,length);
 p->movie=strb_open(vol,"/menu/common/racephoto.strb",text_ok?&p->text:NULL,"RaceDisplay",&host);
 if(!p->movie){free(p->text_data);free(p);return NULL;}return p;
}
void photo_pause_help(photo_pause *p,int tool){if(p)p->blink=tool;}
void photo_pause_draw(photo_pause *p,double dt,int wide){if(p){p->wide=wide;strb_tick(p->movie,dt);strb_draw(p->movie);}}
void photo_pause_close(photo_pause *p){if(p){strb_close(p->movie);free(p->text_data);free(p);}}
