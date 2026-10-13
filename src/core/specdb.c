/* Bounded little-endian GTDB/GTID reader. Original implementation of the
   table layout documented by PDTools.SpecDB; no disc data is embedded. */
#include "specdb.h"
#include <string.h>
static unsigned r16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static uint32_t r32(const uint8_t *p){return r16(p)|(uint32_t)r16(p+2)<<16;}
static int span(size_t n,size_t p,size_t count){return p<=n && count<=n-p;}
const char *gt4_specdb_label_for_id(const uint8_t *b,size_t n,uint32_t id)
{
    if(!b || n<16 || memcmp(b,"GTID",4))return NULL;
    size_t count=r32(b+4);if(count>(n-16)/8)return NULL;
    size_t base=16+count*8;
    for(size_t i=0;i<count;i++)if(r32(b+20+i*8)==id){
        size_t off=base+(size_t)r32(b+16+i*8);
        if(!span(n,off,2))return NULL;
        unsigned length=r16(b+off);
        if(!length || !span(n,off+2,length) || b[off+1+length]!=0 || memchr(b+off+2,0,length-1))return NULL;
        return (const char *)b+off+2;
    }
    return NULL;
}
int gt4_specdb_label(const uint8_t *b,size_t n,const char *label,uint32_t *id)
{
    if(!b || !label || !id || n<16 || memcmp(b,"GTID",4))return 0;
    size_t count=r32(b+4),base=16+count*8,len=strlen(label)+1;
    if(count>(n-16)/8)return 0;
    for(size_t i=0;i<count;i++) {
        size_t off=base+(size_t)r32(b+16+i*8);
        if(!span(n,off,2))return 0;
        unsigned size=r16(b+off);
        if(!span(n,off+2,size))return 0;
        if(size==len && !memcmp(b+off+2,label,len)){*id=r32(b+20+i*8);return 1;}
    }
    return 0;
}
/* func_00449190: binary search over original GTDB entries. lw sign
   extends each identifier before the unsigned 64-bit comparison. The host
   boundary rejects malformed buffers; valid tables use the original order. */
int gt4_specdb_row_index(const uint8_t *b,size_t n,uint64_t id){
    if(!b || n<16 || memcmp(b,"GTDB",4))return -1;
    uint32_t count=r32(b+8);if(!count || count>INT32_MAX || count>(n-16)/8)return -1;
    int low=-1,high=(int)count;
    for(;;){
        int middle=(int)(((int64_t)low+high)/2);
        uint32_t bits=r32(b+16+(size_t)middle*8);int32_t signed_id;memcpy(&signed_id,&bits,4);
        uint64_t candidate=(uint64_t)(int64_t)signed_id;
        if(candidate==id)return middle;
        if(candidate<id)low=middle;else high=middle;
        if(low+1==high)return -1;
    }
}
int gt4_specdb_row_at(const uint8_t *b,size_t n,uint32_t row_index,uint8_t *out,size_t cap)
{
    if(!b || !out || n<16 || memcmp(b,"GTDB",4))return 0;
    size_t count=r32(b+8),width=r32(b+12),base;
    /* 004492D8 +0x88..+0xc8 copies the table's row width. ALLOW_ENTRY
       rows contain 60 qwords (480 bytes, 001A2BB0); 256 was a host-only
       limit, unrelated to the compressed stream's one-byte length. */
    if(count>(n-16)/8 || row_index>=count || !width || width>cap)return 0;
    base=16+count*8;
    size_t rowoff=r32(b+20+(size_t)row_index*8);
    if(!(r16(b+4)&1)) {
        if(!span(n,base+rowoff,width))return 0;
        memcpy(out,b+base+rowoff,width);return (int)width;
    }
    if(!span(n,base,520))return 0;
    size_t templates=base+(size_t)r32(b+base),codes=r32(b+base+4);
    if(!span(n,base+520,codes*8) || codes>256 || !span(n,templates,8))return 0;
    size_t data=templates+(size_t)r32(b+templates),pos=data+rowoff;
    if(!span(n,pos,1))return 0;
    uint8_t decoded[255];unsigned length=b[pos++];size_t bit=0;
    if(!length)return 0;
    for(unsigned i=0;i<length;i++) {
        uint32_t bits=0;
        for(unsigned j=0;j<32;j++) {
            size_t at=pos+(bit+j)/8;
            if(at<n)bits|=(uint32_t)((b[at]>>((bit+j)%8))&1)<<j;
        }
        const uint8_t *entry=b+base+8+(bits&255)*2;
        unsigned used=entry[1];uint8_t value=entry[0];
        if(!used)for(size_t j=0;j<codes;j++) {
            const uint8_t *code=b+base+520+j*8;unsigned len=code[0];
            if(len>8 && len<32 && (bits&((1u<<len)-1))==r32(code+4)){used=len;value=code[1];break;}
        }
        if(!used || used>31 || bit+used>(n-pos)*8)return 0;
        decoded[i]=value;bit+=used;
    }
    unsigned kind=decoded[0]>>6,index=decoded[0]&63;
    if(kind==1) {
        if(length<width+1)return 0;
        memcpy(out,decoded+1,width);return (int)width;
    }
    if(kind!=0 && kind!=2)return 0;
    size_t source=templates+8+index*width;
    if(source>data || width>data-source || !span(n,source,width))return 0;
    memcpy(out,b+source,width);
    if(kind==2) {
        size_t mask=(width+7)/8,patch=1+mask;
        if(length<patch)return 0;
        for(size_t i=0;i<width;i++)if(decoded[1+i/8]&(1u<<(i%8))) {
            if(patch>=length)return 0;
            out[i]=decoded[patch++];
        }
    }
    return (int)width;
}

const char *gt4_specdb_string(const uint8_t *b,size_t n,uint32_t index)
{
    if(!b || n<16 || memcmp(b,"GTST",4))return NULL;
    size_t count=r32(b+4);
    if(count>(n-16)/4 || index>=count)return NULL;
    size_t off=16+count*4+(size_t)r32(b+16+index*4);
    if(!span(n,off,2))return NULL;
    size_t len=r16(b+off);
    if(!len || !span(n,off+2,len) || !memchr(b+off+2,0,len))return NULL;
    return (const char *)b+off+2;
}

int gt4_specdb_row(const uint8_t *b,size_t n,uint32_t id,uint8_t *out,size_t cap)
{
    if(!b || n<16 || memcmp(b,"GTDB",4))return 0;
    size_t count=r32(b+8);if(count>(n-16)/8)return 0;
    for(size_t i=0;i<count;i++)if(r32(b+16+i*8)==id)return gt4_specdb_row_at(b,n,(uint32_t)i,out,cap);
    return 0;
}

int gt4_specdb_row_nth(const uint8_t *b,size_t n,uint32_t id,uint32_t nth,uint8_t *out,size_t cap)
{
    if(!b || n<16 || memcmp(b,"GTDB",4))return 0;
    size_t count=r32(b+8);if(count>(n-16)/8)return 0;
    for(size_t i=0;i<count;i++)if(r32(b+16+i*8)==id) {
        if((size_t)nth>=count-i || r32(b+16+(i+nth)*8)!=id)return 0;
        if(!out)return 1;
        return gt4_specdb_row_at(b,n,(uint32_t)(i+nth),out,cap);
    }
    return 0;
}
