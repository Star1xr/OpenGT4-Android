/* PC device adapter. PS2 key semantics stay in core/pad_config.c. */
#include "plat_sdl.h"
#include "platform/platform.h"
#include "pad_config.h"
#include <string.h>
#include <math.h>
static SDL_GameController *pads[2];
static SDL_Joystick *wheels[2];
static SDL_Haptic *forces[2];
static int force_id[2]={-1,-1};

/* On-screen pad policy: the frontend's choice, the devices it already saw and
   the devices the player has used since. A backend that draws an on-screen
   pad asks plat_sdl_onscreen_hidden every frame. */
static int onscreen_mode=PLAT_ONSCREEN_AUTO;
static unsigned onscreen_known, onscreen_seen;
static void wheel_close(int port)
{
 if(forces[port]){SDL_HapticStopAll(forces[port]);SDL_HapticClose(forces[port]);forces[port]=NULL;}
 force_id[port]=-1;
 if(wheels[port]){SDL_JoystickClose(wheels[port]);wheels[port]=NULL;}
}
int plat_input_init(void)
{return SDL_WasInit(SDL_INIT_GAMECONTROLLER)?0:SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);}
SDL_GameController *plat_sdl_pad(int index)
{
 if(index<0 || index>1 || plat_input_init())return NULL;
 for(int p=0;p<2;p++){
  if(pads[p] && !SDL_GameControllerGetAttached(pads[p])){SDL_GameControllerClose(pads[p]);pads[p]=NULL;}
  if(wheels[p] && !SDL_JoystickGetAttached(wheels[p]))wheel_close(p);
 }
 for(int i=0;i<SDL_NumJoysticks();i++){
  int wheel=SDL_JoystickGetDeviceType(i)==SDL_JOYSTICK_TYPE_WHEEL;
  if(!wheel && !SDL_IsGameController(i))continue;
  SDL_JoystickID id=SDL_JoystickGetDeviceInstanceID(i);int used=0;
  for(int p=0;p<2;p++)if(pads[p] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[p]))==id)used=1;
  for(int p=0;p<2;p++)if(wheels[p] && SDL_JoystickInstanceID(wheels[p])==id)used=1;
  if(!used)for(int p=0;p<2;p++)if(!pads[p] && !wheels[p]){
   if(wheel)wheels[p]=SDL_JoystickOpen(i);else pads[p]=SDL_GameControllerOpen(i);break;
  }
 }
 return pads[index];
}
void plat_input_shutdown(void)
{for(int p=0;p<2;p++){wheel_close(p);if(pads[p]){SDL_GameControllerClose(pads[p]);pads[p]=NULL;}}}
int plat_input_is_wheel(int index)
{if(index<0 || index>1)return 0;plat_sdl_pad(index);return wheels[index]!=NULL;}
void plat_input_set_onscreen(int mode,unsigned known)
{
 if(mode<PLAT_ONSCREEN_AUTO || mode>PLAT_ONSCREEN_OFF)mode=PLAT_ONSCREEN_AUTO;
 onscreen_mode=mode;onscreen_known=known;onscreen_seen=0;
}
void plat_sdl_note_device(unsigned kind)
{onscreen_seen|=kind;}
/* Whether an on-screen pad must stay hidden: the player's choice wins, and
   otherwise a pad, wheel, keyboard or mouse in use does. Nothing shows an
   on-screen pad on the desktop, so only the app's own launcher and the
   Android backend read this. */
int plat_sdl_onscreen_hidden(void)
{
 if(onscreen_mode==PLAT_ONSCREEN_ON)return 0;
 if(onscreen_mode==PLAT_ONSCREEN_OFF)return 1;
 if((onscreen_known|onscreen_seen)&(PLAT_INPUT_DEVICE_GAMEPAD|PLAT_INPUT_DEVICE_KEYBOARD|PLAT_INPUT_DEVICE_MOUSE))return 1;
 for(int i=0;i<SDL_NumJoysticks();i++){
  if(SDL_JoystickIsVirtual(i))continue;
  if(SDL_IsGameController(i) || SDL_JoystickGetDeviceType(i)==SDL_JOYSTICK_TYPE_WHEEL)return 1;
 }
 return 0;
}
void plat_input_set_ffb(int index,float torque)
{
 if(!plat_input_is_wheel(index))return;
 if(!isfinite(torque))torque=0;
 if(torque>1)torque=1;if(torque< -1)torque=-1;
 /* P: signed normalized torque is an SDL constant force on its steering
    axis. The 100 ms effect expires if the application's clock stops;
    there is no added spring, damping or change to 00353EE8's output. */
 if(!forces[index]){
  if(SDL_InitSubSystem(SDL_INIT_HAPTIC))return;
  forces[index]=SDL_HapticOpenFromJoystick(wheels[index]);
  if(!forces[index])return;
  if(!(SDL_HapticQuery(forces[index])&SDL_HAPTIC_CONSTANT)){SDL_HapticClose(forces[index]);forces[index]=NULL;return;}
 }
 if(!torque){if(force_id[index]>=0)SDL_HapticStopEffect(forces[index],force_id[index]);return;}
 SDL_HapticEffect effect;memset(&effect,0,sizeof effect);
 effect.type=SDL_HAPTIC_CONSTANT;effect.constant.direction.type=SDL_HAPTIC_CARTESIAN;
 effect.constant.direction.dir[0]=1;effect.constant.length=100;effect.constant.level=(Sint16)(torque*32767);
 if(force_id[index]<0)force_id[index]=SDL_HapticNewEffect(forces[index],&effect);
 else if(SDL_HapticUpdateEffect(forces[index],force_id[index],&effect)){SDL_HapticStopAll(forces[index]);return;}
 if(force_id[index]>=0)SDL_HapticRunEffect(forces[index],force_id[index],1);
}
int plat_input_poll(int index,plat_pad_state *out)
{
 memset(out,0,sizeof *out);SDL_GameController *pad=plat_sdl_pad(index);
 if(!pad && index>=0 && index<2 && wheels[index]){
  SDL_Joystick *wheel=wheels[index];SDL_JoystickUpdate();
  /* P: SDL raw wheel axes: steer 0, accelerator 1, brake 2. An SDL
     mapping may differ between wheel models; physical calibration A. */
  int steer=SDL_JoystickGetAxis(wheel,0);out->axes[0]=steer<0?steer/32768.f:steer/32767.f;
  for(int k=1;k<=2;k++)if(SDL_JoystickNumAxes(wheel)>k)out->axes[k+3]=(32767.f-SDL_JoystickGetAxis(wheel,k))/65535.f;
  int count=SDL_JoystickNumButtons(wheel);for(int k=0;k<count && k<16;k++)if(SDL_JoystickGetButton(wheel,k))out->buttons|=1u<<k;
  for(int k=0;k<SDL_JoystickNumHats(wheel);k++){
   int h=SDL_JoystickGetHat(wheel,k);if(h&SDL_HAT_UP)out->buttons|=1u<<GT4_PAD_UP;if(h&SDL_HAT_DOWN)out->buttons|=1u<<GT4_PAD_DOWN;
   if(h&SDL_HAT_LEFT)out->buttons|=1u<<GT4_PAD_LEFT;if(h&SDL_HAT_RIGHT)out->buttons|=1u<<GT4_PAD_RIGHT;
  }return 1;
 }
 if(!pad)return 0;
 SDL_GameControllerUpdate();
 static const SDL_GameControllerButton map[16]={SDL_CONTROLLER_BUTTON_BACK,SDL_CONTROLLER_BUTTON_LEFTSTICK,
  SDL_CONTROLLER_BUTTON_RIGHTSTICK,SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_DPAD_UP,
  SDL_CONTROLLER_BUTTON_DPAD_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,
  SDL_CONTROLLER_BUTTON_INVALID,SDL_CONTROLLER_BUTTON_INVALID,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
  SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_B,
  SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_X};
 for(int k=0;k<16;k++)if(map[k]!=SDL_CONTROLLER_BUTTON_INVALID && SDL_GameControllerGetButton(pad,map[k]))out->buttons|=1u<<k;
 for(int k=0;k<6;k++){
  int v=SDL_GameControllerGetAxis(pad,(SDL_GameControllerAxis)k);
  out->axes[k]=v<0?v/32768.f:v/32767.f;
 }
 /* P: PC stick dead zone, independent of the untouched original solver.
    Preserve the existing port's 8% stick / 5% trigger thresholds. */
 for(int k=0;k<4;k++)if(out->axes[k]>-.08f && out->axes[k]<.08f)out->axes[k]=0;
 for(int k=4;k<6;k++)if(out->axes[k]<.05f)out->axes[k]=0;
 /* P: a PC trigger has a long travel where the DualShock 2's L2/R2 is a
    short button with pressure: its button bit (what the game reads for
    shifting, views and menus) only past half its travel; the pressure
    (axes[4], axes[5]) stays for the pedals. */
 if(out->axes[4]>=.5f)out->buttons|=1u<<GT4_PAD_L2;
 if(out->axes[5]>=.5f)out->buttons|=1u<<GT4_PAD_R2;
 return 1;
}
unsigned plat_sdl_menu_buttons(void)
{
 plat_pad_state p;if(!plat_input_poll(0,&p))return 0;
 /* getPadButtonState: the engine's button bits (func_00214AC0): up 1,
    down 2, left 4, right 8, L1 0x100, L2 0x200, L3 0x400, R1 0x800,
    R2 0x1000, R3 0x2000, Triangle 0x4000, Square 0x8000, Cross 0x10000,
    Circle 0x20000, Select 0x40000, Start 0x80000 (D_006192D8
    key/keysym pairs: Select/Delete and Start/Pause) */
 static const struct { int key; unsigned bit; } bits[]={
  {GT4_PAD_UP,1},{GT4_PAD_DOWN,2},{GT4_PAD_LEFT,4},{GT4_PAD_RIGHT,8},
  {GT4_PAD_L1,0x100},{GT4_PAD_L2,0x200},{GT4_PAD_L3,0x400},{GT4_PAD_R1,0x800},
  {GT4_PAD_R2,0x1000},{GT4_PAD_R3,0x2000},{GT4_PAD_TRIANGLE,0x4000},{GT4_PAD_SQUARE,0x8000},
  {GT4_PAD_CROSS,0x10000},{GT4_PAD_CIRCLE,0x20000},{GT4_PAD_SELECT,0x40000},{GT4_PAD_START,0x80000}};
 unsigned out=0;
 for(size_t i=0;i<sizeof bits/sizeof bits[0];i++)if(p.buttons&(1u<<bits[i].key))out|=bits[i].bit;
 return out;
}
int plat_sdl_ps2_key(const SDL_Event *e,int *release)
{
 /* the engine's D_006192E4 keysyms for the buttons the menus' four
    directions, Cross, Circle and Triangle (plat_sdl_controller_key) do
    not already carry: L1 0xFFC8, L2 0xFFC9, R1 0xFFD2, R2 0xFFD3,
    Square 0xFF63 (Insert), Select 0xFFFF (Delete), Start 0xFF13
    (Pause): D_006192D8 +0x44..0x54. Start must never activate Cross. */
 *release=0;
 if(e->type==SDL_KEYDOWN || e->type==SDL_KEYUP){
  /* P: the keyboard's Q/E (the L1/R1 held for the menus), Insert, Delete */
  if(e->key.repeat)return 0;
  *release=e->type==SDL_KEYUP;
  switch(e->key.keysym.sym){
  case SDLK_q:return 0xffc8;case SDLK_e:return 0xffd2;
  case SDLK_INSERT:return 0xff63;case SDLK_DELETE:return 0xffff;
  case SDLK_PAUSE:return 0xff13;
  default:break;
  }
  /* the keys whose press is the menu's own input (menu_send_input sends
     its onKeyPress): only their release here */
  if(!*release)return 0;
  switch(e->key.keysym.sym){
  case SDLK_RETURN:case SDLK_KP_ENTER:return 0xff0d;case SDLK_ESCAPE:return 0xff1b;
  case SDLK_F4:return 0xff8d;case SDLK_LEFT:return 0xff51;case SDLK_UP:return 0xff52;
  case SDLK_RIGHT:return 0xff53;case SDLK_DOWN:return 0xff54;
  default:return 0;
  }
 }
 SDL_GameController *pad=plat_sdl_pad(0);if(!pad)return 0;
 SDL_JoystickID id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
 if(e->type==SDL_CONTROLLERBUTTONDOWN || e->type==SDL_CONTROLLERBUTTONUP){
  if(e->cbutton.which!=id)return 0;
  *release=e->type==SDL_CONTROLLERBUTTONUP;
  switch(e->cbutton.button){
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:return 0xffc8;
  case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:return 0xffd2;
  case SDL_CONTROLLER_BUTTON_X:if(*release)return 0xff63;return 0;
  case SDL_CONTROLLER_BUTTON_BACK:return 0xffff;
  case SDL_CONTROLLER_BUTTON_START:return 0xff13;
  default:break;
  }
  if(!*release)return 0;   /* as the keyboard's: the menu input's release */
  switch(e->cbutton.button){
  case SDL_CONTROLLER_BUTTON_A:return 0xff0d;
  case SDL_CONTROLLER_BUTTON_B:return 0xff1b;case SDL_CONTROLLER_BUTTON_Y:return 0xff8d;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT:return 0xff51;case SDL_CONTROLLER_BUTTON_DPAD_UP:return 0xff52;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:return 0xff53;case SDL_CONTROLLER_BUTTON_DPAD_DOWN:return 0xff54;
  default:return 0;
  }
 }
 if(e->type==SDL_CONTROLLERAXISMOTION && e->caxis.which==id &&
    (e->caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT || e->caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT)){
  /* the trigger's press at half its travel (plat_input_poll's bit) */
  static SDL_JoystickID trigger_id=-1;static int held[2];
  int k=e->caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT,down=e->caxis.value>=16384;
  if(trigger_id!=id){trigger_id=id;held[0]=held[1]=0;}
  if(down==held[k])return 0;
  held[k]=down;*release=!down;
  return k?0xffd3:0xffc9;
 }
 return 0;
}
int plat_sdl_controller_key(const SDL_Event *e)
{
 SDL_GameController *pad=plat_sdl_pad(0);if(!pad)return 0;
 SDL_JoystickID id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
 if(e->type==SDL_CONTROLLERAXISMOTION && e->caxis.which==id && e->caxis.axis<2){
  /* P: stick navigation is a host key edge: a push past half the travel,
     let go under three tenths (the 8% dead zone alone made every wobble
     near the centre another step). */
  static SDL_JoystickID stick_id=-1;static int held[2];
  if(stick_id!=id){stick_id=id;held[0]=held[1]=0;}
  int axis=e->caxis.axis,previous=held[axis],v=e->caxis.value,
      side=v<=-16384?-1:v>=16384?1:previous && (v*previous)>9830?previous:0;
  held[axis]=side;if(!side || side==previous)return 0;
  return axis==0?(side<0?SDLK_LEFT:SDLK_RIGHT):(side<0?SDLK_UP:SDLK_DOWN);
 }
 if(e->type!=SDL_CONTROLLERBUTTONDOWN || e->cbutton.which!=id)return 0;
 switch(e->cbutton.button){
 case SDL_CONTROLLER_BUTTON_DPAD_UP:return SDLK_UP;
 case SDL_CONTROLLER_BUTTON_DPAD_DOWN:return SDLK_DOWN;
 case SDL_CONTROLLER_BUTTON_DPAD_LEFT:return SDLK_LEFT;
 case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:return SDLK_RIGHT;
 case SDL_CONTROLLER_BUTTON_A:return SDLK_RETURN;
 case SDL_CONTROLLER_BUTTON_B:return SDLK_ESCAPE;
 case SDL_CONTROLLER_BUTTON_Y:return SDLK_F4;
 case SDL_CONTROLLER_BUTTON_X:return SDLK_INSERT;
 default:return 0;
 }
}
