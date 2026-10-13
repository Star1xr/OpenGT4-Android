/* Host JPEG decoding; PS2 PhotoData contains JPEG (001D0CC0 +A0..110).
   stb_image, MIT/public domain, pinned 31c1ad37456438565541f4919958214b6e762fb4. */
#include "platform/platform.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 4096
#include "../../../third_party/stb/stb_image.h"
#include "miniz.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
int plat_jpeg_png(const uint8_t *jpeg,size_t size,uint8_t **png,size_t *length){
 *png=NULL;*length=0;if(!jpeg || size>INT_MAX)return 0;
 int w,h,c;if(!stbi_info_from_memory(jpeg,(int)size,&w,&h,&c) || w<1 || h<1 || w>4096 || h>4096)return 0;
 uint8_t *pixels=stbi_load_from_memory(jpeg,(int)size,&w,&h,&c,4);if(!pixels)return 0;
 void *encoded=tdefl_write_image_to_png_file_in_memory(pixels,w,h,4,length);stbi_image_free(pixels);
 if(!encoded)return 0;*png=malloc(*length);if(*png)memcpy(*png,encoded,*length);mz_free(encoded);return *png!=NULL;
}
