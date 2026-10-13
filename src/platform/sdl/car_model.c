/* A car's menu model with its wheels, from the SpecDB and /car/menu (moved
   out of showroom.c, which the race view shares it with). */
#include "car_model.h"
#include "showroom_math.h"
#include "wheel_geometry.h"
#include "specdb.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned u32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
typedef struct {uint8_t *data;size_t size;} blob;
static blob read_table(gt4_vol *v,const char *name){char path[160];blob b={0};snprintf(path,sizeof path,"/specdb/GT4_US2560/%s",name);b.data=gt4_vol_read(v,path,&b.size);return b;}
static int row(blob b,unsigned id,uint8_t *r){return gt4_specdb_row(b.data,b.size,id,r,256);}
static const char *str(blob b,unsigned i){return gt4_specdb_string(b.data,b.size,i);}
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int disc_row(gt4_vol *v,const char *table,unsigned id,uint8_t *out)
{blob b=read_table(v,table);int n=row(b,id,out);free(b.data);return n;}
/* 00458A30 -> 00458AB8 deforms the tire once at model preparation.
   Bounds include every authored LOD, not only the visible shell. */
static void fit_tire(gt4_model *part,float width,float rim,float radius,const float bounds[3])
{
    gt4_tire_fit fit=gt4_tire_fit_prepare(width,radius,radius-rim,bounds);
    for(int i=0;i<part->count;i++)for(int k=0;k<3;k++)
        gt4_tire_fit_position(part->triangles[i].v[k].p,fit);
}
static void add_wheels(gt4_vol *v,unsigned id,car_model *a,const uint8_t *data,size_t size,const uint8_t *custom,size_t custom_size,const float recorded[4][3])
{
    uint8_t r[256],parts[256];
    if(disc_row(v,"GENERIC_CAR.dbt",id,r)!=24 || disc_row(v,"DEFAULT_PARTS.dbt",u32(r),parts)!=208)return;
    if(disc_row(v,"CHASSIS.dbt",u32(parts+40),r)<6)return;float wheelbase=u16(r+4)*.001f;
    if(wheelbase<.1f)return;
    if(disc_row(v,"SUSPENSION.dbt",u32(parts+16),r)>=12) {
        float front=(int16_t)u16(r+4)*.001f,rear=(int16_t)u16(r+10)*.001f;
        if(front>=0 && front<1 && rear>=0 && rear<1 && fabsf(rear-front)<wheelbase) {
            float sn=(rear-front)/wheelbase,cs=sqrtf(1-sn*sn),lift=(front+rear)*.5f;
            a->body_sine=sn;a->body_cosine=cs;a->body_lift=lift;
            for(int part=0;part<2;part++) {
                gt4_model *body=part?a->hull:a->car;
                for(int i=0;body && i<body->count;i++)for(int k=0;k<3;k++) {
                    gt4_vertex *vertex=&body->triangles[i].v[k];
                    if(body->triangles[i].attachment>=1 && body->triangles[i].attachment<=4)continue;
                    showroom_body_pose(vertex->p,sn,cs,lift);
                    showroom_body_pose(vertex->normal,sn,cs,0);
                }
            }
        }
    }
    if(disc_row(v,"RACINGMODIFY.dbt",u32(parts+48),r)<6)return;float tread[2]={u16(r+2)*.001f,u16(r+4)*.001f};
    a->body_count=a->car->count;
    for(int axle=0;axle<2;axle++) {
        if(disc_row(v,axle?"REARTIRE.dbt":"FRONTTIRE.dbt",u32(parts+192+axle*8),r)<8)continue;
        if(disc_row(v,"TIRESIZE.dbt",u32(r),r)!=4)continue;
        float width,rim,radius;showroom_tire_size(r,&width,&rim,&radius);
        if(!(rim>0 && radius>0 && width>0))continue;
        for(int side=0;side<2;side++) {
            int slot=axle*2+side,native=slot^1,count=0;float sign=side?-1:1;
            float fitted_width=recorded?recorded[slot][0]:width;
            float hub[3]={sign*tread[axle]*.5f,recorded?recorded[slot][1]:radius,(axle?1:-1)*wheelbase*.5f};
            memcpy(a->hub[slot],hub,sizeof hub);
            for(int i=0;i<a->body_count;i++)if(a->car->triangles[i].attachment==native+1)count++;
            if(!count)continue;
            gt4_model *brake=calloc(1,sizeof *brake);if(!brake)continue;
            brake->triangles=malloc((size_t)count*sizeof *brake->triangles);
            if(!brake->triangles){free(brake);continue;}
            a->brake[slot]=brake;brake->capacity=count;
            for(int i=0;i<a->body_count;i++) {
                gt4_triangle *tr=&a->car->triangles[i];if(tr->attachment!=native+1)continue;
                /* 00454110: hub translation then +/- half tire width.
                   Brake meshes already carry metres and opposite normals. */
                gt4_triangle *local=&brake->triangles[brake->count++];*local=*tr;
                for(int j=0;j<3;j++) {
                    showroom_wheel_attachment(local->v[j].p,native,fitted_width);
                    for(int k=0;k<3;k++)tr->v[j].p[k]=local->v[j].p[k]+hub[k];
                }
                /* 00453798: each brake drawn under a colour multiplier whose
                   alpha is its wheel's block +0x28 (004A60E0); the blended
                   layers (the hot disc's glow, additive 0x48, and its tint,
                   0x44) scale with it and vanish while it is 0. A: its writer
                   (the brake's heat) is not found; the brakes are drawn cold. */
                if(local->blend_enabled)brake->count--;
            }
        }
        /* 00453028+0x16C draws the tire before +0x198 draws the rim.
           Race body MDLS likewise calls 15..18 before nested 36..39.
           Transparent rim texels still write depth: a later tire would
           be cut out by the rim's polygon instead of showing through. */
        for(int pass=0;pass<2;pass++) {
            int piece=1-pass;
            const uint8_t *mesh=data;size_t mesh_size=size;
            size_t off=u32(data+(piece?44+axle*4:32));
            if(!piece && custom){mesh=custom;mesh_size=custom_size;off=0;}
            if(off>=mesh_size)continue;
            gt4_model *part=piece?gt4_model_decode_part(data+off,size-off,0,0):gt4_model_decode_wheel_part(mesh+off,mesh_size-off,0);if(!part)continue;
            float bounds[3]={0};
            if(piece && !gt4_model_tire_bounds(data+off,size-off,bounds)){gt4_model_free(part);continue;}
            if(piece)fit_tire(part,width,rim,radius,bounds);
            float extent=0,xmin=1e9f,xmax=-1e9f;
            for(int i=0;i<part->count;i++)for(int k=0;k<3;k++){float *p=part->triangles[i].v[k].p;extent=fmaxf(extent,sqrtf(p[1]*p[1]+p[2]*p[2]));xmin=fminf(xmin,p[0]);xmax=fmaxf(xmax,p[0]);}
            if(extent>.01f && xmax-xmin>.001f) {
                /* 0045314C..00453158 and 00453848..00453868 scale
                   rim and prepared tire uniformly by the nominal rim radius.
                   Normalizing the rim by its mesh extent shrank its lip. */
                float ys=rim,xs=rim,centre=.5f*width/rim,tr[2][6];
                for(int side=0;side<2;side++){float sign=side?-1:1,scale=-sign*xs;tr[side][0]=sign*tread[axle]*.5f-centre*scale;tr[side][1]=radius;tr[side][2]=(axle?1:-1)*wheelbase*.5f;tr[side][3]=scale;tr[side][4]=tr[side][5]=ys;}
                /* A zero front tread denotes a single central tire (e.g. MP5).
                   Keep the two outward rim faces, but only one tire shell.
                   A rim whose mesh holds both sides of the axle (the
                   Daimler Motor Carriage's: x -1.6..1.7 rim radii, nothing
                   near its hub) is the whole axle: its body calls the
                   front tyre once, at the axle's middle (CarModel::Callback
                   15, 0.58 from the hub, no wheel 1), so it is drawn once
                   there, unmirrored. Its width is not the rim radius's: at
                   that scale the rear rims stood 0.2 m outside the tyres,
                   so across the axle its rims' outer rings (beyond 0.85
                   radii) are put on the tyres' middle (A: how the game
                   fits it is not read). */
                int axle_rim=!piece && xmin<-.5f && xmax>.5f;
                if(axle_rim) {
                    double ring=0;int rings=0;
                    for(int i=0;i<part->count;i++)for(int k=0;k<3;k++){const float *q=part->triangles[i].v[k].p;if(q[1]*q[1]+q[2]*q[2]>.85f*.85f){ring+=fabsf(q[0]);rings++;}}
                    tr[0][0]=0;tr[0][3]=rings && ring>0?(tread[axle]*.5f+width*.5f)/(float)(ring/rings):xs;
                }
                int copies=(piece && tread[axle]<.001f) || axle_rim?1:2;
                gt4_model_attach(a->car,part,tr,copies);
                for(int side=0;side<copies;side++) {
                    int slot=axle*2+side;float hub[3]={(side?-1:1)*tread[axle]*.5f,radius,tr[side][2]};
                    float local[1][6]={{tr[side][0]-hub[0],0,0,tr[side][3],ys,ys}};
                    gt4_model *copy=piece?gt4_model_decode_part(data+off,size-off,0,0):gt4_model_decode_wheel_part(mesh+off,mesh_size-off,0);if(!copy)continue;
                    /* 00450F50 +38..4C -> 00458A30: recorded width, outer
                       radius and rim radius prepare the tire; 00453848
                       scales the rim. CPRM getter offsets 0045ABD0..AC30. */
                    float fw=recorded?recorded[slot][0]:width;
                    float fr=recorded?recorded[slot][1]:radius;
                    float fm=recorded?recorded[slot][2]:rim;
                    if(piece)fit_tire(copy,fw,fm,fr,bounds);
                    if(recorded){float sign=side?-1.f:1.f;local[0][0]=sign*fw*.5f;local[0][3]=-sign*fm;local[0][4]=local[0][5]=fm;hub[1]=fr;}
                    memcpy(a->tire_size[slot],(float[3]){fw,fr,fm},12);
                    if(!a->wheel[slot])a->wheel[slot]=calloc(1,sizeof *a->wheel[slot]);
                    if(a->wheel[slot])gt4_model_attach(a->wheel[slot],copy,local,1);
                    memcpy(a->hub[slot],hub,sizeof hub);gt4_model_free(copy);
                }
            }
            gt4_model_free(part);
        }
    }
}
struct car_model_tables {blob labels,links,variants,strings;};
car_model_tables *car_model_tables_open(gt4_vol *v)
{
    car_model_tables *t=calloc(1,sizeof *t);if(!t)return NULL;
    t->labels=read_table(v,"GENERIC_CAR.idi");t->links=read_table(v,"CAR_VARIATION_american.dbt");
    t->variants=read_table(v,"VARIATIONamerican.dbt");t->strings=read_table(v,"american_StrDB.sdb");
    return t;
}
void car_model_tables_close(car_model_tables *t)
{if(t){free(t->labels.data);free(t->links.data);free(t->variants.data);free(t->strings.data);free(t);}}
int car_model_race_driver(gt4_vol *v,const car_model_tables *t,const char *code,int cars,gt4_driver_car *out)
{
    /* 0038D0C0/0038D130 read CARINFO geometry; 003882D4 selects roof.
       LicenseConcourse needs seat/shift points, never a second mesh. */
    if(!v || !t || !code || !out || cars<1)return 0;
    unsigned id;uint8_t r[256],generic[256];
    if(!gt4_specdb_label(t->labels.data,t->labels.size,code,&id) || row(t->links,id,r)!=8 ||
       row(t->variants,u32(r),r)!=52 || disc_row(v,"GENERIC_CAR.dbt",id,generic)!=24)return 0;
    const char *model=str(t->strings,u32(r));if(!model || strlen(model)>24 || strchr(model,'/') || strchr(model,'\\'))return 0;
    char path[96];snprintf(path,sizeof path,"/car/%s/%s",generic[20]>=2 || (generic[20]==1 && cars<3)?"open":"lod",model);
    size_t size;uint8_t *data=gt4_vol_read(v,path,&size);int ok=data && gt4_driver_car_read(data,size,out);free(data);return ok;
}
void car_model_free(car_model *a){if(a){gt4_model_free(a->car);gt4_model_free(a->shadow);gt4_model_free(a->hull);gt4_model_free(a->cast);for(int i=0;i<4;i++){gt4_model_free(a->wheel[i]);gt4_model_free(a->brake[i]);}for(int i=0;i<GT4_MODEL_STEERING_MAX;i++)gt4_model_free(a->steering[i]);free(a->driver_data);free(a);}}
static car_model *load_car(gt4_vol *v,const car_model_tables *t,const char *code,int choice,int cars,unsigned entry,const char *wheel,const char *wing,const float recorded[4][3])
{
    if(!t)return NULL;blob labels=t->labels,links=t->links,variants=t->variants,strings=t->strings;
    uint32_t id;uint8_t r[256];
    if(!gt4_specdb_label(labels.data,labels.size,code,&id) || row(links,id,r)!=8)return NULL;
    if(row(variants,u32(r),r)!=52)return NULL;const char *model=str(strings,u32(r));
    if(!model || strlen(model)>24 || strchr(model,'/') || strchr(model,'\\'))return NULL;
    car_model *a=calloc(1,sizeof *a);if(!a)return NULL;
    a->body_cosine=1;
    snprintf(a->model,sizeof a->model,"%s",model);snprintf(a->code,sizeof a->code,"%s",code);
    unsigned count=variants.size>=16?u32(variants.data+8):0;
    if(count>(variants.size-16)/8)goto fail;
    for(unsigned i=0;i<count && a->count<128;i++) {
        if(gt4_specdb_row_at(variants.data,variants.size,i,r,sizeof r)!=52)continue;
        const char *name=str(strings,u32(r));if(!name || strcmp(name,model))continue;
        car_color *c=&a->colors[a->count++];c->order=u32(r+4);name=str(strings,u32(r+12));snprintf(c->name,sizeof c->name,"%s",name?name:"");
    }
    for(int i=0;i<a->count;i++)for(int j=i+1;j<a->count;j++)if(a->colors[j].order<a->colors[i].order){car_color c=a->colors[j];a->colors[j]=a->colors[i];a->colors[i]=c;}
    if(!a->count)goto fail;a->choice=((choice%a->count)+a->count)%a->count;
    const char *directory="menu";
    if(cars>0) {
        uint8_t generic[256];if(disc_row(v,"GENERIC_CAR.dbt",id,generic)!=24)goto fail;
        /* Ordinary A-spec, 003882D4..003882F4. Roof type 1 opens only in
           a field of fewer than three; roof type >=2 always selects open. */
        a->race_open=generic[0x14]>=2 || (generic[0x14]==1 && cars<3);
        directory=a->race_open?"open":"lod";
    }
    char path[96];size_t size=0;snprintf(path,sizeof path,"/car/%s/%s",directory,model);uint8_t *data=gt4_vol_read(v,path,&size);
    /* The race's models carry no paints (CAR4 +0x1C zero): func_003B5988
       loads the path plus ".pat" (0x003B59D0), the same Pat0 table as the
       menu model's. Appended here, with +0x1C at it, for the decoder. */
    if(cars>0 && data && size>=32 && !memcmp(data,"CAR4",4) && !u32(data+28)) {
        char pat_path[100];size_t pat_size=0;snprintf(pat_path,sizeof pat_path,"%s.pat",path);
        uint8_t *pat=gt4_vol_read(v,pat_path,&pat_size);
        if(pat && pat_size>=32 && !memcmp(pat,"Pat0",4)) {
            size_t at=(size+15)&~(size_t)15;uint8_t *joined=realloc(data,at+pat_size);
            if(joined){data=joined;memset(data+size,0,at-size);memcpy(data+at,pat,pat_size);size=at+pat_size;
                data[28]=(uint8_t)at;data[29]=(uint8_t)(at>>8);data[30]=(uint8_t)(at>>16);data[31]=(uint8_t)(at>>24);}
        }
        free(pat);
    }
    if(data && size>=0x88 && !memcmp(data,"CAR4",4)) {
        unsigned n=u32(data+0x80),at=u32(data+0x84);
        for(unsigned i=0;i<n && i<32 && at+(i+1)*32u<=size;i++)
            for(int k=0;k<8;k++){uint32_t b=u32(data+at+i*32+k*4);memcpy(&a->cam_point[i][k],&b,4);}
        a->cam_points=n<32?(int)n:32;
    }
    /* 004514A0 +24..58: CARINFO +58/+5C are front/rear upper wheel Y.
       Zero disables the clamp. They are not the HumanGeom pointer +50. */
    if(data && size>=0x14){size_t info=u32(data+0x10);if(info && info<=size && size-info>=0x60){
        memcpy(a->wheel_limit,data+info+0x58,8);
        if(!isfinite(a->wheel_limit[0]) || !isfinite(a->wheel_limit[1]))goto fail_data;
    }}
    a->exhaust_count=gt4_car_exhaust_count(data,size);
    a->exhaust_points=gt4_car_exhaust_points(data,size,a->exhaust,8);
    if(cars>0 && data && gt4_driver_car_read(data,size,&a->driver)) {
        gt4_driver_selection selection=gt4_driver_select(&a->driver,entry,-1);
        a->driver_path=selection.path;
        if(selection.source==GT4_DRIVER_BUILTIN) {
            size_t off=a->driver.builtin_offset;a->driver_size=u32(data+off+16);
            a->driver_data=malloc(a->driver_size);
            if(!a->driver_data)goto fail_data;memcpy(a->driver_data,data+off,a->driver_size);
        }
    }
    /* Model 1 is an auxiliary volume, not additional opaque bodywork.
       Its closed silhouette covered open cabins (S2000). Native model-0
       body and model-2 contact-shadow passes supply visible presentation;
       model 1 requires its separate pass context (004528A4). */
    if(data){int installed=wing && *wing && strcmp(wing,"p000");
        a->car=installed?gt4_model_decode_car_wing(data,size,(int)a->colors[a->choice].order-1,1):gt4_model_decode(data,size,(int)a->colors[a->choice].order-1);if(a->car){
        if(cars>0)for(unsigned i=0;i<a->car->steering_count;i++)a->steering[i]=gt4_model_steering_part(a->car,i);
        /* 00454270 +0x20..0x48 constructs wheel/menu/<label>, or with its
           third argument 0 wheel/lod/<label>: the race's model (00390200
           passes 0 unless the car model is the menu one, +0x14). The
           replacement rim uses the same 0045314C attachment as stock. */
        size_t wheel_size=0;uint8_t *wheel_data=NULL;
        if(wheel && *wheel && strcmp(wheel,"w000")){
            char wheel_path[128];
            if(strchr(wheel,'/') || strchr(wheel,'\\'))goto fail_data;
            snprintf(wheel_path,sizeof wheel_path,"/wheel/%s/%s",cars>0?"lod":"menu",wheel);
            wheel_data=gt4_vol_read(v,wheel_path,&wheel_size);
            if(!wheel_data || !wheel_size){fprintf(stderr,"showroom: missing wheel %s\n",wheel_path);free(wheel_data);goto fail_data;}
        }
        /* 004533A8 +0x30..0xa8: CarInfo+0x50 -> geometry,
           translation +0xd0, uniform scale +0xdc * 0.5. */
        if(installed){
            size_t info=u32(data+16);if(info>size || size-info<0x54){free(wheel_data);goto fail_data;}
            size_t geometry=u32(data+info+0x50);
            if(!geometry || geometry>size || size-geometry<0xe0 || strchr(wing,'/') || strchr(wing,'\\')){free(wheel_data);goto fail_data;}
            char wing_path[128];snprintf(wing_path,sizeof wing_path,"/wing/%s/%s",cars>0?"lod":"menu",wing);   /* 00454378 */
            size_t wing_size=0;uint8_t *wing_data=gt4_vol_read(v,wing_path,&wing_size);
            gt4_model *part=wing_data?gt4_model_decode_part(wing_data,wing_size,0,0):NULL;free(wing_data);
            if(!part){fprintf(stderr,"showroom: cannot decode wing %s\n",wing_path);free(wheel_data);goto fail_data;}
            float tr[1][6];memcpy(tr[0],data+geometry+0xd0,12);float scale;memcpy(&scale,data+geometry+0xdc,4);scale*=.5f;
            tr[0][3]=tr[0][4]=tr[0][5]=scale;
            int attached=gt4_model_attach(a->car,part,tr,1);gt4_model_free(part);
            if(!attached){free(wheel_data);goto fail_data;}
        }
        add_wheels(v,id,a,data,size,wheel_data,wheel_size,recorded);free(wheel_data);a->shadow=gt4_model_decode_part(data,size,(int)a->colors[a->choice].order-1,2);
        a->cast=gt4_model_decode_part(data,size,(int)a->colors[a->choice].order-1,1);
    }free(data);}
    if(!a->car || !a->car->count){fprintf(stderr,"showroom: empty car %s wing=%s\n",code,wing?wing:"");goto fail;}
    fprintf(stderr,"showroom: %s model=%s color=%u triangles=%d textures=%d unsupported=%d\n",code,model,a->colors[a->choice].order,a->car->count,a->car->ntextures,a->car->unsupported);return a;
fail:car_model_free(a);return NULL;
fail_data:free(data);goto fail;
}
car_model *car_model_load(gt4_vol *v,const car_model_tables *t,const char *code,int choice)
{return load_car(v,t,code,choice,0,0,NULL,NULL,NULL);}
car_model *car_model_load_race(gt4_vol *v,const car_model_tables *t,const char *code,int choice,int cars,unsigned entry)
{return cars>0?load_car(v,t,code,choice,cars,entry,NULL,NULL,NULL):NULL;}
car_model *car_model_load_race_parts(gt4_vol *v,const car_model_tables *t,const char *code,int choice,int cars,unsigned entry,const char *wheel,const char *wing)
{return cars>0?load_car(v,t,code,choice,cars,entry,wheel,wing,NULL):NULL;}

car_model *car_model_load_wheel(gt4_vol *v,const car_model_tables *t,const char *code,int choice,const char *wheel)
{return load_car(v,t,code,choice,0,0,wheel,NULL,NULL);}

car_model *car_model_load_parts(gt4_vol *v,const car_model_tables *t,const char *code,int choice,const char *wheel,const char *wing)
{return load_car(v,t,code,choice,0,0,wheel,wing,NULL);}

car_model *car_model_load_film_parts(gt4_vol *v,const car_model_tables *t,const char *code,int choice,int cars,unsigned entry,const char *wheel,const char *wing,const float recorded[4][3])
{
    if(cars<=0 || !recorded)return NULL;
    for(int i=0;i<4;i++)if(!isfinite(recorded[i][0]) || !isfinite(recorded[i][1]) || !isfinite(recorded[i][2]) || recorded[i][0]<=0 || recorded[i][2]<=0 || recorded[i][1]<recorded[i][2])return NULL;
    return load_car(v,t,code,choice,cars,entry,wheel,wing,recorded);
}
