/* Virtual SDL controller -> the application's actual physics host command. */
#include "../../src/app/race_game.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"controller input line %d: %s (%s)\n",__LINE__,#x,SDL_GetError());return 1;}}while(0)
int main(int argc,char **argv)
{
 /* Only virtual pads belong to this test; connected user devices must
    never take player 1 or contribute physical input. */
 SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI,"0");SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT,"0");
 SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"0");SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED,"0");
 (void)argv;CHECK(!SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS));
 int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);CHECK(index>=0);
 SDL_Joystick *joy=SDL_JoystickOpen(index);CHECK(joy);
 CHECK(!SDL_JoystickSetVirtualAxis(joy,4,-32768) && !SDL_JoystickSetVirtualAxis(joy,5,-32768));
 SDL_GameControllerUpdate();CHECK(plat_sdl_pad(0));
 race_game g={0};playing=&g;player=42;g.finished=0;g.camera=1;
 menu_options_storage(NULL);uint8_t cmd[32]={0},ev[32]={0};
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_A,1));
 CHECK(!SDL_JoystickSetVirtualAxis(joy,0,-16384));
 read_controls(&g);CHECK(host_read(NULL,player,cmd,ev));
 CHECK(cmd[1]==2 && (cmd[0]&1) && !(cmd[0]&2));CHECK(*(int16_t *)(cmd+4)==2048 && *(int16_t *)(cmd+8)==1);
 if(argc>1)cmd[8]=0;CHECK(cmd[8]==1); /* deliberate lost accelerator */
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_A,0));
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_Y,1));
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1));
 CHECK(!SDL_JoystickSetVirtualAxis(joy,5,32767));
 read_controls(&g);memset(cmd,0,sizeof cmd);host_read(NULL,player,cmd,ev);
 CHECK(*(int16_t *)(cmd+0xc)==1 && cmd[7]==1 && (int8_t)cmd[2]==1);
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_Y,0));
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0));
 CHECK(!SDL_JoystickSetVirtualAxis(joy,5,-32768));
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,1));
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_BACK,1));
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_DPAD_UP,1));
 read_controls(&g);host_read(NULL,player,cmd,ev);
 CHECK(!command.reverse && !command.special && !command.shift && g.look_back && g.ghost_hidden && g.camera==6);
 read_controls(&g);CHECK(g.camera==6 && g.ghost_hidden); /* held buttons are not repeated */
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_BACK,0));
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_DPAD_UP,0));read_controls(&g);
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_BACK,1));
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_DPAD_UP,1));read_controls(&g);
 CHECK(g.camera==0 && !g.ghost_hidden);
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1));
 CHECK(!SDL_JoystickSetVirtualAxis(joy,5,32767));
 plat_pad_state state;CHECK(plat_input_poll(0,&state));
 gt4_pad_config c;gt4_pad_defaults(&c,0);c.button[GT4_PAD_R2]=GT4_PAD_ACCEL;c.button[GT4_PAD_CROSS]=-1;
 gt4_pad_actions action;gt4_pad_evaluate(&c,&state,&action);CHECK(action.action[GT4_PAD_ACCEL]==1 && (action.analog&(1u<<GT4_PAD_ACCEL)));
 /* A trigger's light pull: pressure for a pedal, no press for a button
    function (the default R2 upshift); half its travel is the press. */
 CHECK(!SDL_JoystickSetVirtualAxis(joy,5,-16384));   /* a quarter of the travel */CHECK(plat_input_poll(0,&state) && !(state.buttons&(1u<<GT4_PAD_R2)));
 gt4_pad_evaluate(&c,&state,&action);CHECK(action.action[GT4_PAD_ACCEL]>0.2f && action.action[GT4_PAD_ACCEL]<0.3f);
 {gt4_pad_config d;gt4_pad_defaults(&d,0);gt4_pad_evaluate(&d,&state,&action);CHECK(action.action[GT4_PAD_SHIFTUP]==0 && !(action.analog&(1u<<GT4_PAD_SHIFTUP)));
  if(getenv("GT4_TRIGGER_THRESHOLD_NEGATIVE"))CHECK(action.action[GT4_PAD_SHIFTUP]>0);
  CHECK(!SDL_JoystickSetVirtualAxis(joy,5,6554));   /* six tenths */CHECK(plat_input_poll(0,&state) && (state.buttons&(1u<<GT4_PAD_R2)));
  gt4_pad_evaluate(&d,&state,&action);CHECK(action.action[GT4_PAD_SHIFTUP]==1);}
 CHECK(!SDL_JoystickSetVirtualAxis(joy,5,32767));CHECK(plat_input_poll(0,&state));
 /* SDL events reach the same menu routes as keyboard; Triangle included. */
 SDL_Event e={0};e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.which=SDL_JoystickInstanceID(joy);e.cbutton.button=SDL_CONTROLLER_BUTTON_Y;
 CHECK(plat_sdl_controller_key(&e)==SDLK_F4);e.cbutton.button=SDL_CONTROLLER_BUTTON_A;CHECK(plat_sdl_controller_key(&e)==SDLK_RETURN);
 e.cbutton.button=SDL_CONTROLLER_BUTTON_B;CHECK(plat_sdl_controller_key(&e)==SDLK_ESCAPE);
 e.cbutton.button=SDL_CONTROLLER_BUTTON_X;CHECK(plat_sdl_controller_key(&e)==SDLK_INSERT);
 {int release;CHECK(plat_sdl_ps2_key(&e,&release)==0 && !release);
  e.type=SDL_CONTROLLERBUTTONUP;CHECK(plat_sdl_ps2_key(&e,&release)==0xff63 && release);
  e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.button=SDL_CONTROLLER_BUTTON_A;}
 /* Start and Select retain distinct PS2 keysyms, on press and release. */
 {int release;SDL_Event special=e;
  special.cbutton.button=SDL_CONTROLLER_BUTTON_START;
  CHECK(!plat_sdl_controller_key(&special));CHECK(plat_sdl_ps2_key(&special,&release)==0xff13 && !release);
  special.type=SDL_CONTROLLERBUTTONUP;CHECK(plat_sdl_ps2_key(&special,&release)==0xff13 && release);
  special.type=SDL_CONTROLLERBUTTONDOWN;special.cbutton.button=SDL_CONTROLLER_BUTTON_BACK;
  CHECK(!plat_sdl_controller_key(&special));CHECK(plat_sdl_ps2_key(&special,&release)==0xffff && !release);
  special.type=SDL_CONTROLLERBUTTONUP;CHECK(plat_sdl_ps2_key(&special,&release)==0xffff && release);
  if(getenv("GT4_START_KEY_NEGATIVE"))CHECK(plat_sdl_ps2_key(&special,&release)==0xff0d);
 }
 CHECK(SDL_PushEvent(&e)==1);plat_key_event key;int accept=0;
 while(plat_sdl_poll(&key))if(key.pad && key.key==SDLK_RETURN)accept=1;
 CHECK(accept);
 /* An ignored pad event must not mark the next keyboard event as pad input. */
 memset(&e,0,sizeof e);e.type=SDL_CONTROLLERAXISMOTION;e.caxis.which=SDL_JoystickInstanceID(joy);e.caxis.axis=3;
 CHECK(SDL_PushEvent(&e)==1);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;e.key.repeat=0;CHECK(SDL_PushEvent(&e)==1);
 int keyboard=0;while(plat_sdl_poll(&key))if(key.key==SDLK_ESCAPE && !key.pad)keyboard=1;CHECK(keyboard);
 e.type=SDL_CONTROLLERAXISMOTION;e.caxis.which=SDL_JoystickInstanceID(joy);e.caxis.axis=0;e.caxis.value=32767;
 CHECK(plat_sdl_controller_key(&e)==SDLK_RIGHT && !plat_sdl_controller_key(&e));
 e.caxis.value=0;CHECK(!plat_sdl_controller_key(&e));e.caxis.value=-32768;CHECK(plat_sdl_controller_key(&e)==SDLK_LEFT);
 /* a wobble near the centre and a push held under half: no more steps */
 e.caxis.value=0;CHECK(!plat_sdl_controller_key(&e));
 {const int wobble[]={4000,-4000,9000,-9000,12000,3000,15000};
  for(int k=0;k<7;k++){e.caxis.value=wobble[k];CHECK(!plat_sdl_controller_key(&e));}}
 e.caxis.value=20000;CHECK(plat_sdl_controller_key(&e)==SDLK_RIGHT);
 e.caxis.value=12000;CHECK(!plat_sdl_controller_key(&e));e.caxis.value=20000;CHECK(!plat_sdl_controller_key(&e));
 e.caxis.value=5000;CHECK(!plat_sdl_controller_key(&e));e.caxis.value=20000;CHECK(plat_sdl_controller_key(&e)==SDLK_RIGHT);
 e.caxis.value=0;CHECK(!plat_sdl_controller_key(&e));
 CHECK(!SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,1));CHECK((plat_sdl_menu_buttons()&0x900)==0x900); /* shoulder chord; other keys from preceding cases remain held */
 SDL_JoystickClose(joy);CHECK(!SDL_JoystickDetachVirtual(index));SDL_PumpEvents();CHECK(!plat_input_poll(0,&state));CHECK(!state.buttons);
 memset(g.pause_keys,1,sizeof g.pause_keys);memset(g.pad_held,1,sizeof g.pad_held);g.replay_back_held=1;
 pause_input(&g);CHECK(!g.pause_keys[0] && !g.pad_held[0] && !g.replay_back_held);
 read_controls(&g);CHECK(!g.look_back && !command.reverse && !command.special && command.device==1);
 index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);CHECK(index>=0 && plat_sdl_pad(0));
 plat_input_shutdown();CHECK(!SDL_JoystickDetachVirtual(index));SDL_Quit();
 puts("PASS virtual controller: concurrent stick/digital pedals, reverse, nitro, gears, trigger remap, menus, disconnect/reconnect; negative drops accelerator.");return 0;
}
