#include "specdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void w32(uint8_t *p,unsigned v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}

int main(void)
{
    uint8_t idi[40]={0},db[2048]={0},out[256];uint32_t id;
    memcpy(idi,"GTID",4);w32(idi+4,1);w32(idi+20,42);
    idi[24]=6;memcpy(idi+26,"track",6);
    CHECK(gt4_specdb_label(idi,32,"track",&id) && id==42);
    CHECK(!strcmp(gt4_specdb_label_for_id(idi,32,42),"track"));
    CHECK(!gt4_specdb_label_for_id(idi,31,42));
    CHECK(!gt4_specdb_label_for_id(idi,32,43));
    CHECK(!gt4_specdb_label(idi,31,"track",&id));
    CHECK(!gt4_specdb_label(idi,32,"missing",&id));
    w32(idi+16,0xffffffff);CHECK(!gt4_specdb_label(idi,32,"track",&id));

    memcpy(db,"GTDB",4);w32(db+8,1);w32(db+12,4);w32(db+16,42);
    memcpy(db+24,"abcd",4);
    CHECK(gt4_specdb_row(db,28,42,out,4)==4 && !memcmp(out,"abcd",4));
    CHECK(!gt4_specdb_row(db,27,42,out,4));
    CHECK(!gt4_specdb_row(db,28,41,out,4));
    CHECK(!gt4_specdb_row(db,28,42,out,3));

    /* Synthetic Huffman alphabet: all bytes are represented by eight bits. */
    db[4]=1;w32(db+24,520);w32(db+28,0);
    for(unsigned i=0;i<256;i++){db[32+i*2]=(uint8_t)i;db[33+i*2]=8;}
    w32(db+544,12);w32(db+548,1);memcpy(db+552,"base",4);
    db[556]=5;db[557]=0x40;memcpy(db+558,"raw!",4);
    CHECK(gt4_specdb_row(db,562,42,out,4)==4 && !memcmp(out,"raw!",4));
    CHECK(!gt4_specdb_row(db,561,42,out,4));
    db[556]=1;db[557]=0;
    CHECK(gt4_specdb_row(db,558,42,out,4)==4 && !memcmp(out,"base",4));
    db[557]=1;CHECK(!gt4_specdb_row(db,558,42,out,4)); /* absent template */
    db[556]=4;db[557]=0x80;db[558]=5;db[559]='C';db[560]='S';
    CHECK(gt4_specdb_row(db,561,42,out,4)==4 && !memcmp(out,"CaSe",4));
    db[556]=3;CHECK(!gt4_specdb_row(db,560,42,out,4)); /* missing patch byte */
    db[557]=0xc0;CHECK(!gt4_specdb_row(db,560,42,out,4)); /* invalid kind */

    /* Long-code fallback with a nine-bit code for the template marker. */
    memset(db+32,0,512);w32(db+24,528);w32(db+28,1);
    db[544]=9;db[545]=0;w32(db+548,0x101);
    w32(db+552,12);w32(db+556,1);memcpy(db+560,"long",4);
    db[564]=1;db[565]=1;db[566]=1;
    CHECK(gt4_specdb_row(db,567,42,out,4)==4 && !memcmp(out,"long",4));
    CHECK(!gt4_specdb_row(db,566,42,out,4));
    for(size_t n=0;n<567;n++)CHECK(!gt4_specdb_row(db,n,42,out,4));
    w32(db+8,0xffffffff);CHECK(!gt4_specdb_row(db,sizeof db,42,out,4));
    uint8_t strings[32]={0};memcpy(strings,"GTST",4);w32(strings+4,1);
    strings[20]=6;memcpy(strings+22,"Hello",6);
    CHECK(!strcmp(gt4_specdb_string(strings,28,0),"Hello"));
    CHECK(!gt4_specdb_string(strings,27,0));
    CHECK(!gt4_specdb_string(strings,28,1));
    strings[27]='!';CHECK(!gt4_specdb_string(strings,28,0));
    /* Colour variants deliberately share a row ID; preserve their ordinal. */
    uint8_t repeated[40]={0};memcpy(repeated,"GTDB",4);w32(repeated+8,2);w32(repeated+12,4);
    w32(repeated+16,9);w32(repeated+24,9);w32(repeated+28,4);memcpy(repeated+32,"red blue",8);
    CHECK(gt4_specdb_row_at(repeated,40,0,out,4)==4 && !memcmp(out,"red ",4));
    CHECK(gt4_specdb_row_at(repeated,40,1,out,4)==4 && !memcmp(out,"blue",4));
    CHECK(!gt4_specdb_row_at(repeated,40,2,out,4));
    puts("SpecDB labels, raw, Huffman, templates, patches and bounds: passed");
    return 0;
}
