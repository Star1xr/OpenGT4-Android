/* GPU boundary test: original-EE-verified CLUT/matrix, 65536 RGB inputs
   at native sampling positions. Does not certify the complete PS2 scene. */
#include "platform/platform.h"
#include "video_gl.h"
#include "license_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"license GPU %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
 (void)argc;(void)argv;plat_video_desc d={640,480,0,0,0};CHECK(!plat_video_init(&d));
 SDL_HideWindow(SDL_GL_GetCurrentWindow());plat_video_set_view(640,480);
 uint8_t *pixels=malloc(256*256*4),*got=malloc(256*256*4);CHECK(pixels && got);
 for(int y=0;y<256;y++)for(int x=0;x<256;x++){uint8_t *p=pixels+(y*256+x)*4;p[0]=(uint8_t)x;p[1]=(uint8_t)y;p[2]=(uint8_t)(x^y);p[3]=255;}
 plat_texture *t=plat_texture_create(256,256,pixels);CHECK(t);
 const float saturation[]={1,.98f,.25f,0,-.5f};unsigned checked=0;
 for(unsigned n=0;n<sizeof saturation/sizeof *saturation;n++){
  int options[4]={n&1?3:0,0,0,0};float p[6];uint8_t lut[256][3],mix[3][3],sent[3][3];
  gt4_license_image_options(options,p);gt4_license_image_lut(p,lut);gt4_license_image_mix(saturation[n],mix);memcpy(sent,mix,9);
  if(getenv("GT4_LICENSE_GPU_NEGATIVE"))sent[0][0]^=16;
  plat_video_begin_frame();video_license_image(t,128,112,256,256,lut,sent);
  glReadPixels(128,112,256,256,GL_RGBA,GL_UNSIGNED_BYTE,got);
  for(int i=0;i<256*256;i++){uint8_t expected[3];gt4_license_image_pixel(lut,mix,pixels+i*4,expected);
   for(int c=0;c<3;c++)CHECK(abs((int)got[i*4+c]-expected[c])<=1);CHECK(got[i*4+3]==255);checked++;
  }
  /* Next ordinary UI draw must restore blend/program/texture state. */
  const uint8_t red[4]={255,0,0,255};plat_draw_quad(NULL,10,10,10,10,0,0,1,1,red);video_flush2d();
  uint8_t mark[4];glReadPixels(15,465,1,1,GL_RGBA,GL_UNSIGNED_BYTE,mark);CHECK(mark[0]==255 && !mark[1] && !mark[2]);
  plat_video_end_frame();
 }
 plat_texture_destroy(t);free(pixels);free(got);plat_video_shutdown();printf("PASS %u GPU pixels, tolerance 1 byte, and restored UI state.\n",checked);return 0;
}
