#include "save.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* The engine namespace contains transient requests, sound handles and callbacks.
   Persist the game data namespaces; unknown data members inside these are kept. */
static const char *const roots[]={"username","money","option","garage","calendar","race_record","license_record","course_record","machine_record","favorite_car","favorite_course","presents","championship","used_car","statistics","available","major_menu_page","major_menu_widget","ps2_import"};
typedef struct { uint8_t *b;size_t n,cap;int ok; } output;
static void put(output *o,const void *b,size_t n){if(!o->ok || n>GT4_SAVE_LIMIT-o->n){o->ok=0;return;}if(o->n+n>o->cap){size_t cap=(o->n+n)*2;if(cap>GT4_SAVE_LIMIT)cap=GT4_SAVE_LIMIT;void *p=realloc(o->b,cap);if(!p){o->ok=0;return;}o->b=p;o->cap=cap;}memcpy(o->b+o->n,b,n);o->n+=n;}
static void u32(output *o,uint32_t v){uint8_t b[4];for(int i=0;i<4;i++)b[i]=(uint8_t)(v>>(8*i));put(o,b,4);}
static void u64(output *o,uint64_t v){uint8_t b[8];for(int i=0;i<8;i++)b[i]=(uint8_t)(v>>(8*i));put(o,b,8);}
static int data_value(ah_val v){if(v.t==AV_NIL || ah_is_num(v))return 1;if(v.t!=AV_OBJ)return 0;int k=ah_object_kind(v.u.o);return k==AO_STRING || k==AO_ARRAY || k==AO_MAP || (k==AO_MODULE && (!ah_host_of(v.u.o) || ah_host_of(v.u.o)->persist_members));}
static void encode(output *o,ah_val v,int depth){if(depth>20){o->ok=0;return;}uint8_t tag=0;
 if(v.t==AV_BOOL)tag=1;else if(v.t==AV_INT || v.t==AV_LONG)tag=2;else if(v.t==AV_FLOAT || v.t==AV_DOUBLE)tag=3;
 else if(v.t==AV_OBJ){int k=ah_object_kind(v.u.o);tag=k==AO_STRING?4:k==AO_ARRAY?5:k==AO_MODULE?6:k==AO_MAP?7:0;}
 put(o,&tag,1);if(tag==1){uint8_t b=(uint8_t)ah_truthy(v);put(o,&b,1);}else if(tag==2)u64(o,(uint64_t)v.u.i);
 else if(tag==3){if(!isfinite(v.u.f)){o->ok=0;return;}uint64_t bits;memcpy(&bits,&v.u.f,8);u64(o,bits);}
 else if(tag==4){const char *s=ah_cstr(v);size_t len=strlen(s);if(len>65536){o->ok=0;return;}u32(o,(uint32_t)len);put(o,s,len);}
 else if(tag==5){int n=ah_array_size(v.u.o);if(n>100000){o->ok=0;return;}u32(o,n);for(int i=0;i<n;i++){ah_val e=ah_array_get(v.u.o,i);if(!data_value(e)){o->ok=0;return;}encode(o,e,depth+1);}}
 else if(tag==7){int n=ah_object_count(v.u.o);if(n>100000){o->ok=0;return;}u32(o,n);for(int i=0;i<n;i++){ah_val key=ah_map_key(v.u.o,i),value=ah_object_value(v.u.o,i);if(!data_value(key) || !data_value(value)){o->ok=0;return;}encode(o,key,depth+1);encode(o,value,depth+1);}}
 else if(tag==6){int total=ah_object_count(v.u.o),n=0;for(int i=0;i<total;i++)if(data_value(ah_object_value(v.u.o,i)))n++;u32(o,n);
 for(int i=0;i<total;i++){ah_val e=ah_object_value(v.u.o,i);if(!data_value(e))continue;const char *name=ah_object_member_name(v.u.o,i);size_t len=strlen(name);u32(o,(uint32_t)len);put(o,name,len);encode(o,e,depth+1);}}
}
int gt4_state_pack(ah_vm *vm,ah_obj *game,uint8_t **data,size_t *size){output o={0};o.ok=1;ah_obj *filtered=ah_module_new(vm,"snapshot",NULL,NULL,NULL);for(unsigned i=0;i<sizeof roots/sizeof roots[0];i++){ah_val v;if(ah_get_member(vm,game,roots[i],&v) && data_value(v))ah_set_member(vm,filtered,roots[i],v);}encode(&o,ah_objv(filtered),0);if(!o.ok){free(o.b);return 0;}*data=o.b;*size=o.n;return 1;}
typedef struct {const uint8_t *b;size_t n,pos;int ok,nodes;ah_vm *vm;} input;
static uint64_t get(input *in,int n){if(!in->ok || (size_t)n>in->n-in->pos){in->ok=0;return 0;}uint64_t v=0;for(int i=0;i<n;i++)v|=(uint64_t)in->b[in->pos++]<<(8*i);return v;}
static char *string(input *in){size_t len=(size_t)get(in,4);if(!in->ok || len>65536 || len>in->n-in->pos || memchr(in->b+in->pos,0,len)){in->ok=0;return NULL;}char *s=malloc(len+1);if(!s){in->ok=0;return NULL;}memcpy(s,in->b+in->pos,len);s[len]=0;in->pos+=len;return s;}
static ah_val decode(input *in,int depth){if(depth>20 || ++in->nodes>200000){in->ok=0;return ah_nil();}unsigned tag=(unsigned)get(in,1);if(!in->ok)return ah_nil();
 if(tag==0)return ah_nil();if(tag==1){unsigned b=(unsigned)get(in,1);if(b>1)in->ok=0;return ah_bool(b);}
 if(tag==2)return ah_int((int64_t)get(in,8));if(tag==3){uint64_t bits=get(in,8);double v;memcpy(&v,&bits,8);if(!isfinite(v))in->ok=0;return ah_float(v);}
 if(tag==4){char *s=string(in);ah_val v=s?ah_str(in->vm,s):ah_nil();free(s);return v;}
 if(tag==7){unsigned n=(unsigned)get(in,4);if(n>100000 || n>(in->n-in->pos)/2){in->ok=0;return ah_nil();}ah_obj *map=ah_map_new(in->vm);for(unsigned i=0;i<n && in->ok;i++){ah_val key=decode(in,depth+1),value=decode(in,depth+1);if((key.t==AV_OBJ && ah_object_kind(key.u.o)!=AO_STRING) || !ah_map_set(in->vm,map,key,value))in->ok=0;}return ah_objv(map);}
 if(tag==5 || tag==6){unsigned n=(unsigned)get(in,4);if(n>100000 || n>in->n-in->pos){in->ok=0;return ah_nil();}ah_obj *obj=tag==5?ah_array_new(in->vm):ah_module_new(in->vm,"saved",NULL,NULL,NULL);
 for(unsigned i=0;i<n && in->ok;i++){char *name=tag==6?string(in):NULL;ah_val v=decode(in,depth+1);if(tag==6){if(!name || !*name || strlen(name)>128)in->ok=0;else {ah_val old;if(ah_get_member(in->vm,obj,name,&old))in->ok=0;else ah_set_member(in->vm,obj,name,v);}free(name);}else ah_array_push(in->vm,obj,v);}return ah_objv(obj);}
 in->ok=0;return ah_nil();}
static void merge(ah_vm *vm,ah_obj *dst,ah_obj *src,int depth){if(depth>20)return;int n=ah_object_count(src);for(int i=0;i<n;i++){const char *key=ah_object_member_name(src,i);ah_val v=ah_object_value(src,i),old;int exists=ah_get_member(vm,dst,key,&old);
 if(ah_as_obj(v,AO_MODULE) && exists && ah_as_obj(old,AO_MODULE) && !ah_host_of(old.u.o))merge(vm,old.u.o,v.u.o,depth+1);
 else if(!exists || data_value(old))ah_set_member(vm,dst,key,v);}}
static int valid_roots(ah_vm *vm,ah_obj *src){for(int i=0;i<ah_object_count(src);i++){const char *key=ah_object_member_name(src,i);ah_val v=ah_object_value(src,i);int known=0;for(unsigned j=0;j<sizeof roots/sizeof roots[0];j++)known|=!strcmp(key,roots[j]);if(!known)return 0;if(!strcmp(key,"username") && (!ah_cstr(v) || !gt4_save_valid_name(ah_cstr(v))))return 0;if(!strcmp(key,"money") && (v.t!=AV_INT && v.t!=AV_LONG))return 0;if(strcmp(key,"username") && strcmp(key,"money") && strcmp(key,"major_menu_page") && strcmp(key,"major_menu_widget") && !ah_as_obj(v,AO_MODULE))return 0;}return 1;}
int gt4_state_merge(ah_vm *vm,ah_obj *game,const uint8_t *data,size_t size){if(!data || !size || size>GT4_SAVE_LIMIT)return 0;input in={data,size,0,1,0,vm};ah_val v=decode(&in,0);ah_obj *src=ah_as_obj(v,AO_MODULE);if(!in.ok || in.pos!=size || !src)return 0;
 if(!valid_roots(vm,src))return 0;
 merge(vm,game,src,0);return 1;}

static void clear_missing(ah_vm *vm,ah_obj *dst,ah_obj *src,int depth){if(depth>20)return;for(int i=ah_object_count(dst)-1;i>=0;i--){const char *key=ah_object_member_name(dst,i);ah_val old=ah_object_value(dst,i),v;if(!data_value(old))continue;int exists=ah_get_member(vm,src,key,&v);if(ah_as_obj(old,AO_MODULE)){clear_missing(vm,old.u.o,exists?ah_as_obj(v,AO_MODULE):NULL,depth+1);}else if(!exists)ah_remove_member(vm,dst,key);}}
int gt4_state_replace(ah_vm *vm,ah_obj *game,const uint8_t *data,size_t size){if(!data || !size || size>GT4_SAVE_LIMIT)return 0;input in={data,size,0,1,0,vm};ah_obj *src=ah_as_obj(decode(&in,0),AO_MODULE);if(!in.ok || in.pos!=size || !src || !valid_roots(vm,src))return 0;for(unsigned j=0;j<sizeof roots/sizeof roots[0];j++){ah_val old,v;if(ah_get_member(vm,game,roots[j],&old) && ah_as_obj(old,AO_MODULE)){ah_get_member(vm,src,roots[j],&v);clear_missing(vm,old.u.o,ah_as_obj(v,AO_MODULE),0);}}return gt4_state_merge(vm,game,data,size);}
