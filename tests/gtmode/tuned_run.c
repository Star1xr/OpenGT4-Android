/* Reuse the test-only original runner and runtime SpecDB fixture. */
#define main purchase_component_main
#include "purchase_run.c"
#undef main
static const gt4_specdb_src *runtime_db;
/* Explicit disc-data boundary: original setup arithmetic still executes.
   The captured cache is optional; do not count a failing capture as proof.
   00443ED0/00443F40 return a full-width Boolean in v0: clear its upper
   word too, or an absent row can retain a preceding 64-bit key. */
static void table_exists(ee_ctx *c){
    uint64_t key=c->r[5].ud[0];c->r[2].ud[0]=runtime_db->row(runtime_db->ctx,(uint32_t)(key>>32),(uint32_t)key,0,NULL,0)>0;
}
static void table_fetch(ee_ctx *c){
    uint64_t key=c->r[5].ud[0];uint8_t row[256];
    int n=runtime_db->row(runtime_db->ctx,(uint32_t)(key>>32),(uint32_t)key,0,row,sizeof row);
    if(n>0)memcpy(c->ram+c->r[6].uw[0],row,(size_t)n);c->r[2].ud[0]=n>0;
}
static ee_fn disc_boundary(uint32_t pc){return pc==0x443ed0?table_fetch:pc==0x443f40?table_exists:NULL;}
int main(int argc,char **argv)
{
    if(argc!=4){fprintf(stderr,"usage: gtmode_tuned_oracle RAM SPR ISO\n");return 2;}
    ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*native=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE);
    if(!ram || !native || !spr || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[3],&error);
    gt4_spec_tables *tables=vol?gt4_spec_tables_open(vol,NULL):NULL;if(!tables)return 2;
    gt4_specdb_src db={gt4_spec_tables_row,tables,gt4_spec_tables_str,gt4_spec_tables_car_parts};
    if(getenv("GT4_TUNED_RUNTIME_DB")){runtime_db=&db;c.hook=disc_boundary;}
    size_t size=0;const uint8_t *rows=gt4_spec_tables_data(tables,0x26,&size);
    if(!rows || size<16)return 2;unsigned count=r32(rows+8),unavailable=0,passed=0;
    for(unsigned i=0;i<count;i++){
        uint64_t key=UINT64_C(0x2600000000)|r32(rows+16+i*8);
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
        memset(c.ram+STACK-0x10000,0,0x10000);
        for(unsigned j=0;j<0x178;j++)c.ram[SETUP+j]=(uint8_t)(i+j*37);
        memcpy(native,c.ram,EE_RAM_SIZE);
        if(!original(&c,0x444690,SETUP,key,0,0)){
            uint8_t row[32]={0};db.row(db.ctx,0x26,(uint32_t)key,0,row,sizeof row);
            fprintf(stderr,"UNVERIFIED index=%u/%u key=%llx base=%u ra=%08x\n",i,count,(unsigned long long)key,r32(row),c.r[31].uw[0]);unavailable++;continue;
        }
        gt4_phys_mem=native;ee_fpu_enter();int result=gt4_garage_tuned(SETUP,key,&db,SCRATCH);ee_fpu_leave();
        if(getenv("GT4_CAREER_TUNED_NEGATIVE") && !i)native[SETUP+0x50]^=1;
        if(result!=c.r[2].sw[0] || memcmp(native+SETUP,c.ram+SETUP,0x178)){
            fprintf(stderr,"tuned mismatch key=%llx result=%d/%d\n",(unsigned long long)key,result,c.r[2].sw[0]);
            for(unsigned j=0;j<0x178;j++)if(native[SETUP+j]!=c.ram[SETUP+j])fprintf(stderr," +%03x C=%02x original=%02x\n",j,native[SETUP+j],c.ram[SETUP+j]);
            return 1;
        }
        passed++;
    }
    printf("Tuned cars: %u/%u disc rows matched return and complete 0x178-byte setups; %u original cases unavailable.\n",passed,count,unavailable);
    gt4_phys_mem=NULL;gt4_spec_tables_close(tables);gt4_vol_close(vol);free(ram);free(native);free(spr);ee_free(&c);return unavailable?1:0;
}
