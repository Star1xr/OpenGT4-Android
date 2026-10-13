/* Original scripts plus native album storage; no game assets in fixtures. */
#include "../../src/core/menu.c"
#include "ee.h"
#include <fenv.h>
#include <direct.h>
#include "miniz.h"
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"outside line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int ri(menu *m,const char *n){for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),n))return i;return -1;}
static void noop(ee_ctx *e){(void)e;}
static int original_index;static int photo_count;
static void photos(ee_ctx *e){e->r[2].uw[0]=(uint32_t)photo_count;}
static void integer(ee_ctx *e){uint32_t p=e->r[4].uw[0],i=e->r[5].uw[0];memcpy(e->ram+p,&i,4);}
static void script(ee_ctx *e){memcpy(&original_index,e->ram+e->r[7].uw[0],4);uint32_t yes=1;memcpy(e->ram+e->r[4].uw[0],&yes,4);}
static ee_fn hook(uint32_t a){switch(a){case 0x1c6918:case 0x1c6aa0:return photos;case 0x2fe278:return integer;case 0x329cb0:return script;
case 0x30bb18:case 0x3285a8:case 0x3285f8:case 0x2fc870:case 0x309378:return noop;}return NULL;}
static ee_fn lookup(uint32_t a){ee_fn f=hook(a);return f?f:recomp_lookup(a);}
static int invoke(ee_ctx *e,uint32_t fn,uint32_t a,uint32_t b,uint32_t c)
{e->r[4].uw[0]=a;e->r[5].uw[0]=b;e->r[6].uw[0]=c;e->r[29].uw[0]=0x1ef0000;jmp_buf j;e->exit_jmp=&j;if(setjmp(j))return 0;ee_fn f=recomp_lookup(fn);if(!f)return 0;f(e);return 1;}
static void put(ee_ctx *e,uint32_t p,int32_t n){memcpy(e->ram+p,&n,4);}
static int32_t get(ee_ctx *e,uint32_t p){int32_t n;memcpy(&n,e->ram+p,4);return n;}
static int slide_oracle(ee_ctx *e,int negative)
{
 CHECK(sizeof(gt4_album_slide)==0x54);unsigned frames=0,rects=0;float v[4]={13,29,640,448};
 for(int shape=0;shape<4;shape++)for(int effect=0;effect<2;effect++)for(int reverse=0;reverse<2;reverse++){
  gt4_album_slide s;gt4_album_slide_init(&s,shape==0?480:1920,shape==1?1920:1080,shape==3?3:8,shape==3?1:4,effect,reverse,.37f,.62f);
  s.focal_x=.21f;s.focal_y=-.17f;memcpy(e->ram+0x1800000,&s,sizeof s);
  for(int f=0;f<1100;f++){
   if(f==600){gt4_album_slide_out(&s);CHECK(invoke(e,0x1c60b0,0x1800000,0,0));}
   gt4_album_slide_tick(&s);if(negative==2 && !f)s.phase++;CHECK(invoke(e,0x1c5f80,0x1800000,0,0));CHECK(!memcmp(&s,e->ram+0x1800000,sizeof s));frames++;
   memcpy(e->ram+0x1800100,v,sizeof v);memset(e->ram+0x1800200,0,32);
   CHECK(invoke(e,0x1c6160,0x1800000,0x1800200,0x1800100));gt4_album_rect r;int visible=gt4_album_slide_rect(&s,v,&r);
   if(negative==1 && !f)r.w+=1;
   CHECK(visible==(int)e->r[2].uw[0]);
   float expected[4];memcpy(expected,e->ram+0x1800200,16);float actual[]={r.x,r.y,r.w,r.h};
   for(int k=0;k<4;k++){if(fabsf(actual[k]-expected[k])>.001f){fprintf(stderr,"rectangle %d/%d/%d frame%d component%d native%.9f original%.9f\n",shape,effect,reverse,f,k,actual[k],expected[k]);return 1;}}
   CHECK(r.alpha==e->ram[0x1800210]);rects++;
  }
 }
 for(photo_count=1;photo_count<=GT4_ALBUM_CAPACITY;photo_count++){
  int order[GT4_ALBUM_CAPACITY];uint32_t seed=123u+photo_count;for(int k=0;k<photo_count;k++)order[k]=k;
  memset(e->ram+0x1800400,0,0x1e00);put(e,0x874d50,(int32_t)seed);
  CHECK(invoke(e,0x1c6be8,0x1800400,1,0));gt4_album_slide_shuffle(order,photo_count,&seed);
  if(negative==3)order[0]^=1;CHECK(!memcmp(order,e->ram+0x180040c,(size_t)photo_count*4));CHECK(seed==(uint32_t)get(e,0x874d50));
 }
 printf("PASS %u original slide states, %u geometry/alpha comparisons (0.001 pixel tolerance), 400 shuffle capacities.\n",frames,rects);return 0;
}
static int attract_oracle(ee_ctx *e,menu *m,int negative)
{
 uint32_t obj=0x1800400;char paths[32][64];int n=gt4_disc_replay_list(m->vol,paths,32,0);CHECK(n==13);
 ah_val opt;CHECK(ah_get_member(m->vm,game_object(m),"option",&opt));put(e,0x622f4c,0x1801000);
 for(int interval=0;interval<=4;interval++){
  ah_set_member(m->vm,opt.u.o,"demo_movie_interval",ah_int(interval));put(e,0x1801000+0x39e1c,interval);
  memset(e->ram+obj,0,0x80);put(e,obj+0x4c,n);put(e,obj+0x50,4);put(e,obj+0x54,4);put(e,obj+0x6c,interval-1);
  attract_index=0;demo_movie_count=interval-1;attract_left=4;
  for(int i=0;i<40;i++){
   m->next_project[0]=0;attract_pending[0]=0;original_index=-1;
   CHECK(invoke(e,0x434770,obj,0,0));attract_next(m);
   int movie=original_index<0;CHECK(movie==!strcmp(m->next_project,"boot:MovieRoot"));
   if(!movie)CHECK(!strcmp(attract_pending,paths[original_index]));
   if(negative && !i)demo_movie_count++;
   CHECK(attract_index==get(e,obj+0x48));CHECK(demo_movie_count==get(e,obj+0x6c));
  }
 }
 ah_set_member(m->vm,opt.u.o,"demo_movie_interval",ah_int(0));
 snprintf(attract_origin,sizeof attract_origin,"arcade:TopRoot");attract_left=4;attract_cancelled=0;
 for(int i=0;i<3;i++){m->next_project[0]=0;menu_attract_finish(m,0);CHECK(menu_attract_active() && attract_left==3-i && attract_pending[0]);}
 menu_attract_finish(m,0);CHECK(!menu_attract_active() && !attract_pending[0] && !strcmp(m->next_project,attract_origin));
 attract_left=4;menu_attract_finish(m,1);CHECK(!menu_attract_active() && !strcmp(m->next_project,attract_origin));
 puts("PASS original demo scheduler: 200 choices, movies, all 13 tapes, wrap, cancellation.");return 0;
}
static void be(uint8_t *p,uint32_t x){p[0]=x>>24;p[1]=x>>16;p[2]=x>>8;p[3]=x;}
static size_t chunk(uint8_t *p,const char *tag,const uint8_t *data,size_t n){be(p,(uint32_t)n);memcpy(p+4,tag,4);memcpy(p+8,data,n);be(p+8+n,(uint32_t)mz_crc32(0,p+4,n+4));return n+12;}
static int fixture(const char *dir)
{
 _mkdir(dir);char photos[1024],path[1024];snprintf(photos,sizeof photos,"%s/photos",dir);_mkdir(photos);
 for(int i=0;i<2;i++){uint8_t png[256]={137,80,78,71,13,10,26,10},ihdr[13]={0},raw[26]={0},z[128];be(ihdr,3);be(ihdr+4,2);ihdr[8]=8;ihdr[9]=6;
  for(int y=0;y<2;y++)for(int x=0;x<3;x++){int p=y*13+1+x*4;raw[p]=(uint8_t)(50+i*150);raw[p+1]=100;raw[p+2]=200;raw[p+3]=255;}
  mz_ulong zn=sizeof z;CHECK(mz_compress(z,&zn,raw,sizeof raw)==MZ_OK);size_t n=8;n+=chunk(png+n,"IHDR",ihdr,13);n+=chunk(png+n,"IDAT",z,zn);n+=chunk(png+n,"IEND",raw,0);
  snprintf(path,sizeof path,"%s/photo_%d.png",photos,i);CHECK(plat_file_write(path,png,n));int w,h;uint8_t *px=plat_png_read(path,&w,&h);CHECK(px && w==3 && h==2 && px[0]==raw[1]);free(px);
  png[29]^=1;snprintf(path,sizeof path,"%s/invalid.png",dir);CHECK(plat_file_write(path,png,n));CHECK(!plat_png_read(path,&w,&h));plat_file_remove(path);
 }
 snprintf(path,sizeof path,"%s/slideshow.list",photos);plat_file_remove(path);return 0;
}
static int album_flow(menu *m,int negative)
{
 int r=ri(m,"PhotoRoot"),slide=ri(m,"SlideRoot");CHECK(r>=0 && slide>=0);CHECK(menu_open_labo(m,"PhotoRoot"));menu_tick(m);
 menu_send_input(m,MENU_DOWN);menu_send_input(m,MENU_ACCEPT);for(int i=0;i<10;i++)menu_tick(m);CHECK(album_files_count==2);
 ah_obj *page=widget_module(m,ui_root_widget(m->ui,r));ah_val ctx=ah_objv(m->context),ret,selected;
 menu_send_input(m,MENU_RIGHT);menu_set_pad_held(m,MENU_PAD_L1|MENU_PAD_R1);menu_send_input(m,MENU_ACCEPT);menu_set_pad_held(m,0);
 CHECK(ah_get_member(m->vm,m->project,"selection_",&selected) && ah_array_size(ah_as_obj(selected,AO_ARRAY))==2);
 CHECK(ah_call_member(m->vm,page,"commandToSlide",1,&ctx,&ret)>=0);for(int i=0;i<10;i++)menu_tick(m);
 CHECK(photo_playlist_count==2);char expected[128];memcpy(expected,photo_playlist_names,sizeof expected);photo_playlist_read=0;photo_playlist_count=0;photo_playlist_load();CHECK(photo_playlist_count==2 && !memcmp(expected,photo_playlist_names,128));
 CHECK(ah_call_member(m->vm,page,"commandSlideShow",1,&ctx,&ret)>=0);menu_tick(m);CHECK(menu_active_root(m)==slide && m->album_playing && m->album_count==2);
 for(int i=0;i<60 && !m->pcm_serial;i++)menu_tick(m);CHECK(m->pcm_serial);
 unsigned serial=m->pcm_serial;menu_pcm_state(m,1);for(int i=0;i<60;i++)menu_tick(m);CHECK(m->pcm_serial==serial);
 menu_pcm_state(m,0);for(int i=0;i<120;i++)menu_tick(m);CHECK(m->pcm_serial>serial);menu_pcm_state(m,1);
 int swaps=0,old=m->album_front;for(int i=0;i<2000;i++){menu_tick(m);if(old!=m->album_front){swaps++;old=m->album_front;}}
 if(negative)swaps=0;CHECK(swaps>=4 && m->album_playing);CHECK(m->pcm_requested && !strcmp(m->pcm_group,"racing"));
 menu_send_input(m,MENU_BACK);menu_tick(m);CHECK(menu_active_root(m)==r && !m->album_playing);
 CHECK(ah_call_member(m->vm,page,"commandSlideShow",1,&ctx,&ret)>=0);menu_tick(m);CHECK(m->album_playing);menu_send_input(m,MENU_ACCEPT);menu_tick(m);CHECK(!m->album_playing);
 char bad[128];CHECK(!album_path(bad,sizeof bad,"../bad.png"));
 printf("PASS original PhotoRoot selection/save, %d transitions, infinite wrap, PCM request, both exits and replay isolation.\n",swaps);return 0;
}
static int album_music_persistence(menu *m,gt4_vol *v,const char *dir,int negative)
{
 ah_vm *stale=ah_vm_new();ah_obj *stale_game=ah_module_new(stale,"game",NULL,NULL,NULL);gt4_save_restore(stale,stale_game);
 char error[256];menu *cached=menu_open(v,"slide",error,sizeof error);CHECK(cached);
 ah_val option,list,rows;CHECK(ah_get_member(m->vm,game_object(m),"option",&option));
 CHECK(ah_get_member(m->vm,option.u.o,"slide_playlist",&list));CHECK(ah_get_member(m->vm,list.u.o,"play_data",&rows));
 ah_obj *a=ah_as_obj(rows,AO_ARRAY);CHECK(a && ah_array_size(a)>1);
 ah_obj *row=ah_as_obj(ah_array_get(a,0),AO_ARRAY);CHECK(row);
 int enabled=!ah_truthy(ah_array_get(row,2));ah_array_set(row,2,ah_int(enabled));
 ah_set_member(m->vm,list.u.o,"shuffle",ah_int(0));
 album_music_store(m);gt4_music_playlist expected=m->album_music_snapshot;
 CHECK(gt4_save_capture(stale,stale_game,0));uint8_t kept[0x810];CHECK(gt4_save_option_blob_get("slide_playlist_ps2",kept,sizeof kept));CHECK(!memcmp(kept,expected.bytes,sizeof kept));ah_vm_free(stale);
 if(!gt4_save_active())CHECK(gt4_save_create(1,"Album Test"));
 menu_save_capture(m);
 menu_save_capture(cached);CHECK(gt4_save_store());
 menu_save_refresh(cached);CHECK(!memcmp(cached->album_music_snapshot.bytes,expected.bytes,sizeof expected.bytes));
 menu_close(cached);
 /* Reopen both the disk-backed session and a different project. */
 gt4_save_shutdown();gt4_save_init(dir);
 menu *next=menu_open(v,"slide",error,sizeof error);CHECK(next);
 if(negative)next->album_music_snapshot.bytes[16]^=1;
 CHECK(!memcmp(next->album_music_snapshot.bytes,expected.bytes,sizeof expected.bytes));
 CHECK(ah_get_member(next->vm,game_object(next),"option",&option));
 CHECK(ah_get_member(next->vm,option.u.o,"slide_playlist",&list));CHECK(ah_get_member(next->vm,list.u.o,"play_data",&rows));
 row=ah_as_obj(ah_array_get(rows.u.o,0),AO_ARRAY);CHECK(ah_truthy(ah_array_get(row,2))==enabled);
 CHECK(!memcmp(next->album_music_snapshot.bytes+16,expected.bytes+16,sizeof expected.bytes-16));
 menu_close(next);puts("PASS slideshow selection, order and shuffle survive profile/project reload and cached-menu capture.");return 0;
}
int main(int argc,char **argv)
{
 CHECK(argc>=4);CHECK(!fixture(argv[3]));gt4_save_init(argv[3]);menu_options_storage(NULL);const char *why;char error[256];
 plat_video_desc desc={640,480,0,0,0};CHECK(plat_video_init(&desc)==0);gt4_vol *v=gt4_vol_open(argv[1],&why);CHECK(v);
 ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[2],0x100000)>0x5a3900);e.lookup=lookup;e.hook=hook;fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 CHECK(!slide_oracle(&e,argc<=4?0:!strcmp(argv[4],"--geometry-negative")?1:!strcmp(argv[4],"--state-negative")?2:!strcmp(argv[4],"--shuffle-negative")?3:0));fesetenv(&env);
 menu *m=menu_open(v,"labomode",error,sizeof error);CHECK(m);CHECK(!album_flow(m,argc>4 && !strcmp(argv[4],"--cycle-negative")));
 CHECK(!attract_oracle(&e,m,argc>4 && !strcmp(argv[4],"--demo-negative")));CHECK(!album_music_persistence(m,v,argv[3],argc>4 && !strcmp(argv[4],"--playlist-negative")));menu_close(m);ee_free(&e);gt4_vol_close(v);gt4_save_shutdown();plat_video_shutdown();return 0;
}
