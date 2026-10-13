/* GPU equal-depth comparison against the game's mapping D_00629DD8.
   Native API 2 is strict, 3 includes equality. No guessed depth offset. */
#include "render3d.h"
#include "plat_sdl.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"depth line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
 CHECK(argc>1);FILE *file=fopen(argv[1],"rb");CHECK(file);CHECK(!fseek(file,0x629dd8-0x100000,SEEK_SET));unsigned char map[4];CHECK(fread(map,1,4,file)==4);fclose(file);CHECK(map[2]==3 && map[3]==2);
 CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));SDL_HideWindow(plat_sdl_window());CHECK(r3d_available());
 gt4_triangle triangles[2]={0};float p[4][3]={{-1,-1,8},{1,-1,8},{1,1,8},{-1,1,8}};int index[2][3]={{0,1,2},{0,2,3}};
 for(int t=0;t<2;t++){triangles[t].texture=-1;triangles[t].state.depth_test=triangles[t].state.depth_write=1;triangles[t].state.depth_func=3;
  for(int v=0;v<3;v++){gt4_vertex *q=&triangles[t].v[v];memcpy(q->p,p[index[t][v]],12);q->alpha=1;q->rgb[0]=1;}}
 gt4_model model={0};model.triangles=triangles;model.count=2;
 r3d_mesh *red=r3d_mesh_create(&model);CHECK(red);r3d_view view={0};view.rot[0]=view.rot[4]=view.rot[8]=1;view.r[0]=view.u[1]=view.f[2]=1;view.focal=320;view.focal_y=224;view.unlit=1;
 for(int t=0;t<2;t++)for(int v=0;v<3;v++){triangles[t].v[v].rgb[0]=0;triangles[t].v[v].rgb[1]=1;}
 unsigned char image[64*64*4];
 for(int func=2;func<=3;func++)for(int write=0;write<2;write++) {
  for(int t=0;t<2;t++){triangles[t].state.depth_func=(unsigned char)func;triangles[t].state.depth_write=(unsigned char)write;}
  r3d_mesh *green=r3d_mesh_create(&model);CHECK(green);
  for(int frame=0;frame<8;frame++){view.cam[0]=frame/64.f;CHECK(r3d_begin_target(64));r3d_draw(red,&view);r3d_draw(green,&view);r3d_end_target(image);
   int equal=map[func]==2;if(argc>2 && !strcmp(argv[2],"--negative"))equal=!equal;
   const unsigned char *pixel=image+(32*64+32)*4;CHECK(pixel[equal?1:0]>=127 && pixel[equal?1:0]<=129 && pixel[equal?0:1]<=1);
  }r3d_mesh_destroy(green);
 }
 r3d_mesh_destroy(red);plat_video_shutdown();puts("original depth mapping, equality and ZMSK: passed (32 frames, tolerance 1)");return 0;
}
