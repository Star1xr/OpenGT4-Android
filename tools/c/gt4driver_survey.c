/* Read-only driver audit of every catalog car and the shared character MDLS.
   It reports /car/open availability, /car/lod metadata and menu metadata;
   availability does not assert which race/roof/LOD setting selects a file. */
#include "driver_model.h"
#include "driver_pose.h"
#include "model.h"
#include "specdb.h"
#include "vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {uint8_t *d;size_t n;} blob;
static unsigned u32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static blob table(gt4_vol *v,const char *name)
{char path[160];blob b={0};snprintf(path,sizeof path,"/specdb/GT4_US2560/%s",name);b.d=gt4_vol_read(v,path,&b.n);return b;}
static int car(gt4_vol *v,const char *dir,const char *model,gt4_driver_car *info,int *builtin_tris)
{
    char path[128];size_t n;snprintf(path,sizeof path,"/car/%s/%s",dir,model);uint8_t *d=gt4_vol_read(v,path,&n);
    *builtin_tris=0;if(!d)return 0;int ok=gt4_driver_car_read(d,n,info);
    if(ok && info->builtin_offset) {
        const uint8_t *mdls=d+info->builtin_offset;size_t size=n-info->builtin_offset;
        gt4_driver_rig rig;
        if(!gt4_driver_rig_read(mdls,size,&rig))ok=0;
        unsigned models=mdls[22]|(unsigned)mdls[23]<<8;
        if(!models || models>256)ok=0;
        for(unsigned i=0;ok && i<models;i++) {
            gt4_model *mesh=gt4_model_decode_skin_part(mdls,size,(int)i,NULL);
            if(!mesh)ok=0;else *builtin_tris+=mesh->count;
            gt4_model_free(mesh);
        }
        if(!*builtin_tris)ok=0;
    }
    free(d);return ok?1:-1;
}
static const char *source(const gt4_driver_car *c)
{gt4_driver_selection s=gt4_driver_select(c,0,-1);return s.source==GT4_DRIVER_BUILTIN?"builtin":s.path?s.path:"none";}
static int characters(gt4_vol *v)
{
    static const char *files[]={"driver","driver2","driver3","driver4","classic1","classic2","driver_mtr","driver_nike"};int failed=0;
    for(unsigned i=0;i<sizeof files/sizeof *files;i++) {
        char path[128];size_t n;snprintf(path,sizeof path,"/character/%s.mdl",files[i]);uint8_t *d=gt4_vol_read(v,path,&n);
        gt4_driver_rig r;int ok=d && gt4_driver_rig_read(d,n,&r),tris=0,skin=0,rigid=0;
        if(ok){gt4_driver_skeleton s;float poses[20][16];ok=gt4_driver_skeleton_init(r.dimensions,&s) && gt4_driver_skeleton_matrices(&s,poses);}
        if(ok)for(int j=0;j<r.skin_count;j++) {
            gt4_model_stats stats;gt4_model *m=gt4_model_decode_skin_part(d,n,r.skin_first+j,&stats);
            if(!m || !m->skin || !m->count)ok=0;
            if(m){tris+=m->count;for(int t=0;t<m->count;t++)for(int k=0;k<3;k++)if(m->skin && m->skin[t].v[k].weight[0]+m->skin[t].v[k].weight[1]+m->skin[t].v[k].weight[2]+m->skin[t].v[k].weight[3]>0)skin++;}
            gt4_model_free(m);
        }
        if(ok)for(int b=0;b<GT4_DRIVER_BONES;b++)for(int j=0;j<r.bone[b].model_count;j++) {
            gt4_model *m=gt4_model_decode_skin_part(d,n,r.bone[b].model_first+j,NULL);
            if(!m || !m->count)ok=0;else rigid+=m->count;
            gt4_model_free(m);
        }
        fprintf(stderr,"%s %s slots=%u skin_triangles=%d skin_vertices=%d rigid_triangles=%d\n",ok?"OK":"FAIL",path,ok?r.slots:0,tris,skin,rigid);
        if(!ok)failed++;free(d);
    }
    return failed;
}
int main(int argc,char **argv)
{
    if(argc!=2){fprintf(stderr,"usage: gt4driver_survey disc.iso > catalog.csv\n");return 2;}
    const char *err=NULL;gt4_vol *v=gt4_vol_open(argv[1],&err);if(!v){fprintf(stderr,"volume: %s\n",err?err:"failed");return 1;}
    blob labels=table(v,"GENERIC_CAR.idi"),links=table(v,"CAR_VARIATION_american.dbt"),variants=table(v,"VARIATIONamerican.dbt"),strings=table(v,"american_StrDB.sdb");
    int failed=characters(v);unsigned checked=0,open=0,lod_builtin=0,lod_shared=0,lod_none=0;
    unsigned count=labels.d && labels.n>=16?u32(labels.d+4):0;
    if(!count || count>(labels.n-16)/8){failed++;goto done;}
    puts("label,model,menu_type,menu_driver,open_type,open_driver,lod_type,lod_driver,menu_builtin_triangles,open_builtin_triangles,lod_builtin_triangles");
    for(unsigned i=0;i<count;i++) {
        const char *label=gt4_specdb_label_for_id(labels.d,labels.n,u32(labels.d+20+(size_t)i*8));uint32_t id;uint8_t row[256];
        if(!label || !gt4_specdb_label(labels.d,labels.n,label,&id) || gt4_specdb_row(links.d,links.n,id,row,sizeof row)!=8 ||
           gt4_specdb_row(variants.d,variants.n,u32(row),row,sizeof row)!=52){failed++;continue;}
        const char *model=gt4_specdb_string(strings.d,strings.n,u32(row));if(!model || strlen(model)>24 || strchr(model,'/') || strchr(model,'\\')){failed++;continue;}
        gt4_driver_car m={0},o={0},l={0};int mt=0,ot=0,lt=0;
        int menu_ok=car(v,"menu",model,&m,&mt),open_ok=car(v,"open",model,&o,&ot),lod_ok=car(v,"lod",model,&l,&lt);
        if(menu_ok!=1 || open_ok<0 || lod_ok!=1){fprintf(stderr,"FAIL %s menu=%d open=%d lod=%d\n",label,menu_ok,open_ok,lod_ok);failed++;continue;}
        checked++;if(open_ok)open++;
        gt4_driver_selection selected=gt4_driver_select(&l,0,-1);
        if(selected.source==GT4_DRIVER_BUILTIN)lod_builtin++;else if(selected.source==GT4_DRIVER_SHARED)lod_shared++;else lod_none++;
        printf("%s,%s,%u,%s,",label,model,m.type,source(&m));
        if(open_ok)printf("%u,%s,",o.type,source(&o));else printf("absent,none,");
        printf("%u,%s,%d,%d,%d\n",l.type,source(&l),mt,ot,lt);
    }
done:
    fprintf(stderr,"catalog: %u checked, %u open assets, lod: %u builtin / %u shared / %u none; %d failures\n",checked,open,lod_builtin,lod_shared,lod_none,failed);
    free(labels.d);free(links.d);free(variants.d);free(strings.d);gt4_vol_close(v);return failed?1:0;
}
