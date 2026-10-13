/* Disc teacher tapes through the unchanged native player examination.
   00110230 +98..110: import driving options and commands, not old setup IDs.
   A nonzero GT4M race time is a limited timing oracle. This DC=0 audit
   does not exercise the Demonstration Scene (Training: 00115290 +20..28,
   DC=1, mode 2) or establish its duration/completion behavior. */
#include "career_license_disc.h"
#include "career_license.h"
#include "disc_replay.h"
#include "car.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t player;
static const gt4_replay_file *recording;
static size_t cursor;
static int read_pad(void *ctx,uint32_t car,uint8_t *command,const uint8_t *event){
    (void)ctx;if(car!=player)return 0;
    if(cursor<recording->steps)memcpy(command,recording->tape+32*cursor++,32);
    else {memset(command,0,32);command[1]=1;command[3]=event[3];}
    return 1;
}
static int wheel(void *ctx,uint32_t car,int16_t out[2]){
    (void)ctx;out[0]=out[1]=0;return car==player;
}
static int write_pad(void *ctx,uint32_t car,const uint8_t *data){
    (void)ctx;(void)data;return car==player;
}
int main(int argc,char **argv){
    if(argc<2)return 2;
    gt4_career_race_lookup_data lookup;
    if(!gt4_career_race_lookup_data_load(argv[1],&lookup))return 2;
    const char *reason=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&reason);
    if(!vol){fprintf(stderr,"%s\n",reason);return 2;}
    gt4_disc_replay *disc=gt4_disc_replay_open(vol);if(!disc)return 2;
    const char *groups[]={"l0b","l0a","lib","lia","l0s"};
    unsigned passed=0,failed=0;
    for(unsigned g=0;g<5;g++)for(unsigned n=1;n<=16;n++){
        char code[32],path[64],error[256];
        snprintf(code,sizeof code,"%s%04u",groups[g],n);
        if(argc>2 && strcmp(argv[2],code))continue;
        snprintf(path,sizeof path,"/replay/license/us%s",code);
        gt4_replay_file tape;gt4_career_license_exam exam;
        if(!gt4_disc_replay_read(disc,path,&tape,error,sizeof error) ||
           !gt4_career_license_prepare(&exam,argv[1],&lookup,code,error,sizeof error)){
            fprintf(stderr,"%s: %s\n",code,error);failed++;continue;
        }
        exam.desc.manual=tape.manual;exam.desc.assist_asm=tape.assist_asm;exam.desc.assist_tcs=tape.assist_tcs;
        gt4_race_run *run=gt4_race_run_open(argv[1],&exam.desc,error,sizeof error);
        if(!run){fprintf(stderr,"%s: %s\n",code,error);gt4_replay_free(&tape);failed++;continue;}
        player=gt4_race_run_car(run,0);recording=&tape;cursor=0;
        gt4_phys_pad.read=read_pad;gt4_phys_pad.wheel_state=wheel;gt4_phys_pad.write=write_pad;
        /* Continue neutral commands after EOF, just as host_read does.
           Guard is a test timeout, never a gameplay timing constant. */
        size_t frame=0;int advanced=1;
        while(frame<tape.steps+1800 && !gt4_race_run_finished(run)){
            if(!gt4_race_run_frame(run)){advanced=0;break;}frame++;
        }
        int32_t state=-2;uint32_t elapsed=0x157529ff;
        int result=gt4_race_run_license_result(run,&state,&elapsed);
        uint32_t expected=tape.race_time;
        if(getenv("GT4_LICENSE_TEACHER_NEGATIVE"))expected++;
        int ok=advanced && result && gt4_race_run_finished(run) && state==2 && elapsed==expected;
        printf("%s %s class=%s kind=%u state=%d time=%u original=%u gold=%u commands=%zu/%zu frames=%zu\n",
            ok?"PASS":"FAIL",code,tape.disc_info?(char *)tape.disc_info+16:"unknown",
            GT4_U8(gt4_race_run_conductor(run)+0xf81a),state,elapsed,expected,
            gt4_career_license_time(exam.row,0),cursor,tape.steps,frame);
        if(argc>2 && !ok){
            fprintf(stderr,"student flags=%u rank=%u mode=%d distance=%.6f; guides",
                GT4_U8(player+0x5b7),GT4_U8(player+0x5b3),(int8_t)GT4_U8(player+0x56a),GT4_F32(player+0x5a8));
            for(int i=1;i<exam.desc.cars;i++){
                uint32_t car=gt4_race_run_car(run,i);
                fprintf(stderr," %d:rank=%u mode=%d distance=%.6f",i,GT4_U8(car+0x5b3),(int8_t)GT4_U8(car+0x56a),GT4_F32(car+0x5a8));
            }
            fputc('\n',stderr);
        }
        fflush(stdout);if(ok)passed++;else failed++;
        gt4_race_run_close(run);gt4_replay_free(&tape);
    }
    gt4_disc_replay_close(disc);gt4_vol_close(vol);
    printf("Teacher player audit: %u pass, %u fail. Dynamics unchanged.\n",passed,failed);
    return passed && !failed?0:1;
}
