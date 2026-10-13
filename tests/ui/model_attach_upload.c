/* Regression for allocator bytes in attached triangle source_model.
   Synthetic geometry; checks the PC assembly/upload boundary, not GS pixels. */
#include <SDL.h>
#include "plat_sdl.h"
#include "render3d.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"attachment upload line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){640,448,0,0,0}));
    SDL_HideWindow(plat_sdl_window());CHECK(r3d_available());
    gt4_triangle triangles[2]={0};
    const float points[4][3]={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};
    const int indices[2][3]={{0,1,2},{0,2,3}};
    for(int t=0;t<2;t++){
        triangles[t].texture=-1;triangles[t].source_model=3;
        triangles[t].state.depth_test=triangles[t].state.depth_write=1;triangles[t].state.depth_func=3;
        for(int v=0;v<3;v++){
            gt4_vertex *q=&triangles[t].v[v];memcpy(q->p,points[indices[t][v]],12);
            q->alpha=1;q->rgb[0]=q->rgb[1]=q->rgb[2]=1;
        }
    }
    gt4_model part={0};part.triangles=triangles;part.count=2;part.source_models=4;
    const float transforms[4][6]={{-1.6f,-1.6f,8,1,1,1},{1.6f,-1.6f,8,1,1,1},
                                {-1.6f,1.6f,8,1,1,1},{1.6f,1.6f,8,1,1,1}};
    r3d_view view={0};view.rot[0]=view.rot[4]=view.rot[8]=1;
    view.r[0]=view.u[1]=view.f[2]=1;view.focal=320;view.focal_y=224;view.unlit=1;
    for(int run=0;run<32;run++){
        gt4_model assembly={0};assembly.capacity=8;
        assembly.triangles=malloc(8*sizeof *assembly.triangles);CHECK(assembly.triangles);
        memset(assembly.triangles,0xa5+run,8*sizeof *assembly.triangles);
        CHECK(gt4_model_attach(&assembly,&part,transforms,4));CHECK(assembly.count==8);
        for(int t=0;t<8;t++)CHECK(assembly.triangles[t].source_model==0);
        /* Recreate the old stale index: uploading a wheel must fail visibly. */
        if(argc>1 && !strcmp(argv[1],"--negative"))assembly.triangles[0].source_model=0xa5a5;
        r3d_mesh *mesh=r3d_mesh_create(&assembly);CHECK(mesh);
        unsigned char image[64*64*4];CHECK(r3d_begin_target(64));r3d_draw(mesh,&view);r3d_end_target(image);
        for(int y=26;y<=38;y+=12)for(int x=26;x<=38;x+=12){
            const unsigned char *pixel=image+(y*64+x)*4;
            for(int k=0;k<3;k++)CHECK(pixel[k]>=127 && pixel[k]<=129);
        }
        r3d_mesh_destroy(mesh);free(assembly.triangles);free(assembly.textures);
    }
    plat_video_shutdown();puts("attachment upload: 32 poisoned buffers, four visible instances each (tolerance 1/255)");return 0;
}
