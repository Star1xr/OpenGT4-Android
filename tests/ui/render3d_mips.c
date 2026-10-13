/* GPU integration: authored levels and region addressing must survive upload.
   C: RGB byte tolerance 1; deliberately disabling MXL must fail. */
#include "render3d.h"
#include "plat_sdl.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"GPU mip line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
 CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));SDL_HideWindow(plat_sdl_window());CHECK(r3d_available());
 tex1_image texture={0};texture.width=texture.height=texture.tex_w=texture.tex_h=64;texture.minify=5;texture.magnify=1;
 texture.wrap_u=texture.wrap_v=2;texture.max_u=texture.max_v=63;texture.mip_count=3;
 texture.rgba=calloc(64*64,4);CHECK(texture.rgba);
 for(int i=0;i<64*64;i++){texture.rgba[i*4]=255;texture.rgba[i*4+3]=255;}
 for(int l=1;l<=3;l++){int size=64>>l;texture.mip_rgba[l-1]=malloc(size*size*4);CHECK(texture.mip_rgba[l-1]);memset(texture.mip_rgba[l-1],255,size*size*4);}
 if(argc>1 && !strcmp(argv[1],"--negative"))texture.mip_count=0;
 gt4_triangle triangles[2]={0};float p[4][3]={{-1,-1,8},{1,-1,8},{1,1,8},{-1,1,8}};int index[2][3]={{0,1,2},{0,2,3}};
 for(int t=0;t<2;t++)for(int v=0;v<3;v++){gt4_vertex *q=&triangles[t].v[v];int i=index[t][v];memcpy(q->p,p[i],12);q->uv[0]=(p[i][0]+1)/2;q->uv[1]=(p[i][1]+1)/2;q->alpha=1;q->rgb[0]=q->rgb[1]=q->rgb[2]=1;}
 gt4_model model={0};model.triangles=triangles;model.count=2;model.textures=&texture;model.ntextures=1;
 r3d_mesh *mesh=r3d_mesh_create(&model);CHECK(mesh);r3d_view view={0};view.rot[0]=view.rot[4]=view.rot[8]=1;view.r[0]=view.u[1]=view.f[2]=1;view.focal=320;view.focal_y=224;view.unlit=1;
 unsigned char image[64*64*4];
 for(int frame=0;frame<8;frame++){view.cam[0]=frame/64.f;CHECK(r3d_begin_target(64));r3d_draw(mesh,&view);r3d_end_target(image);const unsigned char *pixel=image+(32*64+32)*4;CHECK(pixel[0]>=254 && pixel[1]>=254 && pixel[2]>=254);}
 r3d_mesh_destroy(mesh);free(texture.rgba);for(int i=0;i<3;i++)free(texture.mip_rgba[i]);plat_video_shutdown();puts("GPU region mipmap integration: passed");return 0;
}
