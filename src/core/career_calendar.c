#include "career_calendar.h"
#include <string.h>
static int32_t signed32(uint32_t v){int32_t n;memcpy(&n,&v,4);return n;}
static int32_t add(int32_t a,int32_t b){return signed32((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return signed32((uint32_t)a-(uint32_t)b);}
static int32_t shift2(int32_t x){uint32_t n=(uint32_t)x>>2;if(x<0)n|=0xc0000000u;return signed32(n);}
/* func_005797A8. Every addu/subu/mult result wraps at 32 bits. Signed
   division truncates toward zero; sra is reproduced without host assumptions.
   Month normalization deliberately accepts out-of-range months and days. */
int32_t gt4_career_date_to_day(const gt4_career_calendar_data *data,int32_t year,int32_t month,int32_t day){
    int32_t m=sub(month,1),q=m/12,r=m%12,y=add(year,q);
    if(r<0){y=sub(y,1);r=add(r,12);}
    int32_t prev=sub(y,1),century=prev/100,remainder=prev%100;
    unsigned leap=((uint32_t)century&3u)==3;
    if(remainder!=99)leap=((uint32_t)prev&3u)==3;
    int32_t quarter=shift2(prev<0?add(prev,3):prev);
    int32_t centquarter=shift2(century<0?add(century,3):century);
    int32_t value=signed32((uint32_t)prev*365u);
    value=add(value,quarter);value=sub(value,century);value=add(value,centquarter);
    unsigned offset=(13u*leap+(unsigned)r)*2;
    int32_t days=data->month_offsets[offset]|(uint32_t)data->month_offsets[offset+1]<<8;
    value=add(value,add(sub(day,1),days));return add(value,0x1a4451);
}
/* func_0042F4F0 and func_0042F530. */
int32_t gt4_career_elapsed_days(const gt4_career_calendar_data *data,int32_t today){return sub(today,gt4_career_date_to_day(data,2005,4,2));}
int32_t gt4_career_elapsed_weeks(const gt4_career_calendar_data *data,int32_t today){return gt4_career_elapsed_days(data,today)/7;}
/* func_00136270 +0x34..0x4c: addiu -5 wraps before signed div/mfhi.
   Preserve negative remainders rather than normalizing them to 0..6. */
int32_t gt4_career_day_to_weekday(int32_t day){return sub(day,5)%7;}
static unsigned offset16(const uint8_t *p,unsigned index){return p[index*2]|(unsigned)p[index*2+1]<<8;}
/* func_00579708: signed comparison in the original half-open binary search. */
static unsigned find_offset(int32_t value,const uint8_t *table,unsigned count){
    unsigned low=0,high=count;
    do{unsigned mid=(low+high)/2;if(value<(int32_t)offset16(table,mid))high=mid;else low=mid;}while(low+1<high);
    return low;
}
/* func_00579898: decomposition into 400/100/4-year intervals, followed by
   the original searches. Return the original packed u16/u8/u8 date fields. */
uint32_t gt4_career_day_to_date(const gt4_career_calendar_data *data,int32_t day){
    int32_t relative=sub(day,0x1a4451),era=relative/146097,remainder=relative%146097;
    int32_t century=remainder/36524;remainder%=36524;
    if(century==4){century=3;remainder=36524;}
    int32_t fours=remainder/1461,within=remainder%1461;
    unsigned year=find_offset(within,data->year_offsets,5);
    unsigned leap=fours!=24?year==3:(century==3 && year==3);
    int32_t combined=add(signed32((uint32_t)era*4u),century);
    int32_t centuries=signed32((uint32_t)combined*25u);
    int32_t years=add(fours,centuries);years=signed32((uint32_t)years*4u);
    uint16_t output_year=(uint16_t)add(add((int32_t)year,years),1);
    int32_t day_of_year=sub(within,(int32_t)offset16(data->year_offsets,year));
    const uint8_t *months=data->month_offsets+leap*26;
    unsigned month=find_offset(day_of_year,months,13);
    uint8_t output_day=(uint8_t)add(sub(day_of_year,(int32_t)offset16(months,month)),1);
    return output_year|((uint32_t)(uint8_t)(month+1)<<16)|((uint32_t)output_day<<24);
}
static uint32_t read32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void write32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint64_t read64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static void write64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(8*i));}
/* func_0042F0E0, including leaf helpers func_006009F0/func_00600A18.
   The event ring wraps once, removes its oldest record when full, stores
   two doublewords in the original order, then increments day and dirty flag. */
void gt4_career_calendar_append(uint8_t *calendar,const uint8_t event[16]){
    uint32_t count=read32(calendar+0x10);
    if(count==2922){uint32_t first=read32(calendar+0xc)+1;if(first>=2922)first-=2922;write32(calendar+0xc,first);write32(calendar+0x10,--count);}
    uint32_t next=read32(calendar+8);uint8_t *destination=calendar+0x18+(next<<4);
    write64(destination,read64(event));write64(destination+8,read64(event+8));
    if(++next>=2922)next-=2922;write32(calendar+8,next);write32(calendar+0x10,count+1);
    uint32_t today=read32(calendar+0xb6c0);write32(calendar+0xb6c4,1);write32(calendar+0xb6c0,today+1);
}

/* func_00435148, with func_0042F4C8: acquisition age occupies low 24 bits.
   The original AND with sign-extended 0xff000000 preserves upper 32 bits. */
void gt4_career_car_acquisition_day(uint8_t *slot,const gt4_career_calendar_data *data,int32_t day){
    uint64_t flags=0;
    for(unsigned i=0;i<8;i++)flags|=(uint64_t)slot[0x10+i]<<(8*i);
    uint32_t age=(uint32_t)day-(uint32_t)gt4_career_date_to_day(data,2005,4,2);
    flags=(flags&UINT64_C(0xffffffffff000000))|(age&0xffffffu);
    for(unsigned i=0;i<8;i++)slot[0x10+i]=(uint8_t)(flags>>(8*i));
}

/* func_0042F1B8 and 0042F218: preserve undefined/reserved stack bytes.
   Sale price is capped with a signed 64-bit slt before truncating to u16. */
void gt4_career_calendar_car_event(uint8_t event[16],int sell,uint64_t key,int64_t color,int64_t detail){
    event[0]=sell?2:1;event[1]=(uint8_t)color;
    if(sell){if(detail>65535)detail=65535;event[2]=(uint8_t)detail;event[3]=(uint8_t)((uint64_t)detail>>8);}
    else event[2]=(uint8_t)detail;
    write64(event+8,key);
}

/* func_00600B88: lw followed by sw zero, including non-boolean values. */
int32_t gt4_career_calendar_consume_dirty(uint8_t *calendar){
    uint32_t bits=0;for(unsigned i=0;i<4;i++){bits|=(uint32_t)calendar[0xb6c4+i]<<(8*i);calendar[0xb6c4+i]=0;}
    int32_t value;memcpy(&value,&bits,4);return value;
}

/* func_0042F270 / 0042F2E0: race and license differ only in event type.
   Byte 3 is original caller stack history; preserve it rather than invent data. */
void gt4_career_calendar_race_event(uint8_t event[16],int license,uint64_t key,uint64_t grade,uint64_t method,uint64_t prize){
    event[0]=license?4:3;event[1]=(uint8_t)grade;event[2]=(uint8_t)method;
    write32(event+4,(uint32_t)prize);write64(event+8,key);
}

/* func_0042F3C0 +0x18..0x34 / 0042F408 +0x1c..0x2c.
   Preserve reserved caller bytes, exactly as the original stack record. */
void gt4_career_calendar_custom_event(uint8_t event[16],int wing,uint64_t key,int64_t category,int64_t detail){
    event[0]=wing?7:6;
    if(!wing){event[1]=(uint8_t)category;event[2]=(uint8_t)detail;}
    write64(event+8,key);
}
