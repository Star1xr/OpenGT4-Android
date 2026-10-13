/* Original scalar interpolation is independently proved by photo_optics.
   This checks all resident render entries and exact restoration, no solver. */
#include "../../src/platform/sdl/race_view.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"photo motion %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
 race_view *v=calloc(1,sizeof *v);CHECK(v);v->now=calloc(1,sizeof *v->now);CHECK(v->now);
 float rows[9]={1,0,0,0,1,0,0,0,1},eye[3]={0,2,8},projection[3];
 for(int frame=0;frame<2;frame++){
  for(int i=0;i<=OPPONENTS;i++){
   float p[3]={(float)i+frame*2.f,0,(float)i-frame*4.f},w[4][3]={{0}};
   for(int k=0;k<4;k++)w[k][1]=frame*.4f;
   if(i)race_view_set_opponent(v,i-1,p,rows,w);else{race_view_set_pose(v,p,rows);race_view_set_wheels(v,w);}
   race_view_set_shadow(v,i,p,rows,.8f,.6f);
  }
  eye[0]=frame*2.f;eye[2]=8-frame*4.f;race_view_set_camera(v,eye,rows,2,2,.2f);race_view_photo_record(v);
 }
 /* The user's edited camera remains at the chosen offset. */
 eye[1]=3;race_view_set_camera(v,eye,rows,2,2,.2f);photo_frame frozen,after;photo_get(v,&frozen);
 race_view_photo_begin(v);float e[3],r[9];race_view_photo_sample(v,.5f,e,r);
 if(argc>1)v->other_pose[3].pos[0]+=.1f;
 photo_get(v,&after);
 for(int i=0;i<=OPPONENTS;i++){
  CHECK(after.car[i].pos[0]==i+1.f && after.car[i].pos[2]==i-2.f);
  CHECK(after.car[i].wheel_pose[0][1]==.2f);
  CHECK(after.shadow[i].pos[0]==i+1.f && after.shadow[i].pos[2]==i-2.f);
 }
 CHECK(e[0]==1 && e[1]==3 && e[2]==6);
 race_view_photo_sample(v,0,e,r);photo_get(v,&after);CHECK(!memcmp(&after,&frozen,sizeof after));
 race_view_photo_sample(v,.75f,e,r);race_view_photo_end(v);photo_get(v,&after);CHECK(!memcmp(&after,&frozen,sizeof after));
 /* An absent previous frame yields a static exposure, never stale another race. */
 v->photo_history=0;race_view_photo_begin(v);race_view_photo_sample(v,.75f,e,r);race_view_photo_end(v);photo_get(v,&after);CHECK(!memcmp(&after,&frozen,sizeof after));
 free(v->now);free(v);puts("PASS Photo motion: eight cars/wheels/shadows, edited moving camera, non-cumulative sampling, exact restore, static fallback.");return 0;
}
