/* Integration of original teacher pad tapes with the existing native run.
   This checks consumption/return isolation, not trajectory fidelity. */
#include <SDL.h>
#include "plat_sdl.h"
#include "career_license_disc.h"
#include "../../src/app/race_game.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"demo line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int ready(race_game *g){int n=0;for(int i=0;i<4000 && !n;i++){SDL_PumpEvents();SDL_Delay(5);n=race_game_poll(g);}return n;}
int main(int argc,char **argv)
{
    CHECK(argc>=2);int negative=argc>2 && !strcmp(argv[2],"--negative");
    CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));SDL_HideWindow(SDL_GetWindowFromID(1));
    gt4_career_race_lookup_data lookup;gt4_career_license_exam exam;char error[256];
    CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
    race_game *g=race_game_open(argv[1]);CHECK(g);
    if(argc>2 && !strcmp(argv[2],"--theater")){
        char paths[32][64];int count=gt4_disc_replay_list(g->vol,paths,32,0);CHECK(count==13);
        for(int k=0;k<count;k++){
            gt4_replay_file file;CHECK(race_game_read_replay(g,paths[k],&file,error,sizeof error));
            CHECK(race_game_replay_import(g,&file));CHECK(race_game_replay(g,error,sizeof error));CHECK(ready(g)==1);
            CHECK(g->replaying && !g->demonstrating);
            float p[3],r[9],w[4][3];
            for(int i=0;i<120;i++)race_game_update(g,1./60);
            CHECK(g->tape_at>0 && g->tape_at<=120);CHECK(g->disc_segment==file.disc_segments);
            CHECK(gt4_race_run_pose(g->run,0,p,r,w));for(int i=0;i<3;i++)CHECK(isfinite(p[i]));
            race_game_key(g,SDLK_ESCAPE);CHECK(race_game_quit(g));race_game_stop(g);
            printf("PASS native Theater %s, pad playback, finite pose, recorded checkpoints and cancellation.\n",paths[k]);fflush(stdout);
            gt4_replay_free(&file);
        }
        race_game_close(g);plat_video_shutdown();return 0;
    }
    CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,"l0b0001",error,sizeof error));
    CHECK(race_game_start_prepared(g,&exam.desc,error,sizeof error));CHECK(ready(g)==1);
    /* A player's previous attempt must survive teacher playback. */
    g->tape=malloc(96);CHECK(g->tape);memset(g->tape,0x5a,96);g->tape_n=g->tape_cap=3;g->thumb_on=2;
    uint8_t *old=g->tape;g->license_log.count=2;
    CHECK(race_game_license_demo(g,NULL,error,sizeof error));CHECK(ready(g)==1);
    CHECK(race_game_demonstrating(g) && race_game_replaying(g));
    CHECK(!g->concourse.enabled && !g->license_log_collecting);
    /* Test only: 003F8AB0 skips grade writes for DC=1 demonstrations.
       DC=0 evaluates the same original pad tape as a player attempt. */
    if(getenv("GT4_DEMO_TEST_PLAYER_GRADE"))GT4_U32(GT4_U32(gt4_race_run_conductor(g->run))+0xdc)=0;
    gt4_replay_file reference;CHECK(race_game_read_replay(g,"/replay/license/usl0b0001",&reference,error,sizeof error));
    if(negative)g->tape[4]^=1;
    CHECK(g->tape_n==reference.steps && !memcmp(g->tape,reference.tape,reference.steps*32));
    size_t steps=reference.steps;uint32_t original_time=reference.race_time;
    printf("B-1 original recording metadata: race=%u best=%u; targets gold=%u silver=%u bronze=%u ms\n",reference.race_time,reference.best_lap,gt4_career_license_time(g->last_event_row,0),gt4_career_license_time(g->last_event_row,1),gt4_career_license_time(g->last_event_row,2));
    gt4_replay_free(&reference);
    /* Each command delivered at the pad boundary must be byte exact. */
    uint8_t command_bytes[32],ev[32]={0};
    for(size_t i=0;i<steps;i++){CHECK(host_read(NULL,player,command_bytes,ev));CHECK(!memcmp(command_bytes,g->tape+i*32,32));}
    g->tape_at=0;float before[3],rot[9],wheels[4][3],after[3];CHECK(gt4_race_run_pose(g->run,0,before,rot,wheels));
    for(unsigned i=0;i<4000 && !race_game_over(g);i++)race_game_update(g,1./60);
    CHECK(g->tape_at==steps && race_game_over(g) && !g->result);
    int32_t teacher_state;uint32_t teacher_time;CHECK(race_game_license_result(g,&teacher_state,&teacher_time));
    gt4_race_hud teacher_hud;CHECK(gt4_race_run_hud(g->run,0,&teacher_hud));
    printf("B-1 teacher native: state=%d time=%u ms lap=%u speed=%.6f; no award/result UI\n",teacher_state,teacher_time,teacher_hud.lap_time,teacher_hud.speed);
    if(getenv("GT4_DEMO_TEST_PLAYER_GRADE")){
        CHECK(teacher_state==(getenv("GT4_DEMO_TEST_GRADE_NEGATIVE")?3:2));
        CHECK(teacher_time==original_time && teacher_time<=gt4_career_license_time(g->last_event_row,0));
    }
    CHECK(gt4_race_run_pose(g->run,0,after,rot,wheels));CHECK(memcmp(before,after,sizeof before));
    CHECK(race_game_end_demo(g,error,sizeof error));CHECK(ready(g)==1);
    CHECK(!g->demonstrating && !g->replaying && g->tape==old && g->tape_n==3 && g->tape_cap==3 && g->thumb_on==2 && g->license_log.count==2);
    for(unsigned i=0;i<96;i++)CHECK(old[i]==0x5a);
    printf("PASS B-1 teacher: %zu exact pad commands, native motion, no result, prior tape/thumbnail/logger restored.\n",steps);
    CHECK(race_game_license_demo(g,"l0b0001",error,sizeof error));CHECK(ready(g)==1);
    race_game_key(g,SDLK_ESCAPE);CHECK(race_game_quit(g));
    CHECK(race_game_end_demo(g,error,sizeof error));CHECK(ready(g)==1 && g->tape==old && g->tape_n==3);
    puts("PASS teacher cancellation and fresh exam return.");
    race_game_close(g);plat_video_shutdown();return 0;
}
