/* Actual application host commands from two SDL virtual controllers. */
#include <SDL.h>
#undef main
#define main single_controller_main
#include "controller_input.c"
#undef main
int main(int argc,char **argv)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI,"0");SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT,"0");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"0");SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED,"0");
    (void)argv;CHECK(!SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS));
    SDL_Joystick *joy[2];int index[2];
    for(int i=0;i<2;i++){
        index[i]=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);CHECK(index[i]>=0);
        joy[i]=SDL_JoystickOpen(index[i]);CHECK(joy[i]);
        CHECK(!SDL_JoystickSetVirtualAxis(joy[i],4,-32768));CHECK(!SDL_JoystickSetVirtualAxis(joy[i],5,-32768));
    }
    CHECK(plat_sdl_pad(0) && plat_sdl_pad(1));
    /* This host-only fixture has no physical world. Keep its run active;
       a finished race reads the real P2 finish flag from world RAM. */
    race_game g={0};g.split=1;g.camera=g.camera_2p=1;g.finished=-1;playing=&g;player=42;player_2p=84;
    menu_options_storage(NULL);uint8_t cmd[2][32],ev[32]={0};
    CHECK(!SDL_JoystickSetVirtualButton(joy[0],SDL_CONTROLLER_BUTTON_A,1));
    CHECK(!SDL_JoystickSetVirtualButton(joy[1],SDL_CONTROLLER_BUTTON_X,1));
    CHECK(!SDL_JoystickSetVirtualAxis(joy[0],0,-16384));
    CHECK(!SDL_JoystickSetVirtualAxis(joy[1],0,16384));
    read_controls(&g);CHECK(host_read(NULL,42,cmd[0],ev));CHECK(host_read(NULL,84,cmd[1],ev));
    if(argc>1)memcpy(cmd[1],cmd[0],32); /* detects port aliasing */
    CHECK(*(int16_t *)(cmd[0]+8)==1 && !*(int16_t *)(cmd[1]+8));
    CHECK(!*(int16_t *)(cmd[0]+10) && *(int16_t *)(cmd[1]+10)==1);
    CHECK(*(int16_t *)(cmd[0]+4)>0 && *(int16_t *)(cmd[1]+4)<0);
    CHECK(!host_read(NULL,99,cmd[0],ev));
    CHECK(!SDL_JoystickSetVirtualButton(joy[1],SDL_CONTROLLER_BUTTON_BACK,1));
    read_controls(&g);CHECK(g.camera==1 && g.camera_2p==6);read_controls(&g);CHECK(g.camera_2p==6);
    CHECK(!SDL_JoystickSetVirtualButton(joy[1],SDL_CONTROLLER_BUTTON_BACK,0));read_controls(&g);
    CHECK(!SDL_JoystickSetVirtualButton(joy[1],SDL_CONTROLLER_BUTTON_BACK,1));read_controls(&g);CHECK(g.camera_2p==0 && g.camera==1);
    SDL_JoystickClose(joy[1]);CHECK(!SDL_JoystickDetachVirtual(index[1]));read_controls(&g);
    CHECK(!command_2p.steer && !command_2p.throttle && !command_2p.brake && command.throttle);
    uint8_t keys[SDL_NUM_SCANCODES]={0};keys[SDL_SCANCODE_W]=keys[SDL_SCANCODE_A]=1;
    read_second_controls(&g,keys);CHECK(command_2p.throttle==1 && command_2p.steer==4096);
    free(g.tape);free(g.tape_2p);
    SDL_JoystickClose(joy[0]);plat_input_shutdown();CHECK(!SDL_JoystickDetachVirtual(index[0]));SDL_Quit();
    puts("PASS split controls: isolated pedals/steering, camera edges, disconnect neutrality, P2 keyboard");return 0;
}
