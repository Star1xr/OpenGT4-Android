/* Original on-disc demonstrations, not a substitute controller/AI.
   Base: 0044D278 -> 00579E00. GT4M: 00388E38, commands: 00346BB8. */
#include "disc_replay.h"
#include "save_ps2.h"
#include "inflate.h"
#include "specdb.h"
#include "adhoc/adhoc_code.h"
#include "../../third_party/ps2float/ps2float.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gt4_disc_replay { gt4_vol *vol;uint32_t table[256]; };
static uint32_t word(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put16(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static uint32_t crc32(const uint8_t *p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;i++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}return ~c;}
static unsigned reverse8(unsigned x){x=((x&0x55)<<1)|((x>>1)&0x55);x=((x&0x33)<<2)|((x>>2)&0x33);return ((x&15)<<4)|(x>>4);}
int gt4_disc_replay_base(uint8_t *p,size_t n,uint32_t key,const uint32_t table[256])
{
    if(!p || !n || !table)return 0;
    uint32_t state=(key&0xffffff)|0x3f000000;
    /* 00579E00 +40..B4: update from the encrypted byte, with a separately
       rounded multiply and divide. Host IEEE arithmetic loses the cipher. */
    for(size_t i=0;i<n;i++){
        unsigned b=p[i],a=state&255,c=(state>>8)&255,d=(state>>16)&255;
        p[i]=(uint8_t)(b^a^c^d);
        uint32_t divisor=0x3f000000|(reverse8(a)<<16)|(reverse8(c)<<8)|reverse8(d);
        state=ps2f_raw(ps2f_div(ps2f_raw(ps2f_mul(table[b],state)),divisor));
    }
    return ((crc32(p,n)&0xffffff)|((uint32_t)p[0]<<24))==key;
}
gt4_disc_replay *gt4_disc_replay_open(gt4_vol *vol)
{
    gt4_disc_replay *d=NULL;uint8_t *raw=NULL,*image=NULL;size_t n=0;int found=0;
    if(!vol)return NULL;
    raw=gt4_disc_read_root(gt4_vol_source(vol),"CORE.GT4",&n);
    if(!raw || n<6 || raw[0]!=1 || raw[1]!=1)goto done;
    uint32_t size=word(raw+2);if(size<0x12c || size>32u*1024*1024)goto done;
    image=malloc(size);if(!image || core_inflate_raw(raw+6,n-6,image,size)!=size || word(image+0x100)!=0x176265b3)goto done;
    d=calloc(1,sizeof *d);if(!d)goto done;d->vol=vol;
    for(size_t at=0x12c;at<size;){
        if(size-at<8)goto invalid;uint32_t base=word(image+at),length=word(image+at+4);at+=8;
        if(length>size-at)goto invalid;
        if(base<=0x655888 && (uint64_t)base+length>=0x655c88){
            for(unsigned i=0;i<256;i++)d->table[i]=word(image+at+0x655888-base+i*4);found=1;
        }
        at+=length;
    }
    if(!found)goto invalid;goto done;
invalid:free(d);d=NULL;
done:free(raw);free(image);return d;
}
void gt4_disc_replay_close(gt4_disc_replay *d){free(d);}

typedef struct {const uint8_t *p;size_t bits,at;int bad;} bits;
/* 0057AD58: MSB first. A failed read remains failed, with no out-of-bounds. */
static uint32_t read_bits(bits *b,unsigned n)
{
    if(n>31 || b->at>b->bits || n>b->bits-b->at){b->bad=1;return 0;}
    uint32_t v=0;while(n--){v=(v<<1)|((b->p[b->at/8]>>(7-b->at%8))&1);b->at++;}return v;
}
static uint32_t positive(bits *b)
{
    unsigned n=0;while(read_bits(b,1) && !b->bad){if(++n>30){b->bad=1;return 1;}}
    return (1u<<n)+read_bits(b,n); /* 00345C90 */
}
static int delta(bits *b)
{
    unsigned n=read_bits(b,3),sign=read_bits(b,1);
    /* 00346080 constructs D_00620218's cumulative powers of four. */
    int v=(int)(2*((1u<<(2*n))-1)/3+read_bits(b,2*n+1));return sign?-v:v;
}
int gt4_disc_replay_commands(const uint8_t *p,size_t size,uint8_t **out,size_t *steps)
{
    if(!out || !steps)return 0;*out=NULL;*steps=0;
    if(!p || size<8)return 0;uint32_t n=word(p),bytes=word(p+4);
    size_t aligned=((size_t)bytes+3)&~(size_t)3;
    if(n<1 || n>60u*60*60 || bytes<8 || aligned>size || bytes>0xeb04)return 0;
    uint8_t *tape=calloc(n,32);if(!tape)return 0;
    /* 00346610 +40..4C clears the recorder pool; 00346890 uses 0xEB04
       bytes for this command recorder. 00346528 copies only blob+4 bytes,
       leaving zero tail (several disc recordings consume that tail). */
    uint8_t *padded=calloc(0xeb04,1);if(!padded){free(tape);return 0;}
    memcpy(padded,p,bytes);
    bits b={padded+8,(0xeb04-8)*8,0,0};uint8_t cmd[32]={0};int previous=0;uint32_t repeat=0;
    static const unsigned masks[]={0x100,0x80,0x10,2,1,4,8,0x40,0x20};
    static const unsigned offsets[]={0,1,2,4,7,8,10,12,14};
    for(uint32_t frame=1;frame<n;frame++){
        /* 00345C38 predicts steering even during run-length repetitions. */
        int v=(int16_t)(cmd[4]|cmd[5]<<8),prediction=2*v-previous;previous=v;
        if(abs(v)>=16)put16(cmd+4,prediction < -4096?-4096:prediction>4096?4096:prediction);
        if(repeat)repeat--;
        else{
            uint32_t mask=positive(&b)-1;if(mask&~0x3ffu)b.bad=1;
            for(unsigned j=0;j<9;j++)if(mask&masks[j]){
                unsigned at=offsets[j];int difference=delta(&b);
                if(j==3 || j>=5)put16(cmd+at,(cmd[at]|cmd[at+1]<<8)+difference);
                else cmd[at]=(uint8_t)(cmd[at]+difference);
            }
            if(mask&0x200){uint32_t count=positive(&b)-1;if(count>16)b.bad=1;else{
                cmd[3]=(uint8_t)count;for(unsigned j=0;j<count;j++)cmd[16+j]=(uint8_t)(cmd[16+j]+delta(&b));}}
            repeat=positive(&b)-1;
        }
        if(b.bad){free(padded);free(tape);return 0;}memcpy(tape+(frame-1)*32,cmd,32);
    }
    /* 00345D70 +3C..5C: declared last frame clears the complete command. */
    free(padded);*out=tape;*steps=n;return 1;
}

static int label(gt4_vol *vol,const char *table,uint32_t id,char *out,size_t cap)
{
    char path[96];size_t n=0;snprintf(path,sizeof path,"/specdb/GT4_US2560/%s.idi",table);
    uint8_t *p=gt4_vol_read(vol,path,&n);const char *s=p?gt4_specdb_label_for_id(p,n,id):NULL;
    int ok=s && strlen(s)<cap;if(ok)snprintf(out,cap,"%s",s);free(p);return ok;
}
/* 00346BB8/00346CA0: legacy stream or up to three 0x14F4 checkpoints,
   each followed by its own aligned RecorderCommand blob. Keep the
   checkpoints separate from commands; flattening loses scene state. */
static int recorder(const uint8_t *info,size_t size,gt4_replay_file *r)
{
    if(size<0x1aac)return 0;unsigned streams=word(info+0x1aa0);
    if(streams<1 || streams>2)return 0;size_t at=0x1aa4;
    /* RaceSplitBattle 003303F0 +20..70 appends TWO complete recorders
       through 00346DA0. Each has its own checkpoint count and tape. */
    for(unsigned player=0;player<streams;player++){
        if(size-at<4)return 0;unsigned count=word(info+at);at+=4;if(count>3)return 0;
        unsigned *segments=player?&r->disc_segments_2p:&r->disc_segments;*segments=count;
        size_t *starts=player?r->disc_segment_at_2p:r->disc_segment_at;
        size_t *lengths=player?r->disc_segment_steps_2p:r->disc_segment_steps;
        uint32_t *offsets=player?r->disc_segment_offset_2p:r->disc_segment_offset;
        uint8_t **dest=player?&r->tape_2p:&r->tape;size_t *steps=player?&r->steps_2p:&r->steps;
        for(unsigned i=0;i<(count?count:1);i++){
            if(count){if(size-at<0x14f4)return 0;offsets[i]=(uint32_t)at;at+=0x14f4;}
            uint8_t *tape=NULL;size_t frames=0;
            if(size-at<8 || !gt4_disc_replay_commands(info+at,size-at,&tape,&frames))return 0;
            size_t total=*steps+frames;if(total>60u*60*60){free(tape);return 0;}
            uint8_t *joined=realloc(*dest,total*32);if(!joined){free(tape);return 0;}
            *dest=joined;starts[i]=*steps;lengths[i]=frames;memcpy(joined+*steps*32,tape,frames*32);free(tape);*steps=total;
            at+=((size_t)word(info+at+4)+3)&~(size_t)3;if(at>size)return 0;
        }
    }
    return 1;
}
/* 001D3B20 +18..1B4 validates the icon and skips vertices/animation.
   001D3D70 +14..38 / 001D3FE8 select columns 0..63 of each
   128-pixel row (mask 0x40), retaining the original RGB555 texels. */
int gt4_disc_replay_icon(const uint8_t *p,size_t n,uint16_t out[4096])
{
    if(!p || !out || n<20 || word(p)!=0x10000)return 0;
    unsigned shapes=word(p+4),flags=word(p+8),vertices=word(p+16);
    if((shapes!=1 && shapes!=2 && shapes!=4 && shapes!=6 && shapes!=8) || !(flags&4) ||
       vertices<3 || vertices%3 || vertices/3>600 ||
       (shapes==2 && vertices/3>550) || (shapes==4 && vertices/3>500) ||
       (shapes==6 && vertices/3>450) || (shapes==8 && vertices/3>400))return 0;
    size_t at=20+(size_t)vertices*(16+8*shapes);
    if(at>n || n-at<4)return 0;unsigned clips=word(p+at);at+=4;if(clips>256)return 0;
    for(unsigned i=0;i<clips;i++){
        if(n-at<16)return 0;unsigned tracks=word(p+at+12);at+=16;if(tracks>8)return 0;
        for(unsigned j=0;j<tracks;j++){
            if(n-at<8)return 0;unsigned keys=word(p+at+4);at+=8;if(keys>10 || n-at<keys*8)return 0;at+=keys*8;
        }
    }
    unsigned produced=0,source=0;
    if(flags&8){
        if(n-at<4)return 0;size_t length=word(p+at)&~(size_t)1;at+=4;
        if(length>n-at)return 0;size_t end=at+length;
        while(at<end && produced<4096){
            if(end-at<2)return 0;int count=(int16_t)(p[at]|p[at+1]<<8);at+=2;
            unsigned run=count<0?(unsigned)-count:(unsigned)count;uint16_t value=0;
            if(count>0){if(end-at<2)return 0;value=(uint16_t)(p[at]|p[at+1]<<8);at+=2;}
            else if(run>(end-at)/2)return 0;
            for(unsigned j=0;j<run && produced<4096;j++,source++){
                uint16_t v=value;if(count<0){v=(uint16_t)(p[at]|p[at+1]<<8);at+=2;}
                if(!(source&0x40))out[produced++]=v|0x8000;
            }
        }
    }else{
        if(n-at<16384)return 0;
        for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
            size_t k=at+(y*128+x)*2;out[produced++]=(uint16_t)(p[k]|p[k+1]<<8|0x8000);
        }
    }
    return produced==4096;
}
static void disc_icon(gt4_disc_replay *d,const char *path,gt4_replay_file *r)
{
    const char *slash=strrchr(path,'/');if(!slash || slash-path>100)return;
    char name[128];snprintf(name,sizeof name,"%.*s/icon0.ico",(int)(slash-path),path);
    size_t n=0;uint8_t *raw=gt4_vol_read(d->vol,name,&n);uint16_t pixels[4096];
    if(raw && gt4_disc_replay_icon(raw,n,pixels)){
        r->disc_icon=malloc(4096*4);
        if(r->disc_icon)for(unsigned i=0;i<4096;i++){
            /* PSMCT16, 00176FF0 +178..244; use tex1.c RGB555
               expansion. GL colour/filtering is outside the texel oracle. */
            for(unsigned c=0;c<3;c++){unsigned v=(pixels[i]>>(5*c))&31;r->disc_icon[4*i+c]=(uint8_t)((v<<3)|(v>>2));}
            r->disc_icon[4*i+3]=255;
        }
    }
    free(raw);
}

int gt4_disc_replay_decode(gt4_disc_replay *d,const char *path,const uint8_t *bytes,size_t byte_count,gt4_replay_file *r,char *error,size_t cap)
{
    const char *why="missing file";uint8_t *raw=NULL,*base=NULL,*info=NULL;size_t size=0,nb=0;int ok=0;
    if(!r)return 0;memset(r,0,sizeof *r);if(!d || !path)goto done;
    size=byte_count;raw=size?malloc(size):NULL;if(!raw || !bytes)goto done;memcpy(raw,bytes,size);
    /* 001CFFF8 +D0..F0: DeviceDemo EncryptUnit. School recordings start
       directly with Base; the theater's have this additional envelope. */
    if(size>=0x40 && !memcmp(raw+0x10,"Base",5)){base=raw;raw=NULL;nb=size;}
    else {why="EncryptUnit checksum";if(!gt4_ps2_unwrap(raw,size,0x3f322311,0x3e6b3ef8,64,1,&base,&nb))goto done;}
    why="Base header";if(nb<0x40 || memcmp(base+0x10,"Base",5) || word(base+8)!=0x40000)goto done;
    uint32_t header=word(base),n=word(base+4);if(header<0x40 || header>nb || n>nb-header || n<0x1aa4)goto done;
    info=malloc(n);if(!info)goto done;memcpy(info,base+header,n);
    why="Base checksum";if(!gt4_disc_replay_base(info,n,word(base+12),d->table))goto done;
    why="GT4M header";if(word(info)!=n || word(info+4)!=0x4d345447 || word(info+8)!=0x40000 || !memchr(info+16,0,64))goto done;
    r->cars=(int)word(info+0x958);if(r->cars<1 || r->cars>6)goto done;
    why="command stream";if(!recorder(info,n,r))goto done;
    why="course";if(word(info+0xbc)!=0x22 || !label(d->vol,"COURSE",word(info+0xb8),r->course,sizeof r->course))goto done;
    why="race class";const char *cls=(char *)info+16;
    int license=!strcmp(cls,"RaceLicense") || !strcmp(cls,"RaceTraining");
    int mission=!strcmp(cls,"RaceMission"),career=!strcmp(cls,"RaceGTmode") || !strcmp(cls,"RaceChampionship");
    /* Demo 003878E0 and Single 00387570 both construct 00386E08;
       saved command playback uses the same Arcade conductor. */
    int arcade=!strcmp(cls,"RaceArcadeSingle") || !strcmp(cls,"RaceArcadeDemo") || !strcmp(cls,"RaceArcade");
    int free_run=!strcmp(cls,"RaceFreeRun"),practice=!strcmp(cls,"RaceFreePractice"),machine=!strcmp(cls,"RaceMachineTest"),split=!strcmp(cls,"RaceSplitBattle");
    if(!license && !mission && !career && !free_run && !practice && !machine && !split && !arcade)goto done;
    if(word(info+0x1aa0)!=(split?2u:1u) || (split && r->cars!=2))goto done;
    r->disc_license=license?(!strcmp(cls,"RaceTraining")?2:1):mission?3:0;
    r->championship=!strcmp(cls,"RaceChampionship");
    /* World constructors: 003EDE50/003EF498, 003B92D0, 003BDD60.
       These are the existing native session classes, not Arcade aliases. */
    r->world_mode=practice?12:machine?15:0;
    r->manual=info[0x960+0x1a0];r->assist_asm=info[0x960+0x1a2];r->assist_tcs=info[0x960+0x1a3];
    int teacher=license && !strncmp(path,"/replay/license/",16);
    if(license || mission || career){
        why="RACE identifier";char code[32];uint32_t id=word(info+0xb0);
        /* 00388E38 +150/+158 serializes race+68's 64-bit RACE key.
           Saved files resolve this key; their filenames have no event code.
           The school demonstration intentionally uses the CURRENT exam. */
        if(teacher){const char *name=strrchr(path,'/');if(!name || strlen(name+1)>=sizeof code)goto done;snprintf(code,sizeof code,"%s",name+1);}
        else if(word(info+0xb4)!=0x23 || !label(d->vol,"RACE",id,code,sizeof code))goto done;
        size_t ni=0,nr=0;uint8_t *idi=gt4_vol_read(d->vol,"/specdb/GT4_US2560/RACE.idi",&ni),*rows=gt4_vol_read(d->vol,"/specdb/GT4_US2560/RACE.dbt",&nr);
        int rowok=idi && rows && gt4_specdb_label(idi,ni,code,&id) && gt4_specdb_row(rows,nr,id,r->event_row,sizeof r->event_row)==sizeof r->event_row;
        free(idi);free(rows);if(!rowok)goto done;r->disc_event=!teacher;snprintf(r->license_code,sizeof r->license_code,"%s",code);
    }
    /* 00110230 -> 003EAD58 imports only the teacher's command recorder. */
    if(teacher){r->race_time=word(info+0xc0);r->best_lap=word(info+0xc4);r->disc_info=info;info=NULL;ok=1;goto done;}
    r->prepared=1;r->laps=(int)word(info+0x50);r->nothers=r->cars-1;r->has_colors=1;r->time_trial=free_run;r->split_battle=split;
    if(split){const uint8_t *second=info+0x960+0x228;r->manual_2p=second[0x1a0];r->assist_asm_2p=second[0x1a2];r->assist_tcs_2p=second[0x1a3];r->split_boost=(int)word(info+0x64);r->split_handicap=(int)word(info+0x68);r->split_tire_damage=(int)word(info+0x6c);}
    for(int i=0;i<r->cars;i++){
        const uint8_t *entry=info+0x960+i*0x228;why="car setup";
        if(word(entry+4) || !label(d->vol,"GENERIC_CAR",word(entry),i?r->others[i-1]:r->car,i?sizeof r->others[0]:sizeof r->car))goto done;
        r->car_id[i]=word(entry);r->owned_on[i]=1;
        /* 00388E38 +148..264: serialized 0x178 setup and 0x20 wear.
           Existing owned-car API consumes these, without changing physics. */
        for(unsigned k=0;k<3;k++)memcpy(r->owned[i]+k*0x178,entry,0x178);
        memcpy(r->owned[i]+0x498,entry+0x178,32);
        if(i)r->colors[i-1]=(int8_t)entry[0x18];else r->color=(int8_t)entry[0x18];
    }
    r->race_time=word(info+0xc0);r->best_lap=word(info+0xc4);
    r->disc_info=info;info=NULL;disc_icon(d,path,r);ok=1;
done:free(raw);free(base);free(info);if(!ok)gt4_replay_free(r);
    if(error && cap)snprintf(error,cap,"%s",ok?"":why);return ok;
}
int gt4_disc_replay_list(gt4_vol *vol,char (*paths)[64],int max,int sub)
{
    if(!vol || !paths || max<=0 || sub)return 0;
    /* DeviceDemo 0017C348 +40..54 selects DemonstrationFiles, which
       001D19F8 +168..26C enumerates in authored order. US config-main.ad
       L146..160 supplies the second literal array (first is PAL L115).
       Read its strings from the disc, never enumerate unused test files. */
    size_t size=0;char error[128];uint8_t *raw=gt4_vol_read(vol,"/config/config-main.adc",&size);
    ah_code *c=raw?ah_load(raw,size,error,sizeof error):NULL;free(raw);int n=0,arrays=0;
    for(int i=0;c && i<c->count;i++){
        const ah_ins *x=c->ins+i;
        if(x->op!=AH_VARIABLE_PUSH || !x->npath || strcmp(x->path[0],"DemonstrationFiles"))continue;
        int end=i+1;while(end<c->count && c->ins[end].op==AH_STRING_CONST)end++;
        if(end==i+1 || end>=c->count || c->ins[end].op!=AH_ARRAY_CONST_OLD || c->ins[end].i!=end-i-1)continue;
        if(++arrays!=2)continue;
        for(int k=i+1;k<end && n<max;k++){
            const char *name=c->ins[k].s;
            if(!name || strlen(name)>23 || strchr(name,'/')){n=0;break;}
            snprintf(paths[n++],64,"/replay/%s/%s",name,name);
        }
        break;
    }
    ah_free(c);return n;
}

int gt4_disc_replay_read(gt4_disc_replay *d,const char *path,gt4_replay_file *r,char *error,size_t cap){
 size_t n=0;uint8_t *raw=d?gt4_vol_read(d->vol,path,&n):NULL;
 int ok=gt4_disc_replay_decode(d,path,raw,n,r,error,cap);free(raw);return ok;
}
/* P: keep the source envelope losslessly in an atomic native container;
   decode it with the same original Base/GT4M path whenever it is opened. */
int gt4_disc_replay_saved(gt4_disc_replay *d,const char *name,gt4_replay_file *r,char *error,size_t cap){
 char path[1200];if(!name || strchr(name,'/') || strchr(name,'\\') || strstr(name,".."))return 0;
 snprintf(path,sizeof path,"%s/replays/%s",gt4_save_directory(),name);
 uint8_t *data=NULL;size_t size=0;uint64_t stamp=0;if(gt4_save_read(path,&data,&size,&stamp)<=0)return 0;
 int ok=size>8 && !memcmp(data,"PS2R",4) && word(data+4)==size-8 && gt4_disc_replay_decode(d,name,data+8,size-8,r,error,cap);
 free(data);if(ok)r->when=stamp;return ok;
}
