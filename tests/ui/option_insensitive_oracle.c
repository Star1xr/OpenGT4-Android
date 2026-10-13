/* Native OptionMenu host versus original 002C9B68/002C9740.
   Container traversal and UI redraw are explicit boundaries. */
#define main pregrid_menu_fixture_main
#include "pregrid_menu.c"
#undef main
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
#define OPT 0x1800000u
#define NODE 0x1801000u
#define MASK 0x1802000u
static int selected;
static uint32_t word(ee_ctx *c,uint32_t at){uint32_t v;memcpy(&v,c->ram+at,4);return v;}
static void put(ee_ctx *c,uint32_t at,uint32_t v){memcpy(c->ram+at,&v,4);}
static void count_items(ee_ctx *c){c->r[2].sd[0]=6;}
static void first_item(ee_ctx *c){c->r[2].uw[0]=NODE;}
static void next_item(ee_ctx *c){c->r[2].uw[0]=word(c,c->r[4].uw[0]+4);}
static void reselection(ee_ctx *c){
    c->r[5].uw[0]=c->r[6].uw[0];recomp_lookup(0x2c9740)(c);
    if(c->r[2].sw[0]>=0)selected=c->r[2].sw[0];
}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x57ce60:return count_items;case 0x206868:return first_item;
 case 0x25c300:return next_item;case 0x2c9898:return reselection;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
int main(int argc,char **argv){
 CHECK(argc==3);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x500000);
 c.lookup=lookup;c.hook=boundary;
 const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[2],&error);CHECK(vol);char msg[256];
 menu *m=menu_open(vol,"quick-arcade",msg,sizeof msg);CHECK(m);
 ui_widget *option=ui_widget_from_text(m->ui,0,
 "OptionMenu { name string {\"MaskFixture\"} children [1] { Composite { name string {\"popup\"} children [1] { VBox { name string {\"vbox\"} children [7] { Composite {} ColorFace {} Composite {} Composite {} Composite {} Composite {} Composite {} } } } } } }");
 CHECK(option && ui_option_count(option)==6);ui_widget_append(ui_root_widget(m->ui,0),option);bind_tree(m,option);
 uint32_t seed=0x20261004;
 for(unsigned test=0;test<4096;test++){
  unsigned n=test%10;selected=(int)(test%6);option->focus_index=selected;
  put(&c,OPT+0xc4,0x1803000);put(&c,OPT+0xd8,(uint32_t)selected);
  ah_obj *mask=ah_array_new(m->vm);int logical=0;
  for(unsigned j=0;j<7;j++){
   uint32_t at=NODE+j*0x100;put(&c,at+4,j==6?0:at+0x100);
   seed=seed*1664525u+1013904223u;unsigned disabled=(seed>>30)&1;
   put(&c,at+0x98,j==1?0x2000u:disabled<<3);
   if(j!=1)ui_option_item(option,logical++)->insensitive=(int)disabled;
  }
  for(unsigned j=0;j<n;j++){
   seed=seed*1664525u+1013904223u;int64_t value=(seed&1)?-7:0;
   if(test%5==0)value=INT64_C(0x100000000); /* integer conversion's low word */
   ah_array_push(m->vm,mask,ah_int(value));put(&c,MASK+j*4,(uint32_t)value!=0);
  }
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=OPT;c.r[5].uw[0]=0;c.r[6].uw[0]=MASK;c.r[7].uw[0]=n;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x2c9b68)(&c);
  ah_val args[2]={ah_objv(m->context),ah_objv(mask)},ret;
  CHECK(ah_call_member(m->vm,widget_module(m,option),"setInsensitive",2,args,&ret)>=0 && ret.t==AV_NIL);
  if(getenv("GT4_OPTION_INSENSITIVE_NEGATIVE"))ui_option_item(option,0)->insensitive^=1;
  logical=0;for(unsigned j=0;j<7;j++)if(j!=1){int native=ui_option_item(option,logical++)->insensitive,original=(word(&c,NODE+j*0x100+0x98)>>3)&1;
   if(native!=original)fprintf(stderr,"mask test=%u n=%u child=%u native=%d original=%d\n",test,n,j,native,original);CHECK(native==original);
  }
  CHECK(option->focus_index==selected);ah_gc(m->vm);
 }
 puts("PASS 4096 original OptionMenu masks: short/long/empty lists, untouched tails, decoration skipped, disabled current reselection, all-disabled preservation and native integer array bridge.");
 menu_close(m);gt4_vol_close(vol);ee_free(&c);return 0;
}
