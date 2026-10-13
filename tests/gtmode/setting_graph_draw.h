/* Test-only MGraphFace drawing boundary. 002E88F8 runs original code;
   only render-context methods +198/+260/+268 record their arguments.
   0025BAA0 resolves the captured parent chain, independent of PC layout.
   GS rasterization is deliberately outside this draw-list comparison. */
#include "ee.h"
#define DRAW_CHECK(x) do{if(!(x)){fprintf(stderr,"original graph draw line %d: %s\n",__LINE__,#x);return 0;}}while(0)
ee_fn recomp_lookup(uint32_t);
static float original_color[4],original_size[2],original_points[256][2];
static unsigned original_count,original_kind;
static float original_width;
static void graph_color(ee_ctx *c){memcpy(original_color,c->ram+c->r[5].uw[0],16);}
static void graph_record(ee_ctx *c){original_count=c->r[6].uw[0];original_width=c->fpr[12].f;
 if(original_count<=256)memcpy(original_points,c->ram+c->r[5].uw[0],original_count*8);
 memcpy(original_size,c->ram+c->r[7].uw[0],8);
}
static void graph_polyline(ee_ctx *c){original_kind=0;graph_record(c);}
static void graph_pairs(ee_ctx *c){original_kind=1;graph_record(c);}
static ee_fn graph_lookup(uint32_t pc){switch(pc){case 0x70000:return graph_color;case 0x70004:return graph_polyline;case 0x70008:return graph_pairs;default:return recomp_lookup(pc);}}
static int graph_invoke(ee_ctx *c,uint32_t pc){jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump))return 0;recomp_lookup(pc)(c);return 1;}
static int setting_graph_draw(const uint8_t *ram,const uint8_t *graph,ui_widget *gf){
 ee_ctx c;DRAW_CHECK(!ee_init(&c));memcpy(c.ram,ram,EE_RAM_SIZE);c.lookup=graph_lookup;
 uint32_t address=(uint32_t)(graph-ram),context=0x80000,vt=0x80100,bounds=0x80500;
 ee_w32(&c,context+4,vt);ee_w32(&c,vt+0x19c,0x70000);ee_w32(&c,vt+0x264,0x70004);ee_w32(&c,vt+0x26c,0x70008);
 memset(c.r,0,sizeof c.r);c.r[4].uw[0]=address;c.r[5].uw[0]=context;c.r[29].uw[0]=0x1ff0000;
 original_count=0;DRAW_CHECK(graph_invoke(&c,0x2e88f8));DRAW_CHECK(original_count==10 && original_kind==1);
 memset(c.r,0,sizeof c.r);c.r[4].uw[0]=address;
 for(int j=0;j<4;j++)c.r[5+j].uw[0]=bounds+4*j;c.r[29].uw[0]=0x1ff0000;
 DRAW_CHECK(graph_invoke(&c,0x25baa0));float rect[4];memcpy(rect,c.ram+bounds,16);
 DRAW_CHECK(fabsf(rect[2]-original_size[0])<1e-4f && fabsf(rect[3]-original_size[1])<1e-4f);
 /* Match all six captured widget rectangles, including the project root,
    before comparing the normalized graph endpoints at the drawing boundary. */
 float x=0,y=0;unsigned widgets=0;uint32_t parent=address;
 for(ui_widget *w=gf;w;w=w->parent){
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=parent;c.r[5].uw[0]=parent+0x34;
  for(int j=0;j<4;j++)c.r[6+j].uw[0]=bounds+4*j;c.r[29].uw[0]=0x1ff0000;
  DRAW_CHECK(graph_invoke(&c,0x25b578));float local[4];memcpy(local,c.ram+bounds,16);
  float native[4]={w->x,w->y,w->w,w->h};
  if(getenv("GT4_SETTING_DRAW_GEOMETRY_NEGATIVE") && widgets==0)native[1]+=1;
  for(int j=0;j<4;j++)DRAW_CHECK(fabsf(local[j]-native[j])<1e-4f);
  x+=w->x;y+=w->y;parent=ee_r32(&c,parent+0x54);widgets++;
 }
 DRAW_CHECK(widgets==6 && lines==original_count/2);
 for(unsigned i=0;i<lines;i++){
  for(unsigned end=0;end<2;end++){
   float cx=(drawn[i][end][0]+drawn[i][end+2][0])*.5f;
   float cy=(drawn[i][end][1]+drawn[i][end+2][1])*.5f;
   float ox=rect[0]+original_points[2*i+end][0]*original_size[0];
   float oy=rect[1]+(1-original_points[2*i+end][1])*original_size[1];
   DRAW_CHECK(fabsf(cx-ox)<1e-4f && fabsf(cy-oy)<1e-4f);
  }
 }
 DRAW_CHECK(fabsf(x-rect[0])<1e-4f && fabsf(y-rect[1])<1e-4f);
 DRAW_CHECK(original_width==gf->graph_line_width || (!gf->graph_line_width && original_width==2));
 const uint8_t rgba[4]={78,125,160,255};
 for(int j=0;j<4;j++)DRAW_CHECK(fabsf(original_color[j]-rgba[j]/255.f)<1e-6f);
 printf("PASS original MGraphFace draw list: 6 widget rectangles, 10 endpoints, type, width and RGBA; absolute geometry tolerance 1e-4.\n");
 ee_free(&c);return 1;
}

#undef DRAW_CHECK
