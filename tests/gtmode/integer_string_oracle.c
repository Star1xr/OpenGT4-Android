/* Exercise the actual VM helper and the original Integer relational
   callbacks. Only allocation/boxing/refcounts are test boundaries. */
#include "../../src/core/adhoc/adhoc_vm.c"
#include "ee.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
ee_fn recomp_lookup(uint32_t);
static int result;
static void box(ee_ctx *c){result=(int)c->r[5].uw[0];memset(c->ram+c->r[4].uw[0],0,4);}
static void ignore(ee_ctx *c){(void)c;}
static ee_fn boundary(uint32_t pc){switch(pc){case 0x2fe278:return box;
 case 0x2fc870:case 0x3285a8:case 0x3285f8:return ignore;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
int main(int argc,char **argv){
 CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));FILE *file=fopen(argv[1],"rb");CHECK(file);
 CHECK(fread(c.ram+0x100000,1,EE_RAM_SIZE-0x100000,file)>0x592880);fclose(file);c.lookup=lookup;c.hook=boundary;
 ah_vm *vm=ah_vm_new();CHECK(vm);
 const char *operators[]={"<",">","<=",">="};
 const uint32_t functions[]={0x2fd0b0,0x2fcf80,0x2fd148,0x2fd018};
 const char *strings[]={"", "0", "3400", "3401", "3399", "999", "10000", "-12", " +42rest", "garbage", "2147483647", "2147483648", "-2147483648", "-2147483649"};
 const int32_t values[]={0,3400,42,-12,INT32_MAX,INT32_MIN};unsigned cases=0;
 for(unsigned s=0;s<sizeof strings/sizeof *strings;s++)for(unsigned v=0;v<sizeof values/sizeof *values;v++)for(unsigned k=0;k<4;k++){
  memset(c.r,0,sizeof c.r);uint32_t rhs=0x1811000,text=0x1812010,length=(uint32_t)strlen(strings[s]),vt=0x675268;
  memcpy(c.ram+rhs+4,&vt,4);memcpy(c.ram+rhs+0x10,&text,4);memcpy(c.ram+text-0x10,&length,4);memcpy(c.ram+text,strings[s],length+1);
  memcpy(c.ram+0x1810000+0x10,&values[v],4);memcpy(c.ram+0x1813000,&rhs,4);memset(c.ram+0x1814000,0,4);
  c.r[4].uw[0]=0x1810000;c.r[5].uw[0]=0x1814000;c.r[8].uw[0]=0x1813000;c.r[29].uw[0]=0x1ef0000;result=-1;
  jmp_buf jump;c.exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"original stopped %08x: %s\n",c.end_pc,c.end_detail);return 1;}lookup(functions[k])(&c);CHECK(result>=0);
  ah_val native=binop(vm,operators[k],ah_int(values[v]),ah_str(vm,strings[s]));int actual=ah_truthy(native);
  if(getenv("GT4_INTEGER_STRING_NEGATIVE") && !cases)actual^=1;
  if(actual!=result)fprintf(stderr,"%d %s '%s': native=%d original=%d\n",values[v],operators[k],strings[s],actual,result);
  CHECK(actual==result);cases++;
 }
 printf("PASS %u Integer/String relational results, numeric boundaries and invalid decimal strings.\n",cases);
 ah_vm_free(vm);ee_free(&c);return 0;
}
