/* Real virtual SDL wheel with a recorded haptic driver boundary. */
#define SDL_HapticOpenFromJoystick record_open
#define SDL_HapticQuery record_query
#define SDL_HapticNewEffect record_new
#define SDL_HapticUpdateEffect record_update
#define SDL_HapticRunEffect record_run
#define SDL_HapticStopEffect record_stop
#define SDL_HapticStopAll record_stop_all
#define SDL_HapticClose record_close
#include "../../src/platform/sdl/pad_sdl.c"
#include <stdio.h>
#include <stdlib.h>
static SDL_HapticEffect last;static int starts,stops,closes;
SDL_Haptic *record_open(SDL_Joystick *j){return j?(SDL_Haptic *)(uintptr_t)1:NULL;}
unsigned record_query(SDL_Haptic *h){(void)h;return SDL_HAPTIC_CONSTANT;}
int record_new(SDL_Haptic *h,SDL_HapticEffect *e){(void)h;last=*e;return 7;}
int record_update(SDL_Haptic *h,int id,SDL_HapticEffect *e){(void)h;(void)id;last=*e;return 0;}
int record_run(SDL_Haptic *h,int id,Uint32 iterations){(void)h;(void)id;(void)iterations;starts++;return 0;}
int record_stop(SDL_Haptic *h,int id){(void)h;(void)id;stops++;return 0;}
int record_stop_all(SDL_Haptic *h){(void)h;stops++;return 0;}
void record_close(SDL_Haptic *h){(void)h;closes++;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"wheel %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
 (void)argv;SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI,"0");SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT,"0");SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"0");SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED,"0");
 CHECK(!SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_HAPTIC|SDL_INIT_GAMECONTROLLER));
 int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_WHEEL,3,16,1);CHECK(device>=0);
 SDL_Joystick *j=SDL_JoystickOpen(device);CHECK(j);
 CHECK(!SDL_JoystickSetVirtualAxis(j,0,-16384));CHECK(!SDL_JoystickSetVirtualAxis(j,1,-32768));CHECK(!SDL_JoystickSetVirtualAxis(j,2,32767));
 CHECK(!SDL_JoystickSetVirtualButton(j,GT4_PAD_CROSS,1));CHECK(!SDL_JoystickSetVirtualHat(j,0,SDL_HAT_RIGHT));
 plat_pad_state p;CHECK(plat_input_poll(0,&p) && plat_input_is_wheel(0));CHECK(p.axes[0]==-.5f && p.axes[4]==1 && p.axes[5]==0);
 CHECK(p.buttons&(1u<<GT4_PAD_CROSS));CHECK(p.buttons&(1u<<GT4_PAD_RIGHT));
 for(int i=0;i<256;i++){
  float force=(128.f-i)/127.f;plat_input_set_ffb(0,force);
  int expected=(int)(fmaxf(-1,fminf(1,force))*32767);
  if(argc>1 && i==0)expected=-expected;
  if(i!=128)CHECK(last.type==SDL_HAPTIC_CONSTANT && last.constant.level==expected && last.constant.length==100 && last.constant.direction.dir[0]==1);
 }
 CHECK(starts==255 && stops==1);plat_input_set_ffb(0,NAN);CHECK(stops==2);
 CHECK(!SDL_JoystickDetachVirtual(device));CHECK(!plat_input_poll(0,&p));CHECK(closes==1 && stops==3);
 plat_input_shutdown();SDL_JoystickClose(j);SDL_Quit();puts("wheel: axes, buttons, 256 torque bytes, stop, NaN and disconnect passed");return 0;
}
