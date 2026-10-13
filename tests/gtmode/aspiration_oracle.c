/* The original Car callbacks query the same owned setup and disc rows.
   Adhoc handle conversion/boxing and proven SpecDB IO are boundaries. */
#define GT4_SUNDAY_EMBEDDED 1
#include "sunday_playable.c"
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
static gt4_spec_tables *query_tables;
static uint32_t full=0x1810000;
static int boxed;
static void handle(ee_ctx *c){uint32_t p=full;memcpy(c->ram+c->r[4].uw[0],&p,4);}
static void body_handle(ee_ctx *c){c->r[2].uw[0]=full;}
static void box(ee_ctx *c){boxed=(int)c->r[5].uw[0];memset(c->ram+c->r[4].uw[0],0,4);}
static void ignore(ee_ctx *c){(void)c;}
static void row(ee_ctx *c){uint64_t key=c->r[5].ud[0];int n=gt4_spec_tables_row(query_tables,(uint32_t)(key>>32),(uint32_t)key,0,c->ram+c->r[6].uw[0],256);c->r[2].uw[0]=n;}
static void generic(ee_ctx *c){uint64_t key=c->r[4].ud[0];uint8_t prepared[24];
    if(key>>32==0x26 && gt4_spec_tables_row(query_tables,0x26,(uint32_t)key,0,prepared,sizeof prepared)==24)memcpy(&key,prepared,8);
    c->r[2].uw[0]=key>>32==0 && gt4_spec_tables_row(query_tables,0,(uint32_t)key,0,c->ram+c->r[5].uw[0],24)==24;
}
static ee_fn boundary(uint32_t pc){switch(pc){
    case 0x13bdc0:return handle;case 0x147d80:return body_handle;case 0x2fe278:return box;
    case 0x2fc870:case 0x13bd68:case 0x3285a8:case 0x3285f8:return ignore;
    case 0x443ed0:return row;case 0x448820:return generic;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static int invoke(ee_ctx *c,uint32_t pc,int callback){
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=callback?0x1820000:full;
    c->r[29].uw[0]=0x1ef0000;boxed=-1;memset(c->ram+0x1820000,0,4);
    jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"aspiration oracle %08x stopped %08x: %s\n",pc,c->end_pc,c->end_detail);return -1;}
    lookup(pc)(c);return callback?boxed:(int)c->r[2].uw[0];
}
int main(int argc,char **argv){
    CHECK(argc==3);ee_ctx c;CHECK(!ee_init(&c));FILE *core=fopen(argv[1],"rb");CHECK(core);
    CHECK(fread(c.ram+0x100000,1,EE_RAM_SIZE-0x100000,core)>0x592880);fclose(core);c.lookup=lookup;c.hook=boundary;
    const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[2],&error);CHECK(vol && gt4_game_load_disc(argv[2]));
    char message[256];menu *m=menu_open(vol,"gtmode",message,sizeof message);CHECK(m);load_car_tables(m);
    ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);query_tables=gt4_spec_tables_open(vol,NULL);CHECK(query_tables);
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[2]);CHECK(factory);
    ah_obj *car=ah_module_new(vm,"aspiration_owned_fixture",NULL,NULL,NULL);ah_set_member(vm,m->project,"_aspiration_fixture",ah_objv(car));
    const uint32_t functions[]={0x13f598,0x13f668,0x1467a0,0x13f488};const char *names[]={"isNAtune","isSuperCharger","getAspiration","isTurbo"};unsigned cases=0;
    for(unsigned i=0;i<car_u32(m->car_labels+4);i++){
        uint32_t id=car_u32(m->car_labels+20+i*8);const char *label=gt4_specdb_label_for_id(m->car_labels,m->car_labels_size,id);CHECK(label);
        uint8_t unit[32]={0},base[0x4b8];CHECK(gt4_career_vehicle_create(factory,label,0,0,unit,base));
        size_t n;uint8_t *parts=gt4_spec_tables_car_parts(query_tables,id,&n);CHECK(parts);
        uint64_t turbo=UINT64_MAX,super=UINT64_MAX;
        for(unsigned j=0;j<car_u32(parts);j++){uint64_t key;memcpy(&key,parts+8+j*16,8);
            if(key>>32==14 && parts[16+j*16]>0)turbo=key;
            if(key>>32==28 && parts[16+j*16]==1)super=key;}
        free(parts);ah_set_member(vm,car,"car_code",ah_str(vm,label));ah_set_member(vm,car,"color_index",ah_int(0));
        ah_val selected=ah_objv(car),ret;CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",base,sizeof base));CHECK(nat_bind_game_car(vm,NULL,m,1,&selected,&ret)>=0);
        for(unsigned slot=0;slot<3;slot++)for(unsigned variant=0;variant<4;variant++){
            uint8_t body[0x4b8];memcpy(body,base,sizeof body);memcpy(body+0x488,&slot,4);
            if(variant&1)memcpy(body+slot*0x178+0xb0,&turbo,8);
            if(variant&2)memcpy(body+slot*0x178+0x108,&super,8);
            CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body));memcpy(c.ram+full+8,body,sizeof body);
            for(unsigned k=0;k<4;k++){
                int original=invoke(&c,functions[k],k!=2);CHECK(original>=0);
                CHECK(ah_call_member(vm,car,names[k],0,NULL,&ret)>=0);
                int actual=(int)ah_num(ret);if(getenv("GT4_ASPIRATION_NEGATIVE") && !cases)actual^=1;
                if(actual!=original)fprintf(stderr,"%s %s slot=%u variant=%u native=%d original=%d\n",label,names[k],slot,variant,actual,original);
                CHECK(actual==original);cases++;
            }
            uint8_t after[0x4b8];CHECK(gt4_game_car_body(vm,car,after) && !memcmp(after,body,sizeof body));
        }
    }
    CHECK(!script_errors);printf("PASS %u aspiration callbacks, all cars/three active setups, stock/turbo/supercharger/removed parts; body unchanged.\n",cases);
    gt4_career_vehicle_close(factory);gt4_spec_tables_close(query_tables);menu_close(m);gt4_vol_close(vol);ee_free(&c);return 0;
}
