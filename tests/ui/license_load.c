/* Construct the authored exam through the actual application's loading
   bridge, then enter its menu and start the attempt. Integration C. */
#include "../../src/app/race_game.c"
#include "career_license_disc.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"licence load %s line %d: %s: %s\n",code,__LINE__,#x,error); return 1; } } while (0)
int main(int argc,char **argv)
{
    if(argc<3)return 2;const char *code=argv[2];char error[256]="";
    CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));SDL_HideWindow(SDL_GetWindowFromID(1));
    race_game *g=race_game_open(argv[1]);CHECK(g);
    gt4_career_race_lookup_data lookup;gt4_career_license_exam exam;
    CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
    CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,code,error,sizeof error));
    if(argc>3)exam.desc.cars=0;
    CHECK(race_game_start_prepared(g,&exam.desc,error,sizeof error));
    menu_quick_race grid;CHECK(race_game_quick_grid(g,&grid));
    int ready=0;for(int i=0;i<4000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}
    CHECK(ready==1 && race_game_license_active(g));
    for(int i=0;i<120;i++)race_game_pregrid_update(g,1/60.);
    CHECK(race_game_restart(g,error,sizeof error));ready=0;
    for(int i=0;i<4000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}
    CHECK(ready==1);for(int i=0;i<120;i++)race_game_update(g,1/60.);
    CHECK(gt4_race_run_cars(g->run)==exam.desc.cars);
    race_game_close(g);plat_video_shutdown();puts("PASS application licence grid, asynchronous assets, menu and attempt.");return 0;
}
