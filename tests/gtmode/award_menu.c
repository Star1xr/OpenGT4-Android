/* Native application bridge + original authored prize/progression scripts.
   No EE runtime or generated original C is linked into this executable. */
#include "../../src/core/menu.c"
#include "career_vehicle.h"
#include "career_blob.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"award menu line %d: %s\n",__LINE__,#x);return 1;}}while(0)
struct plat_texture{int dummy;};
plat_texture *plat_texture_create(int w,int h,const uint8_t *p){(void)w;(void)h;(void)p;return calloc(1,sizeof(plat_texture));}
void plat_texture_destroy(plat_texture *t){free(t);}
void plat_draw_quad4(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4]){(void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;}
void plat_draw_solid_quad(const float xy[4][2],const uint8_t c[4]){(void)xy;(void)c;}
void plat_draw_quad(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4]){(void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;}
void plat_draw_quad_blend(const plat_texture *t,float x,float y,float w,float h,
                           float u,float v,float a,float b,const uint8_t c[4],float factor)
{
    uint8_t tint[4];memcpy(tint,c,4);tint[3]=(uint8_t)(factor*255.0f+.5f);
    plat_draw_quad(t,x,y,w,h,u,v,a,b,tint);
}
void plat_set_clip(float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;}
void plat_draw_glyph(const plat_texture*t,float x,float y,float w,float h,float b,float s,float u,float v,float u1,float v1,const uint8_t c[4]){(void)t;(void)x;(void)y;(void)w;(void)h;(void)b;(void)s;(void)u;(void)v;(void)u1;(void)v1;(void)c;}
void plat_draw_rotated(const plat_texture*t,float x,float y,float w,float h,float cx,float cy,float a,const uint8_t c[4]){(void)t;(void)x;(void)y;(void)w;(void)h;(void)cx;(void)cy;(void)a;(void)c;}
int plat_dialog(const char *title,const char *text,int confirm){(void)title;(void)text;return confirm?1:0;}
int plat_audio_play(const int16_t*p,size_t n,int r,int c){(void)p;(void)n;(void)r;(void)c;return 1;}
static ah_val get(ah_vm *vm,ah_obj *o,const char *name){ah_val v=ah_nil();ah_get_member(vm,o,name,&v);return v;}
static int load(const char*path,uint8_t*out,size_t n){FILE*f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n;fclose(f);return ok;}
static unsigned script_errors;
static void log_script(void *user,const char *msg){(void)user;if(strstr(msg,"error:") || strstr(msg,"undefined") || strstr(msg,"call of nil") || strstr(msg,"call of module")){script_errors++;fprintf(stderr,"%s\n",msg);}}
int main(int argc,char **argv){
 if(argc!=4){fprintf(stderr,"usage: gtmode_award_menu ISO BEFORE-RAM AFTER-RAM\n");return 2;}
 uint8_t *before=malloc(32u*1024*1024),*after=malloc(32u*1024*1024);CHECK(before && after && load(argv[2],before,32u*1024*1024) && load(argv[3],after,32u*1024*1024));
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&err);CHECK(vol && gt4_game_load_disc(argv[1]));
 char directory[160];snprintf(directory,sizeof directory,"build/career-native/award-menu-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);
 char error[256];menu *m=menu_open(vol,"gtmode",error,sizeof error);CHECK(m);menu_save_capture(m);CHECK(gt4_save_create(1,"Award validation"));menu_save_refresh(m);ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);
 ah_obj *game=game_object(m),*records= get(vm,get(vm,game,"race_record").u.o,"records").u.o;
 for(unsigned i=0;i<768;i++){uint32_t word;memcpy(&word,before+0xa0cf58+0x13ae4+i*4,4);ah_set_member(vm,ah_array_get(records,i).u.o,"original_record",ah_int(word));}
 load_race_tables(m);const char *code=gt4_specdb_label_for_id(m->race_labels,m->race_labels_size,car_u32(m->race_db+16+16*8));CHECK(code && !strcmp(code,"am_sunday_0001"));
 ah_obj *page=get(vm,m->project,"PresentRoot").u.o;CHECK(page);
 ah_obj *data=get(vm,m->project,"PresentData").u.o;if(!data)data=get(vm,ah_root(vm),"PresentData").u.o;CHECK(data);
 ah_obj *detail=get(vm,data,"detail_carlist").u.o;int prize_id=-1;
 for(int i=0;i<ah_array_size(detail);i++){const char *label=ah_cstr(ah_array_get(detail,i));if(label && !strcmp(label,"am_sunday_0000")){prize_id=i;break;}}CHECK(prize_id>=0);
 ah_val id=ah_int(prize_id),column,ret;CHECK(ah_call_member(vm,page,"makeColumn",1,&id,&column)==1 && ah_as_obj(column,AO_ARRAY));
 ah_val args[]={id,column};CHECK(ah_call_member(vm,page,"win",2,args,&ret)==1);CHECK(script_errors==0);
 for(unsigned i=0;i<768;i++){uint32_t word;memcpy(&word,after+0xa0cf58+0x13ae4+i*4,4);uint32_t actual=(uint32_t)get(vm,ah_array_get(records,i).u.o,"original_record").u.i;if(actual!=word){fprintf(stderr,"record %u %08x/%08x\n",i,actual,word);return 1;}}
 CHECK(gt4_save_capture(vm,game,1));
 CHECK(gt4_save_store());CHECK(gt4_save_load_slot(1));menu_save_refresh(m);game=game_object(m);records=get(vm,get(vm,game,"race_record").u.o,"records").u.o;
 for(unsigned i=0;i<768;i++){uint32_t word;memcpy(&word,after+0xa0cf58+0x13ae4+i*4,4);CHECK((uint32_t)get(vm,ah_array_get(records,i).u.o,"original_record").u.i==word);}
 puts("PASS original authored Sunday Cup prize eligibility/consumption: all 768 records identical to slots 7 -> 8 and native file reload; no script errors.");
 menu_close(m);gt4_save_shutdown();gt4_vol_close(vol);free(before);free(after);return 0;
}
