#include "launcher_photos.h"
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
static uint32_t *read_scaled(const launcher_photos *p,int index)
{
    int w,h;uint8_t *rgba=plat_png_read(p->paths[index],&w,&h);if(!rgba)return NULL;
    uint32_t *out=malloc((size_t)p->width*p->height*4);if(!out){free(rgba);return NULL;}
    /* P: crop to cover the PC window; letterboxes/UI were never exported by
       Photo Travel. Resample once per slide, not once per frame. */
    double scale=fmax((double)p->width/w,(double)p->height/h);
    double x0=(w-p->width/scale)*.5,y0=(h-p->height/scale)*.5;
    for(int y=0;y<p->height;y++)for(int x=0;x<p->width;x++){
        double fx=fmax(0,fmin(w-1,x0+(x+.5)/scale-.5));
        double fy=fmax(0,fmin(h-1,y0+(y+.5)/scale-.5));
        int sx=(int)fx,sy=(int)fy,tx=sx+1<w?sx+1:sx,ty=sy+1<h?sy+1:sy;
        double dx=fx-sx,dy=fy-sy;
        const uint8_t *samples[]={rgba+((size_t)sy*w+sx)*4,rgba+((size_t)sy*w+tx)*4,
            rgba+((size_t)ty*w+sx)*4,rgba+((size_t)ty*w+tx)*4};
        double weights[]={(1-dx)*(1-dy),dx*(1-dy),(1-dx)*dy,dx*dy};
        uint32_t pixel=0;
        for(int c=0;c<3;c++){
            double color=0;for(int k=0;k<4;k++)color+=samples[k][c]*samples[k][3]/255.*weights[k];
            pixel|=(uint32_t)(color+.5)<<((2-c)*8);
        }
        out[y*p->width+x]=pixel;
    }
    free(rgba);return out;
}
static void discover(launcher_photos *p,const char *folder)
{
    plat_directory_entry entries[32];int count=0;
#ifdef _WIN32
    /* P: local album paths are UTF-8, including names returned by the PC
       folder picker. Discovery must use wide paths, as does the PNG reader. */
    wchar_t search[1200];char pattern[1200];snprintf(pattern,sizeof pattern,"%s/*.png",folder);
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,pattern,-1,search,1200))return;
    WIN32_FIND_DATAW data;HANDLE scan=FindFirstFileW(search,&data);
    if(scan==INVALID_HANDLE_VALUE)return;
    do {
        if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY || data.nFileSizeHigh || !data.nFileSizeLow || data.nFileSizeLow>16u*1024*1024)continue;
        char name[sizeof entries[0].name];
        if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,data.cFileName,-1,name,sizeof name,NULL,NULL))continue;
        /* Keep the first 32 names in lexical order, independent of filesystem
           enumeration order. Native Photo Travel filenames include the date. */
        int at=0;while(at<count && strcmp(entries[at].name,name)>0)at++;
        if(at>=32)continue;
        if(count<32)count++;
        for(int i=count-1;i>at;i--)entries[i]=entries[i-1];
        strcpy(entries[at].name,name);entries[at].bytes=data.nFileSizeLow;
    }while(FindNextFileW(scan,&data));
    FindClose(scan);
#else
    count=plat_directory_list(folder,".png",entries,32);
#endif
    for(int i=count-1;i>=0 && p->count<32;i--)if(entries[i].bytes && entries[i].bytes<=16u*1024*1024){
        char *path=p->paths[p->count];int n=snprintf(path,1100,"%s/%s",folder,entries[i].name);
        if(n>0 && n<1100)p->count++;
    }
}
static uint32_t *next_valid(launcher_photos *p,int *index)
{
    for(int tried=0;tried<p->count;tried++){
        *index=(*index+1)%p->count;uint32_t *photo=read_scaled(p,*index);if(photo)return photo;
    }
    return NULL;
}
int launcher_photos_open(launcher_photos *p,const char *cache,const char *album,int width,int height)
{
    memset(p,0,sizeof *p);if(width<=0 || height<=0 || width>1920 || height>1440)return 0;
    p->width=width;p->height=height;discover(p,cache);discover(p,album);p->index=-1;
    if(p->count){p->current=next_valid(p,&p->index);p->next=next_valid(p,&p->index);}
    return p->current!=NULL;
}
void launcher_photos_close(launcher_photos *p){free(p->current);free(p->next);memset(p,0,sizeof *p);}
void launcher_photos_step(launcher_photos *p,double seconds)
{
    if(!p->current || !p->next || !isfinite(seconds) || seconds<0)return;
    /* P: eight seconds per photograph, 1.5-second dissolve. This new launcher
       has no PS2 timing counterpart. Album slideshow in the game is separate. */
    p->phase+=fmin(seconds,8);
    while(p->phase>=8){p->phase-=8;free(p->current);p->current=p->next;p->next=next_valid(p,&p->index);}
}
int launcher_photos_draw(const launcher_photos *p,uint32_t *out)
{
    if(!p->current || !out)return 0;
    int alpha=p->phase<=6.5?0:(int)((p->phase-6.5)*255/1.5);if(alpha>255)alpha=255;
    size_t count=(size_t)p->width*p->height;
    if(!alpha || !p->next){memcpy(out,p->current,count*4);return 1;}
    for(size_t i=0;i<count;i++){
        uint32_t a=p->current[i],b=p->next[i],pixel=0;
        for(int shift=0;shift<24;shift+=8)pixel|=(((((a>>shift)&255)*(255-alpha)+((b>>shift)&255)*alpha)/255)<<shift);
        out[i]=pixel;
    }return 1;
}
