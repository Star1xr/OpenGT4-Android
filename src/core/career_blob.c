#include "career_blob.h"
#include <stdlib.h>
#include <string.h>
int gt4_career_blob_set(ah_vm *vm,ah_obj *obj,const char *name,const unsigned char *data,size_t size){
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t length=(size+2)/3*4;char *text=malloc(length+1);if(!text)return 0;size_t at=0;
    for(size_t i=0;i<size;i+=3){unsigned x=(unsigned)data[i]<<16;if(i+1<size)x|=(unsigned)data[i+1]<<8;if(i+2<size)x|=data[i+2];
        text[at++]=alphabet[x>>18];text[at++]=alphabet[(x>>12)&63];text[at++]=i+1<size?alphabet[(x>>6)&63]:'=';text[at++]=i+2<size?alphabet[x&63]:'=';}
    text[at]=0;ah_obj *blob=ah_array_new(vm);ah_array_push(vm,blob,ah_str(vm,text));free(text);ah_set_member(vm,obj,name,ah_objv(blob));return 1;
}
static int digit(unsigned char c){if(c>='A' && c<='Z')return c-'A';if(c>='a' && c<='z')return c-'a'+26;if(c>='0' && c<='9')return c-'0'+52;return c=='+'?62:c=='/'?63:-1;}
int gt4_career_blob_get(ah_vm *vm,ah_obj *obj,const char *name,unsigned char *data,size_t size){
    ah_val value;if(!obj || !ah_get_member(vm,obj,name,&value))return 0;ah_obj *blob=ah_as_obj(value,AO_ARRAY);size_t at=0;
    if(!blob)return 0;
    for(int i=0;i<ah_array_size(blob);i++){
        const char *text=ah_cstr(ah_array_get(blob,i));if(!text)return 0;size_t length=strlen(text);if(length%4)return 0;
        for(size_t j=0;j<length;j+=4){int a=digit(text[j]),b=digit(text[j+1]),c=text[j+2]=='='?0:digit(text[j+2]),d=text[j+3]=='='?0:digit(text[j+3]);
            if(a<0 || b<0 || c<0 || d<0)return 0;unsigned x=(unsigned)a<<18|(unsigned)b<<12|(unsigned)c<<6|(unsigned)d;
            unsigned bytes=text[j+2]=='='?1:text[j+3]=='='?2:3;
            if(bytes<3 && (j+4!=length || i+1!=ah_array_size(blob) || (bytes==1 && (text[j+3]!='=' || (b&15))) || (bytes==2 && (c&3))))return 0;
            if(bytes>size-at)return 0;for(unsigned k=0;k<bytes;k++)data[at++]=(unsigned char)(x>>(16-8*k));
        }
    }
    return at==size;
}
