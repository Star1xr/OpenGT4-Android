#include "career_license.h"
#include "career_original.h"
#include <string.h>
#include <stdio.h>

uint32_t gt4_career_license_attempt(gt4_career_license_attempts *s,
                                   int32_t work_state,uint32_t time)
{
    /* 001105A8 +0x68..0xb4: race work +0x88 states 2,3,4 map to
       result grades 9,10,11. 00110660 rejects every other work state. */
    uint32_t grade;
    switch(work_state){
        case 2:grade=9;break;
        case 3:grade=10;break;
        case 4:grade=11;break;
        default:return 13;
    }
    /* 00110664..0011067C: unsigned, strict improvement. A tie preserves
       the session grade even if the supplied completion state differs. */
    if(time<s->best_time){s->best_grade=grade;s->best_time=time;}
    /* 00110694..001106A8 -> 00430A58: every successful attempt updates
       the persistent record, independent of the best time of this visit. */
    gt4_career_record_update(&s->record,1,grade);
    return grade;
}

int gt4_career_license_history(const gt4_career_license_attempts *s,
                              uint32_t *grade,uint32_t *method,uint32_t *time)
{
    /* 00110820..00110878: grade 13 calls 0042F198 (advance day);
       otherwise 0042F2E0 receives low-byte grade, method 14 and best time. */
    if(s->best_grade==13)return 0;
    *grade=s->best_grade&255u;*method=14;*time=s->best_time;
    return 1;
}

int gt4_career_license_index(const char *code)
{
    /* 004323D0 +0x20..0x154: region prefix, group and 504-byte unit. */
    if(!code)return -1;
    if(strlen(code)>=8)code+=2;
    static const char *groups[]={"l0b","l0a","lib","lia","l0s","l0c","l0m"};
    int group=-1,n=0;for(int i=0;i<7;i++)if(!strncmp(code,groups[i],3)){group=i;break;}
    if(group<0)return -1;sscanf(code+3,"%04d",&n);
    uint32_t index=group<5?(uint32_t)group*16+(uint32_t)n-1:group==5?(uint32_t)n+79:(uint32_t)n+84;
    return index<125?(int)index:-1;
}
int gt4_career_license_next(const char *code,char next[32])
{
    /* 0010FCD0 +0x108..0x14c: standard exams increment within their
       category; number 16 returns to GT Mode, without wrapping. The
       authored quick-license.available_next gates entry into exam 16. */
    int index=gt4_career_license_index(code),n=0;
    if(index<0 || index>=80 || strlen(code)>=32)return 0;
    size_t prefix=strlen(code)-4;sscanf(code+prefix,"%04d",&n);
    if(n<1 || n>=16)return 0;
    snprintf(next,32,"%.*s%04d",(int)prefix,code,n+1);return 1;
}

int gt4_career_license_music(int32_t state)
{
    /* 003E1C80 +0xb8..0x138, jtbl_006A3920; failure 003E1EA0 +0x28:
       PCM enums of standard examinations, outside coffee break modes. */
    if(state<=0)return 24;
    if(state==1 || state==2)return 21;
    if(state==3)return 22;
    if(state==4 || state==5)return 23;
    return -1;
}
