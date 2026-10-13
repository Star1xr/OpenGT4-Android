/* Labomode's authored PhotoRoot / SlideRoot, backed by native PNG exports.
   PS2 card I/O (001C65B8/001C6918) is replaced at the platform boundary. */
static plat_directory_entry album_files[GT4_ALBUM_CAPACITY];static int album_files_count;
static char album_storage[1024];
static int album_is_film(const char *name){return name && strlen(name)>9 && !strcmp(name+strlen(name)-9,".ogt4film");}
static int album_path(char *path,size_t size,const char *name)
{
 if(!name || !*name || strchr(name,'/') || strchr(name,'\\') || strstr(name,".."))return 0;
 snprintf(path,size,"%s/%s/%s",gt4_save_directory(),album_is_film(name)?"films":"photos",name);return 1;
}
static int album_film(const char *name,gt4_film *film){char path[1200];uint8_t *bytes=NULL;size_t n=0;
 int ok=album_path(path,sizeof path,name) && gt4_save_read(path,&bytes,&n,NULL)>0 && gt4_media_film_decode(bytes,n,film);free(bytes);return ok;
}
static void photo_playlist_load(void)
{
 char path[1024];snprintf(path,sizeof path,"%s/photos/slideshow.list",gt4_save_directory());
 if(photo_playlist_read && !strcmp(path,album_storage))return;
 snprintf(album_storage,sizeof album_storage,"%s",path);photo_playlist_read=1;photo_playlist_count=0;
 plat_file *f=plat_file_open(path);if(!f)return;
 uint64_t size=plat_file_size(f);if(size<=sizeof photo_playlist_names && !(size%64) && plat_file_read_at(f,0,photo_playlist_names,(size_t)size)==size){
  for(int k=0;k<(int)(size/64);k++)if(memchr(photo_playlist_names[k],0,64) && album_path(path,sizeof path,photo_playlist_names[k]))photo_playlist_count++;else break;
 }plat_file_close(f);
}
static void album_refresh(void)
{char dir[1024];snprintf(dir,sizeof dir,"%s/photos",gt4_save_directory());album_files_count=plat_directory_list(dir,".png",album_files,GT4_ALBUM_CAPACITY);photo_playlist_load();}
static ah_obj *album_playlist(menu *m)
{
 for(int k=0;k<ui_root_count(m->ui);k++)if(!strcmp(ui_root_name(m->ui,k),"PhotoRoot")){
  ah_val list;ah_obj *page=widget_module(m,ui_root_widget(m->ui,k));
  if(ah_get_member(m->vm,page,"playlist",&list) && ah_as_obj(list,AO_MODULE)){
   ah_set_member(m->vm,list.u.o,"_photo_playlist",ah_bool(1));ah_set_member(m->vm,list.u.o,"size",ah_int(photo_playlist_count));return list.u.o;
  }
 }return NULL;
}
static int album_load_image(menu *m,const char *name,const char *key,int thumbnail,int *w,int *h)
{
 char path[1024];if(!album_path(path,sizeof path,name))return 0;
 uint8_t *px=plat_png_read(path,w,h);if(!px)return 0;
 if(thumbnail){
  /* P: exported PNG thumbnail; original PhotoItem handles layout/portrait. */
  uint8_t thumb_pixels[64*64*4];for(int y=0;y<64;y++)for(int x=0;x<64;x++)memcpy(thumb_pixels+(y*64+x)*4,px+((size_t)(y*(*h)/64)*(*w)+x*(*w)/64)*4,4);
  ui_image_put(m->ui,key,64,64,thumb_pixels);
 }else ui_image_put(m->ui,key,*w,*h,px);
 free(px);return 1;
}
static int album_file_op(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
 (void)argc;(void)argv;const char *op=data,*name=file_name_of(vm,ah_objv(self));char path[1024];
 if(!strcmp(op,"saveNew")){menu *m=ah_user(vm);int ok=m->photo_saver && m->photo_saver(m->photo_saver_user);*ret=ah_str(vm,ok?"OK":"DEVICE_ERROR");return 0;}
 if(!strcmp(op,"getInfo")){*ret=ah_objv(self);return 0;}
 if(!strcmp(op,"remove")){*ret=ah_bool(album_path(path,sizeof path,name) && plat_file_remove(path));return 0;}
 if(!strcmp(op,"load") || !strcmp(op,"loadData")){
  if(album_is_film(name)){gt4_film film;int ok=album_film(name,&film);if(ok && !strcmp(op,"load"))snprintf(film_loaded,sizeof film_loaded,"%s",name);*ret=ah_str(vm,ok?"OK":"INVALID_FILE");return 0;}
  int w,h;uint8_t *px=album_path(path,sizeof path,name)?plat_png_read(path,&w,&h):NULL;
  *ret=ah_str(vm,px?"OK":"INVALID_FILE");free(px);return 0;
 }
 if(!strcmp(op,"isExist")){plat_file *f=album_path(path,sizeof path,name)?plat_file_open(path):NULL;*ret=ah_bool(f!=NULL);plat_file_close(f);return 0;}
 *ret=ah_bool(1);return 0;
}
static ah_obj *album_file_object(menu *m,const char *name)
{
 ah_vm *vm=m->vm;ah_val cache_value,existing;ah_obj *cache=NULL;
 if(ah_get_member(vm,m->project,"_native_album_files",&cache_value))cache=ah_as_obj(cache_value,AO_MODULE);
 if(!cache){cache=ah_module_new(vm,"native_album_files",NULL,NULL,NULL);ah_set_member(vm,m->project,"_native_album_files",ah_objv(cache));}
 if(ah_get_own_member(vm,cache,name,&existing))return ah_as_obj(existing,AO_MODULE);
 /* Retain decoded thumbnail/info across the authored visible/update callbacks.
    Full images still occupy only the preview and two slideshow texture slots. */
 ah_obj *o=ah_module_new(vm,"native_photo_file",NULL,NULL,NULL);int w=0,h=0;char key[160],date[32]="";
 snprintf(key,sizeof key,"pc/photo/%s",name);gt4_film film;int isfilm=album_is_film(name);
 int valid=isfilm?album_film(name,&film):album_load_image(m,name,key,1,&w,&h);
 if(isfilm && valid){w=film.width;h=film.height;}
 ah_set_member(vm,o,"_name",ah_str(vm,name));ah_set_member(vm,o,"objectName",ah_str(vm,isfilm?"FilePhotoFilm":"FilePhotoPicture"));
 for(int k=0;k<album_files_count;k++)if(!strcmp(album_files[k].name,name)){
  time_t t=(time_t)album_files[k].modified;struct tm *tm=localtime(&t);if(tm){strftime(date,sizeof date,"%d/%m/%y %H:%M",tm);}
  ah_set_member(vm,o,"size",ah_int((album_files[k].bytes+1023)/1024));break;
 }
 ah_set_member(vm,o,"title",ah_str(vm,name));ah_set_member(vm,o,"date",ah_str(vm,date));ah_set_member(vm,o,"time",ah_str(vm,""));
 ah_set_member(vm,o,"carName",ah_str(vm,isfilm && valid?film.car[0].code:""));ah_set_member(vm,o,"courseName",ah_str(vm,isfilm && valid?film.course:""));ah_set_member(vm,o,"portrait",ah_int(h>w));ah_set_member(vm,o,"dispstep",ah_int(0));
 if(valid && !isfilm)ah_set_member(vm,o,"_icon",ah_str(vm,key));
 const char *info[]={"isInfoValid","isIconValid","getIconTexture"};
 for(int k=0;k<3;k++)ah_set_member(vm,o,info[k],ah_bind(vm,o,ah_native(vm,info[k],nat_rp_info,(void *)info[k])));
 const char *ops[]={"getInfo","loadData","load","remove","isExist","setNewFile","saveNew"};
 for(int k=0;k<sizeof ops/sizeof *ops;k++)ah_set_member(vm,o,ops[k],ah_bind(vm,o,ah_native(vm,ops[k],album_file_op,(void *)ops[k])));
 ah_set_member(vm,cache,name,ah_objv(o));return o;
}
static void album_manager_files(menu *m,ah_obj *self,const char *mode)
{
 album_refresh();album_playlist(m);ah_obj *files=ah_array_new(m->vm);
 if(mode && strstr(mode,"FILM")){
  char dir[1100];snprintf(dir,sizeof dir,"%s/films",gt4_save_directory());
  album_files_count=plat_directory_list(dir,".ogt4film",album_files,GT4_ALBUM_CAPACITY);
 }
 if(strstr(mode,"PLAYLIST")){
  for(int k=0;k<photo_playlist_count;k++)for(int i=0;i<album_files_count;i++)if(!strcmp(photo_playlist_names[k],album_files[i].name)){ah_array_push(m->vm,files,ah_str(m->vm,photo_playlist_names[k]));break;}
 }else if(!strstr(mode,"PRINT")){
  for(int k=0;k<album_files_count;k++)ah_array_push(m->vm,files,ah_str(m->vm,album_files[k].name));
 }
 ah_set_member(m->vm,self,"_files",ah_objv(files));ah_set_member(m->vm,self,"count",ah_int(ah_array_size(files)));
}
static int album_manager_op(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
 menu *m=ah_user(vm);const char *op=data;ah_val mode,files;ah_get_member(vm,self,"_mode",&mode);
 if(!strcmp(op,"getNewFile")){*ret=ah_objv(album_file_object(m,""));return 0;}
 if(!strcmp(op,"isUseNewFile")){*ret=ah_bool(ah_cstr(mode) && strstr(ah_cstr(mode),"SAVE"));return 0;}
 if(!strcmp(op,"getFile")){
  ah_get_member(vm,self,"_files",&files);const char *name=ah_cstr(ah_array_get(ah_as_obj(files,AO_ARRAY),argc?to_int(argv[0]):-1));
  *ret=name?ah_objv(album_file_object(m,name)):ah_nil();return 0;
 }
 if(!strcmp(op,"start") || !strcmp(op,"restart") || !strcmp(op,"clear")){album_manager_files(m,self,ah_cstr(mode));*ret=ah_nil();return 0;}
 if(!strcmp(op,"isNoFile")){ah_val count;ah_get_member(vm,self,"count",&count);*ret=ah_bool(!ah_num(count));return 0;}
 *ret=ah_bool(strcmp(op,"isChanged") && strcmp(op,"isAutoSaveDisabled") && strcmp(op,"isUseNewFile"));return 0;
}
static int album_playlist_save(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
 (void)self;(void)data;(void)argc;(void)argv;photo_playlist_load();
 *ret=ah_str(vm,plat_file_write(album_storage,photo_playlist_names,(size_t)photo_playlist_count*64)?"OK":"DEVICE_ERROR");return 0;
}
static void album_manager(ah_vm *vm,ah_obj *self,const char *mode)
{
 menu *m=ah_user(vm);replay_manager(vm,self,mode); /* shared card status/progress only */
 const char *ops[]={"getFile","getNewFile","start","restart","clear","isNoFile","isChanged","isAutoSaveDisabled","isUseNewFile"};
 for(int k=0;k<sizeof ops/sizeof *ops;k++)ah_set_member(vm,self,ops[k],ah_bind(vm,self,ah_native(vm,ops[k],album_manager_op,(void *)ops[k])));
 ah_val autoload;ah_get_member(vm,self,"autoloadPlayList",&autoload);
 ah_set_member(vm,autoload.u.o,"save",ah_bind(vm,autoload.u.o,ah_native(vm,"save",album_playlist_save,NULL)));
 album_manager_files(m,self,mode);ah_set_member(vm,self,"list",ah_objv(album_playlist(m)));
}
static int album_next(menu *m,int slot)
{
 for(int tries=0;tries<m->album_count;tries++){
  int i=m->album_order[m->album_at++];if(m->album_at>=m->album_count)m->album_at=0;
  char key[32];snprintf(key,sizeof key,"pc/slide/%d",slot);int w,h;
  if(album_load_image(m,photo_playlist_names[i],key,0,&w,&h)){
   /* 001AAFD0 +60..78 and 001C5E88 +A4..B8 draw direction, then zoom.
      P: timer seed belongs to the native host; same original RNG thereafter. */
   int reverse=gt4_pcm_music_random_unit(&m->album_random)<.5f;
   float random1=gt4_pcm_music_random_unit(&m->album_random),random2=gt4_pcm_music_random_unit(&m->album_random);
   gt4_album_slide_init(&m->album_layer[slot],w,h,(int)menu_option_number(m,"album_slide_play_time",8),(int)menu_option_number(m,"album_slide_transition_time",4),
    menu_option_number(m,"album_slide_effect",1)!=0,reverse,random1,random2);
   fprintf(stderr,"album slide: %s (%dx%d)\n",photo_playlist_names[i],w,h);return 1;
  }
 }return 0;
}
static int nat_album_widget(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
 (void)self;menu *m=ah_user(vm);const char *op=data;*ret=ah_nil();
 if(!strcmp(op,"outUSBStorage")){int ok=m->photo_saver && m->photo_saver(m->photo_saver_user);*ret=ah_str(vm,ok?"OK":"DEVICE_ERROR");return 0;}
 if(!strcmp(op,"doStop")){m->album_playing=0;return 0;}
 if(!strcmp(op,"doPlay")){
  album_refresh();m->album_count=photo_playlist_count;m->album_at=0;m->album_front=0;
  /* P: host wall clock seeds the original RNG; PS2 clock seed is unavailable. */
  if(!m->album_random)m->album_random=(uint32_t)time(NULL);
  for(int k=0;k<m->album_count;k++)m->album_order[k]=k;
  /* 001C6BE8 +B0..108 uses the selected order until the next play. */
  if(m->album_shuffle)gt4_album_slide_shuffle(m->album_order,m->album_count,&m->album_random);
  memset(m->album_layer,0,sizeof m->album_layer);m->album_layer[1].state=5;
  m->album_playing=m->album_count && album_next(m,0);return 0;
 }
 if(!strcmp(op,"setPhotoFile")){
  const char *name=argc?file_name_of(vm,argv[0]):NULL;
  if(!name || !album_load_image(m,name,"pc/album/preview",0,&m->album_preview_w,&m->album_preview_h))return ah_error(vm,"album photo unavailable");
  m->photo.image_width=m->album_preview_w;m->photo.image_height=m->album_preview_h;m->photo.image_scale=1;return 0;
 }
 /* PNG exports are already developed pictures; allocation/export are host I/O. */
 return 0;
}
static void album_tick(menu *m)
{
 if(!m->album_playing)return;
 if(strcmp(ui_root_name(m->ui,menu_active_root(m)),"SlideRoot")){m->album_playing=0;return;}
 int front=m->album_front,back=1-front;
 /* 001AAFD0/001C7358 double buffers: new slide starts when front is
    waiting and back is free. Fading the outgoing layer advances both. */
 if(m->album_layer[front].state==2 && m->album_layer[back].state==5){
  if(album_next(m,back)){gt4_album_slide_out(&m->album_layer[front]);m->album_front=back;}
  else m->album_playing=0;
 }
 gt4_album_slide_tick(&m->album_layer[0]);gt4_album_slide_tick(&m->album_layer[1]);
}
static int album_draw(void *user,const ui_widget *widget,float x,float y,float w,float h,float alpha)
{
 menu *m=user;float v[4]={x,y,w,h};uint8_t black[4]={0,0,0,(uint8_t)(255*alpha)};plat_draw_quad(NULL,x,y,w,h,0,0,1,1,black);
 if(!strcmp(widget->type,"SlideShowFace")){
  for(int k=0;k<2;k++){int slot=k?m->album_front:1-m->album_front;gt4_album_rect r;
   if(gt4_album_slide_rect(&m->album_layer[slot],v,&r)){char key[32];snprintf(key,sizeof key,"pc/slide/%d",slot);ui_image_draw(m->ui,key,r.x,r.y,r.w,r.h,alpha*r.alpha/255.f);}
  }return 1;
 }
 if(m->album_preview_w>0 && m->album_preview_h>0){
  float scale=fminf(w/m->album_preview_w,h/m->album_preview_h)*m->photo.image_scale;
  float iw=m->album_preview_w*scale,ih=m->album_preview_h*scale;
  ui_image_draw(m->ui,"pc/album/preview",x+(w-iw)*.5f+m->photo.image_x,y+(h-ih)*.5f+m->photo.image_y,iw,ih,alpha);return 1;
 }return 0;
}

static int album_music_list(ah_vm *vm,ah_obj *self,void *d,int argc,const ah_val *argv,ah_val *ret)
{
 (void)self;(void)d;(void)argc;(void)argv;menu *m=ah_user(vm);ah_obj *list=ah_array_new(vm);
 unsigned n=gt4_pcm_music_count(m->vol,"racing");
 for(unsigned i=0;i<n;i++){char info[3][128];if(!gt4_pcm_music_info(m->vol,i,info))continue;ah_obj *row=ah_array_new(vm);
  for(int k=0;k<3;k++)ah_array_push(vm,row,ah_str(vm,info[k]));ah_array_push(vm,row,ah_int(i));ah_array_push(vm,list,ah_objv(row));
 }*ret=ah_objv(list);return 0;
}
/* The Jukebox (gtmode MusicTheater -> slide.mproject MusicRoot): the race
   playlist the race plays (option.bgm_playlist, kept as the save's
   bgm_playlist_ps2, 004379F0), the library's order (config-main
   init_baseplaylist, main.default_library_playlist) and the replay's
   music volume. Rows [original, used, enabled, order] as 0019E900 maps
   them, as the slide playlist above. */
static gt4_music_playlist jukebox_playlist;
static int jukebox_loaded;
static void jukebox_bind(menu *m)
{
    ah_vm *vm=m->vm;ah_obj *game=game_object(m);ah_val option,main_;
    ah_get_member(vm,game,"option",&option);
    jukebox_loaded=gt4_save_option_blob_get("bgm_playlist_ps2",jukebox_playlist.bytes,sizeof jukebox_playlist.bytes) ||
                   gt4_music_playlist_default(&jukebox_playlist,m->vol,0);
    if(jukebox_loaded){
        ah_obj *playlist=ah_module_new(vm,"bgm_playlist",NULL,NULL,NULL),*rows=ah_array_new(vm);
        for(unsigned i=0;i<jukebox_playlist.bytes[0] && i<128;i++){const uint8_t *e=jukebox_playlist.bytes+16+i*16;ah_obj *row=ah_array_new(vm);
            ah_array_push(vm,row,ah_int(e[11]));ah_array_push(vm,row,ah_int(e[8]));ah_array_push(vm,row,ah_int(e[9]));ah_array_push(vm,row,ah_int(e[10]));ah_array_push(vm,rows,ah_objv(row));}
        ah_set_member(vm,playlist,"play_data",ah_objv(rows));ah_set_member(vm,playlist,"shuffle",ah_int(jukebox_playlist.bytes[6]));
        ah_set_member(vm,option.u.o,"bgm_playlist",ah_objv(playlist));
    }
    ah_set_member(vm,option.u.o,"replay_bgm_volume",ah_float((float)menu_option_number(m,"replay_bgm_volume_raw",120)/128.f));
    unsigned library[256],n=gt4_music_library_default(m->vol,library,256);
    ah_obj *list=ah_array_new(vm);for(unsigned i=0;i<n;i++)ah_array_push(vm,list,ah_int(library[i]));
    if(ah_get_member(vm,ah_root(vm),"main",&main_))ah_set_member(vm,ah_as_obj(main_,AO_MODULE),"default_library_playlist",ah_objv(list));
}
/* the race playlist as the Jukebox left it, back into the save */
static void jukebox_store(menu *m)
{
    ah_vm *vm=m->vm;ah_val option,playlist,rows,shuffle;
    if(!jukebox_loaded || !ah_get_member(vm,game_object(m),"option",&option) ||
       !ah_get_member(vm,ah_as_obj(option,AO_MODULE),"bgm_playlist",&playlist) ||
       !ah_get_member(vm,ah_as_obj(playlist,AO_MODULE),"play_data",&rows))return;
    ah_obj *a=ah_as_obj(rows,AO_ARRAY);if(!a)return;
    for(int i=0;i<ah_array_size(a) && i<jukebox_playlist.bytes[0] && i<128;i++){
        ah_obj *row=ah_as_obj(ah_array_get(a,i),AO_ARRAY);if(!row || ah_array_size(row)!=4)continue;
        uint8_t *e=jukebox_playlist.bytes+16+i*16;
        e[11]=(uint8_t)ah_num(ah_array_get(row,0));e[8]=(uint8_t)ah_num(ah_array_get(row,1));
        e[9]=(uint8_t)ah_num(ah_array_get(row,2));e[10]=(uint8_t)ah_num(ah_array_get(row,3));
    }
    if(ah_get_member(vm,ah_as_obj(playlist,AO_MODULE),"shuffle",&shuffle))jukebox_playlist.bytes[6]=(uint8_t)ah_truthy(shuffle);
    gt4_save_option_blob_set("bgm_playlist_ps2",jukebox_playlist.bytes,sizeof jukebox_playlist.bytes);
}

static void album_music_bind(menu *m)
{
 /* ../slide/musiccontrol.h uses the original config-main init_slideplaylist
    and getAllMusicList order. Music runs through the existing PCM worker. */
 ah_vm *vm=m->vm;ah_obj *game=game_object(m);ah_val option;ah_get_member(vm,game,"option",&option);
 ah_set_member(vm,game,"getAllMusicList",ah_native(vm,"getAllMusicList",album_music_list,NULL));
 gt4_music_playlist p;
 if(!gt4_save_option_blob_get("slide_playlist_ps2",p.bytes,sizeof p.bytes) || p.bytes[0]>128 || p.bytes[1] || p.bytes[2] || p.bytes[3])
  if(!gt4_music_playlist_slide(&p,m->vol,0))return;
 m->album_music_ready=1;m->album_music_snapshot=p;
 ah_obj *playlist=ah_module_new(vm,"slide_playlist",NULL,NULL,NULL),*rows=ah_array_new(vm);
 for(unsigned i=0;i<p.bytes[0];i++){const uint8_t *e=p.bytes+16+i*16;ah_obj *row=ah_array_new(vm);
  ah_array_push(vm,row,ah_int(e[11]));ah_array_push(vm,row,ah_int(e[8]));ah_array_push(vm,row,ah_int(e[9]));ah_array_push(vm,row,ah_int(e[10]));ah_array_push(vm,rows,ah_objv(row));
 }
 ah_set_member(vm,playlist,"play_data",ah_objv(rows));ah_set_member(vm,playlist,"shuffle",ah_int(p.bytes[6]));
 ah_set_member(vm,option.u.o,"slide_playlist",ah_objv(playlist));
 ah_set_member(vm,option.u.o,"slide_bgm_volume",ah_float((float)menu_option_number(m,"slide_bgm_volume_raw",120)/128.f));
}

/* musiccontrol.h exposes the original 004379F0 playlist rows. Persist the
   selection/order/shuffle separately from the race Jukebox. */
static void album_music_store(menu *m)
{
 if(!m || !m->album_music_ready)return;
 ah_vm *vm=m->vm;ah_val option,playlist,rows,shuffle;
 if(!ah_get_member(vm,game_object(m),"option",&option) ||
    !ah_get_member(vm,ah_as_obj(option,AO_MODULE),"slide_playlist",&playlist) ||
    !ah_get_member(vm,ah_as_obj(playlist,AO_MODULE),"play_data",&rows))return;
 ah_obj *a=ah_as_obj(rows,AO_ARRAY);if(!a)return;
 gt4_music_playlist p=m->album_music_snapshot;
 if(ah_array_size(a)!=p.bytes[0])return;
 for(int i=0;i<ah_array_size(a);i++){
  ah_obj *row=ah_as_obj(ah_array_get(a,i),AO_ARRAY);if(!row || ah_array_size(row)!=4)return;
  uint8_t *e=p.bytes+16+i*16;
  e[11]=(uint8_t)ah_num(ah_array_get(row,0));e[8]=(uint8_t)ah_num(ah_array_get(row,1));
  e[9]=(uint8_t)ah_num(ah_array_get(row,2));e[10]=(uint8_t)ah_num(ah_array_get(row,3));
 }
 if(ah_get_member(vm,ah_as_obj(playlist,AO_MODULE),"shuffle",&shuffle))p.bytes[6]=(uint8_t)ah_truthy(shuffle);
 if(memcmp(p.bytes,m->album_music_snapshot.bytes,sizeof p.bytes)){
  gt4_save_option_blob_set("slide_playlist_ps2",p.bytes,sizeof p.bytes);
  gt4_career_blob_set(vm,ah_as_obj(option,AO_MODULE),"slide_playlist_ps2",p.bytes,sizeof p.bytes);
  m->album_music_snapshot=p;m->options_dirty=1;
 }
}
