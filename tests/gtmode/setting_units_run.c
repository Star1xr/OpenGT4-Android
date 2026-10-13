/* Original MUnit torque wrapper and conversion, independent of the
   display adapter. VM numeric unboxing/boxing are explicit boundaries. */
#define main setting_menu_fixture_main
#include "setting_menu.c"
#undef main
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
static float input;static int original;
static void number(ee_ctx *c){c->fpr[0].f=input;}
static void integer(ee_ctx *c){original=c->r[5].sw[0];ee_w32(c,c->r[4].uw[0],0);}
static void noop(ee_ctx *c){(void)c;}
static ee_fn boundary(uint32_t pc){switch(pc){case 0x70000:return number;case 0x2fe278:return integer;case 0x3285a8:case 0x3285f8:case 0x2fc870:return noop;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
int main(int argc,char **argv){
 CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0);c.lookup=lookup;c.hook=boundary;
 ah_vm *vm=ah_vm_new();CHECK(vm);saved_option *option=NULL;
 for(unsigned i=0;i<sizeof saved_options/sizeof saved_options[0];i++)if(!strcmp(saved_options[i].name,"unit_torque"))option=&saved_options[i];CHECK(option);
 const char *keys[]={"KGFM","FTLB","NM"};unsigned cases=0;
 for(unsigned unit=0;unit<3;unit++){
  option->valid=1;option->type=1;snprintf(option->text,sizeof option->text,"%s",keys[unit]);ee_w32(&c,0x6244d8+8,unit);
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x6244d8;recomp_lookup(0x472758)(&c);
  ah_val label;CHECK(nat_torque_unit(vm,NULL,NULL,0,NULL,&label)>=0);
  CHECK(!strcmp(ah_cstr(label),(char *)c.ram+c.r[2].uw[0]));
  for(unsigned sample=0;sample<65536;sample++){
   input=(float)(int)sample/64.f-100.f;
   memset(c.r,0,sizeof c.r);ee_w32(&c,0x1800004,0x1800100);ee_w32(&c,0x1800164,0x70000);ee_w32(&c,0x1800200,0x1800000);ee_w32(&c,0x1800300,0);
   c.r[4].uw[0]=0x1800300;c.r[5].uw[0]=1;c.r[6].uw[0]=0x1800200;c.r[29].uw[0]=0x1ef0000;
   recomp_lookup(0x1af760)(&c);ah_val arg=ah_float(input),ret;
   CHECK(nat_torque_unit(vm,NULL,(void *)1,1,&arg,&ret)>=0 && ret.t==AV_INT);
   if(getenv("GT4_SETTING_UNITS_NEGATIVE") && unit==1)ret.u.i++;
   CHECK(ret.u.i==original);cases++;
  }
 }
 for(unsigned i=0;i<sizeof saved_options/sizeof saved_options[0];i++)if(!strcmp(saved_options[i].name,"unit_power"))option=&saved_options[i];
 const char *powers[]={"PS","HP","BHP","CH","KW","CV","PF"};
 for(unsigned unit=0;unit<7;unit++){
  option->valid=1;option->type=1;snprintf(option->text,sizeof option->text,"%s",powers[unit]);ee_w32(&c,0x6244d8+4,unit);
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x6244d8;recomp_lookup(0x4726b8)(&c);
  ah_val label;CHECK(nat_power_unit(vm,NULL,NULL,0,NULL,&label)>=0 && !strcmp(ah_cstr(label),(char *)c.ram+c.r[2].uw[0]));
  for(unsigned sample=0;sample<65536;sample++){
   input=(float)(int)sample/64.f-100.f;
   memset(c.r,0,sizeof c.r);ee_w32(&c,0x1800004,0x1800100);ee_w32(&c,0x1800164,0x70000);ee_w32(&c,0x1800200,0x1800000);ee_w32(&c,0x1800300,0);
   c.r[4].uw[0]=0x1800300;c.r[5].uw[0]=1;c.r[6].uw[0]=0x1800200;c.r[29].uw[0]=0x1ef0000;
   recomp_lookup(0x1af598)(&c);ah_val arg=ah_float(input),ret;
   CHECK(nat_power_unit(vm,NULL,(void *)1,1,&arg,&ret)>=0 && ret.t==AV_INT);
   if(getenv("GT4_SETTING_POWER_NEGATIVE") && unit==4)ret.u.i++;
   CHECK(ret.u.i==original);cases++;
  }
 }
 printf("PASS %u original torque/power display conversions, ten unit labels and signed inputs; integer results identical.\n",cases);
 ah_vm_free(vm);ee_free(&c);return 0;
}
