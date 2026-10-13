/* Native photo device: PNG exports, not PS2 memory-card data. */
#include "platform/platform.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
static int suffix_match(const char *name,const char *suffix)
{size_t n=strlen(name),s=strlen(suffix);return n>s && !strcmp(name+n-s,suffix);}
static int by_name(const void *a,const void *b)
{return strcmp(((const plat_directory_entry *)b)->name,((const plat_directory_entry *)a)->name);}
int plat_directory_list(const char *path,const char *suffix,plat_directory_entry *out,int capacity)
{
 int n=0;if(!path || !suffix || !out || capacity<=0)return 0;
#ifdef _WIN32
 char pattern[1024];snprintf(pattern,sizeof pattern,"%s/*",path);WIN32_FIND_DATAA d;
 HANDLE h=FindFirstFileA(pattern,&d);if(h==INVALID_HANDLE_VALUE)return 0;
 do {if(!(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && suffix_match(d.cFileName,suffix) && strlen(d.cFileName)<128 && n<capacity){
  plat_directory_entry *e=&out[n++];snprintf(e->name,128,"%s",d.cFileName);
  e->bytes=((uint64_t)d.nFileSizeHigh<<32)|d.nFileSizeLow;
  uint64_t t=((uint64_t)d.ftLastWriteTime.dwHighDateTime<<32)|d.ftLastWriteTime.dwLowDateTime;
  e->modified=t/10000000ull-11644473600ull;
 }}while(FindNextFileA(h,&d));FindClose(h);
#else
 DIR *dir=opendir(path);if(!dir)return 0;struct dirent *d;
 while((d=readdir(dir)) && n<capacity)if(suffix_match(d->d_name,suffix) && strlen(d->d_name)<128){
  char full[1024];snprintf(full,sizeof full,"%s/%s",path,d->d_name);struct stat st;
  if(!stat(full,&st) && S_ISREG(st.st_mode)){plat_directory_entry *e=&out[n++];snprintf(e->name,128,"%s",d->d_name);e->bytes=st.st_size;e->modified=st.st_mtime;}
 }closedir(dir);
#endif
 qsort(out,n,sizeof *out,by_name);return n;
}
int plat_file_write(const char *path,const void *data,size_t bytes)
{FILE *f=fopen(path,"wb");if(!f)return 0;int ok=fwrite(data,1,bytes,f)==bytes;if(fclose(f))ok=0;return ok;}
int plat_file_remove(const char *path){return path && !remove(path);}
static uint32_t be32(const uint8_t *b){return (uint32_t)b[0]<<24|(uint32_t)b[1]<<16|(uint32_t)b[2]<<8|b[3];}
static uint8_t paeth(int a,int b,int c)
{int p=a+b-c,pa=abs(p-a),pb=abs(p-b),pc=abs(p-c);return (uint8_t)(pa<=pb && pa<=pc?a:pb<=pc?b:c);}
uint8_t *plat_png_read(const char *path,int *width,int *height)
{
 /* Photo Travel writes RGB8/RGBA8, non-interlaced. Validate chunk bounds,
    CRC, zlib size and PNG filter before uploading anything to the GPU. */
 static const uint8_t sig[]={137,80,78,71,13,10,26,10};
 uint8_t *file=NULL,*packed=NULL,*raw=NULL,*rgba=NULL;size_t bytes=0,packed_n=0;
 int w=0,h=0,bpp=0,ended=0;plat_file *f=plat_file_open(path);if(!f)return NULL;
 uint64_t size=plat_file_size(f);if(size<45 || size>128u*1024*1024)goto done;
 bytes=(size_t)size;file=malloc(bytes);if(!file || plat_file_read_at(f,0,file,bytes)!=bytes || memcmp(file,sig,8))goto done;
 packed=malloc(bytes);if(!packed)goto done;
 for(size_t at=8;at+12<=bytes;){uint32_t n=be32(file+at);if(n>bytes-at-12)goto done;
  uint8_t *type=file+at+4,*data=file+at+8;
  if(mz_crc32(0,type,n+4)!=be32(data+n))goto done;
  if(!memcmp(type,"IHDR",4)){
   if(at!=8 || n!=13 || data[8]!=8 || data[10] || data[11] || data[12])goto done;
   w=(int)be32(data);h=(int)be32(data+4);bpp=data[9]==2?3:data[9]==6?4:0;
   if(!bpp || w<=0 || h<=0 || w>8192 || h>8192 || (uint64_t)w*h>32u*1024*1024)goto done;
  }else if(!memcmp(type,"IDAT",4)){if(!w || packed_n+n>bytes)goto done;memcpy(packed+packed_n,data,n);packed_n+=n;}
  else if(!memcmp(type,"IEND",4)){if(n)goto done;ended=1;break;}
  at+=n+12;
 }
 if(!ended || !w || !packed_n)goto done;
 size_t stride=(size_t)w*bpp,expected=(stride+1)*h;mz_ulong length=(mz_ulong)expected;
 raw=malloc(expected);if(!raw || mz_uncompress(raw,&length,packed,(mz_ulong)packed_n)!=MZ_OK || length!=expected)goto done;
 for(int y=0;y<h;y++){uint8_t *row=raw+y*(stride+1)+1,*previous=y?row-stride-1:NULL;int filter=row[-1];if(filter>4)goto done;
  for(size_t x=0;x<stride;x++){int a=x>=(size_t)bpp?row[x-bpp]:0,b=previous?previous[x]:0,c=previous && x>=(size_t)bpp?previous[x-bpp]:0;
   row[x]+=(uint8_t)(filter==1?a:filter==2?b:filter==3?(a+b)/2:filter==4?paeth(a,b,c):0);
  }
 }
 rgba=malloc((size_t)w*h*4);if(!rgba)goto done;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){uint8_t *d=rgba+((size_t)y*w+x)*4,*s=raw+y*(stride+1)+1+x*bpp;memcpy(d,s,3);d[3]=bpp==4?s[3]:255;}
 if(width)*width=w;if(height)*height=h;
 done:plat_file_close(f);free(file);free(packed);free(raw);return rgba;
}
