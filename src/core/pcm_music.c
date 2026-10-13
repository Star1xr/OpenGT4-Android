/* Original PCM streams, distinct from the /music MIDI groups.
   00460220 resolves the name; 0045FFF8 +0x48 chooses random % group count,
   +0x98 uses 24-byte ADS v2 records. Source is /bgm/us/ads.inf + .ads. */
#include "pcm_music.h"
#include "adhoc/adhoc_vm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
struct gt4_pcm_music{int16_t *samples;size_t frames;uint64_t position;unsigned rate;int once;uint32_t *cues;unsigned ncues,cue_rate;char title[96],artist[96];};
static uint32_t le32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int span(size_t size,size_t off,size_t count){return off<=size && count<=size-off;}
static int clip(int value){return value<-32768?-32768:value>32767?32767:value;}
static unsigned playlist_count(const gt4_music_playlist *p){return le32(p->bytes);}
static uint8_t *playlist_entry(gt4_music_playlist *p,unsigned i){return p->bytes+16+i*16;}
int gt4_music_playlist_uses_preferences(unsigned playback_mode,int override)
{
    /* 0033C180 +0..14 and 0033C278 +0..14: normal races or override;
       replay modes >=2 otherwise keep the independent world jukebox. */
    return playback_mode<2 || override!=0;
}
static void playlist_reset(gt4_music_playlist *p)
{
    /* 00437E50 +0x0..+0x40: disabled entries are already consumed. */
    p->bytes[7]=1;
    for(unsigned i=0;i<playlist_count(p);i++) {
        uint8_t *e=playlist_entry(p,i);e[8]=!e[9];
        if(!e[8])p->bytes[7]=0;
    }
}
float gt4_pcm_music_random_unit(uint32_t *state)
{
    /* 00578598 +0x10..+0x48: original integer-to-float bit construction. */
    uint32_t n=gt4_pcm_music_random(state,0),bits=0;
    if(n) {
        int leading=-1;
        if(!(n&0x80000000u)) {leading=0;uint32_t bit=0x40000000u;while(!(n&bit)){leading++;bit>>=1;}}
        unsigned exponent=(unsigned)(125-leading);
        bits=(exponent<<23)|((n<<((0u-exponent)&31))>>9);
    }
    float f;memcpy(&f,&bits,4);return f;
}
void gt4_music_playlist_order(gt4_music_playlist *p,int shuffle)
{
    unsigned count=playlist_count(p),ordinal=0;
    if(count>128)return;
    /* 00437E98/00437EF0: mode, consumed flags, cursor and ordinals. */
    p->bytes[6]=(uint8_t)shuffle;playlist_reset(p);p->bytes[4]=p->bytes[5]=0;
    for(unsigned i=0;i<count;i++)playlist_entry(p,i)[10]=255;
    if(!shuffle) {
        for(unsigned i=0;i<count;i++)if(playlist_entry(p,i)[9])playlist_entry(p,i)[10]=(uint8_t)ordinal++;
        return;
    }
    /* 00437F58 +0x58..+0x1D8: count draws, forward probing, then seed draw.
       Disabled entries are still assigned in the exhausted-probe fallback. */
    uint32_t rng=p->bytes[8]|(uint32_t)p->bytes[9]<<8;
    for(unsigned i=0;i<count;i++) {
        unsigned at=(unsigned)(gt4_pcm_music_random_unit(&rng)*(float)count);
        for(unsigned probe=0;probe<count;probe++) {
            uint8_t *e=playlist_entry(p,at);
            if(e[9] && (int8_t)e[10]<0)break;
            if(++at>=count)at=0;
        }
        uint8_t *e=playlist_entry(p,at);
        if((int8_t)e[10]<0)e[10]=(uint8_t)ordinal++;
    }
    unsigned next=(unsigned)(gt4_pcm_music_random_unit(&rng)*32768.0f);
    p->bytes[8]=(uint8_t)next;p->bytes[9]=(uint8_t)(next>>8);
}
int gt4_music_playlist_next(gt4_music_playlist *p)
{
    /* 00438170: cycle the authored ordinals; wrap resets usage, not order. */
    unsigned count=playlist_count(p),enabled=0;
    if(count>128)return -1;
    for(unsigned i=0;i<count;i++)enabled+=playlist_entry(p,i)[9]!=0;
    if(!enabled){p->bytes[7]=1;return -1;}
    unsigned cursor=(p->bytes[4]|(unsigned)p->bytes[5]<<8)%enabled;
    p->bytes[4]=(uint8_t)cursor;p->bytes[5]=(uint8_t)(cursor>>8);
    if(p->bytes[7])return -1;
    for(unsigned attempt=0;attempt<enabled;attempt++)for(unsigned i=0;i<count;i++) {
        uint8_t *e=playlist_entry(p,i);
        if((int8_t)e[10]!=(int)cursor)continue;
        int selected=!e[8];if(selected)e[8]=1;
        if(++cursor>=enabled){cursor=0;playlist_reset(p);}
        if(selected){p->bytes[4]=(uint8_t)cursor;p->bytes[5]=(uint8_t)(cursor>>8);return (int)i;}
    }
    return -1;
}
uint32_t gt4_pcm_music_random(uint32_t *state,uint32_t clock)
{
    /* 00579330 +0x4..+0x24; 00579358 +0x2C adds platform clock. */
    uint32_t n=*state*17u+17u;*state=n;return (n^((n<<16)|(n>>16)))+clock;
}
static int group_records(const uint8_t *data,size_t bytes,const char *group,
                          unsigned *count,uint32_t *offset)
{
    /* 00460220: the complete name/index table at 006238C0, eight-byte
       records. PCM race music is group 2, separate from MIDI /music. */
    static const char *names[]={"start","goal","racing","default_quick","result",
        "torophy_solo","torophy_champ","get_car","replay_theater","license_start",
        "license_goal","license_gold","license_silver","license_bronze","license_failed",
        "license_torophy","autodemo","champ_start","champ_goal","timeattack_start",
        "timeattack_goal","coffee_start","coffee_goal","etc_start","etc_goal","last_start","last_goal"};
    if(!group || bytes<20 || memcmp(data,"MADS",4) || le32(data+4)!=2)return 0;
    unsigned index=0;while(index<sizeof names/sizeof *names && strcmp(names[index],group))index++;
    if(index==sizeof names/sizeof *names || index>=le32(data+12) || !span(bytes,20+index*8,8))return 0;
    *offset=le32(data+20+index*8);*count=le32(data+24+index*8);
    return *count && *count<=256 && span(bytes,*offset,(size_t)*count*24);
}
unsigned gt4_pcm_music_count(gt4_vol *vol,const char *group)
{
    size_t bytes;uint8_t *data=gt4_vol_read(vol,"/bgm/us/ads.inf",&bytes);
    unsigned count=0;uint32_t offset;if(data && !group_records(data,bytes,group,&count,&offset))count=0;
    free(data);return count;
}
unsigned gt4_pcm_music_race_sequences(gt4_vol *vol,unsigned *sequences,unsigned capacity)
{
    size_t bytes=0;uint8_t *data=gt4_vol_read(vol,"/bgm/us/ads.inf",&bytes);
    unsigned count=0,used=0;uint32_t offset;
    if(!data || !sequences || !group_records(data,bytes,"racing",&count,&offset))goto done;
    /* 00437B10 +0xA8..+0x110: build the default race jukebox from
       enum 12 (ADS group 2), excluding the filename prefix at 006A5E00,
       "photo_". 00336DE8 +0x24 calls this initializer for the race world.
       The raw group also contains Photo Travel; its count is not a playlist. */
    for(unsigned i=0;i<count;i++) {
        uint32_t name=le32(data+offset+i*24+4);
        if(name>=bytes || !memchr(data+name,0,bytes-name)){used=0;goto done;}
        if(!strncmp((const char *)data+name,"photo_",6))continue;
        if(used==capacity){used=0;goto done;}
        sequences[used++]=i;
    }
done:free(data);return used;
}
static uint64_t music_key(const char *name)
{
    /* 0044FEC0 +0x28..+0x74: sum, rotate left seven, add each byte. */
    uint64_t key=0;const unsigned char *s=(const unsigned char *)name;
    while(*s)key+=*s++;
    key&=65535;
    for(s=(const unsigned char *)name;*s;s++)key=((key<<7)|(key>>57))+*s;
    return key;
}
int gt4_music_playlist_init(gt4_music_playlist *p,gt4_vol *vol,uint32_t seed)
{
    unsigned sequences[128],count=gt4_pcm_music_race_sequences(vol,sequences,128),raw_count;
    size_t bytes=0;uint8_t *data=gt4_vol_read(vol,"/bgm/us/ads.inf",&bytes);uint32_t offset;
    if(!count || !data || !group_records(data,bytes,"racing",&raw_count,&offset)){free(data);return 0;}
    /* 00437B10: seed's low half, enabled entries and original index. */
    memset(p,0,sizeof *p);p->bytes[0]=(uint8_t)count;
    for(unsigned i=0;i<128;i++)playlist_entry(p,i)[10]=playlist_entry(p,i)[11]=255;
    p->bytes[8]=(uint8_t)seed;p->bytes[9]=(uint8_t)(seed>>8);
    for(unsigned i=0;i<count;i++) {
        uint8_t *e=playlist_entry(p,i);uint64_t key=music_key((const char *)data+le32(data+offset+sequences[i]*24+4));
        for(unsigned b=0;b<8;b++)e[b]=(uint8_t)(key>>(b*8));
        e[9]=1;e[11]=(uint8_t)i;
    }
    free(data);gt4_music_playlist_order(p,1);return 1;
}
static int music_region(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *args,ah_val *ret)
{(void)self;(void)data;(void)argc;(void)args;*ret=ah_str(vm,"us");return 0;}
static int authored_playlist_default(gt4_music_playlist *p,gt4_vol *vol,uint32_t seed,int slide)
{
    /* 004379F0 constructs the option's full 65-entry library. The disc's
       /config/config-main.adc, bgmplaylist.h init_raceplaylist/apply, selects its
       regional defaults. Execute those original functions, not a guessed
       track list. 0019E900 +154..168 maps [original,used,enabled,order]. */
    size_t bytes=0;uint8_t *data=gt4_vol_read(vol,"/bgm/us/ads.inf",&bytes);
    unsigned count;uint32_t offset;
    if(!data || !group_records(data,bytes,"racing",&count,&offset) || count>128){free(data);return 0;}
    memset(p,0,sizeof *p);p->bytes[0]=(uint8_t)count;
    p->bytes[8]=(uint8_t)seed;p->bytes[9]=(uint8_t)(seed>>8);
    for(unsigned i=0;i<128;i++) {
        uint8_t *e=playlist_entry(p,i);e[9]=1;e[10]=e[11]=255;
        if(i<count){uint32_t name=le32(data+offset+i*24+4);
            if(name>=bytes || !memchr(data+name,0,bytes-name)){free(data);return 0;}
            uint64_t key=music_key((const char *)data+name);
            for(unsigned b=0;b<8;b++)e[b]=(uint8_t)(key>>(8*b));e[10]=e[11]=(uint8_t)i;}
    }
    free(data);
    data=gt4_vol_read(vol,"/config/config-main.adc",&bytes);char err[256];
    ah_code *code=data?ah_load(data,bytes,err,sizeof err):NULL;free(data);if(!code)return 0;
    ah_code *part=calloc(1,sizeof *part);if(!part){ah_free(code);return 0;}
    part->source=malloc(20);part->ins=calloc(5,sizeof *part->ins);
    if(!part->source || !part->ins){ah_free(part);ah_free(code);return 0;}
    strcpy(part->source,"bgmplaylist.h");part->stack_size=30;part->slots=130;part->local_count=130;
    for(int i=0;i<code->count;i++)if(code->ins[i].op==AH_FUNCTION_DEFINE && code->ins[i].s &&
        (!strcmp(code->ins[i].s,slide?"init_slideplaylist":"init_raceplaylist") || !strcmp(code->ins[i].s,"apply") ||
         !strcmp(code->ins[i].s,"make_range_not_repeat") || !strcmp(code->ins[i].s,"make_range"))) {
        if(part->count>=4){ah_free(part);ah_free(code);return 0;}
        part->ins[part->count++]=code->ins[i];memset(code->ins+i,0,sizeof *code->ins);
    }
    ah_free(code);if(part->count<2){ah_free(part);return 0;}
    part->ins[part->count].op=AH_SET_STATE_OLD;part->ins[part->count++].i=1;
    ah_vm *vm=ah_vm_new();ah_obj *root=ah_root(vm);
    ah_obj *main=ah_module_new(vm,"main",root,NULL,NULL),*game=ah_module_new(vm,"game",main,NULL,NULL);
    ah_obj *option=ah_module_new(vm,"option",game,NULL,NULL),*playlist=ah_module_new(vm,"playlist",option,NULL,NULL);
    ah_obj *menu=ah_module_new(vm,"menu",main,NULL,NULL),*system=ah_module_new(vm,"MSystem",menu,NULL,NULL);
    ah_set_member(vm,root,"main",ah_objv(main));ah_set_member(vm,main,"game",ah_objv(game));
    ah_set_member(vm,main,"menu",ah_objv(menu));ah_set_member(vm,menu,"MSystem",ah_objv(system));
    ah_set_member(vm,system,"GetRegionCode",ah_native(vm,"GetRegionCode",music_region,NULL));
    ah_set_member(vm,game,"option",ah_objv(option));ah_set_member(vm,option,slide?"slide_playlist":"bgm_playlist",ah_objv(playlist));
    ah_obj *rows=ah_array_new(vm);for(unsigned i=0;i<count;i++)ah_array_push(vm,rows,ah_nil());
    ah_set_member(vm,playlist,"play_data",ah_objv(rows));ah_val ret,value;
    int ok=ah_run_script(vm,part)==0 && ah_call_member(vm,root,slide?"init_slideplaylist":"init_raceplaylist",0,NULL,&ret)==1;
    if(ok)ok=ah_get_member(vm,playlist,"play_data",&value) && (rows=ah_as_obj(value,AO_ARRAY)) && ah_array_size(rows)==(int)count;
    for(unsigned i=0;ok && i<count;i++) {
        ah_obj *row=ah_as_obj(ah_array_get(rows,(int)i),AO_ARRAY);if(!row || ah_array_size(row)!=4){ok=0;break;}
        uint8_t *e=playlist_entry(p,i);
        e[11]=(uint8_t)ah_num(ah_array_get(row,0));e[8]=(uint8_t)ah_num(ah_array_get(row,1));
        e[9]=(uint8_t)ah_num(ah_array_get(row,2));e[10]=(uint8_t)ah_num(ah_array_get(row,3));
    }
    ah_vm_free(vm);return ok;
}
int gt4_music_playlist_default(gt4_music_playlist *p,gt4_vol *vol,uint32_t seed)
{return authored_playlist_default(p,vol,seed,0);}

static int music_list_sized(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *args,ah_val *ret)
{
    (void)self;(void)argc;(void)args;ah_obj *list=ah_array_new(vm);
    for(uintptr_t i=0;i<(uintptr_t)data;i++)ah_array_push(vm,list,ah_nil());
    *ret=ah_objv(list);return 0;
}
/* config-main.adc bgmplaylist.h init_baseplaylist: the Jukebox library's
   order (main.default_library_playlist), the racing group's indices as
   the region's original function builds them */
unsigned gt4_music_library_default(gt4_vol *vol,unsigned *out,unsigned capacity)
{
    unsigned count=gt4_pcm_music_count(vol,"racing");
    size_t bytes=0;uint8_t *data=count?gt4_vol_read(vol,"/config/config-main.adc",&bytes):NULL;char err[256];
    ah_code *code=data?ah_load(data,bytes,err,sizeof err):NULL;free(data);if(!code)return 0;
    ah_code *part=calloc(1,sizeof *part);if(!part){ah_free(code);return 0;}
    part->source=malloc(20);part->ins=calloc(5,sizeof *part->ins);
    if(!part->source || !part->ins){ah_free(part);ah_free(code);return 0;}
    strcpy(part->source,"bgmplaylist.h");part->stack_size=30;part->slots=130;part->local_count=130;
    for(int i=0;i<code->count;i++)if(code->ins[i].op==AH_FUNCTION_DEFINE && code->ins[i].s &&
        (!strcmp(code->ins[i].s,"init_baseplaylist") || !strcmp(code->ins[i].s,"make_range") || !strcmp(code->ins[i].s,"make_range_not_repeat"))) {
        if(part->count>=4){ah_free(part);ah_free(code);return 0;}
        part->ins[part->count++]=code->ins[i];memset(code->ins+i,0,sizeof *code->ins);
    }
    ah_free(code);if(part->count<1){ah_free(part);return 0;}
    part->ins[part->count].op=AH_SET_STATE_OLD;part->ins[part->count++].i=1;
    ah_vm *vm=ah_vm_new();ah_obj *root=ah_root(vm);
    ah_obj *main=ah_module_new(vm,"main",root,NULL,NULL),*game=ah_module_new(vm,"game",main,NULL,NULL);
    ah_obj *menu=ah_module_new(vm,"menu",main,NULL,NULL),*system=ah_module_new(vm,"MSystem",menu,NULL,NULL);
    ah_set_member(vm,root,"main",ah_objv(main));ah_set_member(vm,main,"game",ah_objv(game));
    ah_set_member(vm,main,"menu",ah_objv(menu));ah_set_member(vm,menu,"MSystem",ah_objv(system));
    ah_set_member(vm,system,"GetRegionCode",ah_native(vm,"GetRegionCode",music_region,NULL));
    ah_set_member(vm,game,"getAllMusicList",ah_native(vm,"getAllMusicList",music_list_sized,(void *)(uintptr_t)count));
    static const char *const statics[]={"default_library_playlist","default_race_playlist","default_slide_playlist"};
    for(int i=0;i<3;i++)ah_set_member(vm,root,statics[i],ah_objv(ah_array_new(vm)));
    ah_val ret,value;ah_obj *list;unsigned n=0;
    if(ah_run_script(vm,part)==0 && ah_call_member(vm,root,"init_baseplaylist",0,NULL,&ret)==1 &&
       ah_get_member(vm,root,"default_library_playlist",&value) && (list=ah_as_obj(value,AO_ARRAY)))
        for(int i=0;i<ah_array_size(list) && n<capacity;i++){double v=ah_num(ah_array_get(list,i));if(v>=0 && v<count)out[n++]=(unsigned)v;}
    ah_vm_free(vm);return n;
}
int gt4_music_playlist_slide(gt4_music_playlist *p,gt4_vol *vol,uint32_t seed)
{return authored_playlist_default(p,vol,seed,1);}
int gt4_pcm_music_info(gt4_vol *vol,unsigned index,char out[3][128])
{
 /* 00165798 -> 00460C18: title, artist, album, original racing group index. */
 size_t bytes=0;uint8_t *data=gt4_vol_read(vol,"/bgm/us/ads.inf",&bytes);unsigned count;uint32_t offset;
 if(!data || !group_records(data,bytes,"racing",&count,&offset) || index>=count){free(data);return 0;}
 for(int k=0;k<3;k++){uint32_t p=le32(data+offset+index*24+8+k*4);out[k][0]=0;
  if(p && p<bytes && memchr(data+p,0,bytes-p))snprintf(out[k],128,"%s",(const char *)data+p);
 }
 free(data);return 1;
}
void gt4_music_fade_start(float *remaining,float *duration,float seconds,float scale)
{
    /* 003C0F40 +0..40: ignore an already active transition. */
    if(*remaining>=0)return;
    *remaining=seconds;
    if(seconds<=0){uint32_t bits=0x3c888888;memcpy(remaining,&bits,4);}
    *duration=scale;
}
int gt4_music_fade_step(float *remaining,float dt)
{
    /* 003C04E8 +CC..FC: event 1 at the first crossing of zero. */
    if(*remaining>0){*remaining-=dt;if(*remaining<=0){*remaining=0;return 1;}}
    return 0;
}
float gt4_music_fade_gain(float remaining,float duration)
{
    /* 003C0F88 +14 / 003C0FB0 +2C..34: inactive is 1, divide and min. */
    float level=(remaining<0?FLT_MAX:remaining)/duration;
    return level<1.0f?level:1.0f;
}
void gt4_music_replay_fade(float *remaining,float *duration,int32_t frames_left,unsigned mode,int timed_exit,int looping)
{
    /* 00389B08 +0x134..0x1B4: modes 2/3, inactive transition,
       no timed exit or replay loop; last 60 frames at 3C888888h seconds. */
    if(mode-2>=2 || *remaining>=0 || timed_exit || looping || frames_left>60)return;
    uint32_t bits=0x3c888888;float dt;memcpy(&dt,&bits,4);
    gt4_music_fade_start(remaining,duration,(float)frames_left*dt,1.0f);
}
int gt4_music_gain_q15(float gain)
{
    /* 00548060 +0x10..0x50: clamp [-1,1], multiply 32768, cvt.w.s. */
    if(gain < -1.0f)gain=-1.0f;if(gain>1.0f)gain=1.0f;
    return (int)(gain*32768.0f);
}
int gt4_music_playlist_sequence(gt4_vol *vol,const gt4_music_playlist *p,unsigned entry)
{
    if(entry>=playlist_count(p) || entry>=128)return -1;
    const uint8_t *e=p->bytes+16+entry*16;uint64_t key=0;
    for(unsigned b=0;b<8;b++)key|=(uint64_t)e[b]<<(b*8);
    size_t bytes=0;uint8_t *data=gt4_vol_read(vol,"/bgm/us/ads.inf",&bytes);
    unsigned count;uint32_t offset;int found=-1;
    /* 00462288 resolves the jukebox ID back into the ADS racing group. */
    if(data && group_records(data,bytes,"racing",&count,&offset))for(unsigned i=0;i<count;i++) {
        uint32_t name=le32(data+offset+i*24+4);
        if(name>=bytes || !memchr(data+name,0,bytes-name))break;
        if(music_key((const char *)data+name)==key){found=(int)i;break;}
    }
    free(data);return found;
}
gt4_pcm_music *gt4_pcm_music_open(gt4_vol *vol,const char *group,unsigned sequence)
{
    size_t bytes;uint8_t *data=gt4_vol_read(vol,"/bgm/us/ads.inf",&bytes);
    unsigned count;uint32_t offset;gt4_pcm_music *m=NULL;
    if(!data || !group_records(data,bytes,group,&count,&offset))goto done;
    uint32_t name=le32(data+offset+(sequence%count)*24+4);
    if(name>=bytes || !memchr(data+name,0,bytes-name))goto done;
    const char *file=(const char *)data+name;
    if(strlen(file)>96 || strchr(file,'/') || strchr(file,'\\') || strstr(file,".."))goto done;
    char path[128];snprintf(path,sizeof path,"/bgm/us/%s",file);
    size_t stream_bytes;uint8_t *stream=gt4_vol_read(vol,path,&stream_bytes);
    if(stream){m=gt4_pcm_music_decode(stream,stream_bytes);free(stream);}
    if(m) {   /* the record's +8 title and +0xC artist (func_003A22A0 reads them) */
        const uint8_t *r=data+offset+(sequence%count)*24;
        uint32_t t=le32(r+8),a=le32(r+12);
        if(t && t<bytes && memchr(data+t,0,bytes-t))snprintf(m->title,sizeof m->title,"%s",(const char *)data+t);
        if(a && a<bytes && memchr(data+a,0,bytes-a))snprintf(m->artist,sizeof m->artist,"%s",(const char *)data+a);
    }
    {   /* the stream's marks beside it (.adm, read with it by 0045FFF8 into
           D_0062399C, relocated by 0045FD48): "BMRK", +8 rate, +0xC
           length, +0x10 32 counts, +0x90 32 lists of sample positions */
        size_t n=strlen(path),adm_bytes;
        if(m && n>4 && !strcmp(path+n-4,".ads")) {
            path[n-1]='m';
            uint8_t *adm=gt4_vol_read(vol,path,&adm_bytes);
            if(adm && adm_bytes>=0x110 && !memcmp(adm,"BMRK",4)) {
                uint32_t count=le32(adm+0x10),at=le32(adm+0x90)-le32(adm+4),rate=le32(adm+8);
                if(count && rate && span(adm_bytes,at,(size_t)count*4) && (m->cues=malloc((size_t)count*4))) {
                    for(uint32_t i=0;i<count;i++)m->cues[i]=le32(adm+at+i*4);
                    m->ncues=count;m->cue_rate=rate;
                }
            }
            free(adm);
        }
    }
done:free(data);return m;
}
gt4_pcm_music *gt4_pcm_music_decode(const uint8_t *data,size_t bytes)
{
    if(!data || bytes<40 || memcmp(data,"SShd",4) || le32(data+4)!=24 ||
       le32(data+8)!=16 || le32(data+16)!=2 || memcmp(data+32,"SSbd",4))return NULL;
    unsigned rate=le32(data+12),interleave=le32(data+20);size_t payload=le32(data+36);
    if(rate<8000 || rate>48000 || !interleave || interleave%16 || interleave>65536 ||
       !payload || !span(bytes,40,payload) || payload%((size_t)interleave*2))return NULL;
    size_t frames=payload/32*28;if(frames>(size_t)rate*600)return NULL;
    gt4_pcm_music *m=calloc(1,sizeof *m);if(!m)return NULL;
    m->samples=malloc(frames*2*sizeof *m->samples);if(!m->samples){free(m);return NULL;}
    m->frames=frames;m->rate=rate;
    /* SShd type 0x10: SPU ADPCM; same coefficients as sound.c. Channel
       predictor histories persist across 0x400-byte interleaves on this disc. */
    static const int co[5][2]={{0,0},{60,0},{115,-52},{98,-55},{122,-60}};
    int history[2][2]={{0}};size_t frame=0;
    for(size_t group=40;group<40+payload;group+=(size_t)interleave*2){
        for(unsigned ch=0;ch<2;ch++){
            size_t sample=frame;
            for(unsigned at=0;at<interleave;at+=16){
                const uint8_t *block=data+group+ch*interleave+at;
                unsigned shift=block[0]&15,filter=block[0]>>4;
                if(shift>12 || filter>4 || block[1]>7){gt4_pcm_music_close(m);return NULL;}
                for(unsigned i=0;i<28;i++){
                    int nibble=(block[2+i/2]>>((i&1)*4))&15;if(nibble>=8)nibble-=16;
                    int value=clip((nibble*4096>>shift)+((history[ch][0]*co[filter][0]+history[ch][1]*co[filter][1]+32)>>6));
                    m->samples[sample++*2+ch]=(int16_t)value;
                    history[ch][1]=history[ch][0];history[ch][0]=value;
                }
            }
        }
        frame+=interleave/16*28;
    }
    return m;
}
void gt4_pcm_music_render(int16_t *out,int frames,gt4_pcm_music *m)
{
    if(!m){memset(out,0,(size_t)frames*4);return;}
    uint64_t length=(uint64_t)m->frames*48000;
    for(int i=0;i<frames;i++){
        if(m->position>=length){memset(out+i*2,0,(size_t)(frames-i)*4);return;}
        size_t sample=(size_t)(m->position/48000),next=(sample+1)%m->frames;
        if(m->once && sample+1==m->frames)next=sample;
        unsigned fraction=(unsigned)(m->position%48000);
        /* A: linear conversion from the file's 44100 Hz to the PC mixer's
           48000 Hz; the original SPU2 interpolation has not been compared. */
        for(unsigned ch=0;ch<2;ch++){
            int a=m->samples[sample*2+ch],b=m->samples[next*2+ch];
            out[i*2+ch]=(int16_t)(a+(int64_t)(b-a)*fraction/48000);
        }
        m->position+=m->rate;if(!m->once)m->position%=length;
    }
}
void gt4_pcm_music_rewind(gt4_pcm_music *m){if(m)m->position=0;}
void gt4_pcm_music_once(gt4_pcm_music *m,int once){if(m)m->once=!!once;}
int gt4_pcm_music_ended(const gt4_pcm_music *m){return m && m->position>=(uint64_t)m->frames*48000;}
void gt4_pcm_music_close(gt4_pcm_music *m){if(m){free(m->samples);free(m->cues);free(m);}}
const char *gt4_pcm_music_title(const gt4_pcm_music *m,const char **artist)
{
    if(artist)*artist=m?m->artist:"";
    return m?m->title:"";
}
double gt4_pcm_music_seconds(const gt4_pcm_music *m){return m && m->rate?(double)m->position/48000/m->rate:0;}
unsigned gt4_pcm_music_cues(const gt4_pcm_music *m,const uint32_t **samples,unsigned *rate)
{
    if(samples)*samples=m?m->cues:NULL;
    if(rate)*rate=m?m->cue_rate:0;
    return m?m->ncues:0;
}

/* 0033D170, its first list: the time from now (and a 30th of a second)
   to the next mark, NaN when none is left (D_0069F3BC); passing one, or
   a new stream, sets *passed. One mark a frame at most. */
float gt4_music_cue_step(gt4_music_cue *c,const uint32_t *cues,unsigned n,unsigned rate,float now,int track,int *passed)
{
    float t=NAN;
    *passed=0;now+=1.f/30;
    if(cues && n && rate) {
        if(c->cursor<(int)n) {
            t=(float)cues[c->cursor]/(float)rate;
            if(t<now) {
                *passed=1;
                t=++c->cursor<(int)n?(float)cues[c->cursor]/(float)rate:NAN;
            }
        }
        if(track!=c->track){*passed=1;c->cursor=0;t=(float)cues[0]/(float)rate;}
    }
    c->track=track;
    return t-now;
}
unsigned gt4_pcm_music_rate(const gt4_pcm_music *m){return m?m->rate:0;}
size_t gt4_pcm_music_frames(const gt4_pcm_music *m){return m?m->frames:0;}
const int16_t *gt4_pcm_music_samples(const gt4_pcm_music *m){return m?m->samples:NULL;}

/* 0033D510 +0x1C..0x30: disabled streams return +0; otherwise
   multiply stream base (+1BC) by the external gain (+1C0). */
float gt4_music_stream_gain(float base,float external,int enabled)
{
    return enabled?base*external:0.0f;
}
