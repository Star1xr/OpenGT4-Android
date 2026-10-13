#include "race_mission.h"
#include <string.h>
int gt4_mission_presentation(const uint8_t *q,int previous)
{
    if(!q || previous>=GT4_MISSION_RETURN)return previous;
    int32_t count;memcpy(&count,q+0x824,4);
    if(count<0 || count>256)return previous;
    for(int i=0;i<count;i++){
        int16_t type;int32_t car;
        memcpy(&type,q+8+i*8,2);memcpy(&car,q+12+i*8,4);
        /* 00389638 +D8..104: event 10 invokes world+280 (003ED188),
           ending the mission without the result board. It takes precedence
           over the presentation advanced earlier in that world frame. */
        if(type==10 && q[0x808+10])return GT4_MISSION_RETURN;
        /* 003E05A0 +34..BC -> 003DF1B8/003E06F8: RaceMission watches
           entry zero (+4); constructor 003DEE70 leaves +64=-1, +8=0.
           A rival finishing alone must not open the player's board. */
        if(type==5 && car==0 && q[0x808+5])previous=GT4_MISSION_BOARD;
    }
    return previous;
}

int gt4_mission_begin_frame(int previous)
{
    /* 003ED188 +60 -> 0033CCF0 +1C sets world+D80. The next call to
       0033C490 +10..34 stops via 00389E10, before 003B70F0/00389758.
       0038A098 +18..28 sets world+D18=1; this path adds no fade. */
    return previous==GT4_MISSION_RETURN?GT4_MISSION_STOPPED:previous;
}
