#define main pregrid_fixture_main
#define plat_draw_solid_quad fixture_draw_solid_quad
#define plat_draw_glyph fixture_draw_glyph
#include "../ui/pregrid_menu.c"
#undef plat_draw_glyph
#undef plat_draw_solid_quad
#undef main
#include "../../src/core/race_result.c"
#include "career_license.h"
void plat_video_set_view(float w,float h){CHECK(w==640 && (h==480 || h==448));}
static unsigned trophies;
static unsigned screens,screen_moved;
static float screen_first[4],screen_last[4],screen_expected[4];
static float file_float(const uint8_t *d){float f;memcpy(&f,d,4);return f;}
static uint32_t file_word(const uint8_t *d){uint32_t u;memcpy(&u,d,4);return u;}
static void draw_screen(void *ctx,float x,float y,float w,float h){
 (void)ctx;float rect[4]={x,y,w,h};CHECK(w>0 && h>0);
 if(!screens)memcpy(screen_first,rect,sizeof rect);
 else if(memcmp(rect,screen_first,sizeof rect))screen_moved=1;
 memcpy(screen_last,rect,sizeof rect);screens++;
}
static unsigned colored_glyphs;
void plat_draw_glyph(const plat_texture*t,float x,float y,float w,float h,float b,float s,float u,float v,float u1,float v1,const uint8_t c[4]){
 if(c[0]>c[2] && c[1]>c[2] && c[3])colored_glyphs++;
 fixture_draw_glyph(t,x,y,w,h,b,s,u,v,u1,v1,c);
}
/* Independently compare submitted solid geometry with straight contours
   in the original movie, not with the tessellator's intermediate data. */
static float fail_quads[512][4][2];static unsigned fail_quad_count;
static uint8_t *fail_asset;static float fail_transform[4];
void plat_draw_solid_quad(const float xy[4][2],const uint8_t color[4])
{
 if(color[0]==255 && color[1]==255 && color[2]==255 && color[3]==255){
  CHECK(fail_quad_count<512);memcpy(fail_quads[fail_quad_count++],xy,32);
 }
}
static int contour_contains(const uint8_t *d,uint32_t stroke,double x,double y)
{
 double ax=file_float(d+stroke+4),ay=file_float(d+stroke+8);int inside=0;
 uint32_t path=file_word(d+stroke+12),edges=file_word(d+stroke+16);
 for(unsigned i=0;i<edges;i++){
  CHECK(!file_word(d+path+i*20));double bx=file_float(d+path+i*20+12),by=file_float(d+path+i*20+16);
  if((ay>y)!=(by>y) && x<(bx-ax)*(y-ay)/(by-ay)+ax)inside^=1;
  ax=bx;ay=by;
 }return inside;
}
static int disc_fail_contains(double x,double y)
{
 uint32_t shape=file_word(fail_asset+0x38),groups=file_word(fail_asset+shape+16);
 for(unsigned g=0;g<file_word(fail_asset+shape+20);g++){
  uint32_t fills=file_word(fail_asset+groups+g*8);
  for(unsigned k=0;k<file_word(fail_asset+groups+g*8+4);k++){
   uint32_t fs=fills+k*12,strokes=file_word(fail_asset+fs+4);int inside=0;
   for(unsigned j=0;j<file_word(fail_asset+fs+8);j++)inside^=contour_contains(fail_asset,strokes+j*20,x,y);
   if(inside)return 1;
  }
 }return 0;
}
static int submitted_fail_contains(double x,double y)
{
 static const unsigned order[4]={0,1,3,2};
 for(unsigned i=0;i<fail_quad_count;i++){
  int inside=0;for(unsigned j=0;j<4;j++){
   const float *a=fail_quads[i][order[j]],*b=fail_quads[i][order[(j+1)%4]];
   if((a[1]>y)!=(b[1]>y) && x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0])inside^=1;
  }if(inside)return 1;
 }return 0;
}
static void check_fail_geometry(void)
{
 CHECK(fail_quad_count>0);uint32_t shape=file_word(fail_asset+0x38);unsigned samples=0;
 double left=file_float(fail_asset+shape),top=file_float(fail_asset+shape+4);
 double right=file_float(fail_asset+shape+8),bottom=file_float(fail_asset+shape+12);
 /* Off-grid samples cover all four letters, the F notch and the A hole.
    Perturbations skip points within 0.0001 local units of a boundary;
    transformed float coordinates otherwise must match with zero errors. */
 for(double y=top-1+.137;y<bottom+1;y+=.41)for(double x=left-1+.173;x<right+1;x+=.37){
  int expected=disc_fail_contains(x,y);
  if(expected!=disc_fail_contains(x+.0001,y) || expected!=disc_fail_contains(x-.0001,y) ||
     expected!=disc_fail_contains(x,y+.0001) || expected!=disc_fail_contains(x,y-.0001))continue;
  double px=x*fail_transform[0]+fail_transform[2],py=y*fail_transform[1]+fail_transform[3];
  int actual=submitted_fail_contains(px,py);
  if(getenv("GT4_LICENSE_FAIL_VECTOR_NEGATIVE"))actual=x>=left && x<=right && y>=top && y<=bottom;
  if(getenv("GT4_LICENSE_FAIL_HOLE_NEGATIVE") && x>-14 && x<-12 && y>-4 && y<-2)actual=1;
  if(actual!=expected)fprintf(stderr,"FAIL contour at %.6f %.6f: submitted %d disc %d\n",x,y,actual,expected);
  CHECK(actual==expected);samples++;
 }
 printf("PASS FAIL vector %u coverage samples, %u batched solid quads; zero errors outside 0.0001 boundary.\n",samples,fail_quad_count);
}
static int trophy_model;
static void draw_trophy(void *ctx,const race_result_trophy_pose *pose){
 (void)ctx;CHECK(pose->model==trophy_model);
 CHECK(pose->alpha>=0 && pose->alpha<=1);trophies++;
}
int main(int argc,char **argv){
 CHECK(argc==2);gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol);
 /* Independent disc fixture: final root placement, and the screen's
    local bounds. No screenshot measurement or production player calls. */
 size_t size;uint8_t *asset=gt4_vol_read(vol,"/menu/US/result_license.strb",&size);CHECK(asset && size>0xbc);
 uint32_t root=file_word(asset+0x30),place=file_word(asset+root);
 uint32_t count=file_word(asset+place+0x1c),params=file_word(asset+place+0x18),bounds=file_word(asset+0xb8);
 CHECK(count && params+count*36<=size && bounds+24<=size);
 const uint8_t *last=asset+params+(count-1)*36;
 float sx=file_float(last+8),sy=file_float(last+12),x=file_float(last+16),y=file_float(last+20);
 screen_expected[0]=sx*file_float(asset+bounds)+x;screen_expected[1]=sy*file_float(asset+bounds+4)+y;
 screen_expected[2]=sx*(file_float(asset+bounds+8)-file_float(asset+bounds));
 screen_expected[3]=sy*(file_float(asset+bounds+12)-file_float(asset+bounds+4));
 /* Root placement 3 draws shape 0 (FAIL); use its final authored matrix. */
 uint32_t fail_place=place+3*40,fail_params=file_word(asset+fail_place+0x18),fail_count=file_word(asset+fail_place+0x1c);
 CHECK(fail_count && fail_params+fail_count*36<=size);
 const uint8_t *fail_last=asset+fail_params+(fail_count-1)*36;
 for(unsigned i=0;i<4;i++)fail_transform[i]=file_float(fail_last+8+i*4);fail_asset=asset;
 for(int state=-1;state<=5;state++)for(int retry=0;retry<2;retry++){
  race_result *r=race_result_license_open(vol,state,10248,"l0b0001",10000,NULL);CHECK(r);
  trophies=colored_glyphs=0;trophy_model=state<=2?0:state==3?1:2;
  screens=screen_moved=0;
  race_result_set_screen(r,draw_screen);
  race_result_set_trophy(r,draw_trophy);
  CHECK(gt4_career_license_music(state)==(state<=0?24:state<=2?21:state==3?22:23));
  strb_value level;CHECK(call(r,"PrizeLevel",NULL,0,&level));
  int expected=state==1 || state==2?0:state==3?1:state==4 || state==5?2:3;
  if(getenv("GT4_LICENSE_MOVIE_NEGATIVE"))expected++;
  CHECK(level.t==STRB_NUM && level.n==expected);
  CHECK(call(r,"GetTime",NULL,0,&level) && !strcmp(level.s,"0'10.248"));
  CHECK(call(r,"GetTitle",NULL,0,&level) && !strcmp(level.s,"License B-1\n"));
  CHECK(call(r,"GetTimeDif",NULL,0,&level) && !strcmp(level.s,state<=0?"+0'00.248":""));
  race_result_state rows={0};unsigned before=quads;
  for(unsigned tick=0;tick<600 && !race_result_done(r);tick++){
   race_result_update(r,&rows,1./60);
   unsigned glyphs_before=colored_glyphs;fail_quad_count=0;race_result_draw(r);
   if(tick==480 && state<=0)check_fail_geometry();
   if(tick==480 && state>0){
    /* 00479CA0 does not abbreviate/clamp its narrow Time field: all
       eight original GetTime characters must reach the renderer. */
    unsigned timer_glyphs=colored_glyphs-glyphs_before;
    if(getenv("GT4_LICENSE_TIMER_TEXT_NEGATIVE"))timer_glyphs=6;
    CHECK(timer_glyphs==8);
   }
   if(tick==481)race_result_key(r,retry?38:40);
   if(tick>481 && !(tick%30))race_result_press(r);
  }
  CHECK(quads>before && race_result_done(r) && race_result_retry(r)==retry);
  if(state<=0){
   CHECK(screens>0 && screen_moved);
   if(getenv("GT4_LICENSE_SCREEN_NEGATIVE"))screen_last[0]+=1;
   CHECK(!memcmp(screen_last,screen_expected,sizeof screen_last));
  }else CHECK(!screens); /* original Action 0 jumps directly to PASSED */
  if(getenv("GT4_LICENSE_TROPHY_BRIDGE_NEGATIVE"))trophies=0;
  CHECK(state<=0?!trophies:trophies>0);
  if(getenv("GT4_LICENSE_COLOR_BRIDGE_NEGATIVE"))colored_glyphs=0;
  CHECK(state<=0 || colored_glyphs>0);
  printf("PASS license movie state %d exit retry=%d\n",state,race_result_retry(r));race_result_close(r);
 }
 free(fail_asset);gt4_vol_close(vol);return 0;
}
