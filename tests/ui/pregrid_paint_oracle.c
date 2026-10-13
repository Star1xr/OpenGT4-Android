/* The disc's real VARIATION rows through original 00445880 / 00445AE8.
   Only table access is a host boundary; selection and rejection execute EE. */
#define main pregrid_standard_main
#include "pregrid_menu.c"
#undef main
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
static uint8_t paint_rows[128][52];
static int paint_count;
static void count_rows(ee_ctx *c){c->r[2].uw[0]=paint_count;}
static void first_row(ee_ctx *c){c->r[2].uw[0]=0;}
static void read_row(ee_ctx *c){unsigned i=c->r[5].uw[0];CHECK(i<(unsigned)paint_count);memcpy(c->ram+c->r[6].uw[0],paint_rows[i],52);}
static ee_fn boundary(uint32_t pc){return pc==0x445808?count_rows:pc==0x449210?first_row:pc==0x4492d8?read_row:NULL;}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static void original(ee_ctx *c,uint32_t pc,uint32_t a0,uint32_t a1)
{
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=a0;c->r[5].uw[0]=a1;c->r[29].uw[0]=0x1ef0000;
    jmp_buf j;c->exit_jmp=&j;CHECK(!setjmp(j));ee_fn f=lookup(pc);CHECK(f);f(c);
}
int main(int argc,char **argv)
{
    CHECK(argc==3 || argc==4);int negative=argc==4;const char *err;gt4_vol *vol=gt4_vol_open(argv[2],&err);CHECK(vol);
    ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);c.lookup=lookup;c.hook=boundary;
    char msg[256];menu *m=menu_open(vol,"quick-arcade",msg,sizeof msg);CHECK(m);load_car_tables(m);
    const char *cars[]={"_3000gt_mr_95","_3000gt_mr_98","_3000gt_vr4_us_98","_3000gt_vr4t_us_95","_3000gt_vr4t_us_96","alpine_a110_16s_73","ford_gt_05","ka_01"};
    unsigned cases=0;
    for(unsigned car=0;car<sizeof cars/sizeof *cars;car++){
        uint32_t id;uint8_t link[8];CHECK(gt4_specdb_label(m->car_labels,m->car_labels_size,cars[car],&id));
        CHECK(gt4_specdb_row(m->car_links,m->car_links_size,id,link,sizeof link)==8);uint32_t key=car_u32(link);
        paint_count=0;while(paint_count<128 && gt4_specdb_row_nth(m->car_variants,m->car_variants_size,key,paint_count,paint_rows[paint_count],52)==52)paint_count++;
        CHECK(paint_count>0 && paint_count<128);
        for(int index=0;index<=paint_count+8;index++){
            const uint32_t gar=0x1800000,out=0x1810000;c.ram[gar+0x18]=0;memset(c.ram+out,0,16);
            original(&c,0x445880,gar,index);original(&c,0x445ae8,gar,out);CHECK(c.r[2].uw[0]==1);
            uint8_t chip[4][4];CHECK(quick_paint_chip(m,cars[car],index,chip));if(negative && !car && !index)chip[0][3]&=~1;
            CHECK(!memcmp(chip,c.ram+out,16));
            /* Native method and actual ColorChipFace interpretation. */
            menu_quick_race q={0};q.entries=1;strcpy(q.grid[0].car,cars[car]);q.grid[0].color=index;menu_set_quick_race(&q);
            ah_val arg=ah_int(0),colors,result;CHECK(!nat_quick_car(m->vm,NULL,(void *)4,1,&arg,&colors));
            ui_widget w={0};w.type="ColorChipFace";CHECK(!widget_color(m->vm,widget_module(m,&w),NULL,1,&colors,&result));
            unsigned bands=0;for(int k=0;k<4;k++)if(chip[k][3]&1){CHECK(!memcmp(w.chip_bands[bands],chip[k],3));bands++;}
            CHECK(bands && bands==(unsigned)w.chip_band_count);cases++;
        }
        printf("%s: %d paints, valid and rejected indices match original\n",cars[car],paint_count);
    }
    printf("PASS %u paint cases vs original, including native pregrid chips.\n",cases);
    menu_close(m);gt4_vol_close(vol);ee_free(&c);return 0;
}
