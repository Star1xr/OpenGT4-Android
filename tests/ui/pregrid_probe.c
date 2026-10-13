/* Real-disc presentation bridge: no race simulation may advance behind
   SceneViewFace. Local images contain disc assets; never commit them. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "miniz.h"
#include "render3d.h"
#include "../../src/app/race_game.c"
#include "career_event.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pregrid probe line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int shot(race_game *g,const char *path)
{
    plat_video_begin_frame();if(!race_game_draw_pregrid(g))return 0;
    int w,h;size_t len;uint8_t *p=plat_sdl_read_pixels(&w,&h);
    void *png=p?tdefl_write_image_to_png_file_in_memory(p,w,h,4,&len):NULL;
    FILE *o=png?fopen(path,"wb"):NULL;if(o){fwrite(png,1,len,o);fclose(o);}
    free(p);mz_free(png);plat_video_end_frame();return o!=NULL;
}
int main(int argc,char **argv)
{
    CHECK(argc==3 || (argc==4 && (!strcmp(argv[3],"--negative") || !strcmp(argv[3],"--career") || !strcmp(argv[3],"--career-negative") || !strcmp(argv[3],"--championship") || !strcmp(argv[3],"--championship-negative"))));
    int championship=argc==4 && (!strcmp(argv[3],"--championship") || !strcmp(argv[3],"--championship-negative"));
    int career=championship || (argc==4 && (!strcmp(argv[3],"--career") || !strcmp(argv[3],"--career-negative")));
    plat_video_desc desc={960,720,0,0,0};CHECK(!plat_video_init(&desc));
    SDL_HideWindow(SDL_GetWindowFromID(1));plat_video_set_view(640,480);
    race_game *g=race_game_open(argv[1]);char msg[256];CHECK(g);
    const char *others[]={"ka_01","z_act_70","today_g_85","midget2_d_type_98","s500_63"};
    uint8_t setups[6][0x178];uint32_t initial_rng=0;
    gt4_race_desc championship_grid;uint8_t championship_bodies[6][0x4b8];
    if(career){
        uint8_t body[0x4b8];const char *body_path=getenv("RACE_OWNED_CAR");CHECK(body_path);
        FILE *owned=fopen(body_path,"rb");CHECK(owned);CHECK(fread(body,1,sizeof body,owned)==sizeof body && fgetc(owned)==EOF);fclose(owned);
        gt4_career_race_lookup_data lookup;CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
        gt4_career_event event;uint32_t seed;CHECK(gt4_career_event_rng_load(argv[1],&seed));
        CHECK(gt4_career_event_prepare(&event,argv[1],&lookup,championship?"de_audi_tt_0301":"am_sunday_0001",body,championship,&seed,msg,sizeof msg));
        CHECK(race_game_start_prepared(g,&event.desc,msg,sizeof msg));
        if(championship)CHECK(race_game_championship_grid(g,&championship_grid,championship_bodies));
        initial_rng=seed;CHECK(g->last_prepared && g->last_desc.owned_car[0]==g->last_owned[0]);
        uint32_t race=GT4_U32(gt4_race_run_conductor(g->run)),entries=GT4_U32(race+0x60);
        for(int i=0;i<6;i++)memcpy(setups[i],gt4_ptr(GT4_U32(GT4_U32(entries+8)+i*4)+0x20),0x178);
        /* Caller/menu storage may go away as soon as synchronous start
           returns. Restart/replay must own their initial equipped body. */
        memset(&event,0xa7,sizeof event);memset(body,0xa7,sizeof body);
    }else CHECK(race_game_start(g,"grandvalley_east","ka_01",0,others,NULL,5,msg,sizeof msg));
    int ready=0;for(int i=0;i<2000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}CHECK(ready==1);
    uint8_t *before=malloc(32u*1024*1024);CHECK(before);memcpy(before,gt4_phys_mem,32u*1024*1024);
    race_game_pregrid_update(g,1./60);CHECK(g->pregrid);float eye[3],rows[9],projection[3];
    CHECK(race_view_get_camera(g->view,eye,rows,projection));float first[3];memcpy(first,eye,sizeof first);
    CHECK(shot(g,argv[2]));CHECK(r3d_frame_stats().wheel_passes==0);
    for(int i=0;i<1200;i++)race_game_pregrid_update(g,1./60);
    CHECK(race_view_get_camera(g->view,eye,rows,projection) && memcmp(eye,first,sizeof first));
    if(argc==4 && !strcmp(argv[3],"--negative"))before[0]^=1; /* prove the whole-memory freeze check detects a mutation */
    CHECK(!memcmp(before,gt4_phys_mem,32u*1024*1024));
    if(career){
        uint8_t saved_rank[6],saved_finished[6];uint32_t saved_time[6];
        for(unsigned i=0;i<6;i++){
            uint32_t car=gt4_race_run_car(g->run,i);saved_rank[i]=GT4_U8(car+0x5b3);saved_finished[i]=GT4_U8(car+0x5b6);saved_time[i]=GT4_U32(car+0x718);
            GT4_U8(car+0x5b3)=(uint8_t)(5-i);GT4_U8(car+0x5b6)=(uint8_t)(i%2);GT4_U32(car+0x718)=123456+i*1000;
        }
        menu_quick_race result;CHECK(race_game_quick_results(g,&result) && result.player==5);
        if(getenv("GT4_GRID_TIME_RESULT_NEGATIVE"))result.grid[0].time++;
        for(unsigned j=0;j<6;j++){
            unsigned i=5-j;CHECK(result.grid[j].time==(i%2?123456+i*1000:0x157529ffu));
            CHECK(!strcmp(result.grid[j].car,i?g->last_others[i-1]:g->last_car));
            CHECK(result.grid[j].color==(i?g->last_colors[i-1]:g->last_color));
            uint32_t car=gt4_race_run_car(g->run,i);GT4_U8(car+0x5b3)=saved_rank[i];GT4_U8(car+0x5b6)=saved_finished[i];GT4_U32(car+0x718)=saved_time[i];
        }
        CHECK(!memcmp(before,gt4_phys_mem,32u*1024*1024));
        puts("PASS real prepared grid to post-race bridge: reversed finish order, actual driver/paint, finished times and unfinished markers; injected HUD restored.");
    }
    gt4_race_hud hud;CHECK(gt4_race_run_hud(g->run,0,&hud));int start=hud.start_frames;float intro=hud.intro_elapsed;
    if(career){
        uint32_t session=GT4_U32(gt4_race_run_conductor(g->run));
        printf("GT start route: rolling=%d effective kind=%d initial frames=%d\n",hud.rolling,(int32_t)GT4_U32(session+0xb4),start);
    }
    race_game_update(g,1./60);CHECK(gt4_race_run_hud(g->run,0,&hud));
    if(career)CHECK(hud.intro_elapsed>intro);else CHECK(hud.start_frames!=start);
    race_game_return_pregrid(g);race_game_pregrid_update(g,1./60);
    CHECK(race_view_get_camera(g->view,eye,rows,projection) && !memcmp(eye,first,sizeof first));
    if(career){
        CHECK(race_game_can_replay(g));CHECK(race_game_replay(g,msg,sizeof msg));CHECK(race_game_replaying(g));
        uint32_t race=GT4_U32(gt4_race_run_conductor(g->run)),entries=GT4_U32(race+0x60);
        for(int i=0;i<6;i++)CHECK(!memcmp(setups[i],gt4_ptr(GT4_U32(GT4_U32(entries+8)+i*4)+0x20),0x178));
        CHECK(g->last_rng==initial_rng);
    }
    CHECK(race_game_restart(g,msg,sizeof msg));
    if(career){
        uint32_t race=GT4_U32(gt4_race_run_conductor(g->run)),entries=GT4_U32(race+0x60);
        if(!strcmp(argv[3],"--career-negative"))setups[0][0x19]^=1;
        for(int i=0;i<6;i++)CHECK(!memcmp(setups[i],gt4_ptr(GT4_U32(GT4_U32(entries+8)+i*4)+0x20),0x178));
        CHECK(g->last_prepared && g->last_rng==initial_rng);
    }
    if(championship){
        for(unsigned round=1;round<3;round++){
            char label[32];unsigned count;uint32_t restore_rng=g->last_rng;gt4_career_event next;gt4_career_race_lookup_data lookup;
            CHECK(gt4_career_championship_label("de_audi_tt_0300",round,label,&count) && count==3);
            CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
            if(round==2){
                /* Reconstruct final round from the original saved keys and
                   paints, with a newly equipped player body; no RACES draw. */
                uint8_t live[0x90]={0};uint64_t keys[6];int32_t paints[6],points[6]={16,12,8,4,2,0};
                for(unsigned j=0;j<6;j++){memcpy(keys+j,championship_bodies[j]+(j?8:0),8);paints[j]=(int8_t)championship_bodies[j][0x18];}
                gt4_career_championship_context(live,1,round,2,"de_audi_tt_0300",keys,paints,points);
                if(getenv("GT4_CHAMPIONSHIP_RESTORE_NEGATIVE"))live[0x48]^=1;
                CHECK(gt4_career_championship_restore(&next,argv[1],&lookup,live,championship_bodies[0],&restore_rng,msg,sizeof msg));
            }else CHECK(gt4_career_championship_round(&next,argv[1],&lookup,label,&championship_grid,msg,sizeof msg));
            uint32_t rng_before=g->last_rng;CHECK(race_game_start_prepared(g,&next.desc,msg,sizeof msg));
            uint32_t race=GT4_U32(gt4_race_run_conductor(g->run)),entries=GT4_U32(race+0x60);
            if(!strcmp(argv[3],"--championship-negative") && round==1)setups[1][0x18]^=1;
            for(int i=0;i<6;i++)CHECK(!memcmp(setups[i],gt4_ptr(GT4_U32(GT4_U32(entries+8)+i*4)+0x20),0x178));
            CHECK(g->last_rng==rng_before && !memcmp(g->last_event_row,next.race_row,151));
            menu_quick_race metadata;CHECK(race_game_quick_grid(g,&metadata));
            /* Disc RACE laps and original 00395400 runway source.
               Conversion is separately checked by championship_run.c. */
            CHECK(metadata.laps==next.race_row[0x43]);
            uint32_t course=GT4_U32(race+0x80),data=GT4_U32(course+4),runway=GT4_U32(data+0xdc);
            if(getenv("GT4_QUICK_GRID_METADATA_NEGATIVE"))metadata.course_length++;
            CHECK(metadata.course_length==gt4_career_course_length(GT4_F32(runway+0x14)));

            ready=0;for(int i=0;i<2000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}CHECK(ready==1);
            race_game_pregrid_update(g,1./60);CHECK(shot(g,argv[2]));
        }
        puts("PASS three actual championship rounds, including saved-context restoration, load their original courses, preserve six entrants/setups/paints and do not advance grid RNG.");
    }
    if(!championship)CHECK(gt4_race_run_hud(g->run,0,&hud) && hud.start_frames==start);
    race_game_close(g);free(before);plat_video_shutdown();
    puts(career?"PASS prepared career grid: caller buffers released, 1201 pregrid ticks freeze all 32 MB, simulation advances, restart retains all six setups and RNG.":"PASS 1201 pregrid ticks leave all 32 MB of race memory unchanged; camera moves, renders without wheel passes, returns to first shot and race restart resets countdown.");return 0;
}
