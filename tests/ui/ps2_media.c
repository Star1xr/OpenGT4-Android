/* Original encrypted disc replay plus synthetic PS2 photo/film archives.
   Negative changes decoded image reference; corrupt CRC/truncation
   are rejected before any local file is published. */
#include "../../src/core/save_ps2.c"
#include "ps2_media.h"
#include "disc_replay.h"
#include <math.h>
#include "film_fixture.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"media %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint8_t *wrap(const uint8_t *p,size_t length,uint32_t first,uint32_t second,size_t *n){
 *n=(length+8+63)&~(size_t)63;uint8_t *out=calloc(1,*n);if(!out)return NULL;memcpy(out+8,p,length);
 uint32_t random=0x13579abc,crc=checksum(out+8,*n-8),key=random^crc;w32(out,random);w32(out+4,crc);mt m;seed(&m,random+crc);
 for(size_t k=8;k<*n;k+=4){uint32_t u=update(&key,1),r=random32(&m);w32(out+k,(r32(out+k)^r)-u);}
 float a,b;memcpy(&a,&first,4);memcpy(&b,&second,4);swap(out+4,*n-4,b);swap(out,*n,a);return out;
}
static size_t add(uint8_t *archive,size_t at,const char *name,const uint8_t *data,size_t n){
 archive[at]=0x10;w32(archive+at+4,(uint32_t)n);strcpy((char *)archive+at+64,name);at+=512;memcpy(archive+at,data,n);return at+((n+1023)&~(size_t)1023);
}
static void entry(uint8_t *p,const char *name,unsigned bytes,unsigned first,int directory){
 p[0]=directory?0x20:0x10;p[1]=0x80;w32(p+4,bytes);w32(p+16,first);strcpy((char *)p+64,name);
}
static void fat_chain(uint8_t *card,unsigned start,unsigned count){
 for(unsigned k=0;k<count;k++)w32(card+2048+(start+k)*4,k+1<count?0x80000000u|(start+k+1):0xffffffffu);
}
static uint8_t *make_card(const uint8_t *r,size_t nr,const uint8_t *p,size_t np,const uint8_t *f,size_t nf,size_t *size){
 unsigned rc=(unsigned)((nr+1023)/1024),pc=(unsigned)((np+1023)/1024),fc=(unsigned)((nf+1023)/1024);
 unsigned need=rc+pc+fc+16,fats=(need+255)/256,offset=2+fats,clusters=need+offset;
 if(clusters<64)clusters=64;*size=(size_t)clusters*1024;uint8_t *out=calloc(1,*size);if(!out)return NULL;
 memcpy(out,"Sony PS2 Memory Card Format ",27);out[41]=2;out[42]=2;
 w32(out+48,clusters);w32(out+52,offset);w32(out+56,clusters-offset);w32(out+60,0);w32(out+0x50,1);
 for(unsigned k=0;k<fats;k++)w32(out+1024+k*4,2+k);
 fat_chain(out,0,2);fat_chain(out,2,3);fat_chain(out,5,rc);fat_chain(out,5+rc,pc);fat_chain(out,5+rc+pc,fc);
 uint8_t *root=out+(size_t)offset*1024;entry(root,".",3,0,1);entry(root+512,"..",3,0,1);entry(root+1024,names[0],5,2,1);
 uint8_t *sub=root+2048;entry(sub,".",5,2,1);entry(sub+512,"..",3,0,1);
 entry(sub+1024,"Rdemo0",(unsigned)nr,5,0);entry(sub+1536,"P0001",(unsigned)np,5+rc,0);entry(sub+2048,"F0001",(unsigned)nf,5+rc+pc,0);
 memcpy(root+5*1024,r,nr);memcpy(root+(5+rc)*1024,p,np);memcpy(root+(5+rc+pc)*1024,f,nf);return out;
}
int main(int argc,char **argv){
 CHECK(argc==4 || argc==5);gt4_vol *v=gt4_vol_open(argv[1],NULL);CHECK(v);
 char folder[250],path[500];snprintf(folder,sizeof folder,"build/ps2-media-%llu",(unsigned long long)time(NULL)*1000000+(unsigned long long)clock());gt4_save_init(folder);CHECK(gt4_save_mkdir(folder));
 size_t nr,nj,nf,nw;uint8_t *replay=gt4_vol_read(v,"/replay/Rdemo0/Rdemo0",&nr),*jpg=read_file(argv[2],4000000,&nj);CHECK(replay && jpg);
 uint8_t film[2048];size_t film_size=film_fixture(film,sizeof film);gt4_film decoded;
 CHECK(gt4_media_film_valid(film,film_size));CHECK(gt4_media_film_decode(film,film_size,&decoded));
 CHECK(decoded.cars==1 && decoded.frames==2 && decoded.width==640 && decoded.height==480 && decoded.samples==32);
 CHECK(!strcmp(decoded.course,"tsukuba") && !strcmp(decoded.car[0].code,"ka_01") && decoded.car[0].paint==2);
 if(getenv("GT4_FILM_NEGATIVE"))decoded.frame[1].parameters[0][3]+=1;
 CHECK(decoded.frame[0].parameters[0][3]==0 && decoded.frame[1].parameters[0][3]==1 && decoded.frame[1].camera[15]==.035f);
 CHECK(!gt4_media_film_decode(film,film_size-1,&decoded));
 uint8_t *photo=wrap(jpg,nj,0x3f2e4f76,0x3eced916,&nw),*f=wrap(film,film_size,0x3f278194,0x3eb0fcb4,&nf);CHECK(photo && f);
 size_t capacity=nr+nw+nf+16384;uint8_t *psu=calloc(1,capacity);CHECK(psu);psu[0]=psu[512]=psu[1024]=0x20;w32(psu+4,5);
 size_t end=1536;end=add(psu,end,"Rdemo0",replay,nr);end=add(psu,end,"P0001",photo,nw);end=add(psu,end,"F0001",f,nf);
 snprintf(path,sizeof path,"%s/media.psu",folder);CHECK(plat_file_write(path,psu,end));gt4_media_import report;CHECK(gt4_media_import_ps2(v,path,&report));CHECK(report.replays==1 && report.photos==1 && report.films==1);
 /* Synthetic FAT-backed card, with and without ECC spare pages. */
 size_t card_size;uint8_t *card_bytes=make_card(replay,nr,photo,nw,f,nf,&card_size);CHECK(card_bytes);
 snprintf(path,sizeof path,"%s/media.ps2",folder);CHECK(plat_file_write(path,card_bytes,card_size));
 CHECK(gt4_media_import_ps2(v,path,&report) && report.replays==1 && report.photos==1 && report.films==1);
 size_t ecc_size=card_size/512*528;uint8_t *ecc=calloc(1,ecc_size);CHECK(ecc);
 for(size_t k=0;k<card_size/512;k++)memcpy(ecc+k*528,card_bytes+k*512,512);
 CHECK(plat_file_write(path,ecc,ecc_size));CHECK(gt4_media_import_ps2(v,path,&report) && report.replays==1 && report.photos==1 && report.films==1);free(ecc);
 w32(card_bytes+2048,0x80000000u);CHECK(plat_file_write(path,card_bytes,card_size));
 CHECK(!gt4_media_import_ps2(v,path,&report) && report.replays==0 && report.photos==0 && report.films==0);free(card_bytes);
 gt4_disc_replay *d=gt4_disc_replay_open(v);gt4_replay_file a,b;char error[256],name[128];CHECK(d && gt4_disc_replay_read(d,"/replay/Rdemo0/Rdemo0",&a,error,sizeof error));
 snprintf(name,sizeof name,"replay_ps2_%08x.ogt4",(unsigned)checksum(replay,nr));CHECK(gt4_disc_replay_saved(d,name,&b,error,sizeof error));CHECK(a.steps==b.steps && a.steps>0 && !memcmp(a.tape,b.tape,a.steps*32) && !strcmp(a.course,b.course));gt4_replay_free(&a);gt4_replay_free(&b);
 snprintf(path,sizeof path,"%s/films/film_ps2_%08x.ogt4film",folder,(unsigned)checksum(f,nf));uint8_t *plain;size_t np;CHECK(gt4_save_read(path,&plain,&np,NULL)>0);CHECK(np==nf-8 && !memcmp(plain,film,film_size));CHECK(gt4_media_film_decode(plain,np,&decoded));free(plain);
 snprintf(path,sizeof path,"%s/photos/photo_ps2_%08x.png",folder,(unsigned)checksum(photo,nw));int w,h,rw,rh;uint8_t *pixels=plat_png_read(path,&w,&h),*reference=plat_png_read(argv[3],&rw,&rh);CHECK(pixels && reference && w==rw && h==rh);if(argc==5)pixels[0]^=255;
 for(size_t k=0;k<(size_t)w*h*4;k++)CHECK(abs((int)pixels[k]-reference[k])<=3);free(pixels);free(reference);
 /* Invalid second media item must not publish an earlier valid replay. */
 char badfolder[300];snprintf(badfolder,sizeof badfolder,"%s/bad",folder);gt4_save_init(badfolder);CHECK(gt4_save_mkdir(badfolder));
 psu[1536+512+((nr+1023)&~(size_t)1023)+512+nw/2]^=128;
 snprintf(path,sizeof path,"%s/bad.psu",folder);CHECK(plat_file_write(path,psu,end));CHECK(!gt4_media_import_ps2(v,path,&report));CHECK(report.replays==0 && report.photos==0 && report.films==0);
 CHECK(plat_file_write(path,psu,end-1));CHECK(gt4_ps2_visit_media(path,NULL,NULL,error,sizeof error)<0);
 film[16]=1;CHECK(!gt4_media_film_valid(film,127));
 free(psu);free(photo);free(f);free(jpg);free(replay);gt4_disc_replay_close(d);gt4_vol_close(v);puts("PS2 media: archive, FAT/ECC/loop rejection, replay commands, JPEG, film preservation, corrupt/truncated rejection");return 0;
}
