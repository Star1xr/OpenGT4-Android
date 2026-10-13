/* Audit all authored UI text and storage mode names from the user disc. */
#define main pregrid_regression_main
#include "pregrid_menu.c"
#undef main
#include <ctype.h>
#include <time.h>
#include "../../src/core/adhoc/adhoc_code.h"
#include "miniz.h"
#include "../../src/core/rtext.h"
static char projects[128][64];static int nprojects,checked,legacy;
static void project(void *d,const char *name,int dir,uint32_t size){(void)d;(void)size;if(!dir)return;const char *p=strrchr(name,'/');p=p?p+1:name;if(nprojects<128)snprintf(projects[nprojects++],64,"%s",p);}
static int old_text(const char *s){char plain[8192];int n=0;for(;s && *s && n<8191;s++)if(isalnum((unsigned char)*s))plain[n++]=(char)tolower((unsigned char)*s);plain[n]=0;return strstr(plain,"memorycard")!=NULL || strstr(plain,"memcard")!=NULL;}
static uint32_t r32(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static char modes[64][64];static int nmodes;
static void collect_modes(const ah_code*c){for(int i=0;c && i<c->count;i++){const ah_ins*x=&c->ins[i];if(x->op==AH_STRING_CONST && x->s && !strncmp(x->s,"MODE_",5)){int known=0;for(int k=0;k<nmodes;k++)known|=!strcmp(modes[k],x->s);if(!known){CHECK(nmodes<64);snprintf(modes[nmodes++],64,"%s",x->s);}}if(x->frame)collect_modes(x->frame);}}
static int export_png(void *user){const char *path=user;uint8_t pixel[4]={17,34,51,255};size_t n;void *b=tdefl_write_image_to_png_file_in_memory(pixel,1,1,4,&n);CHECK(b);int ok=plat_file_write(path,b,n);mz_free(b);return ok;}
static int export_failed(void *user){(void)user;return 0;}
static void storage_modes(gt4_vol*v,const char *disc){
 char directory[240],path[320],error[256];snprintf(directory,sizeof directory,"build/storage-audit/data-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);CHECK(gt4_save_create(7,"Storage validation"));
 menu *m=menu_open(v,"gtmode",error,sizeof error);CHECK(m);
 for(int k=0;k<nprojects;k++){snprintf(path,sizeof path,"/projects/%s/%s.adc",projects[k],projects[k]);size_t n;uint8_t*b=gt4_vol_read(v,path,&n);if(b){ah_code*c=ah_load(b,n,error,sizeof error);CHECK(c);collect_modes(c);ah_free(c);free(b);}}
 for(int k=0;k<nmodes;k++){
  ah_obj*manager=ah_module_new(m->vm,"storage_probe",NULL,NULL,NULL);ah_val args[]={ah_str(m->vm,modes[k]),ah_int(1)},ret,kind;
  CHECK(nat_pc_manager(m->vm,manager,m,2,args,&ret)>=0);
  const char *wanted=strstr(modes[k],"PHOTO")?"photos":strstr(modes[k],"REPLAY") || strstr(modes[k],"DEMONSTRATION")?"replays":"profiles";
  CHECK(ah_get_member(m->vm,manager,"_storage_kind",&kind) && !strcmp(ah_cstr(kind),wanted));
  const char *ops[]={"start","restart","requestPause","waitPause","isConnect","isFormat","isChanged","isAutoSaveDisabled","clearAutoSaveDisabled","isNoFile","isUseNewFile"};
  for(int i=0;i<sizeof ops/sizeof *ops;i++)CHECK(ah_call_member(m->vm,manager,ops[i],0,NULL,&ret)>=0);
  CHECK(ah_call_member(m->vm,manager,"getNewFile",0,NULL,&ret)>=0 && ret.t!=AV_NIL);
  ah_obj *file=ret.u.o;
  if(!strcmp(wanted,"profiles")){ah_val slot;CHECK(ah_get_member(m->vm,file,"_slot",&slot) && ah_num(slot)==7);}
  if(!strcmp(wanted,"photos")){ah_val object;CHECK(ah_get_member(m->vm,file,"objectName",&object) && !strcmp(ah_cstr(object),"FilePhotoPicture"));}
 }
 char photos[320],png[360];snprintf(photos,sizeof photos,"%s/photos",directory);CHECK(gt4_save_mkdir(photos));snprintf(png,sizeof png,"%s/export.png",photos);
 menu_set_photo_saver(m,export_failed,NULL);ah_obj *photo_manager=ah_module_new(m->vm,"photo_probe",NULL,NULL,NULL);ah_val args[]={ah_str(m->vm,"MODE_SAVE_PHOTO_FILM"),ah_int(99)},ret;
 CHECK(nat_pc_manager(m->vm,photo_manager,m,2,args,&ret)>=0);CHECK(ah_call_member(m->vm,photo_manager,"getNewFile",0,NULL,&ret)>=0);ah_obj *photo=ret.u.o;
 CHECK(ah_call_member(m->vm,photo,"saveNew",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"DEVICE_ERROR"));
 menu_set_photo_saver(m,export_png,png);CHECK(ah_call_member(m->vm,photo,"saveNew",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"OK"));
 int w,h;uint8_t*pixel=plat_png_read(png,&w,&h);CHECK(pixel && w==1 && h==1 && pixel[0]==17 && pixel[1]==34 && pixel[2]==51);free(pixel);
 CHECK(nat_album_widget(m->vm,NULL,"outUSBStorage",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"OK"));
 ah_obj *album=ah_module_new(m->vm,"album_probe",NULL,NULL,NULL);args[0]=ah_str(m->vm,"MODE_LOAD_PHOTO_FILM");CHECK(nat_pc_manager(m->vm,album,m,2,args,&ret)>=0);
 CHECK(ah_call_member(m->vm,album,"isNoFile",0,NULL,&ret)>=0 && !ah_truthy(ret));ah_val zero=ah_int(0);CHECK(ah_call_member(m->vm,album,"getFile",1,&zero,&ret)>=0);photo=ret.u.o;
 CHECK(ah_call_member(m->vm,photo,"load",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"OK"));
 CHECK(ah_call_member(m->vm,photo,"remove",0,NULL,&ret)>=0 && ah_truthy(ret));CHECK(!plat_file_open(png));
 static uint8_t tape[32];gt4_replay_file recording={0};strcpy(recording.course,"tsukuba");strcpy(recording.car,"ka_01");strcpy(recording.title,"Local storage probe");recording.steps=1;recording.tape=tape;recording.laps=1;menu_set_replay_to_save(&recording);
 ah_obj*replay=ah_module_new(m->vm,"replay_probe",NULL,NULL,NULL);args[0]=ah_str(m->vm,"MODE_SAVE_FULL_REPLAY");CHECK(nat_pc_manager(m->vm,replay,m,2,args,&ret)>=0);CHECK(ah_call_member(m->vm,replay,"getNewFile",0,NULL,&ret)>=0);ah_obj *file=ret.u.o;
 CHECK(ah_call_member(m->vm,file,"saveNew",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"OK"));
 args[0]=ah_str(m->vm,"MODE_LOAD_REPLAY");CHECK(nat_pc_manager(m->vm,replay,m,2,args,&ret)>=0);CHECK(ah_call_member(m->vm,replay,"getFile",1,&zero,&ret)>=0 && ret.t!=AV_NIL);file=ret.u.o;
 CHECK(ah_call_member(m->vm,file,"load",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"OK"));
 CHECK(ah_call_member(m->vm,file,"remove",0,NULL,&ret)>=0 && ah_truthy(ret));
 menu_set_replay_to_save(NULL);menu_close(m);gt4_save_shutdown();printf("PASS %d authored storage modes, legacy slot ignored, profile/media types, actual PNG/replay save-load-delete and export failure\n",nmodes);(void)disc;
}
int main(int argc,char **argv){CHECK(argc==2 || argc==3);const char *err=NULL;gt4_vol*v=gt4_vol_open(argv[1],&err);CHECK(v);CHECK(gt4_vol_list(v,"/projects",project,NULL));
 for(int k=0;k<nprojects;k++){char path[160],error[256];snprintf(path,sizeof path,"/projects/%s/US/rtext.rt2",projects[k]);size_t size;uint8_t*b=gt4_vol_read(v,path,&size);if(!b)continue;gt4_rtext rt;CHECK(gt4_rtext_open(&rt,b,size));ui_project*ui=ui_project_load(v,projects[k],error,sizeof error);
  for(unsigned i=0;i<rt.pages;i++){uint8_t *p=b+16+i*16;const char*page=(char*)b+r32(p);unsigned count=r32(p+4),off=r32(p+8);
   for(unsigned j=0;j<count;j++){const char*key=(char*)b+r32(b+off+j*8),*original=(char*)b+r32(b+off+j*8+4);const char *translated=ui?ui_translate(ui,0,page,key):ui_storage_text(original);checked++;
    if(old_text(original))legacy++;
    if(argc==3 && old_text(original))translated=original;
    if(old_text(translated)){fprintf(stderr,"legacy UI: %s / %s / %s -> %s\n",projects[k],page,key,translated);CHECK(0);}
   }
  }ui_project_free(ui);free(b);
 }storage_modes(v,argv[1]);printf("PASS %d UI strings across %d project directories, %d original storage references remapped\n",checked,nprojects,legacy);gt4_vol_close(v);return 0;
}
