/* Two human command streams through the unchanged native race runner.
   P: the PC file format; C: live/playback poses, zero tolerance. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "miniz.h"
#include "career_vehicle.h"
#include "../../src/app/race_audio.c"
#include "../../src/app/race_game.c"
#include "video_gl.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"split replay line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(int argc,char **argv)
{
    CHECK(argc>=2);int negative=argc>2 && !strcmp(argv[2],"--negative"),mirror_negative=argc>2 && !strcmp(argv[2],"--mirror-negative");
    plat_video_desc video={640,448,0,0,0};CHECK(!plat_video_init(&video));SDL_HideWindow(SDL_GetWindowFromID(1));
    race_game *g=race_game_open(argv[1]);CHECK(g);char msg[256];
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
    uint8_t bodies[2][0x4b8],unit[32]={0},setups[2][0x178];
    const char *labels[2]={"ka_01","today_g_85"};
    for(int i=0;i<2;i++){CHECK(gt4_career_vehicle_create(factory,labels[i],i?1:2,0,unit,bodies[i]));uint32_t sheet=i+1;memcpy(bodies[i]+0x488,&sheet,4);CHECK(gt4_career_active_setup(bodies[i],setups[i]));}
    gt4_race_desc options={0};options.tire_2p=1;options.split_boost=0;
    options.owned_car[0]=bodies[0];options.owned_car[1]=bodies[1];
    race_game_set_split(g,&options);race_game_set_driving(g,0,0,0,0);
    const char *others[]={"today_g_85"};int paints[]={1};
    CHECK(race_game_start(g,"tsukuba","ka_01",2,others,paints,1,msg,sizeof msg));
    enum{FRAMES=1200};float (*poses)[2][15]=calloc(FRAMES,sizeof *poses);CHECK(poses);
    for(int frame=0;frame<FRAMES;frame++){
        memset(&command,0,sizeof command);memset(&command_2p,0,sizeof command_2p);
        command.device=command_2p.device=2;command.throttle=frame>650?32767:0;
        command_2p.throttle=frame>760?22000:0;command_2p.brake=frame>1080?14000:0;
        command.steer=frame>1000?3500:0;command_2p.steer=frame>1050?-2000:0;
        CHECK(gt4_race_run_frame(g->run));
        for(int i=0;i<2;i++){float wheels[4][3];CHECK(gt4_race_run_pose(g->run,i,poses[frame][i],poses[frame][i]+3,wheels));
            uint32_t car=gt4_race_run_car(g->run,i);memcpy(poses[frame][i]+12,gt4_ptr(car+0x6c4),12);}
    }
    CHECK(race_game_can_replay(g) && g->tape_n==FRAMES && g->tape_n_2p==FRAMES);
    gt4_replay_file offered,loaded;CHECK(race_game_replay_export(g,&offered,GT4_REPLAY_FULL));
    uint8_t *packed;size_t size;CHECK(gt4_replay_pack(&offered,&packed,&size));
    CHECK(!gt4_replay_unpack(packed,size-1,&loaded));CHECK(gt4_replay_unpack(packed,size,&loaded));free(packed);
    CHECK(loaded.split_battle && loaded.steps_2p==FRAMES && !memcmp(loaded.tape_2p,g->tape_2p,FRAMES*32));
    for(int i=0;i<2;i++)CHECK(loaded.owned_on[i] && !memcmp(loaded.owned[i],bodies[i],sizeof bodies[i]));
    if(negative){uint8_t *p=loaded.tape;loaded.tape=loaded.tape_2p;loaded.tape_2p=p;}
    CHECK(race_game_replay_import(g,&loaded));gt4_replay_free(&loaded);
    CHECK(race_game_replay(g,msg,sizeof msg));CHECK(g->split && g->replaying);
    for(int i=0;i<2;i++){uint8_t setup[0x178];CHECK(gt4_race_run_setup(g->run,i,setup));CHECK(!memcmp(setup,setups[i],sizeof setup));}
    for(int frame=0;frame<FRAMES;frame++){
        CHECK(gt4_race_run_frame(g->run));
        for(int i=0;i<2;i++){float actual[15],wheels[4][3];CHECK(gt4_race_run_pose(g->run,i,actual,actual+3,wheels));
            uint32_t car=gt4_race_run_car(g->run,i);memcpy(actual+12,gt4_ptr(car+0x6c4),12);
            if(memcmp(actual,poses[frame][i],sizeof actual))fprintf(stderr,"P%d replay differs at frame %d\n",i+1,frame);
            CHECK(!memcmp(actual,poses[frame][i],sizeof actual));}
    }
    CHECK(g->tape_at==FRAMES && g->tape_at_2p==FRAMES);
    int status=0;for(int i=0;i<30000 && !status;i++){status=race_game_poll(g);if(!status)SDL_Delay(1);}CHECK(status==1);
    race_game_update(g,0);g->camera=0;g->camera_2p=6;split_camera(g,0);split_camera(g,1);
    plat_video_set_view(640,448);plat_video_begin_frame();
    for(int i=0;i<2;i++){float *p=g->split_projection[i];race_view_set_camera(g->view,g->split_eye[i],g->split_rows[i],p[0],p[1],p[2]);
        CHECK(race_view_draw_player(g->view,i,i!=0,i*224.f,224));}
    int width,height;uint8_t *plain=plat_sdl_read_pixels(&width,&height);CHECK(plain && width==640 && height==448);
    CHECK(race_game_draw(g));uint8_t *mirrors=plat_sdl_read_pixels(&width,&height);CHECK(mirrors);
    if(mirror_negative)mirrors[0]^=1;
    unsigned inside[2]={0},outside=0;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++)if(memcmp(plain+(y*width+x)*4,mirrors+(y*width+x)*4,3)){
        if(x>=200 && x<440 && y>=16 && y<56)inside[0]++;
        else if(x>=200 && x<440 && y>=232 && y<272)inside[1]++;
        else outside++;
    }
    fprintf(stderr,"mirror changed pixels: %u / %u; outside %u\n",inside[0],inside[1],outside);
    CHECK(inside[0]>64 && inside[1]>64 && !outside);free(plain);free(mirrors);
    if(argc>3){race_game_draw_hud(g,0);uint8_t *rgba=plat_sdl_read_pixels(&width,&height);size_t n;void *png=tdefl_write_image_to_png_file_in_memory(rgba,width,height,4,&n);CHECK(png);
        FILE *f=fopen(argv[3],"wb");CHECK(f && fwrite(png,1,n,f)==n);fclose(f);free(rgba);mz_free(png);}
    plat_video_end_frame();
    race_game_close(g);gt4_career_vehicle_close(factory);free(poses);plat_video_shutdown();
    puts("PASS Battle2P: separate commands, file roundtrip/truncation, 2400 exact live/replay poses, both clipped mirror passes");return 0;
}
