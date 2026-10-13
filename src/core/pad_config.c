#include "pad_config.h"
#include <string.h>
#include <math.h>
/* Names/order: 004382B0/004382F8, D_00622E08/28. The original script
   option.ad KeyconfigRoot::DS2 L17..88 supplies the choices/defaults. */
static const char *const buttons[16]={"SELECT","L3","R3","START","UP","RIGHT","DOWN","LEFT",
 "L2","R2","L1","R1","TRIANGLE","CIRCLE","CROSS","SQUARE"};
static const char *const axes[6]={"LX","RX","LYF","LYL","RYF","RYL"};
static const char *const actions[GT4_PAD_ACTIONS]={"BUTTON_LSTEER","BUTTON_RSTEER","BUTTON_SHIFTDOWN",
 "BUTTON_SHIFTUP","BUTTON_ACCEL","BUTTON_BRAKE","BUTTON_SIDEBRAKE","BUTTON_REVERSEGEAR",
 "BUTTON_SPECIAL","BUTTON_PLAYVIEW","BUTTON_BACKVIEW","BUTTON_FRONTVIEW","BUTTON_REARVIEW",
 "BUTTON_LEFTVIEW","BUTTON_RIGHTVIEW","BUTTON_LICENCE_BEST_LINE","BUTTON_GHOST","BUTTON_PAUSE"};
static const char *const analog_actions[]={"ANALOG_STEER","ANALOG_SHIFTDOWN","ANALOG_SHIFTUP",
 "ANALOG_ACCEL","ANALOG_BRAKE","ANALOG_SIDEBRAKE","ANALOG_REVERSEGEAR"};
static const int analog_ids[]={-2,GT4_PAD_SHIFTDOWN,GT4_PAD_SHIFTUP,GT4_PAD_ACCEL,GT4_PAD_BRAKE,GT4_PAD_SIDE,GT4_PAD_REVERSE};
static int find(const char *s,const char *const *names,int count)
{for(int k=0;s && k<count;k++)if(!strcmp(s,names[k]))return k;return -1;}
static const char *property(int port,int analog)
{return analog?(port?"pad_analog_2p":"pad_analog_1p"):(port?"pad_buttons_2p":"pad_buttons_1p");}
void gt4_pad_defaults(gt4_pad_config *out,int port)
{
 for(int k=0;k<16;k++)out->button[k]=-1;
 for(int k=0;k<6;k++)out->analog[k]=-1;
 out->analog[GT4_PAD_LX]=-2;
 out->button[GT4_PAD_LEFT]=GT4_PAD_LSTEER;out->button[GT4_PAD_RIGHT]=GT4_PAD_RSTEER;
 out->button[GT4_PAD_CROSS]=GT4_PAD_ACCEL;out->button[GT4_PAD_SQUARE]=GT4_PAD_BRAKE;
 out->button[GT4_PAD_TRIANGLE]=GT4_PAD_REVERSE;out->button[GT4_PAD_CIRCLE]=GT4_PAD_SIDE;
 out->button[GT4_PAD_R2]=GT4_PAD_SHIFTUP;out->button[GT4_PAD_L2]=GT4_PAD_SHIFTDOWN;
 out->button[GT4_PAD_L1]=GT4_PAD_BACKVIEW;out->button[GT4_PAD_SELECT]=GT4_PAD_PLAYVIEW;
 if(!port){out->button[GT4_PAD_R1]=GT4_PAD_SPECIAL;out->button[GT4_PAD_UP]=GT4_PAD_GHOST;}
 out->button[GT4_PAD_START]=GT4_PAD_PAUSE;
}
void gt4_pad_option(ah_vm *vm,ah_obj *option,int port,gt4_pad_config *out)
{
 gt4_pad_defaults(out,port);
 for(int analog=0;analog<2;analog++){
  ah_val v;ah_obj *a=NULL;int count=analog?6:16,*values=analog?out->analog:out->button;
  if(ah_get_member(vm,option,property(port,analog),&v))a=ah_as_obj(v,AO_ARRAY);
  if(a && ah_array_size(a)==count)for(int k=0;k<count;k++){
   int n=(int)ah_num(ah_array_get(a,k));values[k]=n>=-2 && n<GT4_PAD_ACTIONS?n:-1;
  }
 }
}
/* Adhoc adapters for 00190B50/00191460 (unset within the supplied key
   list), 00190D38/00191648 (assign one key), 00191078/00191988 (search
   returns every matching key). Layout is host-owned, not PS2 save bytes. */
static int config_method(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
 int method=(int)(size_t)data,analog=method&1,op=method/2;
 *ret=ah_nil();if(argc<3 || (op!=0 && argc<4))return 0;
 int port=(int)ah_num(argv[0]);const char *device=ah_cstr(argv[1]);
 if(port<0 || port>1 || !device || strcmp(device,"DUALSHOCK 2"))return ah_error(vm,"pad peripheral is not available");
 ah_val value;ah_obj *a=NULL;if(ah_get_member(vm,self,property(port,analog),&value))a=ah_as_obj(value,AO_ARRAY);
 if(!a)return ah_error(vm,"pad configuration missing");
 const char *const *names=analog?axes:buttons;int count=analog?6:16;
 int fn=find(ah_cstr(argv[op==1?3:2]),analog?analog_actions:actions,analog?7:GT4_PAD_ACTIONS);
 if(analog && fn>=0)fn=analog_ids[fn];
 if(op==0){
  ah_obj *found=ah_array_new(vm);
  for(int k=0;k<count;k++)if((int)ah_num(ah_array_get(a,k))==fn && fn!=-1)ah_array_push(vm,found,ah_str(vm,names[k]));
  *ret=ah_objv(found);
 }else if(op==1 && argc>=4){
  int key=find(ah_cstr(argv[2]),names,count);if(key>=0)ah_array_set(a,key,ah_int(fn));
 }else if(op==2 && argc>=4){
  ah_obj *list=ah_as_obj(argv[3],AO_ARRAY);
  for(int k=0;k<ah_array_size(list);k++){
   int key=find(ah_cstr(ah_array_get(list,k)),names,count);
   if(key>=0 && (int)ah_num(ah_array_get(a,key))==fn)ah_array_set(a,key,ah_int(-1));
  }
 }
 return 0;
}
void gt4_pad_bind_option(ah_vm *vm,ah_obj *option)
{
 for(int port=0;port<2;port++){
  gt4_pad_config c;gt4_pad_defaults(&c,port);
  for(int analog=0;analog<2;analog++){
   ah_val v;ah_obj *a=NULL;if(ah_get_member(vm,option,property(port,analog),&v))a=ah_as_obj(v,AO_ARRAY);
   if(!a){a=ah_array_new(vm);for(int k=0;k<(analog?6:16);k++)ah_array_push(vm,a,ah_int(analog?c.analog[k]:c.button[k]));
    ah_set_member(vm,option,property(port,analog),ah_objv(a));}
  }
 }
 const char *const methods[]={"searchButtonConfig","searchAnalogConfig","setButtonConfig","setAnalogConfig","unsetButtonKey","unsetAnalogKey"};
 for(int k=0;k<6;k++)ah_set_member(vm,option,methods[k],ah_bind(vm,option,ah_native(vm,methods[k],config_method,(void *)(size_t)k)));
}
void gt4_pad_evaluate(const gt4_pad_config *c,const plat_pad_state *p,gt4_pad_actions *out)
{
 memset(out,0,sizeof *out);
 for(int k=0;k<16;k++){
  int f=c->button[k];if(f<0 || f>=GT4_PAD_ACTIONS)continue;
  float v=(p->buttons&(1u<<k))?1.f:0.f;
  /* P: SDL triggers substitute pressure on the PS2's L2/R2 buttons, only
     for the functions the game reads as pressure (the pedals: 00349F48's
     analog flags); the others take the button bit (pad_sdl: half the
     trigger's travel), as the PS2's digital press. */
  if((k==GT4_PAD_L2 || k==GT4_PAD_R2) && (f==GT4_PAD_ACCEL || f==GT4_PAD_BRAKE || f==GT4_PAD_REVERSE || f==GT4_PAD_SIDE)){
   v=fmaxf(v,p->axes[k==GT4_PAD_L2?4:5]);if(v>0)out->analog|=1u<<f;}
  out->action[f]=fmaxf(out->action[f],v);
 }
 float axes_value[6]={-p->axes[0],-p->axes[2],fmaxf(0,-p->axes[1]),fmaxf(0,p->axes[1]),fmaxf(0,-p->axes[3]),fmaxf(0,p->axes[3])};
 for(int k=0;k<6;k++){
  int f=c->analog[k];float v=axes_value[k];
  if(f==-2){out->steer=v;out->analog|=1u<<GT4_PAD_LSTEER;}
  else if(f>=0 && f<GT4_PAD_ACTIONS){out->action[f]=fmaxf(out->action[f],v);out->analog|=1u<<f;}
 }
 if(out->action[GT4_PAD_LSTEER] || out->action[GT4_PAD_RSTEER]){out->steer=out->action[GT4_PAD_LSTEER]-out->action[GT4_PAD_RSTEER];out->analog&=~(1u<<GT4_PAD_LSTEER);}
}
