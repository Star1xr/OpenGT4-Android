/* Authored GT4M archives exercise the native session adapter. The disc's
   original car/course metadata and feedback table are read at runtime. */
#include "../../src/core/disc_replay.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"replay types %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void put32(uint8_t *p,uint32_t u){p[0]=(uint8_t)u;p[1]=(uint8_t)(u>>8);p[2]=(uint8_t)(u>>16);p[3]=(uint8_t)(u>>24);}
static uint8_t *archive(gt4_disc_replay *d,const uint8_t *info,size_t size,size_t *length){
 uint8_t *p=calloc(size+64,1);if(!p)return NULL;put32(p,64);put32(p+4,(uint32_t)size);put32(p+8,0x40000);memcpy(p+16,"Base",5);
 uint32_t key=(crc32(info,size)&0xffffff)|((uint32_t)info[0]<<24),state=(key&0xffffff)|0x3f000000;put32(p+12,key);
 for(size_t i=0;i<size;i++){unsigned a=state&255,c=(state>>8)&255,e=(state>>16)&255,b=info[i]^a^c^e;p[64+i]=(uint8_t)b;
  uint32_t divisor=0x3f000000|(reverse8(a)<<16)|(reverse8(c)<<8)|reverse8(e);state=ps2f_raw(ps2f_div(ps2f_raw(ps2f_mul(d->table[b],state)),divisor));}
 *length=size+64;return p;
}
static size_t stream(uint8_t *p,size_t at,int checkpoint){
 put32(p+at,checkpoint);at+=4;for(int i=0;i<(checkpoint?checkpoint:1);i++){if(checkpoint){memset(p+at,0,0x14f4);p[at]=(uint8_t)i;at+=0x14f4;}put32(p+at,4);put32(p+at+4,8);at+=8;}return at;
}
int main(int argc,char **argv){
 CHECK(argc>=2);gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol);gt4_disc_replay *d=gt4_disc_replay_open(vol);CHECK(d);
 gt4_replay_file donor;char error[256];CHECK(gt4_disc_replay_read(d,"/replay/Rdemo0/Rdemo0",&donor,error,sizeof error));
 size_t ni;uint8_t *idi=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.idi",&ni);CHECK(idi);uint32_t event;CHECK(gt4_specdb_label(idi,ni,"usl0b0001",&event));free(idi);
 const char *classes[]={"RaceArcadeSingle","RaceFreeRun","RaceFreePractice","RaceGTmode","RaceChampionship","RaceLicense","RaceTraining","RaceMission","RaceMachineTest","RaceSplitBattle","RaceArcadeDemo","RaceArcade"};
 for(int k=0;k<12;k++)for(int checkpoint=0;checkpoint<4;checkpoint++){
  uint8_t info[65536];memcpy(info,donor.disc_info,0x1aa4);memset(info+16,0,64);strcpy((char *)info+16,classes[k]);
  put32(info+0xb0,event);put32(info+0xb4,0x23);put32(info+0x1aa0,k==9?2:1);
  size_t size=stream(info,0x1aa4,checkpoint);if(k==9)size=stream(info,size,3-checkpoint);put32(info,(uint32_t)size);
  size_t bytes;uint8_t *raw=archive(d,info,size,&bytes);CHECK(raw);gt4_replay_file r;
  int ok=gt4_disc_replay_decode(d,"arbitrary-imported-name.ogt4",raw,bytes,&r,error,sizeof error);if(!ok)fprintf(stderr,"%s: %s\n",classes[k],error);CHECK(ok);
  if(argc>2 && !k && !checkpoint)r.steps++;
  CHECK(r.steps==4*(checkpoint?checkpoint:1) && r.prepared && r.cars==2 && r.disc_segments==(unsigned)checkpoint);
  CHECK(r.disc_license==(k==5?1:k==6?2:k==7?3:0));CHECK(r.disc_event==(k>=3 && k<=7));CHECK(r.championship==(k==4));
  CHECK(r.world_mode==(k==2?12:k==8?15:0) && r.time_trial==(k==1));CHECK(r.split_battle==(k==9));
  if(k==9)CHECK(r.steps_2p==4*((3-checkpoint)?3-checkpoint:1) && r.tape_2p && r.disc_segments_2p==(unsigned)(3-checkpoint));
  if(k>=3 && k<=7)CHECK(!strcmp(r.license_code,"usl0b0001"));
  for(int j=0;j<checkpoint;j++)CHECK(r.disc_segment_at[j]==j*4 && r.disc_segment_steps[j]==4 && r.disc_info[r.disc_segment_offset[j]]==j);
  gt4_replay_free(&r);CHECK(!gt4_disc_replay_decode(d,"truncated",raw,bytes-1,&r,error,sizeof error));raw[bytes-1]^=1;CHECK(!gt4_disc_replay_decode(d,"fixture",raw,bytes,&r,error,sizeof error));free(raw);
 }
 gt4_replay_free(&donor);gt4_disc_replay_close(d);gt4_vol_close(vol);
 puts("PASS twelve native replay classes, event keys independent of filenames, one/two command streams, legacy/checkpoints and checksum rejection. Session adapter C; not original trajectory parity.");return 0;
}
