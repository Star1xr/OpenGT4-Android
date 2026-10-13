#include "save.h"
#include "save_ps2.h"
#include "career_blob.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
static uint32_t rd32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void wr32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;i++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}return ~c;}
#ifdef _WIN32
static wchar_t *wide(const char *p){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,-1,NULL,0);if(!n)return NULL;wchar_t *w=malloc(n*sizeof *w);if(w)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,-1,w,n);return w;}
static FILE *openfile(const char *path,const char *mode){wchar_t *p=wide(path),*m=wide(mode);FILE *f=p && m?_wfopen(p,m):NULL;free(p);free(m);return f;}
static int replace(const char *src,const char *dst){wchar_t *s=wide(src),*d=wide(dst);int ok=s && d && MoveFileExW(s,d,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);free(s);free(d);return ok;}
#else
#define openfile fopen
static int replace(const char *src,const char *dst){return rename(src,dst)==0;}
#endif
int gt4_save_mkdir(const char *path){char p[1100];if(!path || strlen(path)>=sizeof p)return 0;strcpy(p,path);for(char *at=p;*at;at++)if((at>p+2 && (*at=='/' || *at=='\\')) || !at[1]){char c=*at;int end=!at[1];if(!end)*at=0;
#ifdef _WIN32
 wchar_t *w=wide(p);int ok=w && (CreateDirectoryW(w,NULL) || GetLastError()==ERROR_ALREADY_EXISTS);free(w);
#else
 int ok=mkdir(p,0700)==0 || errno==EEXIST;
#endif
 if(!end)*at=c;if(!ok)return 0;}return 1;}
static int read_one(const char *path,uint8_t **data,size_t *size,uint64_t *stamp){*data=NULL;*size=0;FILE *f=openfile(path,"rb");if(!f)return errno==ENOENT?0:-1;uint8_t h[32];int status=-2;
 if(fread(h,1,32,f)!=32 || memcmp(h,"OGT4SAVE",8))goto done;
 if(rd32(h+8)!=1){status=-3;goto done;}size_t n=rd32(h+12);if(!n || n>GT4_SAVE_LIMIT || rd32(h+28)!=crc(h,28))goto done;
 uint8_t *p=malloc(n);if(!p){status=-1;goto done;}if(fread(p,1,n,f)!=n || fgetc(f)!=EOF || ferror(f) || crc(p,n)!=rd32(h+16)){free(p);goto done;}
 *data=p;*size=n;if(stamp)*stamp=(uint64_t)rd32(h+20)|((uint64_t)rd32(h+24)<<32);status=1;
 done:fclose(f);return status;}
int gt4_save_read(const char *path,uint8_t **data,size_t *size,uint64_t *stamp){int status=read_one(path,data,size,stamp);if(status==1 || status==-3)return status;char bak[1200];if(snprintf(bak,sizeof bak,"%s.bak",path)>=(int)sizeof bak)return -1;int backup=read_one(bak,data,size,stamp);return backup==1?2:status;}
static int atomic_file(const char *path,const uint8_t *p,size_t n){char tmp[1200];
#ifdef _WIN32
 unsigned pid=GetCurrentProcessId();
#else
 unsigned pid=(unsigned)getpid();
#endif
 if(snprintf(tmp,sizeof tmp,"%s.tmp.%u",path,pid)>=(int)sizeof tmp)return 0;FILE *f=openfile(tmp,"wb");if(!f)return 0;int ok=fwrite(p,1,n,f)==n && fflush(f)==0;
#ifdef _WIN32
 if(ok)ok=_commit(_fileno(f))==0;
#else
 if(ok)ok=fsync(fileno(f))==0;
#endif
 if(fclose(f)!=0)ok=0;return ok && replace(tmp,path);}
int gt4_save_write(const char *path,const uint8_t *data,size_t size){if(!data || !size || size>GT4_SAVE_LIMIT)return 0;uint8_t *record=malloc(size+32);if(!record)return 0;memset(record,0,32);memcpy(record,"OGT4SAVE",8);wr32(record+8,1);wr32(record+12,(uint32_t)size);wr32(record+16,crc(data,size));uint64_t stamp=(uint64_t)time(NULL);wr32(record+20,(uint32_t)stamp);wr32(record+24,(uint32_t)(stamp>>32));wr32(record+28,crc(record,28));memcpy(record+32,data,size);
 uint8_t *old;size_t old_size;uint64_t old_stamp;int status=read_one(path,&old,&old_size,&old_stamp);int ok=1;
 if(status==-3 || status==-1)ok=0; /* Never replace an unreadable/newer save. */
 if(status==1){char bak[1200];if(snprintf(bak,sizeof bak,"%s.bak",path)>=(int)sizeof bak)ok=0;else {FILE *f=openfile(path,"rb");uint8_t *raw=malloc(old_size+32);ok=f && raw && fread(raw,1,old_size+32,f)==old_size+32;if(f)fclose(f);if(ok)ok=atomic_file(bak,raw,old_size+32);free(raw);}}free(old);
 if(ok)ok=atomic_file(path,record,size+32);free(record);return ok;}
static struct {char dir[1100],error[256];int slot,dirty;ah_vm *vm;ah_obj *game;} session;
static void error(const char *s){snprintf(session.error,sizeof session.error,"%s",s);}
static int slotpath(int slot,char *p,size_t n){return session.dir[0] && slot>=1 && slot<=GT4_SAVE_SLOTS && snprintf(p,n,"%s/slot%02d.ogt4",session.dir,slot)<(int)n;}
int gt4_save_valid_name(const char *s){if(!s || !*s || strlen(s)>96)return 0;int chars=0,visible=0;const unsigned char *p=(const unsigned char *)s;while(*p){unsigned c=*p++,cp=c;int n=c<128?0:(c>=0xc2 && c<=0xdf)?1:(c>=0xe0 && c<=0xef)?2:(c>=0xf0 && c<=0xf4)?3:-1;if(n<0 || c<32 || c==127)return 0;cp=c&((1u<<(6-n))-1);if(!n)cp=c;for(int i=0;i<n;i++){unsigned next=*p++;if((next&0xc0)!=0x80)return 0;cp=(cp<<6)|(next&63);}if((n==2 && cp<0x800) || (n==3 && cp<0x10000) || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return 0;visible|=cp!=32;if(++chars>24)return 0;}return visible;}
#define valid_name gt4_save_valid_name
static void fresh(const char *name){if(session.vm)ah_vm_free(session.vm);session.vm=ah_vm_new();session.game=ah_module_new(session.vm,"game",ah_root(session.vm),NULL,NULL);ah_set_member(session.vm,ah_root(session.vm),"game",ah_objv(session.game));ah_set_member(session.vm,session.game,"username",ah_str(session.vm,name));ah_set_member(session.vm,session.game,"money",ah_int(10000));ah_obj *calendar=ah_module_new(session.vm,"calendar",NULL,NULL,NULL),*date=ah_array_new(session.vm);ah_array_push(session.vm,date,ah_int(2005));ah_array_push(session.vm,date,ah_int(4));ah_array_push(session.vm,date,ah_int(2));ah_set_member(session.vm,calendar,"today",ah_objv(date));ah_set_member(session.vm,calendar,"elapsed_days",ah_int(0));ah_set_member(session.vm,session.game,"calendar",ah_objv(calendar));const char *names[]={"garage","race_record","license_record","course_record","favorite_car","favorite_course","presents","championship","used_car","statistics","available"};for(unsigned i=0;i<sizeof names/sizeof names[0];i++){ah_obj *o=ah_module_new(session.vm,names[i],NULL,NULL,NULL);ah_set_member(session.vm,session.game,names[i],ah_objv(o));if(i==0){ah_set_member(session.vm,o,"cars",ah_objv(ah_array_new(session.vm)));ah_set_member(session.vm,o,"riding_index",ah_int(-1));}if(i==1 || i==2)ah_set_member(session.vm,o,"records",ah_objv(ah_array_new(session.vm)));}}
static int active_index(int slot){char path[1200];snprintf(path,sizeof path,"%s/active.ogt4",session.dir);uint8_t b=(uint8_t)slot;return gt4_save_write(path,&b,1);}
void gt4_save_shutdown(void){if(session.vm)ah_vm_free(session.vm);memset(&session,0,sizeof session);}
void gt4_save_init(const char *directory){gt4_save_shutdown();if(directory && strlen(directory)<sizeof session.dir)strcpy(session.dir,directory);fresh("Player");if(!session.dir[0])return;if(!gt4_save_mkdir(session.dir)){error("The save folder could not be created.");return;}char path[1200];snprintf(path,sizeof path,"%s/active.ogt4",session.dir);uint8_t *data;size_t size;if(gt4_save_read(path,&data,&size,NULL)>0){int slot=size==1?data[0]:0;free(data);if(slot)gt4_save_load_slot(slot);}}
int gt4_save_active(void){return session.slot;}
int gt4_save_dirty(void){return session.dirty;}
const char *gt4_save_error(void){return session.error;}
int gt4_save_info_get(int slot,gt4_save_info *info){memset(info,0,sizeof *info);char path[1200];if(!slotpath(slot,path,sizeof path))return 0;uint8_t *data;size_t size;int status=gt4_save_read(path,&data,&size,&info->timestamp);info->status=status;if(status<=0)return status;ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",NULL,NULL,NULL);if(!gt4_state_merge(vm,game,data,size)){info->status=-2;}else {ah_val v;if(ah_get_member(vm,game,"username",&v) && ah_cstr(v) && valid_name(ah_cstr(v)))snprintf(info->name,sizeof info->name,"%s",ah_cstr(v));else info->status=-2;}free(data);ah_vm_free(vm);return info->status;}
int gt4_save_load_slot(int slot){char path[1200];if(!slotpath(slot,path,sizeof path)){error("Invalid save slot.");return 0;}uint8_t *data;size_t size;int status=gt4_save_read(path,&data,&size,NULL);if(status<=0){error(status==-3?"This save requires a newer OpenGT4 version.":"The save could not be loaded.");return 0;}ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);ah_set_member(vm,ah_root(vm),"game",ah_objv(game));ah_val name;int ok=gt4_state_merge(vm,game,data,size) && ah_get_member(vm,game,"username",&name) && valid_name(ah_cstr(name));free(data);if(!ok){ah_vm_free(vm);error("The save contains invalid game data.");return 0;}if(!active_index(slot)){ah_vm_free(vm);error("The selected profile could not be recorded.");return 0;}ah_vm_free(session.vm);session.vm=vm;session.game=game;session.slot=slot;session.dirty=0;error(status==2?"Loaded the backup because the main save was damaged.":"");return 1;}
int gt4_save_read_profile(ah_vm *vm,ah_obj *game,int slot){
 char path[1200];uint8_t *data=NULL;size_t size=0;
 if(!slotpath(slot,path,sizeof path) || gt4_save_read(path,&data,&size,NULL)<=0)return 0;
 ah_val name;int ok=gt4_state_merge(vm,game,data,size) && ah_get_member(vm,game,"username",&name) && valid_name(ah_cstr(name));
 free(data);return ok;
}
int gt4_save_store(void){char path[1200];if(!slotpath(session.slot,path,sizeof path)){error("Create or load a player profile first.");return 0;}uint8_t *data;size_t size;if(!gt4_state_pack(session.vm,session.game,&data,&size)){error("The current game state could not be serialized.");return 0;}int ok=gt4_save_write(path,data,size);free(data);if(ok){session.dirty=0;error("");}else error("The save could not be written. Your previous save was preserved.");return ok;}
int gt4_save_create(int slot,const char *name){char path[1200];gt4_save_info info;if(!valid_name(name)){error("Enter a player name (1 to 24 characters).");return 0;}if(!slotpath(slot,path,sizeof path) || gt4_save_info_get(slot,&info)!=0){error("Choose an empty save slot.");return 0;}ah_vm *old=session.vm;ah_obj *game=session.game;int previous=session.slot;session.vm=NULL;fresh(name);ah_val options;if(ah_get_member(old,game,"option",&options)){ah_obj *seed=ah_module_new(old,"seed",NULL,NULL,NULL);ah_set_member(old,seed,"option",options);uint8_t *b;size_t n;if(gt4_state_pack(old,seed,&b,&n)){gt4_state_merge(session.vm,session.game,b,n);free(b);}}session.slot=slot;if(!gt4_save_store() || !active_index(slot)){ah_vm_free(session.vm);session.vm=old;session.game=game;session.slot=previous;error("The new profile could not be activated.");return 0;}ah_vm_free(old);session.dirty=0;return 1;}
int gt4_save_rename(const char *name){if(!session.slot || !valid_name(name)){error("Enter a valid player name.");return 0;}ah_val old;ah_get_member(session.vm,session.game,"username",&old);ah_set_member(session.vm,session.game,"username",ah_str(session.vm,name));if(gt4_save_store())return 1;ah_set_member(session.vm,session.game,"username",old);return 0;}
int gt4_save_delete_slot(int slot){char path[1200],deleted[1250],bak[1250],bak_deleted[1300];if(!slotpath(slot,path,sizeof path)){error("Invalid save slot.");return 0;}snprintf(deleted,sizeof deleted,"%s.deleted",path);snprintf(bak,sizeof bak,"%s.bak",path);snprintf(bak_deleted,sizeof bak_deleted,"%s.bak",deleted);FILE *f=openfile(deleted,"rb");if(f){fclose(f);error("A deleted save already occupies the recovery file.");return 0;}f=openfile(bak_deleted,"rb");if(f){fclose(f);error("A deleted backup already occupies the recovery file.");return 0;}int had_backup=0;f=openfile(bak,"rb");if(f){fclose(f);if(!replace(bak,bak_deleted)){error("The backup could not be moved. The profile was kept.");return 0;}had_backup=1;}f=openfile(path,"rb");int had_main=f!=NULL;if(f)fclose(f);if(!had_main && !had_backup){error("Choose an existing savegame.");return 0;}if(had_main && !replace(path,deleted)){if(had_backup)replace(bak_deleted,bak);error("The profile could not be removed.");return 0;}if(slot==session.slot && !active_index(0)){if(had_main)replace(deleted,path);if(had_backup)replace(bak_deleted,bak);error("The active profile could not be cleared. The save was kept.");return 0;}if(slot==session.slot){session.slot=0;session.dirty=0;fresh("Player");}error("");return 1;}
/* A cached menu VM must not overwrite either playlist written after its
   restore. 0033C278 writes the race jukebox; slideshow uses the same
   original 0x810-byte playlist layout. Versioning is native persistence P. */
static unsigned playlist_version;
static struct {ah_vm *vm;unsigned seen;} playlist_seen[16];
static unsigned *seen_by(ah_vm *vm)
{
    for(int i=0;i<16;i++)if(playlist_seen[i].vm==vm)return &playlist_seen[i].seen;
    for(int i=0;i<16;i++)if(!playlist_seen[i].vm){playlist_seen[i].vm=vm;playlist_seen[i].seen=playlist_version;return &playlist_seen[i].seen;}
    return NULL;
}
int gt4_save_capture(ah_vm *vm,ah_obj *game,int dirty){if(!session.vm)return 1;
 const char *names[]={"bgm_playlist_ps2","slide_playlist_ps2"};
 unsigned *seen=seen_by(vm);unsigned char playlists[2][0x810];int keep[2]={0};
 if(seen && *seen!=playlist_version)for(int k=0;k<2;k++)keep[k]=gt4_save_option_blob_get(names[k],playlists[k],sizeof playlists[k]);
 uint8_t *data;size_t size;if(!gt4_state_pack(vm,game,&data,&size))return 0;int ok=gt4_state_merge(session.vm,session.game,data,size);free(data);
 unsigned version=playlist_version;
 for(int k=0;k<2;k++)if(keep[k]){ah_val option;gt4_save_option_blob_set(names[k],playlists[k],sizeof playlists[k]);
  if(ah_get_member(vm,game,"option",&option))gt4_career_blob_set(vm,ah_as_obj(option,AO_MODULE),names[k],playlists[k],sizeof playlists[k]);}
 playlist_version=version;if(seen)*seen=version;
 if(ok && dirty)session.dirty=1;ah_gc(session.vm);return ok;}
void gt4_save_restore(ah_vm *vm,ah_obj *game){if(!session.vm)return;{unsigned *seen=seen_by(vm);if(seen)*seen=playlist_version;}uint8_t *data;size_t size;if(gt4_state_pack(session.vm,session.game,&data,&size)){if(session.slot)gt4_state_replace(vm,game,data,size);else gt4_state_merge(vm,game,data,size);free(data);}ah_gc(session.vm);}
int gt4_save_restore_options(ah_vm *vm,ah_obj *game){
 ah_val option;if(!session.vm || !ah_get_member(session.vm,session.game,"option",&option))return 0;
 ah_obj *seed=ah_module_new(session.vm,"preferences",NULL,NULL,NULL);
 ah_set_member(session.vm,seed,"option",option);uint8_t *data;size_t size;
 int ok=0;if(gt4_state_pack(session.vm,seed,&data,&size)){ok=gt4_state_merge(vm,game,data,size);free(data);}
 ah_gc(session.vm);return ok;
}
int gt4_save_auto_enabled(void){ah_val options,value;return session.vm && ah_get_member(session.vm,session.game,"option",&options) && ah_get_member(session.vm,ah_as_obj(options,AO_MODULE),"auto_save",&value) && ah_truthy(value);}

int gt4_save_import_ps2(int slot,const char *path){gt4_save_info info;char native_path[1200];if(!slotpath(slot,native_path,sizeof native_path) || gt4_save_info_get(slot,&info)!=0){error("Choose an empty save slot for import.");return 0;}ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);ah_set_member(vm,ah_root(vm),"game",ah_objv(game));ah_val option;if(ah_get_member(session.vm,session.game,"option",&option)){ah_obj *seed=ah_module_new(session.vm,"seed",NULL,NULL,NULL);ah_set_member(session.vm,seed,"option",option);uint8_t *data;size_t size;if(gt4_state_pack(session.vm,seed,&data,&size)){gt4_state_merge(vm,game,data,size);free(data);}}if(!gt4_ps2_read(path,vm,game,session.error,sizeof session.error)){ah_vm_free(vm);return 0;}uint8_t *data;size_t size;if(!gt4_state_pack(vm,game,&data,&size)){ah_vm_free(vm);error("This imported save exceeds the supported size.");return 0;}int ok=gt4_save_write(native_path,data,size);free(data);if(!ok){ah_vm_free(vm);error("The imported profile could not be written. The original PS2 save was kept.");return 0;}if(!active_index(slot)){ah_vm_free(vm);error("Save imported, but could not be activated. Select its card to load it.");return 0;}ah_vm_free(session.vm);session.vm=vm;session.game=game;session.slot=slot;session.dirty=0;error("");return 1;}

double gt4_save_option_number(const char *name,double fallback){ah_val options,value;if(session.vm && ah_get_member(session.vm,session.game,"option",&options) && ah_get_member(session.vm,ah_as_obj(options,AO_MODULE),name,&value) && ah_is_num(value))return ah_num(value);return fallback;}
int gt4_save_option_array_get(const char *name,int *data,size_t count)
{
    ah_val options,value;ah_obj *array;
    if(!session.vm || !ah_get_member(session.vm,session.game,"option",&options) ||
       !ah_get_member(session.vm,ah_as_obj(options,AO_MODULE),name,&value) ||
       !(array=ah_as_obj(value,AO_ARRAY)) || ah_array_size(array)!=(int)count)return 0;
    for(size_t i=0;i<count;i++)if(!ah_is_num(ah_array_get(array,(int)i)))return 0;
    for(size_t i=0;i<count;i++)data[i]=(int)ah_num(ah_array_get(array,(int)i));
    return 1;
}
int gt4_save_option_blob_get(const char *name,unsigned char *data,size_t size)
{
    ah_val option;
    return session.vm && ah_get_member(session.vm,session.game,"option",&option) &&
        gt4_career_blob_get(session.vm,ah_as_obj(option,AO_MODULE),name,data,size);
}
int gt4_save_option_blob_set(const char *name,const unsigned char *data,size_t size)
{
    if(!session.vm)return 0;
    ah_val value;ah_obj *option;
    if(ah_get_member(session.vm,session.game,"option",&value))option=ah_as_obj(value,AO_MODULE);
    else {option=ah_module_new(session.vm,"option",NULL,NULL,NULL);ah_set_member(session.vm,session.game,"option",ah_objv(option));}
    int ok=option && gt4_career_blob_set(session.vm,option,name,data,size);
    if(ok)session.dirty=1;
    if(ok && (!strcmp(name,"bgm_playlist_ps2") || !strcmp(name,"slide_playlist_ps2")))playlist_version++;
    return ok;
}
const char *gt4_save_directory(void){return session.dir;}
const char *gt4_save_player(void){ah_val v;if(!session.vm || !session.game || !ah_get_member(session.vm,session.game,"username",&v) || !ah_cstr(v))return "";return ah_cstr(v);}
