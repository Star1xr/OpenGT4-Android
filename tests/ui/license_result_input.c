/* SDL pad edges -> the original result_license.strb Retry/Exit callbacks.
   Integration C; this does not certify the STRB focus adapter's pixels. */
#include "../../src/app/race_game.c"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"licence result input line %d: %s (%s)\n",__LINE__,#x,SDL_GetError()); return 1; } } while (0)
static void advance_result(race_game *g)
{
    race_result_state state={0};
    for(int i=0;i<360;i++)race_result_update(g->result,&state,1/60.);
}
static int press(race_game *g,SDL_Joystick *joy,SDL_GameControllerButton button)
{
    CHECK(!SDL_JoystickSetVirtualButton(joy,button,1));SDL_GameControllerUpdate();pause_input(g);
    /* Holding an input must not execute a second edge. */
    pause_input(g);
    CHECK(!SDL_JoystickSetVirtualButton(joy,button,0));SDL_GameControllerUpdate();pause_input(g);
    return 0;
}
int main(int argc,char **argv)
{
    if(argc<2)return 2;
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI,"0");SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT,"0");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"0");SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED,"0");
    CHECK(!SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS));
    const char *why;gt4_vol *vol=gt4_vol_open(argv[1],&why);CHECK(vol);
    int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);CHECK(index>=0);
    SDL_Joystick *joy=SDL_JoystickOpen(index);CHECK(joy);SDL_GameControllerUpdate();
    race_game g={0};g.last_prepared=1;g.last_desc.license=1;g.pad_id=-1;
    const int outcomes[]={0,2,3,4};
    for(unsigned s=0;s<sizeof outcomes/sizeof *outcomes;s++)for(int retry=0;retry<2;retry++){
        g.result=race_result_license_open(vol,outcomes[s],11749,"l0b0001",11300,NULL);CHECK(g.result);
        advance_result(&g);CHECK(!race_result_done(g.result));
        CHECK(!press(&g,joy,retry?SDL_CONTROLLER_BUTTON_DPAD_UP:SDL_CONTROLLER_BUTTON_DPAD_DOWN));
        if(argc>2){ /* Reproduce the bug: confirm never reaches the result. */
            g.pause_keys[5]=1;
        }
        CHECK(!press(&g,joy,SDL_CONTROLLER_BUTTON_A));advance_result(&g);
        CHECK(race_result_done(g.result) && race_result_retry(g.result)==retry);
        race_result_close(g.result);g.result=NULL;
    }
    memset(g.pause_keys,1,sizeof g.pause_keys);SDL_JoystickClose(joy);
    CHECK(!SDL_JoystickDetachVirtual(index));SDL_PumpEvents();pause_input(&g);
    for(int i=0;i<6;i++)CHECK(!g.pause_keys[i]);
    index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);CHECK(index>=0);
    joy=SDL_JoystickOpen(index);CHECK(joy);SDL_GameControllerUpdate();
    g.result=race_result_license_open(vol,0,11749,"l0b0001",11300,NULL);CHECK(g.result);advance_result(&g);
    CHECK(!press(&g,joy,SDL_CONTROLLER_BUTTON_DPAD_UP));CHECK(!press(&g,joy,SDL_CONTROLLER_BUTTON_A));
    advance_result(&g);CHECK(race_result_done(g.result) && race_result_retry(g.result));race_result_close(g.result);
    SDL_JoystickClose(joy);plat_input_shutdown();CHECK(!SDL_JoystickDetachVirtual(index));SDL_Quit();gt4_vol_close(vol);
    puts("PASS licence result: pad Retry/Exit for fail/gold/silver/bronze, held edges and reconnect.");return 0;
}
