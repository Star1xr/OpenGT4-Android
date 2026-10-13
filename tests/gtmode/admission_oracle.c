/* Executes the original country/allowed-list callbacks. VM allocation,
   boxing, compressed table IO and key-to-label lookup are boundaries. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
#include "career_admission.h"
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
static uint8_t *allow_db,*generic_ids,*enemy_ids;
static size_t allow_size,generic_size,enemy_size;
static char original_labels[60][128];
static unsigned original_count,heap;static int has_array;
static unsigned passed_series;
static void put(ee_ctx *c,uint32_t a,uint32_t x){memcpy(c->ram+a,&x,4);}
static uint32_t word(ee_ctx *c,uint32_t a){uint32_t x;memcpy(&x,c->ram+a,4);return x;}
static void noop(ee_ctx *c){(void)c;}
static void licence_passed(ee_ctx *c){
    static const char *series[]={"l0b","l0a","lib","lia","l0s"};int index=-1;
    for(int i=0;i<5;i++)if(!strcmp((char *)c->ram+c->r[5].uw[0],series[i]))index=i;
    CHECK(index>=0);c->r[2].ud[0]=(passed_series>>index)&1;
}
static void copy_bytes(ee_ctx *c){
    memcpy(c->ram+c->r[4].uw[0],c->ram+c->r[5].uw[0],c->r[6].uw[0]);
    c->r[2].ud[0]=c->r[4].ud[0];
}
static void race_value(ee_ctx *c){put(c,c->r[4].uw[0],0x1830000);}
static void array_value(ee_ctx *c){put(c,c->r[4].uw[0],0x1840000);has_array=1;}
static void string_length(ee_ctx *c){c->r[2].ud[0]=strlen((char *)c->ram+c->r[4].uw[0]);}
static void string_new(ee_ctx *c){
    uint32_t p=heap+16;heap+=160;put(c,p-8,8);
    strcpy((char *)c->ram+p,(char *)c->ram+c->r[7].uw[0]);put(c,c->r[4].uw[0],p);
}
static void string_box(ee_ctx *c){put(c,c->r[4].uw[0],word(c,c->r[5].uw[0]));}
static void append(ee_ctx *c){CHECK(original_count<60);strcpy(original_labels[original_count++],(char *)c->ram+word(c,c->r[6].uw[0]));}
static void read_allow(ee_ctx *c){
    c->r[2].ud[0]=gt4_specdb_row(allow_db,allow_size,c->r[5].uw[0],c->ram+c->r[6].uw[0],480)==480?480:0;
}
static void label_key(ee_ctx *c){
    uint64_t k=c->r[4].ud[0];const char *s=NULL;
    if(k>>32==0)s=gt4_specdb_label_for_id(generic_ids,generic_size,(uint32_t)k);
    else if(k>>32==0x26)s=gt4_specdb_label_for_id(enemy_ids,enemy_size,(uint32_t)k);
    c->r[2].ud[0]=s!=NULL;if(s)strcpy((char *)c->ram+c->r[5].uw[0],s);
}
static ee_fn boundary(uint32_t pc){switch(pc){
    case 0x1a0b50:return race_value;case 0x2efbf0:return array_value;
    case 0x57f260:return string_length;case 0x5c2630:return string_new;
    case 0x314b20:return string_box;case 0x2eeeb0:return append;
    case 0x449298:return read_allow;case 0x447ca0:return label_key;
    case 0x5a4724:return copy_bytes;
    case 0x4312b0:return licence_passed;
    case 0x309378:case 0x312318:case 0x3285a8:case 0x3285f8:case 0x2ed5c0:case 0x1a0af8:return noop;
    default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn fn=boundary(pc);return fn?fn:recomp_lookup(pc);}
/* Original 004490F8 sets the table pointers; 004493F8 executes the entire
   Huffman + template/patch decoder. Only memcpy is a host boundary here. */
static void wide_rows(ee_ctx *c){
    const uint32_t table=0x1810000,db=0x1600000,out=0x1800000;
    CHECK(allow_size<table-db);memcpy(c->ram+db,allow_db,allow_size);
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=table;c->r[5].uw[0]=db;
    c->r[29].uw[0]=0x1ef0000;c->lookup=lookup;c->hook=boundary;
    jmp_buf jump;c->exit_jmp=&jump;
    if(setjmp(jump)){fprintf(stderr,"wide decoder stopped at %08x: %s\n",c->end_pc,c->end_detail);exit(1);}
    lookup(0x4490f8)(c);
    uint32_t count=word(c,db+8);CHECK(word(c,db+12)==480 && count);
    for(unsigned i=0;i<count;i++){
        uint8_t native[480];CHECK(gt4_specdb_row_at(allow_db,allow_size,i,native,sizeof native)==480);
        memset(c->ram+out-16,0xa5,512);memset(c->r,0,sizeof c->r);
        c->r[4].uw[0]=table;c->r[5].uw[0]=word(c,table+0x20)+word(c,db+20+i*8);
        c->r[6].uw[0]=out;c->r[29].uw[0]=0x1ef0000;lookup(0x4493f8)(c);
        if(getenv("GT4_WIDE_ROW_NEGATIVE") && !i)native[479]^=1;
        CHECK(!memcmp(native,c->ram+out,sizeof native));
        for(unsigned j=0;j<16;j++)CHECK(c->ram[out-16+j]==0xa5 && c->ram[out+480+j]==0xa5);
        uint8_t undersized[479];CHECK(!gt4_specdb_row_at(allow_db,allow_size,i,undersized,sizeof undersized));
    }
    printf("PASS all %u ALLOW_ENTRY rows: 480 decoded bytes and output guards against original 004493F8.\n",count);
}
static void original(ee_ctx *c,uint32_t fn,const uint8_t row[151]){
    memcpy(c->ram+0x1830010,row,151);memset(c->r,0,sizeof c->r);
    c->r[4].uw[0]=0x1820000;c->r[29].uw[0]=0x1ef0000;
    original_count=0;has_array=0;heap=0x1850000;
    put(c,0x659fb0,8);put(c,0x659fb4,0);c->lookup=lookup;c->hook=boundary;
    jmp_buf jump;c->exit_jmp=&jump;
    if(setjmp(jump)){fprintf(stderr,"original %08x stopped at %08x: %s\n",fn,c->end_pc,c->end_detail);exit(1);}lookup(fn)(c);
}
int main(int argc,char **argv){
    CHECK(argc==3);ee_ctx c;CHECK(!ee_init(&c));FILE *f=fopen(argv[1],"rb");CHECK(f);
    size_t n=fread(c.ram+0x100000,1,EE_RAM_SIZE-0x100000,f);fclose(f);CHECK(n>0x592880);
    const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[2],&error);CHECK(vol && gt4_game_load_disc(argv[2]));
    allow_db=gt4_vol_read(vol,"/specdb/GT4_US2560/ALLOW_ENTRY.dbt",&allow_size);
    generic_ids=gt4_vol_read(vol,"/specdb/GT4_US2560/GENERIC_CAR.idi",&generic_size);
    enemy_ids=gt4_vol_read(vol,"/specdb/GT4_US2560/ENEMY_CARS.idi",&enemy_size);CHECK(allow_db && generic_ids && enemy_ids);
    wide_rows(&c);
    uint8_t row[151]={0};
    for(unsigned need=0;need<256;need++)for(passed_series=0;passed_series<32;passed_series++){
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1830000;c.r[29].uw[0]=0x1ef0000;c.ram[0x1830045]=(uint8_t)need;
        lookup(0x110498)(&c);int expected=(int)c.r[2].ud[0];
        if(getenv("GT4_ASPEC_ADMISSION_NEGATIVE") && !need && !passed_series)expected^=1;
        CHECK(gt4_career_aspec_allowed((uint8_t)need,passed_series)==expected);
    }
    printf("PASS A-spec licence eligibility: 8192 cases against original 00110498 (series records boundary).\n");
    for(unsigned i=0;i<256;i++){
        row[0x96]=(uint8_t)i;original(&c,0x1a1c60,row);const char *names[8];unsigned count=gt4_career_countries((uint8_t)i,names);
        if(getenv("GT4_ADMISSION_NEGATIVE") && i==1)count--;
        CHECK(has_array && count==original_count);for(unsigned j=0;j<count;j++)CHECK(!strcmp(names[j],original_labels[j]));
    }
    size_t races_size;uint8_t *races=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.dbt",&races_size);CHECK(races);
    char message[256];menu *m=menu_open(vol,"gtmode",message,sizeof message);CHECK(m);load_race_tables(m);
    ah_vm *vm=m->vm;ah_val main_,native,klass;
    CHECK(ah_get_member(vm,ah_root(vm),"main",&main_));
    CHECK(ah_get_member(vm,main_.u.o,"menu",&native));
    CHECK(ah_get_member(vm,native.u.o,"MRaceData",&klass));ah_set_log(vm,log_script,NULL);
    uint32_t count;memcpy(&count,races+8,4);unsigned restricted=0,total=0;
    for(unsigned i=0;i<count;i++){
        CHECK(gt4_specdb_row_at(races,races_size,i,row,sizeof row)==151);
        original(&c,0x1a2bb0,row);char labels[60][128];unsigned size;
        int available=gt4_career_allowed_cars(vol,row,labels,&size);
        CHECK(available==has_array && size==original_count);
        for(unsigned j=0;j<size;j++)CHECK(!strcmp(labels[j],original_labels[j]));
        uint32_t id;memcpy(&id,races+16+i*8,4);
        const char *race=gt4_specdb_label_for_id(m->race_labels,m->race_labels_size,id);CHECK(race);
        ah_val arg=ah_str(vm,race),object,result;
        CHECK(ah_call(vm,klass,1,&arg,&object)>=0);
        if(getenv("GT4_ADMISSION_BINDING_NEGATIVE") && !i)ah_remove_member(vm,object.u.o,"getNeedCountry");
        CHECK(ah_call_member(vm,object.u.o,"getAllowedCarList",0,NULL,&result)>=0);
        CHECK(available?(result.t==AV_OBJ && ah_array_size(result.u.o)==(int)size):result.t==AV_NIL);
        for(unsigned j=0;j<size;j++)CHECK(!strcmp(ah_cstr(ah_array_get(result.u.o,j)),labels[j]));
        const char *countries[8];unsigned country_count=gt4_career_countries(row[0x96],countries);
        CHECK(ah_call_member(vm,object.u.o,"getNeedCountry",0,NULL,&result)>=0);
        CHECK(result.t==AV_OBJ && ah_array_size(result.u.o)==(int)country_count);
        for(unsigned j=0;j<country_count;j++)CHECK(!strcmp(ah_cstr(ah_array_get(result.u.o,j)),countries[j]));
        CHECK(menu_career_aspec_available(m,race)==gt4_career_aspec_allowed(row[0x45],0));
        restricted+=available;total+=size;
    }
    ah_val car_class;CHECK(ah_get_member(vm,native.u.o,"MCarData",&car_class));
    size_t generic_db_size;uint8_t *generic_db=gt4_vol_read(vol,"/specdb/GT4_US2560/GENERIC_CAR.dbt",&generic_db_size);CHECK(generic_db);
    uint32_t cars;memcpy(&cars,generic_db+8,4);
    unsigned open_checks=0;
    for(unsigned i=0;i<cars;i++){
        uint8_t car[24];CHECK(gt4_specdb_row_at(generic_db,generic_db_size,i,car,sizeof car)==24);
        memcpy(c.ram+0x1830000,car,24);memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1830000;c.r[5].uw[0]=5;
        lookup(0x4489a0)(&c);int expected=c.r[2].uw[0];
        uint32_t id;memcpy(&id,generic_db+16+i*8,4);const char *label=gt4_specdb_label_for_id(generic_ids,generic_size,id);CHECK(label);
        ah_val arg=ah_str(vm,label),result;CHECK(ah_call_member(vm,car_class.u.o,"IsTestCar",1,&arg,&result)>=0);
        CHECK(ah_truthy(result)==expected && gt4_arcade_test_car(arcade_of(m),label)==expected);
        /* Actual registered owned-car predicate. Alternate setups select
           a different original generic key, catching a stale spec_id cache.
           Ownership and serialized bodies are explicit fixture boundaries. */
        ah_obj *owned=ah_module_new(vm,"owned open-car fixture",NULL,NULL,NULL);
        ah_set_member(vm,m->project,"_open_test_car",ah_objv(owned));
        ah_set_member(vm,owned,"car_code",ah_str(vm,label));
        uint8_t body[0x4b8]={0};ah_val selected=ah_objv(owned),bound;
        for(unsigned slot=0;slot<3;slot++){
            unsigned index=(i+slot)%cars;uint32_t active_id;uint8_t active_row[24];
            memcpy(&active_id,generic_db+16+index*8,4);
            CHECK(gt4_specdb_row_at(generic_db,generic_db_size,index,active_row,sizeof active_row)==24);
            uint64_t key=active_id;memcpy(body+slot*0x178,&key,8);memcpy(body+0x488,&slot,4);
            CHECK(gt4_career_blob_set(vm,owned,"full_car_ps2",body,sizeof body));
            CHECK(nat_bind_game_car(vm,NULL,m,1,&selected,&bound)>=0);
            memcpy(c.ram+0x1830000,active_row,24);memset(c.r,0,sizeof c.r);
            c.r[4].uw[0]=0x1830000;c.r[5].uw[0]=4;lookup(0x4489a0)(&c);expected=c.r[2].uw[0];
            CHECK(ah_call_member(vm,owned,"isOpenCar",0,NULL,&result)>=0);
            int actual=ah_truthy(result);
            if(getenv("GT4_OPEN_CAR_NEGATIVE") && !i && !slot)actual^=1;
            CHECK(actual==expected);open_checks++;
            uint8_t after[0x4b8];CHECK(gt4_game_car_body(vm,owned,after) && !memcmp(body,after,sizeof body));
        }
    }
    size_t tuned_size;uint8_t *tuned_db=gt4_vol_read(vol,"/specdb/GT4_US2560/ENEMY_CARS.dbt",&tuned_size);CHECK(tuned_db);
    uint32_t tuned_count;memcpy(&tuned_count,tuned_db+8,4);
    for(unsigned i=0;i<tuned_count;i++){
        uint8_t tuned[32],car[24];CHECK(gt4_specdb_row_at(tuned_db,tuned_size,i,tuned,sizeof tuned)==24);
        uint64_t generic_key;memcpy(&generic_key,tuned,8);CHECK(!(generic_key>>32));
        CHECK(gt4_specdb_row(generic_db,generic_db_size,(uint32_t)generic_key,car,sizeof car)==24);
        const char *label=gt4_specdb_label_for_id(generic_ids,generic_size,(uint32_t)generic_key);CHECK(label);
        ah_obj *owned=ah_module_new(vm,"tuned open-car fixture",NULL,NULL,NULL);
        ah_set_member(vm,m->project,"_open_test_car",ah_objv(owned));ah_set_member(vm,owned,"car_code",ah_str(vm,label));
        uint8_t body[0x4b8]={0};uint32_t id;memcpy(&id,tuned_db+16+i*8,4);
        uint64_t key=UINT64_C(0x2600000000)|id;memcpy(body,&key,8);
        CHECK(gt4_career_blob_set(vm,owned,"full_car_ps2",body,sizeof body));
        ah_val selected=ah_objv(owned),result;CHECK(nat_bind_game_car(vm,NULL,m,1,&selected,&result)>=0);
        memcpy(c.ram+0x1830000,car,24);memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1830000;c.r[5].uw[0]=4;
        lookup(0x4489a0)(&c);CHECK(ah_call_member(vm,owned,"isOpenCar",0,NULL,&result)>=0);
        int actual=ah_truthy(result);if(getenv("GT4_OPEN_TUNED_NEGATIVE") && !i)actual^=1;
        CHECK(actual==(int)c.r[2].uw[0]);open_checks++;
    }
    free(tuned_db);
    CHECK(restricted && total && !errors);menu_close(m);free(generic_db);
    printf("PASS original country arrays (256 selectors), allowed arrays/nil/order (%u races, %u restricted, %u labels). VM and table IO boundaries.\n",count,restricted,total);
    printf("PASS registered IsTestCar on %u disc cars against original flag getter 004489A0.\n",cars);
    printf("PASS registered isOpenCar: %u original active-key/flag checks (%u generic A/B/C, %u ENEMY_CARS); no solver changes.\n",open_checks,cars*3,tuned_count);
    free(races);free(allow_db);free(generic_ids);free(enemy_ids);gt4_vol_close(vol);ee_free(&c);return 0;
}
