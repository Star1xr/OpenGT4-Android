/* RacePause +0x2C (CONFIRMEXIT): the GT Mode event and championship worlds
   (003EDE50 / 003EF498) set it, and pause.strb asks before Exit. */
#include "race_pause.h"
#include "vol.h"
#include <stdio.h>
#include <string.h>
static int run(gt4_vol *vol,int confirm,int downs,int extra_enter,int *after_exit)
{
    race_pause *p=race_pause_open(vol,0,1,confirm,NULL,NULL);if(!p)return -1;
    for(int i=0;i<120;i++)race_pause_update(p,1/60.);
    for(int k=0;k<downs;k++){race_pause_key(p,40);for(int i=0;i<20;i++)race_pause_update(p,1/60.);}
    race_pause_key(p,13);for(int i=0;i<90;i++)race_pause_update(p,1/60.);
    *after_exit=race_pause_state(p);
    for(int e=0;e<extra_enter && race_pause_state(p)==RACE_PAUSE_OPEN;e++){
        race_pause_key(p,40);for(int i=0;i<60;i++)race_pause_update(p,1/60.);   /* "Are you sure?": down onto EXIT, then enter */
        race_pause_key(p,13);for(int i=0;i<90;i++)race_pause_update(p,1/60.);
    }
    int state=race_pause_state(p);race_pause_close(p);return state;
}
int main(int argc,char **argv)
{
    if(argc<2)return 2;const char *why;gt4_vol *vol=gt4_vol_open(argv[1],&why);if(!vol)return 2;
    int exit_downs=-1;
    for(int d=0;d<5 && exit_downs<0;d++){int first;if(run(vol,0,d,0,&first)==RACE_PAUSE_EXIT)exit_downs=d;}
    if(exit_downs<0){fprintf(stderr,"Exit not found without confirm\n");return 1;}
    int first,state=run(vol,1,exit_downs,1,&first);
    printf("exit at %d downs: without confirm EXIT; with confirm %d, then EXIT again %d\n",exit_downs,first,state);
    gt4_vol_close(vol);
    if(first!=RACE_PAUSE_OPEN || state!=RACE_PAUSE_EXIT){fprintf(stderr,"confirm did not ask before Exit\n");return 1;}
    puts("PASS pause confirm");return 0;
}
