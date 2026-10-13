/* Vls0 ordered model lists: alternatives, reverse order and empty list. */
#include "render3d.h"
#include "plat_sdl.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vision GPU line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){

 CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));SDL_HideWindow(plat_sdl_window());CHECK(r3d_available());
 gt4_triangle triangles[2]={0};float p[4][3]={{-1,-1,8},{1,-1,8},{1,1,8},{-1,1,8}};int index[2][3]={{0,1,2},{0,2,3}};
 for(int t=0;t<2;t++){triangles[t].texture=-1;triangles[t].state.depth_test=triangles[t].state.depth_write=1;triangles[t].state.depth_func=3;
  for(int v=0;v<3;v++){gt4_vertex *q=&triangles[t].v[v];memcpy(q->p,p[index[t][v]],12);q->alpha=1;q->rgb[0]=1;}}
 gt4_triangle layers[4];memcpy(layers,triangles,sizeof triangles);memcpy(layers+2,triangles,sizeof triangles);
 for(int t=2;t<4;t++){layers[t].source_model=1;for(int v=0;v<3;v++){layers[t].v[v].rgb[0]=0;layers[t].v[v].rgb[1]=1;}}
 gt4_model model={0};model.triangles=layers;model.count=4;model.source_models=2;
 r3d_mesh *mesh=r3d_mesh_create(&model);CHECK(mesh);r3d_view view={0};view.rot[0]=view.rot[4]=view.rot[8]=1;view.r[0]=view.u[1]=view.f[2]=1;view.focal=320;view.focal_y=224;view.unlit=1;
 const unsigned char lists[4][6]={{1,0,0,0,0,0},{1,0,1,0,0,0},{2,0,1,0,0,0},{0,0,0,0,0,0}};
 unsigned char image[64*64*4];
 for(int test=0;test<4;test++)for(int frame=0;frame<8;frame++){
  view.model_list=lists[test];if(argc>1 && !strcmp(argv[1],"--negative"))view.model_list=NULL;
  view.cam[0]=frame/64.f;CHECK(r3d_begin_target(64));r3d_draw(mesh,&view);r3d_end_target(image);
  const unsigned char *pixel=image+(32*64+32)*4;
  if(test==3)CHECK(pixel[0]<=1 && pixel[1]<=1);
  else {int color=test==1?1:0;CHECK(pixel[color]>=127 && pixel[color]<=129 && pixel[1-color]<=1);}
 }
 r3d_mesh_destroy(mesh);plat_video_shutdown();puts("ordered vision list/empty list/overlapping alternatives: passed (32 frames, tolerance 1)");return 0;
}
