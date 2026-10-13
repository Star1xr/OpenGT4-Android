#include "disc_checkpoint.h"
#include "car.h"
#include <string.h>
void disc_checkpoint_restore(uint32_t cond,unsigned index,const uint8_t *state)
{
    /* 0035ABA8 +2C..80 -> 003F85F0: copy car state, preserve object
       links (003F84E0/003F8510), then reconnect driver 00463930. */
    uint32_t car=cond+0x6698+index*0x10a0,b=car+0x104,keep[5];
    uint8_t slot=GT4_U8(car+0x544),flags=GT4_U8(car+0x545);
    memcpy(keep,gt4_ptr(car),sizeof keep);memcpy(gt4_ptr(car),state+0x54,0x10a0);
    memcpy(gt4_ptr(car),keep,sizeof keep);
    /* Driver begins at +14: +E4 conductor, +E8 car, +EC line manager. */
    GT4_U32(car+0xf8)=cond;GT4_U32(car+0xfc)=car;GT4_U32(car+0x100)=cond+0xda50;
    GT4_U8(car+0x544)=slot;GT4_U8(car+0x545)=flags;
    /* 003455F0 resets the camera/history sample for this slot. */
    uint32_t v=cond+0xcab8+slot*48;GT4_U32(b+0x5fc)=GT4_U32(b+0x600)=0;
    memset(gt4_ptr(v),0,48);GT4_U32(v)=GT4_U32(b+0x570);GT4_U32(v+12)=GT4_U32(b+0x568);
    GT4_U32(b+0x614)=(GT4_U32(cond+0xf898)+50-state[2])/3;
    /* 0045B948 rebuilds the restored car's collision hull and pair
       reaches. Use the existing native setup; no physics code changes. */
    gt4_walls_setup(cond);
    /* 0035ABA8 +90..D8 restores a non-empty collision-hull recorder.
       0045CF18 copies eight 0x44 records, preserving their two links
       and relocating the ground-point index at +2C into runway +84. */
    const uint8_t *h=state+0x10f4;unsigned sum=0;for(unsigned i=0;i<0x400;i++)sum+=h[i];
    if(sum && !*(const uint32_t *)h){
        uint32_t obj=cond+0xda78+index*0x4f0;
        uint32_t runway=GT4_U32(GT4_U32(GT4_U32(obj+0x488)+4)+0xdc),points=GT4_U32(runway+0x84);
        for(unsigned i=0;i<8;i++){
            uint32_t at=obj+i*0x44;uint8_t links[16];memcpy(links,gt4_ptr(at+8),16);
            memcpy(gt4_ptr(at+8),h+8+i*0x44,0x44);memcpy(gt4_ptr(at+8),links,16);
            uint32_t point;memcpy(&point,h+8+i*0x44+0x2c,4);GT4_U32(at+0x34)=points+point*16;
        }
    }
}
