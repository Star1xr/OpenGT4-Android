/* Discover owned-car input fixtures, not race results, from the user's disc.
   Candidate ranking is test policy; admission is the authored regulation
   script. No AI, vehicle parameter, finish or point is patched. */
#define GT4_SERIES_EMBEDDED 1
#include "series_playable.c"
#include "career_admission.h"

typedef struct {char label[96],parts[256];uint8_t body[0x4b8];double score;int tyre;} candidate;
static candidate fixture_candidates[722*9];
static unsigned candidate_count;
static int fixture_order(const void *a,const void *b)
{
    /* Stable, already exercised cars first. This is test input selection,
       not a game rule or an edit to vehicle/AI coefficients. */
    const char *preferred[]={"formula_gran_turismo_04","minolta_toyota_88cv_89","zonda_c12s_gmv_02","cerbera_speed12_00","srt4_03","rsc_tmv_lm_01","cappuccino_95","viper_srt10_03","silverado_sst_02","corvette_z06_00","zz2_00"};
    const candidate *left=a,*right=b;unsigned lp=99,rp=99;
    /* Optional test selection only: prefer disc labels containing a
       supplied token. Every resulting setup still passes all original
       stage regulations below; this never alters enemy entries. */
    const char *prefer=getenv("GT4_SERIES_PREFER");
    if(prefer){int lm=strstr(left->label,prefer)!=NULL,rm=strstr(right->label,prefer)!=NULL;if(lm!=rm)return rm-lm;}
    int stock_terrain=getenv("GT4_SERIES_STOCK_TERRAIN")!=NULL;
    int ls=!strcmp(left->parts,"25=11,26=11") || !strcmp(left->parts,"25=12,26=12");
    int rs=!strcmp(right->parts,"25=11,26=11") || !strcmp(right->parts,"25=12,26=12");
    if(stock_terrain && ls!=rs)return rs-ls;
#ifndef GT4_SERIES_CHASSIS_FIXTURE
    for(unsigned i=0;i<sizeof preferred/sizeof *preferred;i++){
#else
    for(unsigned i=0;stock_terrain && ls && rs && i<sizeof preferred/sizeof *preferred;i++){
#endif
        if(!strcmp(left->label,preferred[i]))lp=i;
        if(!strcmp(right->label,preferred[i]))rp=i;
    }
    if(lp!=rp)return lp<rp?-1:1;
    if(lp<99){
        int lt=!strcmp(left->parts,"25=7,26=7"),rt=!strcmp(right->parts,"25=7,26=7");
        if(lt!=rt)return rt-lt;
        lt=!*left->parts;rt=!*right->parts;if(lt!=rt)return rt-lt;
    }
    double x=((const candidate *)a)->score,y=((const candidate *)b)->score;
    return x>y?-1:x<y?1:strcmp(((const candidate *)a)->label,((const candidate *)b)->label);
}

static int fixture_part(ah_vm *vm,ah_obj *car,int type,int stage,char *description)
{
    ah_val args[]={ah_int(type),ah_int(stage)},ret;
    CHECK(ah_call_member(vm,car,"getParts",2,args,&ret)>=0);
    CHECK(ah_call_member(vm,car,"changeParts",2,args,&ret)>=0);
    CHECK(ah_call_member(vm,car,"setupPartsBuffer",0,NULL,&ret)>=0);
    CHECK(ah_call_member(vm,car,"getEquipedParts",1,args,&ret)>=0 && ah_num(ret)==stage);
    char item[32];snprintf(item,sizeof item,"%s%d=%d",*description?",":"",type,stage);
    CHECK(strlen(description)+strlen(item)<256);strcat(description,item);return 0;
}

int main(int argc,char **argv)
{
    CHECK(argc==2);const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);
    CHECK(vol && gt4_game_load_disc(argv[1]));char msg[256];menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);
    load_car_tables(m);load_race_tables(m);ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
    gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
    gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears,factory);
    gt4_spec_tables *tables=gt4_spec_tables_open(vol,NULL);CHECK(tables);
    /* Root a reusable Car object so authored calls cannot collect a C-only
       reference. Body installation goes through the same Car APIs as UI. */
    ah_obj *car=ah_module_new(vm,"series_owned_fixture",NULL,NULL,NULL);
    ah_set_member(vm,m->project,"_series_owned_fixture",ah_objv(car));
    const int types[]={8,13,14,15,16,18,19,20,11,21,22,23
#ifdef GT4_SERIES_CHASSIS_FIXTURE
        /* Additional owned-part test inputs. Names are the original enum
           006234B0: brake/brakecontroller/suspension/LSD. Every selected
           stage must exist in the car's original CARS list. */
        ,1,2,3,24
#endif
    };
    unsigned cars=car_u32(m->car_labels+4);
    CHECK(cars<=722);
    for(unsigned i=0;i<cars;i++){
        uint32_t id=car_u32(m->car_labels+20+i*8);
        const char *label=gt4_specdb_label_for_id(m->car_labels,m->car_labels_size,id);CHECK(label);
        uint8_t unit[32]={0},stock[0x4b8];
        if(!gt4_career_vehicle_create(factory,label,0,0,unit,stock))continue;
        int32_t stats[3];CHECK(gt4_career_vehicle_stats(factory,stock,stats));
        if(stats[0]<=0 || stats[2]<=0)continue;
        size_t n;uint8_t *parts=gt4_spec_tables_car_parts(tables,id,&n);CHECK(parts && n>=8);
        unsigned available[32]={0};
        for(unsigned j=0;j<car_u32(parts);j++){
            CHECK(8+(j+1)*16<=n);unsigned type=car_u32(parts+12+j*16),stage=parts[16+j*16];
            if(type<32 && stage<32)available[type]|=1u<<stage;
        }
        free(parts);
        ah_set_member(vm,car,"car_code",ah_str(vm,label));ah_set_member(vm,car,"color_index",ah_int(0));
        CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",stock,sizeof stock));
        ah_val selected=ah_objv(car),ret;CHECK(nat_bind_game_car(vm,NULL,m,1,&selected,&ret)>=0);
        CHECK(ah_call_member(vm,car,"beginSetting",0,NULL,&ret)>=0);
        char power_parts[256]="";
        for(unsigned t=0;t<sizeof types/sizeof *types;t++){
            int type=types[t],stage=31;while(stage>0 && !(available[type]&(1u<<stage)))stage--;
            if(stage)CHECK(!fixture_part(vm,car,type,stage,power_parts));
        }
        CHECK(ah_call_member(vm,car,"endSetting",0,NULL,&ret)>=0);
        uint8_t tuned[0x4b8];CHECK(gt4_game_car_body(vm,car,tuned));
        const int tyres[]={-1,10,7,11,12,7,10,11,12};
        for(unsigned variant=0;variant<9;variant++){
            int tyre=tyres[variant];if(tyre>=0 && (!(available[25]&(1u<<tyre)) || !(available[26]&(1u<<tyre))))continue;
            CHECK(candidate_count<sizeof fixture_candidates/sizeof *fixture_candidates);candidate *c=&fixture_candidates[candidate_count];
            snprintf(c->label,sizeof c->label,"%s",label);
            CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",variant && variant<5?tuned:stock,sizeof stock));
            c->parts[0]=0;
            if(variant){
                if(variant<5)strcpy(c->parts,power_parts);CHECK(ah_call_member(vm,car,"beginSetting",0,NULL,&ret)>=0);
                CHECK(!fixture_part(vm,car,25,tyre,c->parts));CHECK(!fixture_part(vm,car,26,tyre,c->parts));
                CHECK(ah_call_member(vm,car,"endSetting",0,NULL,&ret)>=0);
            }
            CHECK(gt4_game_car_body(vm,car,c->body));CHECK(gt4_career_vehicle_stats(factory,c->body,stats));
            c->score=(double)stats[0]/stats[2];
            uint64_t tyre_key;memcpy(&tyre_key,c->body+0x60,8);uint8_t tyre_row[128];
            CHECK(gt4_spec_tables_row(tables,(uint32_t)(tyre_key>>32),(uint32_t)tyre_key,0,tyre_row,sizeof tyre_row)>0x3a);
            c->tyre=tyre_row[0x3a];candidate_count++;
        }
        if(i%50==0){fprintf(stderr,"fixture cars %u/%u\n",i,cars);fflush(stderr);}
    }
    qsort(fixture_candidates,candidate_count,sizeof *fixture_candidates,fixture_order);
    ah_obj *data=get(vm,m->project,"PresentData").u.o,*details=get(vm,data,"detail_carlist").u.o;
    ah_obj *conditions=get(vm,data,"condition_carlist").u.o;unsigned series_count=0,stages=0,checks=0;
    for(int i=0;i<ah_array_size(details);i++){
        if(strcmp(ah_cstr(ah_array_get(conditions,i)),"win"))continue;
        const char *series=ah_cstr(ah_array_get(details,i));char series_copy[32];snprintf(series_copy,sizeof series_copy,"%s",series);
        ah_val arg=ah_str(vm,series_copy),list;CHECK(ah_call_member(vm,m->project,"extractSubRaceCode",1,&arg,&list)>=0);
        unsigned count=ah_array_size(list.u.o);CHECK(count && count<=15);
        char labels[15][32];for(unsigned j=0;j<count;j++)snprintf(labels[j],32,"%s",ah_cstr(ah_array_get(list.u.o,j)));
        uint8_t race_row[151],course_row[24];CHECK(gt4_career_race_row(gt4_game_race_lookup_data(),m->race_labels,m->race_labels_size,m->race_db,m->race_db_size,labels[0],race_row));
        CHECK(gt4_spec_tables_row(tables,0x22,race_row[0]|(unsigned)race_row[1]<<8,0,course_row,sizeof course_row)>0x10);
        int terrain=course_row[0x10];
        char allowed[60][128];unsigned allowed_count;int limited=gt4_career_allowed_cars(vol,race_row,allowed,&allowed_count);
        int best[24];for(unsigned j=0;j<24;j++)best[j]=-1;
        for(unsigned c=0;c<candidate_count;c++){
            candidate *choice=&fixture_candidates[c];
            if(terrain==2?choice->tyre!=11:terrain==3?choice->tyre!=12:choice->tyre>10)continue;
            if(limited){int admitted=0;for(unsigned j=0;j<allowed_count;j++)admitted|=!strcmp(choice->label,allowed[j]);if(!admitted)continue;}
            CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",choice->body,sizeof choice->body));
            ah_set_member(vm,car,"car_code",ah_str(vm,choice->label));
            ah_val selected=ah_objv(car),ret;CHECK(nat_bind_game_car(vm,NULL,m,1,&selected,&ret)>=0);
            int legal=1;
            for(unsigned round=0;round<count && legal;round++){
                ah_val args[]={selected,ah_str(vm,labels[round])},reg;
                CHECK(ah_call_member(vm,m->project,"getViolatedRegulatoin",2,args,&reg)>=0);
                checks++;legal=!ah_truthy(get(vm,reg.u.o,"violated"));
            }
            if(!legal)continue;
            unsigned copies=0;
            for(unsigned j=0;j<24 && best[j]>=0;j++)copies+=!strcmp(choice->label,fixture_candidates[best[j]].label);
            if(copies>=3)continue;
            for(unsigned j=0;j<24;j++)if(best[j]<0){
                best[j]=(int)c;break;
            }
            if(best[23]>=0)break;
        }
        CHECK(best[0]>=0);
        if(getenv("GT4_SERIES_FIXTURES_NEGATIVE") && !series_count)best[0]=-1;
        CHECK(best[0]>=0);
        for(unsigned j=0;j<24 && best[j]>=0;j++){
            candidate *c=&fixture_candidates[best[j]];
            printf("fixture %s stages=%u choice=%u car=%s parts=%s\n",series_copy,count,j,c->label,*c->parts?c->parts:"-");
        }
        fflush(stdout);series_count++;stages+=count;
    }
    CHECK(!script_errors && series_count==144 && stages==527);
    printf("PASS %u series / %u stages, %u admission checks; legal owned-car fixtures, no race-result injection.\n",series_count,stages,checks);
    gt4_spec_tables_close(tables);gt4_game_set_auto_gears(NULL,NULL);gt4_game_set_stats_reader(NULL,NULL);
    gt4_career_vehicle_close(factory);menu_close(m);gt4_vol_close(vol);return 0;
}
