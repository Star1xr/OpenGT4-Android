/* P: unmodified public-domain Tango artwork downloaded separately. No game
   artwork or newly illustrated icons. Bilinear premultiplied scaling is a PC
   choice; both atlas sizes are cached at startup, with no network/frame I/O. */
#include "launcher_icons.h"
#ifdef _WIN32
#include "platform/platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static const char *const names[]={
    "actions-media-playback-start.png","devices-media-optical.png",
    "devices-video-display.png","status-audio-volume-high.png",
    "devices-input-gaming.png","places-folder.png",NULL,"actions-system-log-out.png"};
static unsigned be32(const uint8_t *p){return (unsigned)p[0]<<24|(unsigned)p[1]<<16|(unsigned)p[2]<<8|p[3];}
static uint8_t *read_icon(const char *path,int *w,int *h)
{
    /* Bound local cache files before allocating the decoded image. */
    plat_file *file=plat_file_open(path);if(!file)return NULL;
    uint8_t header[24];uint64_t bytes=plat_file_size(file);
    int ok=bytes>=24 && bytes<=2u*1024*1024 && plat_file_read_at(file,0,header,24)==24;
    plat_file_close(file);
    const uint8_t signature[]={137,80,78,71,13,10,26,10};
    if(!ok || memcmp(header,signature,8) || memcmp(header+12,"IHDR",4) ||
       !be32(header+16) || !be32(header+20) || be32(header+16)>1024 || be32(header+20)>1024)return NULL;
    return plat_png_read(path,w,h);
}
HBITMAP launcher_icons_create(const char *folder,int size)
{
    if(!folder || size<16 || size>160)return NULL;
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof info.bmiHeader;
    info.bmiHeader.biWidth=size*8;info.bmiHeader.biHeight=-size;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    uint32_t *out=NULL;HBITMAP atlas=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,(void **)&out,NULL,0);
    if(!atlas)return NULL;memset(out,0,(size_t)size*size*8*4);
    for(int icon=0;icon<8;icon++){
        if(!names[icon])continue;
        char path[1600];int length=snprintf(path,sizeof path,"%s/%s",folder,names[icon]);
        if(length<0 || length>=sizeof path)continue;
        int w,h;uint8_t *rgba=read_icon(path,&w,&h);if(!rgba)continue;
        /* Preserve the source's aspect ratio. Interpolate premultiplied colors
           to prevent colored fringes from transparent source pixels. */
        double scale=fmin((double)size/w,(double)size/h);
        int dw=(int)(w*scale+.5),dh=(int)(h*scale+.5),left=(size-dw)/2,top=(size-dh)/2;
        for(int y=0;y<dh;y++)for(int x=0;x<dw;x++){
            double fx=fmax(0,fmin(w-1,(x+.5)*w/dw-.5));
            double fy=fmax(0,fmin(h-1,(y+.5)*h/dh-.5));
            int sx=(int)fx,sy=(int)fy,tx=sx+1<w?sx+1:sx,ty=sy+1<h?sy+1:sy;
            double dx=fx-sx,dy=fy-sy,weights[]={(1-dx)*(1-dy),dx*(1-dy),(1-dx)*dy,dx*dy};
            const uint8_t *samples[]={rgba+((size_t)sy*w+sx)*4,rgba+((size_t)sy*w+tx)*4,
                rgba+((size_t)ty*w+sx)*4,rgba+((size_t)ty*w+tx)*4};
            uint32_t pixel=0;
            for(int c=0;c<4;c++){
                double value=0;for(int k=0;k<4;k++)value+=(c==3?samples[k][3]:samples[k][c]*samples[k][3]/255.)*weights[k];
                pixel|=(uint32_t)(value+.5)<<(c==3?24:(2-c)*8);
            }
            out[(y+top)*size*8+icon*size+x+left]=pixel;
        }
        free(rgba);
    }
    return atlas;
}
void launcher_icon_draw(HDC dc,HBITMAP atlas,int icon,int x,int y,int size)
{
    if(!atlas || icon<0 || icon>7 || size<=0)return;
    BITMAP image;if(!GetObjectW(atlas,sizeof image,&image))return;int source=image.bmHeight;
    HDC src=CreateCompatibleDC(dc);HBITMAP old=SelectObject(src,atlas);
    AlphaBlend(dc,x,y,size,size,src,icon*source,0,source,source,(BLENDFUNCTION){AC_SRC_OVER,0,255,AC_SRC_ALPHA});
    SelectObject(src,old);DeleteDC(src);
}
#endif
