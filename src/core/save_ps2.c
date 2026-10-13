/* GT4 cryptography and binary layout adapted from PDTools by Nenkai (MIT).
   See third_party/PDTools/LICENSE and docs/SAVEGAMES.md. Container layouts:
   Ross Ridge's public-domain mymc and PCSX2 PS2-MemoryCardFileSystem reference. */
#include "save_ps2.h"
#include "gtmode_game.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t r64(const uint8_t *p){return r32(p)|((uint64_t)r32(p+4)<<32);}
static void w32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t checksum(const uint8_t *p,size_t n){uint32_t v=~0u;while(n--){v^=*p++;for(int i=0;i<8;i++)v=(v>>1)^((0u-(v&1))&0xedb88320u);}return ~v;}
typedef struct {uint32_t v[624];int at;} mt;
static void seed(mt *m,uint32_t s){m->v[0]=s;for(unsigned i=1;i<624;i++)m->v[i]=1812433253u*(m->v[i-1]^(m->v[i-1]>>30))+i;m->at=624;}
static uint32_t random32(mt *m){if(m->at==624){for(int i=0;i<624;i++){uint32_t y=(m->v[i]&0x80000000u)|(m->v[(i+1)%624]&0x7fffffffu);m->v[i]=m->v[(i+397)%624]^(y>>1)^((0u-(y&1))&0x9908b0dfu);}m->at=0;}uint32_t y=m->v[m->at++];y^=y>>11;y^=(y<<7)&0x9d2c5680u;y^=(y<<15)&0xefc60000u;return y^(y>>18);}
static uint32_t update(uint32_t *v,int old){*v=17u*(*v)+17u;uint32_t x=*v;if(old)return x^((x<<16)|(x>>16));uint32_t reversed=0;for(int i=0;i<32;i++){reversed=(reversed<<1)|(x&1);x>>=1;}x=(reversed<<24)|((reversed&0xff00)<<8)|(reversed>>24)|((reversed>>8)&0xff00);uint32_t y=x*257u+257u;return x^((y<<16)|(y>>16));}
static void swap(uint8_t *p,size_t n,double mult){size_t at=(size_t)(mult*(n-4));for(int i=0;i<4;i++){uint8_t b=p[i];p[i]=p[at+i];p[at+i]=b;}}
/* 001D4E60/001D50E8: only the swap positions use the rounded length;
   cipher and CRC cover the actual file. CRC failure never yields data. */
int gt4_ps2_unwrap(const uint8_t *raw,size_t n,uint32_t first,uint32_t second,
    size_t alignment,int old,uint8_t **out,size_t *length)
{
    if(!out || !length)return 0;*out=NULL;*length=0;
    if(!raw || n<8 || (n&3) || n>16u*1024*1024 || !alignment || (alignment&(alignment-1)))return 0;
    size_t rounded=(n+alignment-1)&~(alignment-1);float a,b;memcpy(&a,&first,4);memcpy(&b,&second,4);
    if(!(a>=0 && a<1 && b>=0 && b<1))return 0;
    size_t at=(size_t)((double)a*(rounded-4)),bt=4+(size_t)((double)b*(rounded-8));
    if(at>n-4 || bt>n-4)return 0;
    uint8_t *p=malloc(n);if(!p)return 0;memcpy(p,raw,n);
    swap(p,rounded,(double)a);swap(p+4,rounded-4,(double)b);
    uint32_t rand=r32(p),crc=r32(p+4),cipher=rand^crc;mt m;seed(&m,rand+crc);
    for(size_t i=8;i<n;i+=4)w32(p+i,(r32(p+i)+update(&cipher,old))^random32(&m));
    if(checksum(p+8,n-8)!=crc){free(p);return 0;}
    memmove(p,p+8,n-8);*out=p;*length=n-8;return 1;
}
int gt4_ps2_decode(const uint8_t *raw,size_t size,uint8_t **out,size_t *length,int *old_crypto){*out=NULL;*length=0;if(size<24 || size>512*1024 || (size&3))return 0;uint8_t *p=malloc(size);if(!p)return 0;int valid=0,old=1;for(;old>=0;old--){memcpy(p,raw,size);swap(p,size,(double)0.65486f);swap(p+4,size-4,(double)0.13579f);uint32_t rand=r32(p),crc=r32(p+4),cipher=rand^crc;mt m;seed(&m,rand+crc);for(size_t i=8;i<size;i+=4){uint32_t r=random32(&m),u=update(&cipher,old);w32(p+i,(r32(p+i)+u)^r);}if(checksum(p+8,size-8)==crc){valid=1;break;}}
 if(!valid || r32(p+8)!=0xff60bad3u){free(p);return 0;}uint32_t packed=r32(p+12);size_t n=(size_t)(0u-packed);if((int32_t)packed>=0 || n>size-24 || (n!=0x3a070 && n!=0x3a160) || checksum(p+24,n)!=r32(p+16)){free(p);return 0;}memmove(p,p+24,n);*out=p;*length=n;if(old_crypto)*old_crypto=old;return 1;}
/* Floating-point bit shuffling is verified against the paired unique ID.
   Never accept a guessed garage header or silently substitute credits. */
static int reverse_bits(uint8_t *p,size_t n,uint32_t key){size_t bits=n*8;uint16_t *positions=malloc(bits*sizeof *positions);if(!positions)return 0;mt m;seed(&m,key);for(size_t i=bits-1;i>0;i--){float f=(float)random32(&m)*(1.0f/(float)UINT32_MAX);size_t at=(size_t)((double)f*(i+1));if(at>i){free(positions);return 0;}positions[i-1]=(uint16_t)at;}for(size_t i=1;i<bits;i++){size_t j=positions[i-1];unsigned a=(p[i/8]>>(i%8))&1,b=(p[j/8]>>(j%8))&1;if(a!=b){p[i/8]^=(uint8_t)(1u<<(i%8));p[j/8]^=(uint8_t)(1u<<(j%8));}}free(positions);return 1;}
static int garage_header(const uint8_t *raw,size_t n,uint32_t key,int old,uint64_t *money){if(n<64)return 0;uint8_t h[64];memcpy(h,raw,64);uint32_t k=key,u1=update(&k,old),u2=update(&k,old);if(!reverse_bits(h,64,u2))return 0;uint32_t cipher=r32(h+60)^u1;w32(h+60,cipher);mt m;seed(&m,key+cipher);for(int i=0;i<60;i++)h[i]^=(uint8_t)random32(&m);if(r32(h+56)!=key)return 0;for(int i=8;i<44;i++)if(h[i])return 0;*money=((uint64_t)r32(h)<<32)|r32(h+4);return *money<=INT64_MAX;}
static FILE *file_open(const char *path){
#ifdef _WIN32
 wchar_t w[1200];if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,w,1200))return NULL;return _wfopen(w,L"rb");
#else
 return fopen(path,"rb");
#endif
}
static uint8_t *read_file(const char *path,size_t limit,size_t *n){*n=0;FILE *f=file_open(path);if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long length=ftell(f);if(length<=0 || (size_t)length>limit || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}uint8_t *p=malloc(length);if(!p || fread(p,1,length,f)!=(size_t)length){free(p);fclose(f);return NULL;}fclose(f);*n=length;return p;}
static const char *names[]={"BESCES-51719GAMEDATA","BASCUS-97328GAMEDATA","BISCPS-17001GAMEDATA","BKSCKA-30001GAMEDATA","BASCUS-97436GAMEDATA","BIPAPX-90523GAMEDATA"};
static int region(const char *name){for(int i=0;i<6;i++)if(!strcmp(name,names[i]))return i;return -1;}
typedef struct {uint8_t *data,*garage;size_t data_size,garage_size;int region;} source;
static int take(source *s,const char *name,const uint8_t *p,size_t n){int r=region(name);uint8_t **dest=r>=0?&s->data:!strcmp(name,"garage")?&s->garage:NULL;if(!dest)return 1;if(*dest || !n || n>2*1024*1024)return 0;*dest=malloc(n);if(!*dest)return 0;memcpy(*dest,p,n);if(r>=0){s->data_size=n;s->region=r;}else s->garage_size=n;return 1;}
static int entry_name(const uint8_t *p,char name[33]){memcpy(name,p+64,32);name[32]=0;return memchr(p+64,0,32)!=NULL;}
static int psu(source *s,const uint8_t *p,size_t n){if(n<1536 || !(p[0]&0x20) || !(p[512]&0x20) || !(p[1024]&0x20))return 0;unsigned count=r32(p+4);if(count<2 || count>258)return 0;size_t at=1536;for(unsigned i=2;i<count;i++){if(n-at<512 || !(p[at]&0x10))return 0;char name[33];if(!entry_name(p+at,name))return 0;size_t length=r32(p+at+4);at+=512;if(length>n-at || !take(s,name,p+at,length))return 0;size_t rounded=(length+1023)&~(size_t)1023;if(rounded>n-at)return 0;at+=rounded;}return s->data && s->garage;}
/* Normalize 512-byte pages, skipping optional 16-byte ECC spare areas. FAT
   clusters are absolute; allocation chains and directory clusters are relative. */
typedef struct {uint8_t *p;size_t n;uint32_t clusters,offset,end,root;} card;
static const uint8_t *cluster(card *c,uint32_t i){return i<c->clusters && (size_t)i*1024<=c->n-1024?c->p+(size_t)i*1024:NULL;}
static uint32_t next_cluster(card *c,uint32_t index){unsigned indirect=index/256,double_indirect=indirect/256;if(double_indirect>=32)return 0;const uint8_t *p=cluster(c,r32(c->p+0x50+double_indirect*4));if(!p)return 0;p=cluster(c,r32(p+(indirect%256)*4));return p?r32(p+(index%256)*4):0;}
static uint8_t *chain(card *c,uint32_t start,size_t n){if(!n || n>4*1024*1024 || start>=c->end)return NULL;uint8_t *out=malloc(n),*seen=calloc(c->clusters,1);if(!out || !seen){free(out);free(seen);return NULL;}size_t at=0;uint32_t index=start;while(at<n){if(index>=c->end || seen[index])goto bad;seen[index]=1;const uint8_t *p=cluster(c,index+c->offset);if(!p)goto bad;size_t bytes=n-at<1024?n-at:1024;memcpy(out+at,p,bytes);at+=bytes;uint32_t next=next_cluster(c,index);if(!(next&0x80000000u) || (at<n && next==0xffffffffu) || (at==n && next!=0xffffffffu))goto bad;index=next&0x7fffffffu;}free(seen);return out;bad:free(out);free(seen);return NULL;}
static int card_directory(card *c,source *s,uint32_t start,unsigned entries,int root){if(entries<2 || entries>4096)return 0;uint8_t *p=chain(c,start,(size_t)entries*512);if(!p)return 0;int found=0;for(unsigned i=2;i<entries;i++){uint8_t *entry=p+i*512;if(!(entry[1]&0x80))continue;char name[33];if(!entry_name(entry,name))goto done;uint32_t n=r32(entry+4),first=r32(entry+16);if(root && (entry[0]&0x20)){/* Only inspect GT4 directories; never traverse unrelated card data. */int gt4=0;for(int k=0;k<6;k++)if(!strncmp(name,names[k],12))gt4=1;if(gt4){if(found){found=0;goto done;}found=card_directory(c,s,first,n,0);if(!found)goto done;}}else if(!root && (entry[0]&0x10) && (region(name)>=0 || !strcmp(name,"garage"))){uint8_t *file=chain(c,first,n);if(!file || !take(s,name,file,n)){free(file);found=0;goto done;}free(file);found=1;}}done:free(p);return found;}
static int memory_card(source *s,const uint8_t *raw,size_t n){if(n<512 || memcmp(raw,"Sony PS2 Memory Card Format ",27) || (raw[40]|raw[41]<<8)!=512 || (raw[42]|raw[43]<<8)!=2)return 0;card c={0};c.clusters=r32(raw+48);c.offset=r32(raw+52);c.end=r32(raw+56);c.root=r32(raw+60);if(c.clusters<64 || c.clusters>131072 || c.offset>=c.clusters || c.end>c.clusters-c.offset)return 0;size_t logical=(size_t)c.clusters*1024;int spare=n==logical?0:n==logical/512*528?16:-1;if(spare<0)return 0;c.p=malloc(logical);if(!c.p)return 0;c.n=logical;for(size_t i=0;i<logical/512;i++)memcpy(c.p+i*512,raw+i*(512+spare),512);const uint8_t *root=cluster(&c,c.root+c.offset);int ok=root && card_directory(&c,s,c.root,r32(root+4),1);free(c.p);return ok && s->data && s->garage;}
static int extracted(source *s,const char *path){char dir[1200],file[1250];if(strlen(path)>=sizeof dir)return 0;strcpy(dir,path);char *slash=strrchr(dir,'/'),*back=strrchr(dir,'\\');if(back && (!slash || back>slash))slash=back;const char *base=slash?slash+1:dir;int r=region(base);if(r>=0){s->data=read_file(path,512*1024,&s->data_size);s->region=r;if(slash)*slash=0;else strcpy(dir,".");}else {for(int i=0;i<6;i++){snprintf(file,sizeof file,"%s/%s",dir,names[i]);s->data=read_file(file,512*1024,&s->data_size);if(s->data){s->region=i;break;}}}if(!s->data)return 0;snprintf(file,sizeof file,"%s/garage",dir);s->garage=read_file(file,2*1024*1024,&s->garage_size);return s->garage!=NULL;}
static void integer(ah_vm *vm,ah_obj *o,const char *key,int64_t v){ah_set_member(vm,o,key,ah_int(v));}
static ah_obj *module(ah_vm *vm,ah_obj *o,const char *key){ah_obj *m=ah_module_new(vm,key,NULL,NULL,NULL);ah_set_member(vm,o,key,ah_objv(m));return m;}
static ah_obj *array(ah_vm *vm,ah_obj *o,const char *key){ah_obj *a=ah_array_new(vm);ah_set_member(vm,o,key,ah_objv(a));return a;}
static void blob(ah_vm *vm,ah_obj *o,const char *key,const uint8_t *p,size_t n){static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";ah_obj *a=array(vm,o,key);while(n){size_t bytes=n<49152?n:49152,encoded=((bytes+2)/3)*4;char *text=malloc(encoded+1);if(!text)return;size_t at=0;for(size_t i=0;i<bytes;i+=3){unsigned x=(unsigned)p[i]<<16;if(i+1<bytes)x|=(unsigned)p[i+1]<<8;if(i+2<bytes)x|=p[i+2];text[at++]=alphabet[x>>18];text[at++]=alphabet[(x>>12)&63];text[at++]=i+1<bytes?alphabet[(x>>6)&63]:'=';text[at++]=i+2<bytes?alphabet[x&63]:'=';}text[at]=0;ah_array_push(vm,a,ah_str(vm,text));free(text);p+=bytes;n-=bytes;}}
static void codes(ah_vm *vm,ah_obj *o,const char *key,const uint8_t *p,unsigned count){ah_obj *a=array(vm,o,key);for(unsigned i=0;i<count;i++,p+=8){ah_obj *entry=ah_module_new(vm,"code",NULL,NULL,NULL);integer(vm,entry,"code",(int32_t)r32(p));integer(vm,entry,"table",(int32_t)r32(p+4));ah_array_push(vm,a,ah_objv(entry));}}
static size_t align16(size_t n){return (n+15)&~(size_t)15;}
/* func_0043DEF0/0043E098 map serialized car+0x498 to live car+0x4a0.
   The serialized object omits the live eight-byte class header. */
static int profile(ah_vm *vm,ah_obj *game,const uint8_t *p,size_t n,const source *s,int old){int online=n==0x3a160;if(online!=(s->region>=4))return 0;char name[97];memcpy(name,p,32);name[32]=0;if(!gt4_save_valid_name(name)){
#ifdef _WIN32
 wchar_t w[40];char converted[97];int chars=MultiByteToWideChar(932,0,name,-1,w,40);if(chars && WideCharToMultiByte(CP_UTF8,0,w,-1,converted,97,NULL,NULL) && gt4_save_valid_name(converted))strcpy(name,converted);else
#endif
 strcpy(name,"PS2 Player");}ah_set_member(vm,game,"username",ah_str(vm,name));ah_obj *stats=module(vm,game,"statistics");integer(vm,stats,"a_spec_score",(int64_t)r64(p+56));integer(vm,stats,"prize_money",(int64_t)r64(p+64));integer(vm,stats,"prize_cars",r32(p+72));/* 0043B168: live +0x54..0x6c follows the omitted eight-byte header. */ah_obj *positions=array(vm,stats,"race_positions");for(unsigned i=0;i<7;i++)ah_array_push(vm,positions,ah_int(r32(p+76+4*i)));integer(vm,stats,"a_spec_distance",(int64_t)r64(p+104));integer(vm,stats,"b_spec_distance",(int64_t)r64(p+112));/* 0043B168: B-Spec matrices live +0x9c, serialized +0x94. */blob(vm,stats,"b_spec_experience_ps2",p+0x94,0x180);size_t at=online?0x260:0x220;ah_obj *calendar=module(vm,game,"calendar");uint32_t days=r32(p+at);if(days>1000000)return 0;integer(vm,calendar,"elapsed_days",days);uint32_t jd=r32(p+at+4+2922*16);if(jd<2453462)jd=2453462;if(jd>5373484)return 0;long l=(long)jd+1+68569,n400=4*l/146097;l=l-(146097*n400+3)/4;long y=4000*(l+1)/1461001;l=l-1461*y/4+31;long month=80*l/2447,day=l-2447*month/80;l=month/11;month=month+2-12*l;y=100*(n400-49)+y+l;int original_date[3];if(gt4_game_decode_calendar_day((int32_t)jd,original_date)){y=original_date[0];month=original_date[1];day=original_date[2];}ah_obj *today=array(vm,calendar,"today");ah_array_push(vm,today,ah_int(y));ah_array_push(vm,today,ah_int(month));ah_array_push(vm,today,ah_int(day));blob(vm,calendar,"events_ps2",p+at+4,2922*16);ah_obj *raw_events=array(vm,calendar,"original_events");for(unsigned i=0;i<days && i<2922;i++){ah_obj *event=ah_array_new(vm);for(unsigned j=0;j<16;j++)ah_array_push(vm,event,ah_int(p[at+4+i*16+j]));ah_array_push(vm,raw_events,ah_objv(event));}at=align16(at+4+2922*16+4);ah_obj *garage=module(vm,game,"garage"),*cars=array(vm,garage,"cars");size_t units=at;blob(vm,garage,"original_units_ps2",p+units,32000);ah_obj *unit_history=array(vm,garage,"original_unit_history");for(unsigned i=0;i<1000;i++)ah_array_push(vm,unit_history,ah_int((int64_t)r64(p+units+i*32+24)));for(unsigned i=0;i<1000;i++){const uint8_t *car=p+units+i*32;uint64_t flags=r64(car+8);if(!(flags&1))continue;ah_obj *entry=ah_module_new(vm,"car",NULL,NULL,NULL);integer(vm,entry,"slot",i);integer(vm,entry,"code",(int32_t)r32(car));integer(vm,entry,"table",(int32_t)r32(car+4));integer(vm,entry,"flags",(int64_t)flags);integer(vm,entry,"odometer",r32(car+20));integer(vm,entry,"acquisition_word",r32(car+16));integer(vm,entry,"variation",(flags>>2)&127);integer(vm,entry,"power",(flags>>9)&0x3ffff);integer(vm,entry,"weight",(flags>>32)&8191);integer(vm,entry,"history",(int64_t)r64(car+24));ah_array_push(vm,cars,ah_objv(entry));}at+=32000;int32_t riding=(int32_t)r32(p+at);uint32_t unique=r32(p+at+4);integer(vm,garage,"riding_index",riding);integer(vm,garage,"unique_id",unique);at+=28;blob(vm,garage,"current_car_ps2",p+at,online?0x500:0x4b8);if(!online){for(int i=0;i<ah_array_size(cars);i++){ah_obj *entry=ah_as_obj(ah_array_get(cars,i),AO_MODULE);ah_val slot;if(entry && ah_get_member(vm,entry,"slot",&slot) && slot.u.i==riding){ah_val full;if(ah_get_member(vm,garage,"current_car_ps2",&full))ah_set_member(vm,entry,"full_car_ps2",full);uint32_t setup=r32(p+at+0x488);if(setup<3){unsigned color=p[at+setup*0x178+0x18];integer(vm,entry,"color_index",color<128?(int)color:(int)color-256);}ah_obj *wear=array(vm,entry,"oil_state");for(unsigned j=0;j<32;j++)ah_array_push(vm,wear,ah_int(p[at+0x498+j]));break;}}}at=align16(at+(online?0x500:0x4b8));uint64_t money;if(s->garage_size!=(size_t)64+1000*(online?0x540:0x500) || !garage_header(s->garage,s->garage_size,unique,old,&money))return 0;integer(vm,game,"money",money);
 ah_obj *race=module(vm,game,"race_record"),*records=array(vm,race,"records");for(int i=0;i<768;i++){const uint8_t *record=p+at+i*4;ah_obj *entry=ah_module_new(vm,"race",NULL,NULL,NULL);integer(vm,entry,"original_record",r32(record));integer(vm,entry,"bits",record[0]|record[1]<<8);integer(vm,entry,"a_spec_score",record[2]);ah_array_push(vm,records,ah_objv(entry));}at=align16(at+768*4);
 ah_obj *course=module(vm,game,"course_record");blob(vm,course,"records_ps2",p+at,132*568);at=align16(at+132*568);ah_obj *license=module(vm,game,"license_record"),*licenses=array(vm,license,"records");for(int i=0;i<125;i++){ah_obj *tests=ah_array_new(vm);for(int j=0;j<10;j++){const uint8_t *entry=p+at+i*504+j*44;ah_obj *result=ah_module_new(vm,"license",NULL,NULL,NULL);integer(vm,result,"time",r32(entry));integer(vm,result,"date",r32(entry+4));integer(vm,result,"result",entry[8]);ah_array_push(vm,tests,ah_objv(result));}ah_array_push(vm,licenses,ah_objv(tests));}at=align16(at+125*504);
 ah_obj *available=module(vm,game,"available");codes(vm,available,"cars",p+at,1024);at+=8192;codes(vm,available,"courses",p+at,128);/* 0042EB20: serialized availability override after 128 keys. */integer(vm,available,"all",p[at+1024]);at=align16(at+1024+1);
 const char *fav[]={"favorite_course","favorite_car"};for(int i=0;i<2;i++){ah_obj *f=module(vm,game,fav[i]);integer(vm,f,"max",r32(p+at));integer(vm,f,"count",r32(p+at+4));codes(vm,f,"codes",p+at+8,32);at=align16(at+264);}blob(vm,module(vm,game,"presents"),"bits_ps2",p+at,0x74);at=align16(at+0x74);blob(vm,module(vm,game,"championship"),"context_ps2",p+at,0x88);at=align16(at+0x88);ah_obj *used=module(vm,game,"used_car");blob(vm,used,"bits_ps2",p+at,32);integer(vm,used,"week",r32(p+at+32));at=align16(at+36);if(at>n)return 0;
 ah_obj *original=module(vm,game,"ps2_import");integer(vm,original,"version",1);integer(vm,original,"region",s->region);ah_set_member(vm,original,"gamedata_name",ah_str(vm,names[s->region]));blob(vm,original,"gamedata",s->data,s->data_size);blob(vm,original,"garage",s->garage,s->garage_size);return 1;}
int gt4_ps2_read(const char *path,ah_vm *vm,ah_obj *game,char *error,size_t error_size){source s={0};size_t n;uint8_t *raw=read_file(path,70*1024*1024,&n);int ok=0;if(raw){if(n>=27 && !memcmp(raw,"Sony PS2 Memory Card Format ",27))ok=memory_card(&s,raw,n);else if(n>=1536 && (raw[0]&0x20))ok=psu(&s,raw,n);else ok=extracted(&s,path);}else ok=extracted(&s,path);free(raw);uint8_t *payload=NULL;size_t size=0;int old=0;if(!ok){snprintf(error,error_size,"Choose a complete GT4 .psu, .ps2 card, or extracted GAMEDATA with its garage file.");goto done;}if(!gt4_ps2_decode(s.data,s.data_size,&payload,&size,&old)){snprintf(error,error_size,"The GT4 save is damaged or its encryption is unsupported.");ok=0;goto done;}ok=profile(vm,game,payload,size,&s,old);if(!ok)snprintf(error,error_size,"The garage and GAMEDATA do not match, or this save layout is unsupported.");else if(error_size)error[0]=0;done:free(payload);free(s.data);free(s.garage);return ok;}

/* Media use the same bounded filesystem reader as the career importer.
   Original FileGT4FilmData 001D0698 writes F%04d, PhotoData 001D0B28 P%04d;
   replay directory data is decoded by 001CFFF8/0044D278. No source writes. */
static int media_name(const char *name){
 return name && *name && strlen(name)<33 && !strchr(name,'/') && !strchr(name,'\\') && !strstr(name,"..") && strcmp(name,"garage") && strcmp(name,"icon.sys") && strcmp(name,"icon0.ico") && strcmp(name,"thumbnail") && region(name)<0;
}
static int media_take(gt4_ps2_media_visitor visit,void *user,const char *name,const uint8_t *data,size_t n){
 if(!media_name(name))return 0;
 if(!n || n>4u*1024*1024)return -1;
 return visit?visit(user,name,data,n):1;
}
static int media_psu(const uint8_t *p,size_t n,gt4_ps2_media_visitor visit,void *user){
 if(n<1536 || !(p[0]&0x20) || !(p[512]&0x20) || !(p[1024]&0x20))return -1;
 unsigned entries=r32(p+4);if(entries<2 || entries>258)return -1;size_t at=1536;int count=0;
 for(unsigned i=2;i<entries;i++){
  if(n-at<512 || !(p[at]&0x10))return -1;char name[33];if(!entry_name(p+at,name))return -1;
  size_t length=r32(p+at+4);at+=512;size_t rounded=(length+1023)&~(size_t)1023;
  if(rounded>n-at)return -1;int taken=media_take(visit,user,name,p+at,length);if(taken<0)return -1;count+=taken;at+=rounded;
 }return count;
}
static int media_directory(card *c,uint32_t start,unsigned entries,int root,gt4_ps2_media_visitor visit,void *user){
 if(entries<2 || entries>4096)return -1;uint8_t *p=chain(c,start,(size_t)entries*512);if(!p)return -1;int count=0;
 for(unsigned i=2;i<entries;i++){
  uint8_t *e=p+i*512;if(!(e[1]&0x80))continue;char name[33];if(!entry_name(e,name)){count=-1;break;}
  uint32_t length=r32(e+4),first=r32(e+16);
  if(root && (e[0]&0x20)){
   int gt4=0;for(unsigned k=0;k<6;k++)if(!strncmp(name,names[k],12))gt4=1;
   if(gt4){int got=media_directory(c,first,length,0,visit,user);if(got<0){count=-1;break;}count+=got;}
  }else if(!root && (e[0]&0x10) && media_name(name)){
   uint8_t *data=chain(c,first,length);if(!data){count=-1;break;}
   int got=media_take(visit,user,name,data,length);free(data);if(got<0){count=-1;break;}count+=got;
  }
 }free(p);return count;
}
static int media_card(const uint8_t *raw,size_t n,gt4_ps2_media_visitor visit,void *user){
 if(n<512 || memcmp(raw,"Sony PS2 Memory Card Format ",27) || (raw[40]|raw[41]<<8)!=512 || (raw[42]|raw[43]<<8)!=2)return -1;
 card c={0};c.clusters=r32(raw+48);c.offset=r32(raw+52);c.end=r32(raw+56);c.root=r32(raw+60);
 if(c.clusters<64 || c.clusters>131072 || c.offset>=c.clusters || c.end>c.clusters-c.offset)return -1;
 size_t logical=(size_t)c.clusters*1024;int spare=n==logical?0:n==logical/512*528?16:-1;if(spare<0)return -1;
 c.p=malloc(logical);if(!c.p)return -1;c.n=logical;
 for(size_t i=0;i<logical/512;i++)memcpy(c.p+i*512,raw+i*(512+spare),512);
 const uint8_t *entry=cluster(&c,c.root+c.offset);int count=entry?media_directory(&c,c.root,r32(entry+4),1,visit,user):-1;free(c.p);return count;
}
int gt4_ps2_visit_media(const char *path,gt4_ps2_media_visitor visit,void *user,char *error,size_t capacity){
 size_t n=0;uint8_t *data=read_file(path,70u*1024*1024,&n);int count=-1;
 if(data && n>=27 && !memcmp(data,"Sony PS2 Memory Card Format ",27)){
  count=media_card(data,n,NULL,NULL);if(count>=0 && visit)count=media_card(data,n,visit,user);
 }else if(data && n>=1536 && (data[0]&0x20) && (data[512]&0x20) && (data[1024]&0x20)){
  count=media_psu(data,n,NULL,NULL);if(count>=0 && visit)count=media_psu(data,n,visit,user);
 }else if(data){const char *base=strrchr(path,'/'),*back=strrchr(path,'\\');if(back && (!base || back>base))base=back;count=media_take(visit,user,base?base+1:path,data,n);}
 else {
  plat_directory_entry files[256];int total=plat_directory_list(path,"",files,256);count=0;
  for(int i=0;i<total;i++)if(media_name(files[i].name)){
   char file[1200];snprintf(file,sizeof file,"%s/%s",path,files[i].name);size_t size;uint8_t *raw=read_file(file,4u*1024*1024,&size);
   int got=raw?media_take(visit,user,files[i].name,raw,size):-1;free(raw);if(got<0){count=-1;break;}count+=got;
  }
 }
 free(data);if(error && capacity)snprintf(error,capacity,"%s",count<0?"Media archive is damaged or a file could not be imported.":count==0?"No GT4 replay, photo or film files found.":"");return count;
}
