#include "ui.h"

#include "gpb.h"
#include "mot1.h"
#include "platform/platform.h"
#include "tex1.h"
#include "font.h"
#include "font_layout.h"
#include "glyph_sdf.h"
#include "ui_hd.h"
#include "ui_motion.h"
#include "rtext.h"
#include <ctype.h>
#include "strobe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    char         *path;     /* "image/..." as used by image_path */
    plat_texture *tex;      /* NULL if the image could not be found/decoded */
    int           w, h;
} ui_image;

typedef struct {
    char    *name;          /* e.g. "TopRoot" */
    uint8_t *gpb;           /* its package, may be NULL */
    size_t   gpb_size;
} ui_package;

typedef struct {
    char *name;
    gt4_font face;
    plat_texture *tex;
    plat_texture *outline;
} ui_font;

typedef struct {char *path;gt4_strobe clip;plat_texture **textures;} ui_flash;

struct ui_project {
    gt4_vol    *vol;
    char        name[64];
    mp_node    *tree;
    mp_node **native_trees;int native_count;
    const mp_node **roots;
    int         nroots;
    ui_widget **rootw;      /* runtime widget tree per root, built on first use */
    ui_package *pkgs;       /* one per root, loaded on first use */
    ui_image   *imgs;
    int         nimgs;
    ui_widget **all;        /* every widget made, freed with the project */
    int         nall, allcap;
    uint8_t    *rtext_data;
    gt4_rtext   rtext;
    ui_font    *fonts;
    int         nfonts;
    /* the KNJI file of codes from 0x300 and the glyphs drawn from it */
    uint8_t    *kanji_data;size_t kanji_size;int kanji_tried;
    struct {unsigned set,code;int w,h;plat_texture *tex,*outline;} *kanji;int nkanji;
    ui_package *extra;int nextra;
    struct {char *path;gt4_mot1 *motion;} *cameras;int ncameras;
    struct {char *path;uint8_t *data;size_t size;} *model_bytes;int nmodel_bytes;
    ui_movie_draw_fn draw_movie;
    void *movie_user;
    ui_model_renderer models;
    ui_car_draw_fn draw_car;void *car_user;
    ui_scene_draw_fn draw_scene;void *scene_user;
    ui_photo_draw_fn draw_photo;void *photo_user;
    ui_photo_draw_fn draw_native;void *native_user;   /* LoggerFace, RaceCourseMapFace */
    ui_photo_draw_fn draw_album;void *album_user;
    struct ui_model_entry { char *path; void *mesh; } *meshes;
    int nmeshes;
    struct {char *path;tex1_image *images;int count;} *model_env;int nmodel_env;
    void (*prepare_presenter)(void *);void *prepare_user;
    ui_flash *flashes;int nflashes;
    gt4_transition_motion transition;
    plat_texture *retained,*transition_frame;uint64_t transition_presented;
    plat_texture *(*capture)(plat_texture *,float,float,float,float);
    float fade_from,fade_to,fade_time,fade_period,fade_value,fade_step;double fade_ticks;int fade_done;
    float cursor_x,cursor_y;int cursor_visible,cursor_hidden;
    int under;                 /* drawing a page below the top one: no cursor */
    unsigned draw_tick;
    /* Engine cursor (GT4 func_0022FFD0/func_00230CF8): tip position and angle
       chase their targets; cursor_angle in degrees. */
    float cursor_tx,cursor_ty,cursor_ta,cursor_angle;int cursor_started;
    char cursor_icon[64];int cursor_frozen,cursor_paused,cursor_held,cursor_icon_reset;
    float cursor_speed_ratio;int cursor_speed_set;double cursor_ticks;
    const ui_widget *cursor_selected;int cursor_selection_set;
    const ui_widget *cursor_focus;float cursor_rect_x,cursor_rect_y,cursor_rect_w,cursor_rect_h;
    const ui_widget *option_popup;
    const ui_widget *option_value;
    float option_x,option_y,option_alpha;
};

typedef struct { float x, y, w, h, alpha; } frame;
typedef struct { float x, y, w, h; int on; } clip_rect;

/* ---- loading ---- */

ui_project *ui_project_load(gt4_vol *vol, const char *name, char *err, size_t err_len)
{
    char path[256];
    size_t n = 0;
    snprintf(path, sizeof path, "/projects/%s/%s.mproject", name, name);
    uint8_t *text = gt4_vol_read(vol, path, &n);
    if (!text) { snprintf(err, err_len, "cannot read %s", path); return NULL; }
    ui_project *p = calloc(1, sizeof *p);
    if (!p) { free(text); return NULL; }
    p->vol = vol;
    snprintf(p->name, sizeof p->name, "%s", name);
    p->tree = mp_parse((const char *)text, n, err, err_len);
    free(text);
    if (!p->tree) { free(p); return NULL; }
    snprintf(path, sizeof path, "/projects/%s/US/rtext.rt2", name);
    p->rtext_data = gt4_vol_read(vol, path, &n);
    if (p->rtext_data && !gt4_rtext_open(&p->rtext, p->rtext_data, n))
        fprintf(stderr, "ui: invalid translation table %s\n", path);
    const mp_node *const *kids;
    int k = mp_children(p->tree, &kids);
    p->roots = calloc((size_t)(k ? k : 1), sizeof *p->roots);
    p->pkgs = calloc((size_t)(k ? k : 1), sizeof *p->pkgs);
    p->rootw = calloc((size_t)(k ? k : 1), sizeof *p->rootw);
    for (int i = 0; i < k; i++)
        if (!strcmp(kids[i]->name, "RootWindow"))
            p->roots[p->nroots++] = kids[i];
    return p;
}

void ui_project_free(ui_project *p)
{
    if(p)for(int i=0;i<p->nmodel_env;i++){free(p->model_env[i].path);tex1_free(p->model_env[i].images,p->model_env[i].count);}
    if(p)free(p->model_env);
    if (!p) return;
    plat_texture_destroy(p->retained);plat_texture_destroy(p->transition_frame);
    for(int i=0;i<p->nflashes;i++) {
        ui_flash *f=&p->flashes[i];
        if(f->textures)for(unsigned j=0;j<f->clip.nbitmaps;j++)plat_texture_destroy(f->textures[j]);
        free(f->textures);gt4_strobe_free(&f->clip);free(f->path);
    }
    free(p->flashes);
    for(int i=0;i<p->nmeshes;i++) {
        if(p->meshes[i].mesh)p->models.destroy(p->meshes[i].mesh);
        free(p->meshes[i].path);
    }
    free(p->meshes);
    for (int i = 0; i < p->nfonts; i++) {
        free(p->fonts[i].name);
        plat_texture_destroy(p->fonts[i].tex);
        plat_texture_destroy(p->fonts[i].outline);
        gt4_font_free(&p->fonts[i].face);
    }
    for(int i=0;i<p->ncameras;i++){free(p->cameras[i].path);gt4_mot1_close(p->cameras[i].motion);}free(p->cameras);
    for(int i=0;i<p->nmodel_bytes;i++){free(p->model_bytes[i].path);free(p->model_bytes[i].data);}free(p->model_bytes);
    for(int i=0;i<p->nextra;i++){free(p->extra[i].name);free(p->extra[i].gpb);}
    free(p->extra);
    free(p->fonts);
    for(int i=0;i<p->nkanji;i++){plat_texture_destroy(p->kanji[i].tex);plat_texture_destroy(p->kanji[i].outline);}
    free(p->kanji);free(p->kanji_data);
    free(p->rtext_data);
    for (int i = 0; i < p->nimgs; i++) { free(p->imgs[i].path); plat_texture_destroy(p->imgs[i].tex); }
    for (int i = 0; i < p->nroots; i++) { free(p->pkgs[i].name); free(p->pkgs[i].gpb); }
    for (int i = 0; i < p->nall; i++) {
        free(p->all[i]->kids);
        free(p->all[i]->image_path);
        free(p->all[i]->movie_name);free(p->all[i]->model_path);free(p->all[i]->car_code);free(p->all[i]->car_wheel);free(p->all[i]->car_wing);
        free(p->all[i]->flash_path);
        free(p->all[i]->text);
        free(p->all[i]->text_key);free(p->all[i]->graph_points);
        free(p->all[i]->car_models);free(p->all[i]->car_motions);
        free(p->all[i]->wrapped_text);free(p->all[i]->wrapped_source);
        free(p->all[i]);
    }
    free(p->all);
    free(p->rootw);
    free(p->imgs);
    free(p->pkgs);
    free(p->roots);
    for(int i=0;i<p->native_count;i++)mp_free(p->native_trees[i]);
    free(p->native_trees);
    mp_free(p->tree);
    free(p);
}

const mp_node *ui_project_tree(const ui_project *p) { return p->tree; }
int ui_root_count(const ui_project *p) { return p->nroots; }
const char *ui_root_name(const ui_project *p, int i) { return p && i >= 0 && i < p->nroots ? mp_string(p->roots[i], "name") : ""; }

static ui_package *package(ui_project *p, int root)
{
    ui_package *pk = &p->pkgs[root];
    if (!pk->name) {
        const char *rn = ui_root_name(p, root);
        char path[256];
        pk->name = strdup(rn ? rn : "");
        /* Packages sit next to the project, per language; US for this disc. */
        snprintf(path, sizeof path, "/projects/%s/US/%s.gpb", p->name, pk->name);
        pk->gpb = gt4_vol_read(p->vol, path, &pk->gpb_size);
    }
    return pk;
}

int ui_load_package(ui_project *p,const char *directory,const char *name)
{
    if(!directory || !name || strchr(name,'/') || strstr(name,".."))return 0;
    char path[256];snprintf(path,sizeof path,"%s/US/%s.gpb",directory,name);
    for(int i=0;i<p->nextra;i++)if(!strcmp(p->extra[i].name,path))return 1;
    size_t size=0;uint8_t *data=gt4_vol_read(p->vol,path,&size);if(!data)return 0;
    ui_package *next=realloc(p->extra,(p->nextra+1)*sizeof *next);
    if(!next){free(data);return 0;}p->extra=next;
    p->extra[p->nextra++]=(ui_package){strdup(path),data,size};return 1;
}

/* 001391D0 -> 0013A678 loads the authored option motion; GPB is
   the native asset boundary. Cache owns motions shared by cloned widgets. */
gt4_mot1 *ui_load_motion(ui_project *p,int root,const char *path){
    for(int i=0;i<p->ncameras;i++)if(!strcmp(p->cameras[i].path,path))return p->cameras[i].motion;
    uint8_t *data=NULL;size_t size=0;
    for(int k=-1;k<p->nroots && !data;k++){
        ui_package *pk=package(p,k<0?root:k);
        if(pk->gpb)data=gpb_load(pk->gpb,pk->gpb_size,path,&size);
    }
    for(int i=0;i<p->nextra && !data;i++)data=gpb_load(p->extra[i].gpb,p->extra[i].gpb_size,path,&size);
    gt4_mot1 *motion=data?gt4_mot1_open(data,size):NULL;free(data);
    if(!motion)return NULL;
    void *next=realloc(p->cameras,(p->ncameras+1)*sizeof *p->cameras);
    if(!next){gt4_mot1_close(motion);return NULL;}p->cameras=next;
    p->cameras[p->ncameras].path=strdup(path);p->cameras[p->ncameras++].motion=motion;return motion;
}
gt4_mot1 *ui_load_camera(ui_project *p,int root,const char *path){
    gt4_mot1 *motion=ui_load_motion(p,root,path);float look[9],fov;
    return motion && gt4_mot1_camera(motion,0,look,&fov)?motion:NULL;
}
const uint8_t *ui_load_model_bytes(ui_project *p,int root,const char *path,size_t *size){
    *size=0;if(!p || !path)return NULL;
    for(int i=0;i<p->nmodel_bytes;i++)if(!strcmp(p->model_bytes[i].path,path)){*size=p->model_bytes[i].size;return p->model_bytes[i].data;}
    uint8_t *data=NULL;size_t bytes=0;
    for(int k=-1;k<p->nroots && !data;k++){ui_package *pk=package(p,k<0?root:k);if(pk->gpb)data=gpb_load(pk->gpb,pk->gpb_size,path,&bytes);}
    for(int i=0;i<p->nextra && !data;i++)data=gpb_load(p->extra[i].gpb,p->extra[i].gpb_size,path,&bytes);
    if(!data || bytes<128 || memcmp(data,"MDLS",4)){free(data);return NULL;}
    void *grown=realloc(p->model_bytes,(p->nmodel_bytes+1)*sizeof *p->model_bytes);if(!grown){free(data);return NULL;}
    p->model_bytes=grown;int i=p->nmodel_bytes++;p->model_bytes[i].path=strdup(path);p->model_bytes[i].data=data;p->model_bytes[i].size=bytes;*size=bytes;return data;
}

/* Image by path: first the root's own package, then the project's others. */
static const tex1_image *model_environment(ui_project *p,int root,const mp_node *node)
{
    /* ModelFace.env_image_path, authored in quick-event PrizeRoot.model:
       image/arcade/car/env0.png. Preserve disc pixels for the 3D reflection. */
    const char *path=mp_string(node,"env_image_path");if(!path || !*path)return NULL;
    for(int i=0;i<p->nmodel_env;i++)if(!strcmp(p->model_env[i].path,path))return p->model_env[i].count>0?p->model_env[i].images:NULL;
    void *grown=realloc(p->model_env,(p->nmodel_env+1)*sizeof *p->model_env);if(!grown)return NULL;
    p->model_env=grown;int i=p->nmodel_env++;memset(&p->model_env[i],0,sizeof p->model_env[i]);
    size_t size=0;uint8_t *data=NULL;
    for(int k=-1;k<p->nroots && !data;k++){ui_package *pk=package(p,k<0?root:k);if(pk->gpb)data=gpb_load(pk->gpb,pk->gpb_size,path,&size);}
    for(int k=0;k<p->nextra && !data;k++)data=gpb_load(p->extra[k].gpb,p->extra[k].gpb_size,path,&size);
    if(!data){char direct[256];snprintf(direct,sizeof direct,"/%s",path[0]=='/'?path+1:path);data=gt4_vol_read(p->vol,direct,&size);}
    p->model_env[i].path=strdup(path);p->model_env[i].count=data?tex1_decode(data,size,&p->model_env[i].images):0;free(data);
    return p->model_env[i].count>0?p->model_env[i].images:NULL;
}
static void *model_mesh(ui_project *p,int root,const char *path)
{
    if(!path || !p->models.create)return NULL;
    for(int i=0;i<p->nmeshes;i++)if(!strcmp(p->meshes[i].path,path))return p->meshes[i].mesh;
    struct ui_model_entry *grown=realloc(p->meshes,(p->nmeshes+1)*sizeof *grown);
    if(!grown)return NULL;
    p->meshes=grown;
    struct ui_model_entry *entry=&p->meshes[p->nmeshes++];
    entry->path=strdup(path);entry->mesh=NULL;
    uint8_t *data=NULL;size_t size=0;
    for(int k=-1;k<p->nroots && !data;k++) {
        ui_package *pk=package(p,k<0?root:k);
        if(pk->gpb)data=gpb_load(pk->gpb,pk->gpb_size,path,&size);
    }
    for(int i=0;i<p->nextra && !data;i++)data=gpb_load(p->extra[i].gpb,p->extra[i].gpb_size,path,&size);
    /* a path outside the packages is the volume's (quick-event's prize:
       piece/prize/1st.mdl), as flash() reads its movies */
    if(!data){char direct[256];snprintf(direct,sizeof direct,"/%s",path[0]=='/'?path+1:path);data=gt4_vol_read(p->vol,direct,&size);}
    if(data){entry->mesh=p->models.create(data,size);free(data);}
    if(!entry->mesh)fprintf(stderr,"ui: cannot render model %s\n",path);
    return entry->mesh;
}

static ui_flash *flash(ui_project *p,int root,const char *path)
{
    if(!path)return NULL;
    for(int i=0;i<p->nflashes;i++)if(!strcmp(p->flashes[i].path,path))return &p->flashes[i];
    ui_flash *grown=realloc(p->flashes,(p->nflashes+1)*sizeof *grown);if(!grown)return NULL;
    p->flashes=grown;ui_flash *f=&grown[p->nflashes++];memset(f,0,sizeof *f);f->path=strdup(path);
    uint8_t *data=NULL;size_t size=0;
    for(int k=-1;k<p->nroots && !data;k++) {
        ui_package *pk=package(p,k<0?root:k);
        if(pk->gpb)data=gpb_load(pk->gpb,pk->gpb_size,path,&size);
    }
    for(int i=0;i<p->nextra && !data;i++)data=gpb_load(p->extra[i].gpb,p->extra[i].gpb_size,path,&size);
    if(!data){char direct[256];snprintf(direct,sizeof direct,"/%s",path[0]=='/'?path+1:path);data=gt4_vol_read(p->vol,direct,&size);}
    int ok=data && gt4_strobe_decode(data,size,&f->clip);free(data);
    if(ok) {
        f->textures=calloc(f->clip.nbitmaps,sizeof *f->textures);
        if(!f->textures){gt4_strobe_free(&f->clip);return f;}
        for(unsigned i=0;i<f->clip.nbitmaps;i++) {
            tex1_image *im=&f->clip.bitmaps[i];f->textures[i]=plat_texture_create(im->width,im->height,im->rgba);
            if(!f->textures[i]) {
                for(unsigned j=0;j<i;j++)plat_texture_destroy(f->textures[j]);
                free(f->textures);f->textures=NULL;gt4_strobe_free(&f->clip);
                fprintf(stderr,"ui: cannot upload FlashFace %s\n",path);return f;
            }
        }
        fprintf(stderr,"flash: %s, %u images, %u frames at %.3f fps\n",path,f->clip.nbitmaps,f->clip.nframes,f->clip.rate);
    }else fprintf(stderr,"ui: unsupported or missing FlashFace %s\n",path);
    return f;
}

/* The local HD cache (ui_hd.h): an upscaled copy of this exact image, if the
   user built one; the widget keeps the original's size. */
static char hd_dir[512];
void ui_set_hd_cache(const char *dir) { snprintf(hd_dir, sizeof hd_dir, "%s", dir ? dir : ""); }
static plat_texture *hd_texture(const tex1_image *t)
{
    /* a strip a few texels across gains nothing upscaled, and the cache's
       copy of labomode's folder_bg (107x10) comes out as noise */
    if (!*hd_dir || !t->rgba || t->width < 16 || t->height < 16) return NULL;
    char path[600];int w=0,h=0;
    snprintf(path,sizeof path,"%s/%016llx.hdi",hd_dir,(unsigned long long)gt4_ui_hd_hash(t->rgba,t->width,t->height));
    uint8_t *px=gt4_ui_hd_read(path,&w,&h);
    if(!px)return NULL;
    plat_texture *tex=w>t->width && h>t->height?plat_texture_create(w,h,px):NULL;
    free(px);return tex;
}

/* The PC layer's disk icon (docs/EDITIONS.md) in the place of labomode's
   memory card (image/labo/memory_header.png, 30x30): a hard disk drawn
   as the card is, a white body the widget tints (blue, red) on a soft
   black shadow, its platter a ring cut out with the spindle and the arm.
   Drawn here, not shipped as an image. */
static float disk_cover(float x, float y)
{
    const float l = 4.5f, t = 4.5f, r = 20.5f, b = 24.5f, round = 2;
    float dx = fmaxf(fmaxf(l + round - x, x - (r - round)), 0), dy = fmaxf(fmaxf(t + round - y, y - (b - round)), 0);
    float cx = 12.5f, cy = 12.5f, d = hypotf(x - cx, y - cy);
    float ax = 18, ay = 21.5f, bx = 14.5f, by = 15, px, py, k;
    int body = dx * dx + dy * dy <= round * round && x >= l && x <= r && y >= t && y <= b;
    if (!body) return 0;
    if (d < 1.4f) return 1;                         /* the spindle */
    k = ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / ((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
    k = fminf(fmaxf(k, 0), 1);
    px = ax + k * (bx - ax) - x;
    py = ay + k * (by - ay) - y;
    if (px * px + py * py < .8f * .8f) return 1;     /* the arm */
    if (d > 2.4f && d < 6) return 0;                 /* the platter */
    if (y > 20 && y < 21.6f && x < 10) return 0;     /* the label's groove */
    return 1;
}

static void disk_icon(uint8_t rgba[30 * 30 * 4])
{
    float body[30][30], shadow[30][30];
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < 30; x++) {
            float c = 0;
            for (int sy = 0; sy < 4; sy++)
                for (int sx = 0; sx < 4; sx++) c += disk_cover(x + (sx + .5f) / 4, y + (sy + .5f) / 4);
            body[y][x] = c / 16;
        }
    for (int y = 0; y < 30; y++)   /* the shadow: the body's outline a pixel down and right, blurred */
        for (int x = 0; x < 30; x++) {
            float s = 0;
            for (int j = -1; j <= 1; j++)
                for (int i = -1; i <= 1; i++) {
                    int u = x - 1 + i, v = y - 1 + j;
                    float in = u >= 4 && u <= 20 && v >= 4 && v <= 24 ? 1.f : 0.f;
                    s += in;
                }
            /* through the cut-outs only faintly, as the card's window */
            shadow[y][x] = s / 9 * (x >= 4 && x <= 20 && y >= 4 && y <= 24 ? .22f : .55f);
        }
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < 30; x++) {
            float a = body[y][x] + shadow[y][x] * (1 - body[y][x]);
            uint8_t *o = rgba + (y * 30 + x) * 4;
            uint8_t v = a > 0 ? (uint8_t)(255 * body[y][x] / a + .5f) : 0;
            o[0] = o[1] = o[2] = v;
            o[3] = (uint8_t)(255 * a + .5f);
        }
}

static ui_image *image(ui_project *p, int root, const char *path)
{
    for (int i = 0; i < p->nimgs; i++)
        if (!strcmp(p->imgs[i].path, path)) return &p->imgs[i];
    ui_image *grown = realloc(p->imgs, (size_t)(p->nimgs + 1) * sizeof *grown);
    if (!grown) return NULL;
    p->imgs = grown;
    ui_image *im = &p->imgs[p->nimgs++];
    memset(im, 0, sizeof *im);
    im->path = strdup(path);
    if (!strcmp(path, "image/labo/memory_header.png")) {   /* the PC's disk, not the card */
        uint8_t px[30 * 30 * 4];
        disk_icon(px);
        im->tex = plat_texture_create(30, 30, px);
        im->w = im->h = 30;
        return im;
    }

    const uint8_t *data = NULL;
    size_t size = 0;
    uint8_t *owned=NULL;
    for (int k = -1; k < p->nroots && !data; k++) {
        ui_package *pk = package(p, k < 0 ? root : k);
        if (pk->gpb) data = owned = gpb_load(pk->gpb, pk->gpb_size, path, &size);
    }
    for(int i=0;i<p->nextra && !data;i++)data=owned=gpb_load(p->extra[i].gpb,p->extra[i].gpb_size,path,&size);
    int packaged=data!=NULL;
    if(!data) {
        char direct[256];
        if(!strncmp(path,"image/piece/",12)) {
            snprintf(direct,sizeof direct,"/%s",path+6);
            char *ext=strrchr(direct,'.');if(ext && !strcmp(ext,".png"))strcpy(ext,".img");
        } else snprintf(direct,sizeof direct,"/%s",path[0]=='/'?path+1:path);
        owned=gt4_vol_read(p->vol,direct,&size);data=owned;
    }
    tex1_image *ti = NULL;
    int count = data ? tex1_decode(data, size, &ti) : -1;
    /* Some catalog packages contain a non-standalone logo payload. Prefer
       the complete disc image when that packaged representation cannot decode. */
    if(count<=0 && packaged && !strncmp(path,"image/piece/",12)) {
        char direct[256];snprintf(direct,sizeof direct,"/%s",path+6);
        char *ext=strrchr(direct,'.');if(ext && !strcmp(ext,".png"))strcpy(ext,".img");
        tex1_free(ti,count);ti=NULL;free(owned);owned=gt4_vol_read(p->vol,direct,&size);
        count=owned?tex1_decode(owned,size,&ti):-1;
    }
    if (count > 0) {
        im->tex = hd_texture(&ti[0]);
        if (!im->tex) im->tex = plat_texture_create(ti[0].width, ti[0].height, ti[0].rgba);
        im->w = ti[0].width;
        im->h = ti[0].height;
    }
    tex1_free(ti, count);free(owned);
    return im;
}

int ui_package_contains(ui_project *p,int root,const char *path)
{
    if(!p || !path)return 0;
    size_t size;
    for(int k=-1;k<p->nroots;k++) {
        ui_package *pk=package(p,k<0?root:k);
        if(pk && pk->gpb && gpb_find(pk->gpb,pk->gpb_size,path,&size))return 1;
    }
    for(int i=0;i<p->nextra;i++)
        if(gpb_find(p->extra[i].gpb,p->extra[i].gpb_size,path,&size))return 1;
    return 0;
}

static void layout_packed(ui_project *p,ui_widget *w);
void ui_adjust_size(ui_project *p,ui_widget *w)
{
    if((!strcmp(w->type,"MBox") || !strcmp(w->type,"FBox")) && mp_number(w->src,"packing",0)){layout_packed(p,w);return;}
    if(!strcmp(w->type,"ImageFace") && w->image_path) {
        ui_image *im=image(p,w->resource_root,w->image_path);
        if(im && im->tex){w->w=(float)im->w;w->h=(float)im->h;w->has_geometry=1;}
    } else {
        float width=0,height=0;
        for(int i=0;i<w->nkids;i++){ui_widget *c=w->kids[i];ui_adjust_size(p,c);width=fmaxf(width,c->x+c->w);height=fmaxf(height,c->y+c->h);}
        if(width>0 && height>0){w->w=width;w->h=height;w->has_geometry=1;}
    }
}

/* ---- widget trees ---- */

static ui_widget *widget_new(ui_project *p)
{
    ui_widget *w = calloc(1, sizeof *w);
    if (!w) return NULL;
    w->car_color_index=-1;
    if (p->nall == p->allcap) {
        int cap = p->allcap ? p->allcap * 2 : 256;
        ui_widget **grown = realloc(p->all, (size_t)cap * sizeof *grown);
        if (!grown) { free(w); return NULL; }
        p->all = grown;
        p->allcap = cap;
    }
    p->all[p->nall++] = w;
    return w;
}

static void widget_set_root(ui_widget *w,int root)
{
    w->root=root;
    for(int i=0;i<w->nkids;i++)widget_set_root(w->kids[i],root);
}

void ui_widget_append(ui_widget *parent, ui_widget *child)
{
    if (!parent || !child) return;
    for(ui_widget *p=parent;p;p=p->parent)if(p==child)return;
    if (parent->nkids == parent->kcap) {
        int cap = parent->kcap ? parent->kcap * 2 : 4;
        ui_widget **grown = realloc(parent->kids, (size_t)cap * sizeof *grown);
        if (!grown) return;
        parent->kids = grown;
        parent->kcap = cap;
    }
    ui_widget_detach(child);
    parent->kids[parent->nkids++] = child;
    child->parent = parent;
    widget_set_root(child,parent->root);
}

void ui_widget_detach(ui_widget *w)
{
    ui_widget *pa = w ? w->parent : NULL;
    if (!pa) return;
    for (int i = 0; i < pa->nkids; i++)
        if (pa->kids[i] == w) {
            memmove(&pa->kids[i], &pa->kids[i + 1], (size_t)(pa->nkids - i - 1) * sizeof *pa->kids);
            pa->nkids--;
            break;
        }
    w->parent = NULL;
}

ui_widget *ui_widget_child(const ui_widget *w, const char *name)
{
    for (int i = 0; w && i < w->nkids; i++)
        if (w->kids[i]->name && !strcmp(w->kids[i]->name, name)) return w->kids[i];
    return NULL;
}

void ui_image_draw(ui_project *p,const char *path,float x,float y,float w,float h,float alpha)
{
 if(!p || !path)return;ui_image *im=image(p,0,path);if(!im || !im->tex)return;
 uint8_t tint[4]={255,255,255,(uint8_t)(255*fminf(1,fmaxf(0,alpha)))};plat_draw_quad(im->tex,x,y,w,h,0,0,1,1,tint);
}
void ui_image_put(ui_project *p, const char *path, int w, int h, const uint8_t *rgba)
{
    if (!p || !path || !rgba || w <= 0 || h <= 0) return;
    ui_image *im = NULL;
    for (int i = 0; i < p->nimgs && !im; i++) if (!strcmp(p->imgs[i].path, path)) im = &p->imgs[i];
    if (!im) {
        ui_image *grown = realloc(p->imgs, (size_t)(p->nimgs + 1) * sizeof *grown);
        if (!grown) return;
        p->imgs = grown;
        im = &p->imgs[p->nimgs++];
        memset(im, 0, sizeof *im);
        im->path = strdup(path);
    } else if (im->tex) plat_texture_destroy(im->tex);
    im->tex = plat_texture_create(w, h, rgba);
    im->w = w;
    im->h = h;
}

void ui_widget_set_image(ui_widget *w, const char *path)
{
    free(w->image_path);
    w->image_path = path ? strdup(path) : NULL;
}

static ui_widget *build(ui_project *p, int root, const mp_node *n, float pw, float ph)
{
    ui_widget *w = widget_new(p);
    if (!w) return NULL;
    w->src = n;
    w->type = n->name;
    /* memcard.ad's FileList: a SelectBar is a SelectBox (a list by its
       direction, its index the focused item) */
    if (!strcmp(w->type, "SelectBar")) w->type = "SelectBox";
    w->name = mp_string(n, "name");
    w->root = root;
    w->resource_root = root;
    double g[4];
    if (mp_numbers(n, "geometry", g, 4) == 4) {
        w->x = (float)g[0]; w->y = (float)g[1]; w->w = (float)g[2]; w->h = (float)g[3];
        w->has_geometry = 1;
    } else {
        w->w = pw; w->h = ph;            /* assumption: no geometry = parent's area */
    }
    w->visible = mp_number(n, "visible", 1) != 0;
    /* P: local storage has no selectable console slot or card-status row. */
    if(w->name && (!strcmp(w->name,"McSlot") || !strcmp(w->name,"Memorycard")))w->visible=0;
    /* the PC layer (docs/EDITIONS.md): labomode's options have no card
       slot to choose; the row is left out and its pane closes up */
    if (!strcmp(p->name, "labomode") && w->name && !strcmp(w->name, "McSlot") && !strcmp(w->type, "Composite"))
        w->visible = 0;
    w->insensitive = mp_number(n, "insensitive", 0) != 0;
    w->opacity = (float)mp_number(n, "opacity", 1.0);
    if(!strcmp(w->type,"ModelFace")) {
        double angles[3]={0};mp_numbers(n,"model_rotate",angles,3);
        for(int i=0;i<3;i++)w->model_angles[i]=(float)angles[i];
        /* model_rotate_v turns it from the start (the prize's trophy:
           1.7143 degrees a frame); without one it rests, the map icons'
           scripts setting target_mode and rotate_v themselves */
        double turn[3]={0};mp_numbers(n,"model_rotate_v",turn,3);
        for(int i=0;i<3;i++)w->model_velocity[i]=(float)turn[i];
        w->model_target_mode=!(turn[0] || turn[1] || turn[2]);
    }
    w->scale_x = (float)mp_number(n, "scale_x", 1);
    w->scale_y = (float)mp_number(n, "scale_y", 1);
    /* 00246F58 +0xD4: original TextFace extra gap defaults to zero.
       Keep the older readability override outside the quick-menu area. */
    w->letter_spacing = (float)mp_number(n, "letter_spacing", !strncmp(p->name,"quick-",6)?0:1);
    const char *movie=mp_string(n,"movie_name");
    if(movie)w->movie_name=strdup(movie);
    const char *flash_path=mp_string(n,"flash_path");
    if(flash_path)w->flash_path=strdup(flash_path);
    w->movie_audio_track=mp_number(n,"audio",0)?(int)mp_number(n,"audio_track",1):0;
    const char *img = mp_string(n, "image_path");
    if (img) w->image_path = strdup(img);
    const char *txt = mp_string(n, "text"), *key = mp_string(n, "key");
    if (txt) w->text = strdup(txt);
    if (key) w->text_key = strdup(key);
    const mp_node *const *kids;
    int k = mp_children(n, &kids);
    for (int i = 0; i < k; i++)
        if (kids[i]->kind == MP_OBJECT) ui_widget_append(w, build(p, root, kids[i], w->w, w->h));
    return w;
}

ui_widget *ui_widget_from_text(ui_project *p,int root,const char *description){if(!p || root<0 || root>=p->nroots || !description)return NULL;char error[128];mp_node *tree=mp_parse(description,strlen(description),error,sizeof error);if(!tree){fprintf(stderr,"native UI: %s\n",error);return NULL;}mp_node **trees=realloc(p->native_trees,(p->native_count+1)*sizeof *trees);if(!trees){mp_free(tree);return NULL;}p->native_trees=trees;p->native_trees[p->native_count++]=tree;return build(p,root,tree,640,480);}

ui_widget *ui_root_widget(ui_project *p, int i)
{
    if (i < 0 || i >= p->nroots) return NULL;
    if (!p->rootw[i]) p->rootw[i] = build(p, i, p->roots[i], 640, 480);
    return p->rootw[i];
}

ui_widget *ui_widget_copy(ui_project *p, const ui_widget *src)
{
    ui_widget *w = widget_new(p);
    if (!w) return NULL;
    *w = *src;
    w->car_models=NULL;w->car_model_count=0;w->car_motions=NULL;w->car_motion_count=0;
    if(src->car_model_count){w->car_models=malloc((size_t)src->car_model_count*sizeof *w->car_models);
        if(w->car_models){memcpy(w->car_models,src->car_models,(size_t)src->car_model_count*sizeof *w->car_models);w->car_model_count=src->car_model_count;}}
    if(src->car_motion_count){w->car_motions=malloc((size_t)src->car_motion_count*sizeof *w->car_motions);
        if(w->car_motions){memcpy(w->car_motions,src->car_motions,(size_t)src->car_motion_count*sizeof *w->car_motions);w->car_motion_count=src->car_motion_count;}}
    w->graph_points=NULL;w->graph_count=0;
    if(src->graph_count>0){w->graph_points=malloc((size_t)src->graph_count*sizeof *w->graph_points);
        if(w->graph_points){memcpy(w->graph_points,src->graph_points,(size_t)src->graph_count*sizeof *w->graph_points);w->graph_count=src->graph_count;}}
    w->parent = NULL;
    w->kids = NULL;
    w->nkids = w->kcap = 0;
    w->script = NULL;
    w->focused = 0;
    w->movie_name=src->movie_name?strdup(src->movie_name):NULL;
    w->model_path=src->model_path?strdup(src->model_path):NULL;
    w->flash_path=src->flash_path?strdup(src->flash_path):NULL;w->flash_time=0;
    w->image_path = src->image_path ? strdup(src->image_path) : NULL;
    w->car_code = src->car_code ? strdup(src->car_code) : NULL;
    w->car_wheel = src->car_wheel ? strdup(src->car_wheel) : NULL;
    w->car_wing = src->car_wing ? strdup(src->car_wing) : NULL;
    w->text = src->text ? strdup(src->text) : NULL;
    w->text_key = src->text_key ? strdup(src->text_key) : NULL;
    w->wrapped_text=NULL;w->wrapped_source=NULL;
    for (int i = 0; i < src->nkids; i++) ui_widget_append(w, ui_widget_copy(p, src->kids[i]));
    return w;
}

/* Logical choice indices exclude decoration faces in the popup's VBox. */
ui_widget *ui_option_item(const ui_widget *w,int index)
{
    if(!w || strcmp(w->type,"OptionMenu") || index<0)return NULL;
    ui_widget *popup=ui_widget_child(w,"popup");
    ui_widget *list=popup?ui_widget_child(popup,"vbox"):NULL;
    for(int i=0;list && i<list->nkids;i++) {
        ui_widget *item=list->kids[i];
        if(strcmp(item->type,"Composite") && strcmp(item->type,"Button"))continue;
        if(index--==0)return item;
    }
    return NULL;
}
int ui_option_count(const ui_widget *w)
{
    int count=0;while(ui_option_item(w,count))count++;return count;
}

/* ---- drawing ---- */

/* Glyph atlases go to the platform as distance fields (glyph_sdf.h): the
   original coverage for the disc's look, and the signed distance the glyph
   shader turns into an edge at the window's own resolution (Text Clarity).
   Up to 4x the atlas, 2048 texels at most. */
static plat_texture *glyph_texture(ui_project *p,const tex1_image *a)
{
    int big=a->width>a->height?a->width:a->height,k=big>0?2048/big:1,w=0,h=0;
    if(k>4)k=4;if(k<1)k=1;
    uint8_t *sdf=gt4_glyph_sdf_presented(a->rgba,a->width,a->height,k,&w,&h,p->prepare_presenter,p->prepare_user);
    if(!sdf)return plat_texture_create(a->width,a->height,a->rgba);
    plat_texture *t=plat_texture_create(w,h,sdf);free(sdf);return t;
}

static ui_font *font(ui_project *p, const char *name)
{
    /* 00246F58 +0x38: default face string at 006991A8 is hn_14. */
    if (!name) name = !strncmp(p->name,"quick-",6) || !strcmp(p->name,"labomode")?"hn_14":"hm_16";   /* labomode: the replay list's date, course, car and size as PCSX2 shows them */
    for (int i = 0; i < p->nfonts; i++)
        if (!strcmp(p->fonts[i].name, name)) return &p->fonts[i];
    ui_font *grown = realloc(p->fonts, (size_t)(p->nfonts + 1) * sizeof *grown);
    if (!grown) return NULL;
    p->fonts = grown;
    ui_font *f = &grown[p->nfonts++];
    memset(f, 0, sizeof *f);
    f->name = strdup(name);
    const char *paths[] = { "/font/system.fnt", "/font/menu.fnt", "/font/race.fnt" };
    for (unsigned i = 0; i < sizeof paths / sizeof paths[0]; i++) {
        size_t n = 0;
        uint8_t *data = gt4_vol_read(p->vol, paths[i], &n);
        int ok = data ? gt4_font_decode(data, n, name, &f->face) : 0;
        free(data);
        if (ok == 1) {
            /* FT01 palette 0 includes a black outline; palette 1 is glyph ink.
               Multiplying palette 0 by black also blackens the outline. */
            tex1_image *a = &f->face.atlases[f->face.natlases>1?1:0];
            f->tex = glyph_texture(p,a);
            a = &f->face.atlases[0];
            f->outline = glyph_texture(p,a);
            return f;
        }
        if (ok < 0) fprintf(stderr, "ui: invalid font %s in %s\n", name, paths[i]);
    }
    fprintf(stderr, "ui: font unavailable: %s\n", name);
    return f;
}

/* A KNJI glyph (00492198 -> 00491CB8). 0048F9B8 loads the file chosen by
   00108D80(1): region "us" (and "jp", "uk") is jp_menu.kf. */
static int kanji_glyph(ui_project *p,unsigned set,unsigned code)
{
    for(int i=0;i<p->nkanji;i++)if(p->kanji[i].set==set && p->kanji[i].code==code)return i;
    if(!p->kanji_tried){p->kanji_tried=1;p->kanji_data=gt4_vol_read(p->vol,"/font/jp_menu.kf",&p->kanji_size);
        if(!p->kanji_data)fprintf(stderr,"ui: /font/jp_menu.kf unavailable\n");}
    uint8_t ink[64*64*4],outlined[64*64*4];int w,h;
    if(!p->kanji_data || !gt4_kanji_glyph(p->kanji_data,p->kanji_size,set,code,&w,&h,ink,outlined,sizeof ink))return -1;
    void *grown=realloc(p->kanji,(size_t)(p->nkanji+1)*sizeof *p->kanji);if(!grown)return -1;
    p->kanji=grown;int i=p->nkanji++;
    tex1_image a={0};a.width=w;a.height=h;a.rgba=ink;
    p->kanji[i].set=set;p->kanji[i].code=code;p->kanji[i].w=w;p->kanji[i].h=h;
    p->kanji[i].tex=glyph_texture(p,&a);a.rgba=outlined;p->kanji[i].outline=glyph_texture(p,&a);
    return i;
}

static int storage_prefix(const char *s,const char *word){
 for(;*word;s++,word++)if(!*s || tolower((unsigned char)*s)!=tolower((unsigned char)*word))return 0;return 1;
}
static unsigned storage_token(const char *s){
 if(storage_prefix(s,"memcard"))return 7;
 if(!storage_prefix(s,"memory"))return 0;
 const char *p=s+6;while(*p && (isspace((unsigned char)*p) || *p=='_' || *p=='-'))p++;
 return storage_prefix(p,"card")?(unsigned)(p-s+4):0;
}
const char *ui_storage_text(const char *text){
 if(!text)return NULL;int found=0;for(const char *p=text;*p;p++)if(storage_token(p)){found=1;break;}if(!found)return text;
 /* Render-only replacement: source script identifiers and saved data remain
    compatible. Rotating buffers are copied by VM/dialog owners as usual. P. */
 static char buffers[32][8192];static unsigned next;char *out=buffers[next++%32];size_t at=0;
 for(const char *p=text;*p && at<sizeof buffers[0]-1;){unsigned n=storage_token(p);
  if(n){const char *replacement="local storage";size_t len=strlen(replacement);if(at+len>=sizeof buffers[0])break;memcpy(out+at,replacement,len);at+=len;p+=n;}
  else if(storage_prefix(p," (PS2)"))p+=6;
  else out[at++]=*p++;
 }out[at]=0;return out;
}

const char *ui_translate(ui_project *p, int root, const char *page_name, const char *key)
{
    if (!key || root < 0 || root >= p->nroots) return NULL;
    /* Native PC profiles replace the PS2 storage-device prompts. Keep the
       authored dialog flow, but describe the actual PC action. */
    static const struct {const char *key,*text;} profile_messages[]={
        {"exist gt3 data on slot n. inherit license?","GT3 data found in local imports.%_\nTransfer credits?"},
        {"exist gt4p data on slot n. inherit license?","GT3 data found in local imports.%_\nImport B/A licenses?"},
        {"Do you load trade data?","Choose a local player profile to browse its garage?"},
        {"no_memory_card","No player profile selected.\nCreate or load a profile to save your progress."},
        {"no_memorycard","No player profile selected.\nCreate or load a profile to save your progress."},
        {"no_memorycard_slot_n","No player profile selected.\nCreate or load a profile to play GT Mode."},
        {"no_memory_card_retry?","Select a player profile and try again?"},
        {"no_memory_card_retry_confirm","Select Player Profile"},
        {"different_card","The selected player profile has changed."},
        {"different_card_retry?","The selected player profile has changed. Try again?"},
        {"different_card_retry?_2","The selected player profile has changed. Try again?"},
        {"different_card_retry_confirm","Player Profile Changed"},
        {"create_game_data_need_n_byte_confirm","Create a player profile to save your progress?"},
        {"not_enough_space_slot_to_create_game_need_more_n_byte","Not enough free disk space to create the profile."},
        {"not_enough_space_slot_to_create_game_need_n_byte","Not enough free disk space to create the profile."},
        {"slot1 card changed try save?","The selected player profile has changed. Save your progress?"},
        /* the replays, photos and playlists on the PC's disk (docs/EDITIONS.md,
           the PC layer): the same arguments as the card's texts, the slot
           taken and dropped (%_), the sizes in KB written to fit (%K) */
        {"n_kbfree_slot_n","%K free : Hard Disk%_"},
        {"create_full_replay_need_n_byte_confirm","Full Lap Replay data will now be created on the\r\nhard disk.%_\r\n%K of free space is required to create data."},
        {"create_best_replay_need_n_byte_confirm","Best lap replay data will now be created on the\r\nhard disk.%_\r\n%K of free space is required to create data."},
        {"broken playlist slot n create?","The playlist data on the hard disk is corrupted.%_\r\nDo you wish to create a new one?"},
        {"no playlist slot n create?","No playlist data found on the hard disk.%_\r\nDo you wish to create a new one?"},
        {"no_card","The hard disk could not be read."},
        {"no_card_slot_n","The hard disk could not be read.%_"},
        {"access_error","An error occurred while loading data from the hard disk."},
        {"LabelQuality_image","Image Quality"},
        {"TOOLTIP|Quality_image","Set image quality when saving. Higher quality means larger file size."},
        {"not_enough_space_slot_to_create_best_replay_need_n_byte","Insufficient free space on the hard disk.%_\r\n%K of free space is required to save Best Lap Replay data."},
        {"not_enough_space_slot_to_create_best_replay_more_need_n_byte","Insufficient free space on the hard disk.%_\r\nAdditional %K of free space is required to overwrite Best Lap Replay data."},
        {"not_enough_space_slot_to_create_full_replay_need_n_byte","Insufficient free space on the hard disk.%_\r\n%K of free space is required to save Full Lap Replay data."},
        {"not_enough_space_slot_to_create_full_replay_more_need_n_byte","Insufficient free space on the hard disk.%_\r\nAdditional %K of free space is required to overwrite Full Lap Replay data."},
        {"not_enough_space_slot_to_create_film_need_n_byte","Insufficient free space on the hard disk.%_\r\n%K of free space is required to save Film data."},
        {"not_enough_space_slot_to_create_film_more_need_n_byte","Insufficient free space on the hard disk.%_\r\nAdditional %K of free space is required to overwrite Film data."},
        {"not_enough_space_slot_to_create_photo_need_n_byte","Insufficient free space on the hard disk.%_\r\n%K of free space is required to save Photo data."},
        {"not_enough_space_slot_to_create_photo_more_need_n_byte","Insufficient free space on the hard disk.%_\r\nAdditional %K of free space is required to overwrite Photo data."},
        {"not_enough_space_slot_to_create_replaylist_need_n_byte","Insufficient free space on the hard disk.%_\r\n%K of free space is required to create a playlist."},
        {"not_enough_space_slot_to_create_replaylist_more_need_n_byte","Insufficient free space on the hard disk.%_\r\nAdditional %K of free space is required to overwrite playlist data."},
        {"not_enough_space_slot_to_create_slideshow_need_n_byte","Insufficient free space on the hard disk.%_\r\n%K of free space is required to create a slideshow."},
        {"not_enough_space_slot_to_create_slideshow_more_need_n_byte","Insufficient free space on the hard disk.%_\r\nAdditional %K of free space is required to overwrite slideshow data."},
        {"create_film_need_n_byte_confirm","Film data will now be created on the hard disk.%_\r\n%K of free space is required to create film data."},
        {"create_photo_need_n_byte_confirm","Photo data will now be created on the hard disk.%_\r\n%K of free space is required to create photo data."},
        {"create_replaylist_need_n_byte_confirm","A playlist will now be created on the hard disk.%_\r\n%K of free space is required to create a playlist."},
        {"create_slideshow_need_n_byte_confirm","A slideshow will now be created on the hard disk.%_\r\n%K of free space is required to create a slideshow."},
    };
    for(size_t i=0;i<sizeof profile_messages/sizeof *profile_messages;i++)
        if(!strcmp(key,profile_messages[i].key))return profile_messages[i].text;
    char page[256];
    const char *project = mp_string(p->tree, "name");
    snprintf(page, sizeof page, "%s::%s", project ? project : "", ui_root_name(p, root));
    const char *txt = gt4_rtext_get(&p->rtext, page_name ? page_name : page, key);
    const char *mapped=ui_storage_text(txt);
    if(mapped!=txt && (strstr(key,"TOOLTIP") || !strcmp(key,"Slot1") || !strcmp(key,"Slot2")))return "Load a garage from a saved local player profile.";
    return mapped;
}

const char *ui_widget_source_text(ui_project *p,const ui_widget *w)
{
    const char *text = w->text ? w->text : ui_translate(p, w->resource_root, NULL, w->text_key);
    return text ? ui_storage_text(text) : "";
}

const char *ui_widget_text(ui_project *p,const ui_widget *w)
{
    const char *source=ui_widget_source_text(p,w);
    return w->wrapped_text && w->wrapped_source && !strcmp(source,w->wrapped_source)?w->wrapped_text:source;
}

static const char *text_font(const ui_widget *w)
{
    const char *name=mp_string(w->src,"font");
    /* The car-selection tab counters omit a face in the resource. Their
       original small text uses hn_14, retaining the authored 1.1 scale. */
    if(!name && w->name && !strcmp(w->name,"info")) {
        const ui_widget *root=w;
        while(root->parent)root=root->parent;
        if(root->name && (!strcmp(root->name,"MakerRoot") ||
                          !strcmp(root->name,"HistoryRoot")) &&
           w->parent && w->parent->name &&
           (!strcmp(w->parent->name,"Maker") ||
            !strcmp(w->parent->name,"History") ||
            !strcmp(w->parent->name,"Favorite")))name="hn_14";
    }
    /* Packed notices omit the face. hn_14 matches the small, unoutlined
       help text in the GT Mode reference; keep explicit faces authoritative. */
    if(!name && w->parent && !strcmp(w->parent->type,"FBox"))name="hn_14";
    return name;
}

/* 00492978's spacing (pair contours, bearing after a glyph): the quick
   menus and GT Mode, whose Tune Shop title in tr_30 measures 121 like the
   user's PCSX2 capture (127 with plain advances). */
static int original_text(const ui_project *p){return !strncmp(p->name,"quick-",6) || !strcmp(p->name,"gtmode");}
static float original_line_width(ui_project *p,const gt4_font *face,const char *text,float scale,int left);
/* a TextFace's horizontal alignment, as draw_text_face reads it */
static int text_left(const ui_project *p,const ui_widget *w){
    int align=(int)mp_number(w->src,"align",!strncmp(p->name,"quick-",6) || !strcmp(p->name,"labomode")?4:1);
    if(w->has_runtime_align)align=w->runtime_align;return align%3==0;
}
void ui_text_size(ui_project *p,const ui_widget *w,float size[2])
{
    size[0]=size[1]=0;
    ui_font *f=font(p,text_font(w));
    if(!f || !f->tex)return;
    const char *s=ui_widget_text(p,w);if(!*s)return;
    int lines=0;
    do {
        float width=original_text(p)?original_line_width(p,&f->face,s,w->scale_x,text_left(p,w)):gt4_font_line_width(&f->face,s,w->letter_spacing)*w->scale_x;
        size[0]=fmaxf(size[0],width);
        lines++;
        while(*s && *s!='\n' && *s!='\r')s++;
        if(!*s)break;
        if(*s++=='\r' && *s=='\n')s++;
    }while(1);
    size[1]=lines*f->face.height*w->scale_y;
}

/* Wrap dialog text at word boundaries using its original font metrics. */
void ui_text_wrap(ui_project *p,ui_widget *w,float width)
{
    if(!w || strcmp(w->type,"TextFace") || width<=0)return;
    ui_font *face=font(p,text_font(w));const char *source=ui_widget_source_text(p,w);
    if(!face || !source)return;char *wrapped=strdup(source);if(!wrapped)return;
    char *line=wrapped,*space=NULL;
    for(char *at=wrapped;*at;at++){
        if(*at=='\n' || *at=='\r'){line=at+1;space=NULL;continue;}
        if(*at==' ' || *at=='\t')space=at;
        char next=at[1];at[1]=0;
        /* Match draw_text_face's 00492978 metrics. */
        float measured=original_text(p)?original_line_width(p,&face->face,line,w->scale_x,text_left(p,w)):gt4_font_line_width(&face->face,line,w->letter_spacing)*w->scale_x;
        at[1]=next;
        if(measured>width && space && space>=line){*space='\n';line=space+1;space=NULL;}
    }
    char *snapshot=strdup(source);
    if(!snapshot){free(wrapped);return;}
    free(w->wrapped_text);free(w->wrapped_source);
    w->wrapped_text=wrapped;w->wrapped_source=snapshot;
}

void ui_light_defaults(double world[4],double position[4],double ambient[4],double diffuse[4],double specular[4])
{
    /* 00210830 +0x9c..0x12c: world RGB zero; direction (0,1,0,0);
       ambient bits 0x3e999999, diffuse/specular bits 0x3f000000.
       UI attribute overrides use byte-valued colour units. */
    for(int k=0;k<4;k++){
        world[k]=k==3?255:0;position[k]=k==1?1:0;
        ambient[k]=k==3?255:.29999998211860657*255;
        diffuse[k]=specular[k]=k==3?255:127.5;
    }
}

void ui_text_adjust_scale(ui_project *p, ui_widget *w)
{
    if (strcmp(w->type, "TextFace") || w->w <= 0) return;
    ui_font *f = font(p, text_font(w));
    if (!f || !f->tex) return;
    const char *s = ui_widget_text(p, w);
    float width = 0;
    while (*s) {
        float line = gt4_font_line_width(&f->face, s, w->letter_spacing) * w->scale_x;
        if (line > width) width = line;
        while (*s && *s != '\n' && *s != '\r') s++;
        if (*s == '\r') s++;
        if (*s == '\n') s++;
    }
    if (width > w->w) w->scale_x *= w->w / width;
}

typedef struct {const gt4_font *face;float scale;} abbreviation_metrics;
static float abbreviation_width(void *user,const char *text)
{abbreviation_metrics *m=user;return gt4_font_original_width(m->face,text,m->scale);}
int ui_text_abbreviate(ui_project *p,ui_widget *w)
{
    if(strcmp(w->type,"TextFace") || w->w<=0)return 0;
    ui_font *f=font(p,text_font(w));if(!f)return 0;
    const char *text=ui_widget_text(p,w);char abbreviated[0x104];
    abbreviation_metrics metrics={&f->face,w->scale_x};
    /* quick-arcade.ad L102 calls this separately from adjustScale().
       00245658 +0x7C passes the authored width and a 0x104-byte buffer. */
    if(!gt4_text_abbreviate(text,abbreviated,sizeof abbreviated,w->w,abbreviation_width,&metrics))return 0;
    char *copy=strdup(abbreviated);if(!copy)return 0;
    free(w->text);w->text=copy;return 1;
}

/* func_004940E8: "%B<c>" is c in the Gaiji font (the pad's buttons):
   c..f (triangle, circle, cross, square) in D_006AE738's colours, the
   others grey 0x646464; the rest of the text in its own font */
static const uint8_t button_rgb[4][3]={{0x00,0x99,0x9b},{0xf7,0x1c,0x54},{0x51,0x72,0xb3},{0xf7,0x68,0xa1}};
static int button_code(const char *t){return t[0]=='%' && t[1]=='B' && t[2] && t[2]!='\n' && t[2]!='\r';}
/* a line's width as func_004940E8 lays it out (the buttons' Gaiji advances) */
/* 004940E8: %B<c> is a Gaiji glyph and %% one %, as rich_line_width. */
static float original_line_width(ui_project *p,const gt4_font *face,const char *text,float scale,int left)
{
    char plain[1024];size_t n=0;float buttons=0;ui_font *gaiji=NULL;
    while(*text && *text!='\n' && *text!='\r' && n<sizeof plain-1) {
        if(button_code(text)) {
            if(!gaiji)gaiji=font(p,"Gaiji");
            const gt4_glyph *g=gaiji && gaiji->tex?gt4_font_glyph(&gaiji->face,(unsigned char)text[2]):NULL;
            if(g)buttons+=g->advance;
            text+=3;continue;
        }
        if(text[0]=='%' && text[1]=='%')text++;
        plain[n++]=*text++;
    }
    plain[n]=0;
    /* 00491418: left-aligned text measures its pen advance less half the
       space cell (+0x1A0..+0x1CC, after drawing at 0); centred and right
       text by the whole advance (00491220). The header's car name and Cr
       icon (gtmode.ad L335..L341) land as in the user's PCSX2 captures. */
    return (n?gt4_font_original_width(face,plain,scale)+(left?0:(float)(face->space_advance/2)*scale):0)+buttons*scale;
}
static float rich_line_width(ui_project *p, const gt4_font *face, const char *text, float spacing)
{
    char plain[1024];size_t n=0;float buttons=0;
    ui_font *gaiji=NULL;
    while(*text && *text!='\n' && *text!='\r' && n<sizeof plain-1) {
        if(button_code(text)) {
            if(!gaiji)gaiji=font(p,"Gaiji");
            const gt4_glyph *g=gaiji && gaiji->tex?gt4_font_glyph(&gaiji->face,(unsigned char)text[2]):NULL;
            if(g)buttons+=g->advance;
            text+=3;continue;
        }
        /* 00494198..004941C8: %% consumes two codes and draws one %. */
        if(text[0]=='%' && text[1]=='%')text++;
        plain[n++]=*text++;
    }
    plain[n]=0;
    return gt4_font_line_width(face,plain,spacing)+buttons;
}

static void draw_text_face(ui_project *p, const ui_widget *w, frame area, const char *face)
{
    const char *text = ui_widget_text(p, w);
    if (!*text) return;
    ui_font *f = font(p, face);
    if (!f || !f->tex) return;
    /* Labels share a row with a value control. Reserve the authored gutter
       even when a translation exceeds the label's nominal rectangle. */
    if(w->parent && w->name && !strcmp(w->name,"label"))
        for(int i=0;i<w->parent->nkids;i++) {
            const ui_widget *sibling=w->parent->kids[i];
            if(!strcmp(sibling->type,"SliderBar") && sibling->x>w->x)
                area.w=fminf(area.w,fmaxf(1,sibling->x-w->x-10));
        }
    float sx = w->scale_x, sy = w->scale_y;
    if (sx <= 0 || sy <= 0) return;
    /* Text style constructor 00246F58 +0x0/+0x8: default RGB is 0xD4.
       Its +0xC4 stores alignment 4 (centre/middle), not top/centre. */
    int quick_style=!strncmp(p->name,"quick-",6);
    double shade=quick_style?212:255;
    double rgba[4] = {shade, shade, shade, 255};
    mp_numbers(w->src, "text_color", rgba, 4);
    uint8_t color[4];
    for (int i = 0; i < 4; i++) color[i] = (uint8_t)rgba[i];
    if(w->has_runtime_color)memcpy(color,w->runtime_color,4);
    color[3] = (uint8_t)(color[3] * area.alpha);
    int align = (int)mp_number(w->src, "align", quick_style || !strcmp(p->name,"labomode") || !strcmp(p->name,"logger")?4:1);   /* labomode: Save Replay's "Create new file" in the item's middle (PCSX2); logger: its menu buttons' labels */
    if(w->has_runtime_align)align=w->runtime_align;
    float shadow = (float)mp_number(w->src, "shadow", 0);
    double skew[2] = {0,0}; mp_numbers(w->src,"skew",skew,2);
    /* TextFace skew is an angle; keep the baseline fixed while shearing. */
    float shear = isfinite(skew[0]) && fabs(skew[0])<80 ? (float)tan(skew[0]*0.0174532925199433) : 0;
    /* The nine align values form rows: top/middle/bottom, then left/centre/right. */
    int horizontal = align % 3, vertical = align / 3;
    int lines=1;
    for(const char *c=text;*c;c++)if(*c=='\n' || (*c=='\r' && c[1]!='\n'))lines++;
    float block_height=lines*f->face.height*sy;
    float y = area.y + f->face.baseline * sy +
              (vertical==1?(area.h-block_height)*0.5f:vertical==2?area.h-block_height:0);

    /* Rounded buttons center visible glyph ink inside the authored face.
       Font cell height includes unused ascent/descent space and is unsuitable
       for centering these short labels. */
    const ui_widget *button_bg=w->parent?ui_widget_child(w->parent,"bg"):NULL;
    const char *button_edge=button_bg?mp_string(button_bg->src,"t_image_path"):NULL;
    int dialog_message=w->parent && w->parent->name && !strcmp(w->parent->name,"Message") &&
        w->parent->parent && w->parent->parent->name && !strcmp(w->parent->parent->name,"Dialog");
    /* The course description is a single line in a short authored band.
       Center its visible ink vertically so the font descent is not clipped. */
    int course_description=w->name && !strcmp(w->name,"info") && w->parent &&
        w->parent->name && !strcmp(w->parent->name,"FG1") && w->parent->parent &&
        w->parent->parent->name && !strcmp(w->parent->parent->name,"EventCourseRoot");
    int center_ink=0;float ink_left=0,ink_right=0,ink_center=0;
    /* Arcade MakerRoot/HistoryRoot authored button children: label y=3,
       info y=20. The supplementary info is outside the button ink rectangle;
       the count must retain its separate TextFace geometry (00246F58). */
    int button_label=button_edge && strstr(button_edge,"/bt_") &&
        (!w->name || strcmp(w->name,"info")) && !strcmp(w->type,"TextFace");
    if(lines==1 && (button_label || dialog_message || course_description)) {
        float top=1e9f,bottom=-1e9f,left=1e9f,right=-1e9f,pen=0;
        const char *c=text;unsigned previous=0;
        while(*c){unsigned code=gt4_text_next(&c);if(code<32)continue;
            const gt4_glyph *g=gt4_font_glyph(&f->face,code);
            if(!g)g=gt4_font_glyph(&f->face,'?');
            float ink=g?(float)g->x:0;
            float step=g?(original_text(p)?gt4_font_original_step(&f->face,previous,code,1,&ink):g->advance+w->letter_spacing):0;
            if(g && g->w && g->h){top=fminf(top,(float)g->y);bottom=fmaxf(bottom,(float)(g->y+g->h));
                left=fminf(left,pen+ink);right=fmaxf(right,pen+ink+g->w);}
            pen+=step;previous=code==' '?0:code;}
        if(bottom>=top) {
            float body_y=course_description?area.y:area.y-w->y+(dialog_message?0:button_bg->y);
            float body_h=course_description?area.h:dialog_message?w->parent->h:(button_bg->h>0?button_bg->h:w->parent->h);
            y=body_y+(body_h-(bottom-top)*sy)*.5f-top*sy;
            if(!course_description){
                float body_x=area.x-w->x+(dialog_message?0:button_bg->x);
                float body_w=dialog_message?w->parent->w:(button_bg->w>0?button_bg->w:w->parent->w);
                center_ink=1;ink_left=left;ink_right=right;ink_center=body_x+body_w*.5f;
            }
        }
    }

    const tex1_image *a = &f->face.atlases[0];
    int original_layout=original_text(p);
    while (*text) {
        float width = original_layout?original_line_width(p,&f->face,text,sx,horizontal==0):rich_line_width(p,&f->face, text, w->letter_spacing) * sx;
        /* 00245408 +0x80/+0x100 submits text to 00490B98 with the
           authored style. The rectangle aligns it; it does not request
           adjustScale. GTAutoRoot's Wash/Oil rectangles are intentionally
           narrower than their labels (36/24 pixels, scale_x 0.95). */
        float line_sx = sx;
        float x = area.x + (horizontal == 1 ? (area.w - width) * 0.5f : horizontal == 2 ? area.w - width : 0);
        if(center_ink)x=ink_center-(ink_left+ink_right)*.5f*line_sx;
        unsigned previous=0;int bare=0;   /* the glyph after a KNJI cell with no glyph before it */
        while (*text && *text != '\n' && *text != '\r') {
            if(button_code(text)) {
                unsigned c=(unsigned char)text[2];text+=3;previous=0;
                ui_font *gaiji=font(p,"Gaiji");
                const gt4_glyph *g=gaiji && gaiji->tex?gt4_font_glyph(&gaiji->face,c):NULL;
                if(!g)continue;
                uint8_t k[4]={0x64,0x64,0x64,color[3]};
                if(c>='c' && c<='f')memcpy(k,button_rgb[c-'c'],3);
                const tex1_image *ga=&gaiji->face.atlases[0];
                if(g->w && g->h)plat_draw_glyph(gaiji->tex,x+g->x*line_sx,y+g->y*sy,g->w*line_sx,g->h*sy,y,shear,
                    (float)g->u/ga->width,(float)g->v/ga->height,(float)(g->u+g->w)/ga->width,(float)(g->v+g->h)/ga->height,k);
                x+=g->advance*line_sx;continue;
            }
            if(text[0]=='%' && text[1]=='%')text++;
            unsigned code=gt4_text_next(&text);
            if(code<32)continue;
            if(code>=GT4_KANJI_FIRST && f->face.kanji_advance) {
                /* 00492EF0 -> 00492748: the face's KNJI cell, one more
                   pixel after a glyph (context +0x78) */
                int after=previous!=0,k=kanji_glyph(p,f->face.kanji_set,code);
                float gx=x+(f->face.kanji_x+after)*line_sx,gy=y+f->face.kanji_y*sy;
                float gw=f->face.kanji_w*line_sx,gh=f->face.kanji_h*sy;
                if(k>=0) {
                    if(shadow>0) {
                        double sc[4]={0,0,0,quick_style?255:128}; mp_numbers(w->src,"shadow_color",sc,4);
                        if(w->has_runtime_shadow)for(int i=0;i<4;i++)sc[i]=w->runtime_shadow[i];
                        uint8_t dark[4]={(uint8_t)sc[0],(uint8_t)sc[1],(uint8_t)sc[2],(uint8_t)(sc[3]*area.alpha)};
                        const plat_texture *shadow_tex=sc[0]+sc[1]+sc[2]>384?p->kanji[k].tex:p->kanji[k].outline;
                        plat_draw_glyph(shadow_tex,gx+shadow,gy+shadow,gw,gh,y+shadow,shear,0,0,1,1,dark);
                    }
                    plat_draw_glyph(p->kanji[k].tex,gx,gy,gw,gh,y,shear,0,0,1,1,color);
                }
                x+=(f->face.kanji_advance+after+(original_layout?0:w->letter_spacing))*line_sx;
                /* 00492748 +0x1A4..: a closing mark next (context +0xA0)
                   ends the pair spacing */
                const char *peek=text;unsigned next=*peek?gt4_text_next(&peek):0;
                static const unsigned closing[]={0x3001,0x3002,0x300C,0x3083,0x3085,0x3087,0x3063,0xFF0D,'.',',','\''};
                if(code==0xFF63)previous=0;
                for(unsigned i=0;i<sizeof closing/sizeof *closing;i++)if(next==closing[i])previous=0;
                bare=!previous;continue;
            }
            const gt4_glyph *g = gt4_font_glyph(&f->face, code);
            if (!g) g = gt4_font_glyph(&f->face, '?');
            if (!g) continue;
            /* 00492978 +0x38C: the bearing only after a glyph */
            float ink=bare?0:g->x*line_sx;bare=0;
            float step=original_layout?gt4_font_original_step(&f->face,previous,code,line_sx,&ink):(g->advance+w->letter_spacing)*line_sx;
            if (g->w && g->h) {
                float gx = x + ink, gy = y + g->y * sy;
                float u0 = (float)g->u / a->width, v0 = (float)g->v / a->height;
                float u1 = (float)(g->u + g->w) / a->width, v1 = (float)(g->v + g->h) / a->height;
                if (shadow > 0) {
                    double sc[4]={0,0,0,quick_style?255:128}; mp_numbers(w->src,"shadow_color",sc,4);
                    if(w->has_runtime_shadow)for(int i=0;i<4;i++)sc[i]=w->runtime_shadow[i];
                    uint8_t dark[4]={(uint8_t)sc[0],(uint8_t)sc[1],(uint8_t)sc[2],(uint8_t)(sc[3]*area.alpha)};
                    /* A light drop shadow uses glyph ink, not the expanded outline. */
                    const plat_texture *shadow_tex=sc[0]+sc[1]+sc[2]>384?f->tex:f->outline;
                    plat_draw_glyph(shadow_tex, gx + shadow, gy + shadow, g->w * line_sx, g->h * sy, y + shadow, shear, u0, v0, u1, v1, dark);
                }
                plat_draw_glyph(f->tex, gx, gy, g->w * line_sx, g->h * sy, y, shear, u0, v0, u1, v1, color);
            }
            x += step;previous=code==' '?0:code;
        }
        if (*text == '\r') text++;
        if (*text == '\n') text++;
        y += f->face.height * sy;
    }
}

static void draw_text(ui_project *p, const ui_widget *w, frame area)
{
    /* multiline breaks the text at the face's own width (gtmode.mproject
       MissionRoot Item.doc, 380 wide): without it a centred description
       ran out of its card over the next one. Packed faces are wrapped to
       their pack's width first and keep it. */
    if(mp_number(w->src,"multiline",0) && w->w>0) {
        const char *source=ui_widget_source_text(p,w);
        if(*source && (!w->wrapped_source || strcmp(source,w->wrapped_source)))ui_text_wrap(p,(ui_widget *)w,w->w);
    }
    draw_text_face(p, w, area, text_font(w));
}

static clip_rect clip_to(clip_rect c, frame f);
static void set_clip(clip_rect c);

void ui_set_tooltips(ui_widget *w, const char *text)
{
    if (!w) return;
    if (!strcmp(w->type, "ToolTipFace")) {
        if (!(w->text && text && !strcmp(w->text, text))) {
            free(w->text);
            w->text = text ? strdup(text) : NULL;
            w->text_scroll = 0;
            memset(&w->tooltip_motion,0,sizeof w->tooltip_motion);
        }
    }
    for (int i = 0; i < w->nkids; i++) ui_set_tooltips(w->kids[i], text);
}

void ui_reset_tooltips(ui_widget *w)
{
    if(!w)return;
    if(!strcmp(w->type,"ToolTipFace")) {
        w->text_scroll=0;
        memset(&w->tooltip_motion,0,sizeof w->tooltip_motion);
    }
    for(int i=0;i<w->nkids;i++)ui_reset_tooltips(w->kids[i]);
}

/* The original controller separates targets and displayed coordinates and
   waits both before scrolling and before the next vertical entrance. */
static void draw_tooltip(ui_project *p, ui_widget *w, frame area, clip_rect clip)
{
    const char *text = w->text;
    if (!text || !*text) return;
    /* No font in the description: hn_14 matches the PCSX2 captures of the
       Arcade help line in style; with the default spacing the line is ~5%
       wider than captured (636 vs ~602 px), still unexplained. */
    const char *face = mp_string(w->src, "font");
    if (!face) face = "hn_14";
    ui_font *f = font(p, face);
    if (!f || !f->tex) return;
    float width = rich_line_width(p, &f->face, text, w->letter_spacing) * w->scale_x;
    gt4_tooltip_motion *motion=&w->tooltip_motion;
    if(!motion->ready || motion->width!=area.w || motion->height!=area.h || motion->text_width!=width)
        gt4_tooltip_start(motion,area.w,area.h,width,f->face.height*w->scale_y,0);
    frame line = area;
    line.x+=motion->x;line.y+=motion->y;line.w=width;
    /* The controller has already positioned/centered the line. Applying
       TextFace alignment a second time would shift it horizontally. */
    ui_widget label=*w;label.runtime_align=0;label.has_runtime_align=1;
    clip_rect c = clip_to(clip, area);
    set_clip(c);
    draw_text_face(p, &label, line, face);
    set_clip(clip);
}

static int color_list(const mp_node *w, const char *key, uint8_t out[4][4])
{
    const mp_node *l = mp_prop(w, key);
    int k = 0;
    for (int i = 0; l && i < l->count && k < 4; i++) {
        const mp_node *c = l->items[i];
        if (c->kind != MP_OBJECT || strcmp(c->name, "RGBA") || c->count < 4) continue;
        for (int j = 0; j < 4; j++) out[k][j] = (uint8_t)c->items[j]->number;
        k++;
    }
    for (int i = k; k && i < 4; i++) memcpy(out[i], out[k - 1], 4);   /* one colour -> all corners */
    return k;
}

static void apply_alpha(uint8_t c[4][4], float alpha)
{
    for (int i = 0; i < 4; i++) c[i][3] = (uint8_t)(c[i][3] * alpha);
}

static void draw_image(ui_project *p, int root, const char *path, float x, float y, float w, float h,
                       uint8_t tint[4][4])
{
    ui_image *im = image(p, root, path);
    if (!im || !im->tex) return;
    plat_draw_quad4(im->tex, x, y, w, h, 0, 0, 1, 1, tint);
}

static int is_container(const char *type)
{
    static const char *const names[] = { "Composite", "RootWindow", "VBox", "HBox", "FBox", "MBox", "ScrollBox", "SelectBox",
                                         "Button", "ScrollWindow", "ListBox", "OptionMenu", "SliderBar", "ScaleBar", "ScrollBase", "ScrollPinch", "GraphFace" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if (!strcmp(type, names[i])) return 1;
    return 0;
}

static clip_rect clip_to(clip_rect c, frame f)
{
    clip_rect r = { f.x, f.y, f.w, f.h, 1 };
    if (c.on) {
        float x1 = f.x + f.w < c.x + c.w ? f.x + f.w : c.x + c.w;
        float y1 = f.y + f.h < c.y + c.h ? f.y + f.h : c.y + c.h;
        r.x = f.x > c.x ? f.x : c.x;
        r.y = f.y > c.y ? f.y : c.y;
        r.w = x1 - r.x > 0 ? x1 - r.x : 0;
        r.h = y1 - r.y > 0 ? y1 - r.y : 0;
    }
    return r;
}

static void set_clip(clip_rect c)
{
    if (!c.on) plat_set_clip(0, 0, 0, 0);
    else if (c.w <= 0 || c.h <= 0) plat_set_clip(-10, -10, 1, 1);    /* nothing visible */
    else plat_set_clip(c.x, c.y, c.w, c.h);
}

/* Where the cursor points on the focused widget, from the game's code
   (func_00230CF8): the widget rectangle inset by 2, a point chosen by its
   cursor_align (0-8, default 8 = centre) and the arrow's tilt in degrees. */
static float strip_target(const ui_widget *w);
static float select_offset(const ui_widget *w,int vertical);
static int horizontal_strip(const ui_widget *w);
static int vertical_select(const ui_widget *w);

/* List items are painted at a virtual position rather than their stored
   x/y. Focus must use that same live position, including the eased offset. */
static void cursor_origin(const ui_widget *widget,float *x,float *y)
{
    *x=*y=0;
    for(const ui_widget *a=widget;a;a=a->parent){
        float ax=a->x,ay=a->y;
        const ui_widget *list=a->parent;
        if(list && (vertical_select(list) || horizontal_strip(list))){
            int i=0;while(i<list->nkids && list->kids[i]!=a)i++;
            if(i<list->nkids){
                float gap=(float)mp_number(list->src,"spacing",0);
                if(vertical_select(list)){
                    float offset=list->strip_ready?list->strip_offset:select_offset(list,1);
                    ax=(float)mp_number(list->src,"border_width",0);
                    ay=offset+i*(list->kids[0]->h+gap);
                }else{
                    int select=!strcmp(list->type,"SelectBox");
                    float offset=list->strip_ready?list->strip_offset:(select?select_offset(list,0):strip_target(list));
                    ax=select?offset:list->w*.5f-offset;
                    for(int j=0;j<i;j++)ax+=list->kids[j]->w+gap;
                    ay=(list->h-a->h)*.5f;
                }
            }
        }
        *x+=ax;*y+=ay;
    }
}

static void cursor_target(ui_project *p, const ui_widget *widget, float x, float y, float w, float h)
{
    /* Parked popup controls keep their focus bit while off screen. They
       must not replace the active list cursor with a clamped edge target. */
    if (p->under || p->cursor_frozen || p->cursor_held) return;
    if(p->cursor_selection_set) {
        const ui_widget *a=widget;
        while(a && a!=p->cursor_selected)a=a->parent;
        if(!a)return;
    } else if(p->cursor_visible && widget!=p->cursor_focus &&
              (x>=640 || y>=480 || x+w<=0 || y+h<=0))return;
    float l = x + 2, t = y + 2, r = x + w - 2, b = y + h - 2, cx = (l + r) * .5f, cy = (t + b) * .5f;
    static const float angle[9] = { 45, 22.5f, 0, -22.5f, -45, 45, 45, 45, -22.5f };
    const mp_node *src=widget?widget->src:NULL;
    int align = src ? (int)mp_number(src, "cursor_align", 8) : 8;
    float px[9] = { l, l, cx, r, r, r, cx, l, cx }, py[9] = { cy, b, b, b, cy, t, t, t, cy };
    if (align < 0 || align > 8) align = 3;          /* the game's default case */
    p->cursor_tx = fminf(640, fmaxf(0, px[align]));
    p->cursor_ty = fminf(480, fmaxf(0, py[align]));
    p->cursor_ta = angle[align];
    p->cursor_focus=widget;
    float origin_x=0,origin_y=0;
    cursor_origin(widget,&origin_x,&origin_y);
    p->cursor_rect_x=x-origin_x;p->cursor_rect_y=y-origin_y;
    p->cursor_rect_w=w;p->cursor_rect_h=h;
    p->cursor_visible = 1;
}

/* A horizontal card strip's scroll position: the offset of the point kept
   at the box's centre. It follows the focused card; when every card fits,
   the whole row stays centred instead (PCSX2: a maker's five cars, or a
   car's colour chips, sit centred with the focus anywhere in the row). */
static float strip_target(const ui_widget *w)
{
    int k=w->focus_index<0?0:w->focus_index>=w->nkids?w->nkids-1:w->focus_index;
    float gap=(float)mp_number(w->src,"spacing",0),total=-gap,target=w->kids[k]->w*0.5f;
    for(int i=0;i<w->nkids;i++){total+=w->kids[i]->w+gap;if(i<k)target+=w->kids[i]->w+gap;}
    return total<=w->w?total*0.5f:target;
}

/* func_002D6598: alignment 0 centres the list and permits a 0.3-cell
   overhang; alignment 1 pins a short list to spacing and a long list to
   the box edge. The authored event lists use alignment 1. */
static float select_offset(const ui_widget *w,int vertical)
{
    int n=w->nkids,k=w->focus_index<0?0:w->focus_index>=n?n-1:w->focus_index;
    float cell=vertical?w->kids[0]->h:w->kids[0]->w,gap=(float)mp_number(w->src,"spacing",0);
    float pitch=cell+gap,box=vertical?w->h:w->w;
    float magnify=(float)mp_number(w->src,"magnify_ratio",1);
    int alignment=(int)mp_number(w->src,"alignment",0);
    float total=(n-1)*pitch+cell*magnify;
    if(total<box)return alignment==0?(box-total)*.5f:alignment==1?gap:0;
    float offset=box*.5f-(k*pitch+cell*magnify*.5f);
    float edge=cell*(alignment==0?0.29999998211860657f:0);
    if(offset>edge)offset=edge;
    if(offset<box-edge-total)offset=box-edge-total;
    return offset;
}
static int horizontal_strip(const ui_widget *w)
{
    return w->nkids && ((!strcmp(w->type,"SelectBox") && (int)mp_number(w->src,"direction",0)==0) ||
        (!strcmp(w->type,"ListBox") && mp_prop(w->src,"spacing")));
}
static int vertical_select(const ui_widget *w)
{
    return w->nkids && !strcmp(w->type,"SelectBox") && (int)mp_number(w->src,"direction",0)==1;
}

static void draw_widget(ui_project *p, const ui_widget *w, frame parent, clip_rect clip, int *drawn, int *skipped)
{
    if (!w->visible) return;
    /* Console card-slot status has no device counterpart in the PC UI. */
    if(w->name && !strcmp(w->name,"Memorycard"))return;
    const mp_node *n = w->src;
    if(w->parent && w->parent->parent && (!strcmp(w->parent->parent->type,"ListBox") || !strcmp(w->parent->parent->type,"SelectBox"))) {
        const ui_widget *list=w->parent->parent;
        int mask=(int)mp_number(n,"visible_compare",0),selected=0;
        if(list->focus_index>=0 && list->focus_index<list->nkids)selected=list->kids[list->focus_index]==w->parent;
        int state=!strcmp(list->type,"SelectBox")?(selected?2:0):selected;
        if(mask && (state&mask)!=(int)mp_number(n,"visible_condition",0))return;
        /* a condition with no mask is the whole state: the face's active (1)
           and its item focused (2); labomode's item frames and their "Create
           new file"/"Overwrite" (3) only on the item under the cursor, as
           the PCSX2 Save Replay shows */
        if(!mask && mp_prop(n,"visible_condition") &&
           ((w->active || mp_number(n,"active",0)?1:0)|(selected?2:0))!=(int)mp_number(n,"visible_condition",0))return;
    }
    /* ScrollBox arrows (CORE 0x002CFBE0): inc_arrow/dec_arrow get state 1
       while the scrolled list can still move that way; their images choose
       by it (TopRoot: a static arrow at an end, a blinking one otherwise). */
    if(w->parent && w->parent->name && w->parent->parent && !strcmp(w->parent->parent->type,"ScrollBox") &&
       (!strcmp(w->parent->name,"inc_arrow") || !strcmp(w->parent->name,"dec_arrow"))) {
        const ui_widget *box=w->parent->parent,*list=NULL;
        for(int i=0;i<box->nkids && !list;i++)
            if(!strcmp(box->kids[i]->type,"SelectBox") || !strcmp(box->kids[i]->type,"ListBox"))list=box->kids[i];
        int state=0;
        if(list && list->nkids) {
            int k=list->focus_index<0?0:list->focus_index>=list->nkids?list->nkids-1:list->focus_index;
            state=!strcmp(w->parent->name,"inc_arrow")?k<list->nkids-1:k>0;
        }
        int mask=(int)mp_number(n,"visible_compare",0);
        if(mask && (state&mask)!=(int)mp_number(n,"visible_condition",0))return;
    }
    /* Button faces pick by the widget state nibble (+0x98 in CORE; setters
       0x00266060/0x00266100/0x002661A0/0x002661D8 for bits 1/2/4/8).
       GameRoot's authored faces use 4 for the focus highlight (bg/bgf, the
       latter blinking via the script) and 8 to hide a disabled icon (bg0). */
    if(w->parent && !strcmp(w->parent->type,"Button")) {
        int state=(w->parent->focused?6:0)|(w->parent->insensitive?8:0);
        int mask=(int)mp_number(n,"visible_compare",0);
        if(mask && (state&mask)!=(int)mp_number(n,"visible_condition",0))return;
    }
    /* OptionMenu decorations encode selected/focused/disabled state bits. */
    for (const ui_widget *owner = w->parent; owner; owner = owner->parent) {
        if (strcmp(owner->type, "OptionMenu")) continue;
        int mask = (int)mp_number(n, "visible_compare", 0);
        int in_popup=0;
        for(const ui_widget *a=w->parent;a && a!=owner;a=a->parent)if(a->name && !strcmp(a->name,"popup"))in_popup=1;
        /* 002C8198 +0x100 -> 00263368 creates a separate closed face
           before 002C9B68 masks popup items. Their insensitive bit must
           not leak into the retained closed value. */
        if(p->option_value==owner)in_popup=0;
        /* the closed value lit by the owner's focus only while the popup
           is shut: open, the hover belongs to the popup's row */
        int state=owner->focused && !(p->option_value==owner && owner->option_open)?2:0;
        if(in_popup){
            state=0;
            for(int k=0;k<ui_option_count(owner);k++){
                const ui_widget *item=ui_option_item(owner,k);int belongs=0;
                for(const ui_widget *a=w;a && a!=owner;a=a->parent)if(a==item){belongs=1;break;}
                if(belongs){state=(owner->option_open && k==owner->focus_index?1:0)|(owner->option_open && k==owner->option_pending?2:0)|(item->insensitive?8:0);break;}
            }
        }
        state|=owner->insensitive?8:0;
        if (mask && (state & mask) != (int)mp_number(n, "visible_condition", 0)) return;
        break;
    }
    /* Any other face: the state nibble is set on a widget and copied to its
       children (CORE 0x00265FF0 recurses), so bit 1 (script `active`) and
       bit 8 (insensitive) hold when the widget or an ancestor has them;
       bit 4 when an ancestor is focused. MakerRoot's tabs: bg / gold bga /
       grey bgi by active and insensitive. */
    int special=(w->parent && w->parent->parent && (!strcmp(w->parent->parent->type,"ListBox") ||
                 !strcmp(w->parent->parent->type,"SelectBox"))) ||
                (w->parent && !strcmp(w->parent->type,"Button")) ||
                (w->parent && w->parent->name && w->parent->parent && !strcmp(w->parent->parent->type,"ScrollBox") &&
                 (!strcmp(w->parent->name,"inc_arrow") || !strcmp(w->parent->name,"dec_arrow")));
    for (const ui_widget *owner = w->parent; owner && !special; owner = owner->parent)
        if (!strcmp(owner->type, "OptionMenu")) special = 1;
    if (!special && mp_prop(n, "visible_compare")) {
        int state = 0;
        for (const ui_widget *a = w; a; a = a->parent) {
            if (a->active) state |= 1;
            if (a->insensitive) state |= 8;
            if (a != w && a->focused) state |= 6;
        }
        if ((state & (int)mp_number(n, "visible_compare", 0)) != (int)mp_number(n, "visible_condition", 0)) return;
    }
    ((ui_widget *)w)->drawn_tick = p->draw_tick;
    int root = w->resource_root;
    frame f = { parent.x + w->x, parent.y + w->y, w->w, w->h, parent.alpha * w->opacity };

    uint8_t col[4][4] = { { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 } };
    const char *type = w->type;

    if (!strcmp(type,"SlideShowFace") || (!strcmp(type,"PhotoRenderFace") && p->draw_album)) {
        /* MSlideShowFace 001AAC98, authored fullscreen widget. */
        if(p->draw_album && p->draw_album(p->album_user,w,f.x,f.y,w->has_geometry?f.w:640,w->has_geometry?f.h:480,f.alpha))(*drawn)++;
        else (*skipped)++;
        set_clip(clip);
    } else if (!strcmp(type,"PhotoMapWindow") || !strcmp(type,"PhotoViewFace") || !strcmp(type,"PhotoRenderFace")) {
        /* photo_shoot.mproject PM/PV/PR: native surfaces in authored
           order; MPhotoMapWindow draw 00194470/00194DB0. */
        if(p->draw_photo && p->draw_photo(p->photo_user,w,f.x,f.y,w->has_geometry?f.w:640,w->has_geometry?f.h:480,f.alpha))(*drawn)++;
        else (*skipped)++;
        set_clip(clip);
    } else if (!strcmp(type,"LoggerFace") || !strcmp(type,"RaceCourseMapFace")) {
        /* logger.mproject: the Logger's graph (001BC1E0 -> 00120CF0) and
           its course map (00128B68), drawn by their owner */
        if(p->draw_native && p->draw_native(p->native_user,w,f.x,f.y,f.w,f.h,f.alpha))(*drawn)++;
        else (*skipped)++;
        set_clip(clip);
    } else if (!strcmp(type,"SceneViewFace")) {
        /* 0012BC18 draws the installed conductor in widget order. A default
           face is the full 640x480 render context (0012BC18 +0x40..+0x60). */
        if(p->draw_scene && p->draw_scene(p->scene_user,f.x,f.y,w->has_geometry?f.w:640,w->has_geometry?f.h:480,f.alpha))(*drawn)++;
        else (*skipped)++;
        set_clip(clip);
    } else if (!strcmp(type,"CarFace")) {
        ui_car_prepare_draw(w);
        if(p->draw_car && w->car_code && *w->car_code){p->draw_car(p->car_user,w,f.x,f.y,w->has_geometry?f.w:640,w->has_geometry?f.h:480,f.alpha);(*drawn)++;}
    } else if (!strcmp(type,"ModelFace")) {
        /* Offscreen map icons must not clear another widget's depth area. */
        if(f.x+f.w>0 && f.y+f.h>0 && f.x<640 && f.y<480 && f.alpha>0) {
            void *mesh=model_mesh(p,root,w->model_path?w->model_path:mp_string(n,"model_set_path"));
            if(mesh && p->models.draw){p->models.draw(mesh,n,w->model_angles,f.x,f.y,f.w,f.h,model_environment(p,root,n));set_clip(clip);(*drawn)++;}
            else (*skipped)++;
        }
    } else if (!strcmp(type,"FlashFace")) {
        ui_flash *fl=flash(p,root,w->flash_path);
        if(fl && fl->textures && fl->clip.nframes) {
            const gt4_strobe *s=&fl->clip;const gt4_strobe_frame *sf=&s->frames[gt4_strobe_frame_at(s,w->flash_time)];
            float sx=f.w/s->width,sy=f.h/s->height;
            for(unsigned i=0;i<sf->count;i++) {
                const gt4_strobe_quad *q=&s->quads[sf->first+i];uint8_t color[4];memcpy(color,q->color,4);
                color[3]=(uint8_t)(color[3]*f.alpha);
                plat_draw_quad(fl->textures[q->bitmap],f.x+q->x*sx,f.y+q->y*sy,q->w*sx,q->h*sy,q->u0,q->v0,q->u1,q->v1,color);
            }
            (*drawn)++;
        }else (*skipped)++;
    } else if (!strcmp(type,"MovieFace")) {
        if(w->movie_name && *w->movie_name && p->draw_movie &&
           p->draw_movie(p->movie_user,w->movie_name,f.x,f.y,w->has_geometry?f.w:640,w->has_geometry?f.h:480,f.alpha,
                         (int)mp_number(n,"interlace",1),(int)mp_number(n,"repeat",1),w->movie_audio_track))(*drawn)++;
        else (*skipped)++;
    } else if (!strcmp(type, "TextFace")) {
        /* 00247448 +0xD8..+0x114 installs the face's authored scissor. */
        if(!strncmp(p->name,"quick-",6))set_clip(clip_to(clip,f));
        draw_text(p, w, f);
        set_clip(clip);
        (*drawn)++;
    } else if (!strcmp(type, "InputTextFace")) {
        /* the entry's text as a TextFace, and the caret: a bar of the text's
           colour at the insertion point, the font's height (A: its blink
           and width not read; the PCSX2 print shows it lit after the text) */
        ui_font *ft=font(p,text_font(w));
        if(w->text && *w->text)draw_text(p,w,f);
        if(ft && ft->tex) {
            double rgba[4]={255,255,255,255};uint8_t c[4][4];
            int align=(int)mp_number(n,"align",4),horizontal=align%3;
            char head[128];snprintf(head,sizeof head,"%.*s",w->caret,w->text?w->text:"");
            float whole=w->text?rich_line_width(p,&ft->face,w->text,w->letter_spacing)*w->scale_x:0;
            float at=rich_line_width(p,&ft->face,head,w->letter_spacing)*w->scale_x;
            float x0=horizontal==0?f.x:horizontal==1?f.x+(f.w-whole)*.5f:f.x+f.w-whole;
            float hgt=f.h,y0=f.y;   /* the field's height, as PCSX2 shows it */
            mp_numbers(n,"text_color",rgba,4);
            for(int k=0;k<4;k++){c[k][0]=(uint8_t)rgba[0];c[k][1]=(uint8_t)rgba[1];c[k][2]=(uint8_t)rgba[2];c[k][3]=(uint8_t)(rgba[3]*f.alpha);}
            plat_draw_quad4(NULL,x0+at+1,y0,2,hgt,0,0,1,1,c);
        }
        (*drawn)++;
    } else if (!strcmp(type, "ToolTipFace")) {
        draw_tooltip(p, (ui_widget *)w, f, clip);
        (*drawn)++;
    } else if (!strcmp(type, "ProgressFace")) {
        float fill=fminf(1,fmaxf(0,w->progress));
        color_list(n,"color",col);
        if(w->has_runtime_color)for(int i=0;i<4;i++)memcpy(col[i],w->runtime_color,4);
        apply_alpha(col,f.alpha);
        ui_image *im=w->image_path?image(p,root,w->image_path):NULL;
        plat_draw_quad4(im?im->tex:NULL,f.x,f.y,f.w*fill,f.h,0,0,fill,1,col);
        (*drawn)++;
    } else if (!strcmp(type, "ColorChipFace")) {
        /* CarRoot's paint chips: the SpecDB chip colour (VARIATION +36:
           r, g, b, then 3 = metallic / 1 = solid; GS scale, 128 = full)
           fills the chip, and image_path0 (metallic) or image_path1
           (solid), white with a low alpha, adds its sheen on top. */
        const char *path = w->chip_kind ? mp_string(n, w->chip_kind == 1 ? "image_path0" : "image_path1") : NULL;
        if (w->chip_kind && w->chip_band_count > 1) {
            /* a grid's chip (the quick menus): the paint's colours, VARIATION
               +0x24.. those marked used, in bands from the top */
            float band = f.h / (float)w->chip_band_count;
            for (int b = 0; b < w->chip_band_count; b++) {
                for (int i = 0; i < 4; i++) {
                    for (int k = 0; k < 3; k++) col[i][k] = (uint8_t)(w->chip_bands[b][k] > 127 ? 255 : w->chip_bands[b][k] * 2);
                    col[i][3] = 255;
                }
                apply_alpha(col, f.alpha);
                plat_draw_quad4(NULL, f.x, f.y + band * (float)b, f.w, band, 0, 0, 1, 1, col);
            }
            uint8_t sheen[4][4] = { { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 } };
            apply_alpha(sheen, f.alpha);
            if (path && *path) draw_image(p, root, path, f.x, f.y, f.w, f.h, sheen);
            (*drawn)++;
        } else if (w->chip_kind) {
            for (int i = 0; i < 4; i++) {
                for (int k = 0; k < 3; k++) col[i][k] = (uint8_t)(w->chip_rgb[k] > 127 ? 255 : w->chip_rgb[k] * 2);
                col[i][3] = 255;
            }
            if(w->has_runtime_color)for(int i=0;i<4;i++)memcpy(col[i],w->runtime_color,4);
            apply_alpha(col, f.alpha);
            plat_draw_quad4(NULL, f.x, f.y, f.w, f.h, 0, 0, 1, 1, col);
            uint8_t sheen[4][4] = { { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 } };
            apply_alpha(sheen, f.alpha);
            if (path && *path) draw_image(p, root, path, f.x, f.y, f.w, f.h, sheen);
            (*drawn)++;
        } else (*skipped)++;
    } else if (!strcmp(type, "ColorFace")) {
        if (color_list(n, "color", col) || w->has_runtime_color) {
            if(w->has_runtime_color)for(int i=0;i<4;i++)memcpy(col[i],w->runtime_color,4);
            apply_alpha(col, f.alpha);
            plat_draw_quad4(NULL, f.x, f.y, f.w, f.h, 0, 0, 1, 1, col);
        }
        (*drawn)++;
    } else if (!strcmp(type, "ImageFace")) {
        if (w->image_path && *w->image_path) {
            color_list(n, "color", col);
            if(w->has_runtime_color)for(int i=0;i<4;i++)memcpy(col[i],w->runtime_color,4);
            apply_alpha(col, f.alpha);
            ui_image *im = image(p, root, w->image_path);
            double s[2] = { 1, 1 };
            mp_numbers(n, "scale", s, 2);
            /* 002A2608 +0x7C..+0xC0: ImageFace quad uses the current client
               width/height (00231420/00231440), including inherited geometry.
               Quick-menu icons inherit their authored 72x68 button rectangle.
               Other menus retain their existing layout until their area is checked. */
            int client_size = w->has_geometry || !strncmp(p->name,"quick-",6) || !strcmp(p->name,"labomode");   /* labomode's folder_bg (107x10) fills its 107x480 */
            /* 002A2608 uses inherited client dimensions as well. Authored
               GT Mode card backgrounds must not draw at PNG source size. */
            if(!strcmp(p->name,"gtmode"))for(const ui_widget *ancestor=w->parent;ancestor;ancestor=ancestor->parent)
                if(ancestor->name && !strcmp(ancestor->name,"Ticket")){client_size=1;break;}
            /* The Tune Shop's tuner logo (378x135 .img) fills its 340x121.43
               Logo box: 0.9 of the source, as the user's PCSX2 captures. */
            if(!strcmp(p->name,"gtmode") && w->parent && w->parent->name && !strcmp(w->parent->name,"Logo"))client_size=1;
            /* The system dialogs: ProgressDialog's 280x24 Bar takes its
               bar image across (ConfirmDialog's caution is already 43x40). */
            if(!strcmp(p->name,"cursor"))client_size=1;
            float iw = client_size ? f.w : (float)(im ? im->w : 0);
            float ih = client_size ? f.h : (float)(im ? im->h : 0);
            /* The manufacturer sidebar fills Nation's authored height,
               including the next-flag preview under the footer gradient. */
            if(!w->has_geometry && w->name && !strcmp(w->name,"bg0") &&
               w->parent && w->parent->name && !strcmp(w->parent->name,"Nation") &&
               w->parent->parent && w->parent->parent->name &&
               !strcmp(w->parent->parent->name,"MakerRoot"))ih=f.h;
            if (w->magnify > 0 && w->magnify != 1) {
                f.x -= iw * (w->magnify - 1) * 0.5f; f.y -= ih * (w->magnify - 1) * 0.5f;
                iw *= w->magnify; ih *= w->magnify;
            }
            /* `scale` shrinks/grows about the image's centre: TopRoot's 0.9
               logo stays centred in its 400x240 frame on the PS2. */
            f.x += iw * (1 - (float)s[0]) * 0.5f; f.y += ih * (1 - (float)s[1]) * 0.5f;
            if(im && im->tex && w->parent && w->parent->rotation!=0)
                plat_draw_rotated(im->tex,f.x,f.y,iw*(float)s[0],ih*(float)s[1],
                    parent.x+w->parent->w*.5f,parent.y+w->parent->h*.5f,w->parent->rotation,col[0]);
            else {
                double slices[4]={0,0,0,0};
                if(im && im->tex && mp_numbers(n,"slice",slices,4)==4 && im->w>0 && im->h>0) {
                    float dw=iw*(float)s[0],dh=ih*(float)s[1];
                    float l=fminf(fmaxf((float)slices[0],0),fminf(dw*.5f,im->w*.5f));
                    float t=fminf(fmaxf((float)slices[1],0),fminf(dh*.5f,im->h*.5f));
                    float r=fminf(fmaxf((float)slices[2],0),fminf(dw*.5f,im->w*.5f));
                    float b=fminf(fmaxf((float)slices[3],0),fminf(dh*.5f,im->h*.5f));
                    float xx[4]={f.x,f.x+l,f.x+dw-r,f.x+dw};
                    float yy[4]={f.y,f.y+t,f.y+dh-b,f.y+dh};
                    float uu[4]={0,l/im->w,1-r/im->w,1};
                    float vv[4]={0,t/im->h,1-b/im->h,1};
                    for(int row=0;row<3;row++)for(int column=0;column<3;column++)
                        if(xx[column+1]>xx[column] && yy[row+1]>yy[row])
                            plat_draw_quad4(im->tex,xx[column],yy[row],xx[column+1]-xx[column],yy[row+1]-yy[row],uu[column],vv[row],uu[column+1],vv[row+1],col);
                } else draw_image(p, root, w->image_path, f.x, f.y, iw*(float)s[0], ih*(float)s[1], col);
            }
            (*drawn)++;
        }
    } else if (!strcmp(type, "FrameImageFace")) {
        /* 9-slice frame: corners at natural size, edges stretched between them. */
        const char *tl = mp_string(n, "tl_image_path"), *tr = mp_string(n, "tr_image_path");
        const char *bl = mp_string(n, "bl_image_path"), *br = mp_string(n, "br_image_path");
        const char *t = mp_string(n, "t_image_path"), *b = mp_string(n, "b_image_path");
        /* image() can grow the cache: copy dimensions before the next lookup. */
        ui_image *corner = tl ? image(p, root, tl) : NULL;
        float cw = corner && corner->tex ? (float)corner->w : 0;
        float ch = corner && corner->tex ? (float)corner->h : 0;
        corner = bl ? image(p, root, bl) : NULL;
        float bh = corner && corner->tex ? (float)corner->h : ch;
        corner = tr ? image(p, root, tr) : NULL;
        float rw = corner && corner->tex ? (float)corner->w : cw;
        float fw = (float)mp_number(n, "frame_width", cw);
        float fh = (float)mp_number(n, "frame_height", ch);
        cw = rw = fminf(fmaxf(fw, 0), f.w * .5f);
        ch = bh = fminf(fmaxf(fh, 0), f.h * .5f);
        const char *middle = mp_string(n, "image_path");
        const char *left = mp_string(n, "l_image_path"), *right = mp_string(n, "r_image_path");
        color_list(n, "color", col);
        if(w->has_runtime_color)for(int i=0;i<4;i++)memcpy(col[i],w->runtime_color,4);
        apply_alpha(col, f.alpha);
        if (middle) draw_image(p, root, middle, f.x + cw, f.y + ch, f.w - cw - rw, f.h - ch - bh, col);
        if (left) draw_image(p, root, left, f.x, f.y + ch, cw, f.h - ch - bh, col);
        if (right) draw_image(p, root, right, f.x + f.w - rw, f.y + ch, rw, f.h - ch - bh, col);
        if (tl) draw_image(p, root, tl, f.x, f.y, cw, ch, col);
        if (tr) draw_image(p, root, tr, f.x + f.w - rw, f.y, rw, ch, col);
        if (bl) draw_image(p, root, bl, f.x, f.y + f.h - bh, cw, bh, col);
        if (br) draw_image(p, root, br, f.x + f.w - rw, f.y + f.h - bh, rw, bh, col);
        if (t) draw_image(p, root, t, f.x + cw, f.y, f.w - cw - rw, ch, col);
        if (b) draw_image(p, root, b, f.x + cw, f.y + f.h - bh, f.w - cw - rw, bh, col);
        (*drawn)++;
    } else if (is_container(type)) {
        /* containers: only their children draw */
    } else {
        (*skipped)++;          /* TextFace, MovieFace, ToolTipFace, ... not yet */
        return;
    }

    /* Use authored clipping: MakerRoot intentionally previews the next
       country flag beneath the current country's column. */
    int clips = mp_number(n, "clip", 0) != 0;
    clip_rect inner = clips ? clip_to(clip, f) : clip;
    if (clips) set_clip(inner);
    if(!strcmp(type,"GraphFace")) {
        /* 00148B60/00148940 supply normalized points; 0029F798 stores
           256 float pairs. Authored GraphFace geometry, colour and width
           come from the project/script. A: PC stroke rasterization and
           coordinate composition still need comparison with GS drawing. */
        float width=w->graph_line_width;
        /* 0029E9D0 +0x14..0x28: MGraphFace ctor stores 2.0 at +BC. */
        if(!(width>0))width=(float)mp_number(n,"line_width",2);
        const char *kind=mp_string(n,"type");/* 0029EA04: default +B8=1 draws independent endpoint pairs. */
        int segments=w->graph_type_set?w->graph_segments:(!kind || !strcmp(kind,"lines"));
        color_list(n,"color",col);uint8_t color[4];memcpy(color,col[0],4);if(w->has_runtime_color)memcpy(color,w->runtime_color,4);color[3]=(uint8_t)(color[3]*f.alpha);
        int step=segments?2:1;
        for(int i=0;i+1<w->graph_count;i+=step){
            float x0=f.x+w->graph_points[i][0]*f.w,y0=f.y+(1-w->graph_points[i][1])*f.h;
            float x1=f.x+w->graph_points[i+1][0]*f.w,y1=f.y+(1-w->graph_points[i+1][1])*f.h;
            float dx=x1-x0,dy=y1-y0,length=sqrtf(dx*dx+dy*dy);if(!(length>0) || !(width>0))continue;
            float ox=-dy/length*width*.5f,oy=dx/length*width*.5f;
            float xy[4][2]={{x0+ox,y0+oy},{x1+ox,y1+oy},{x0-ox,y0-oy},{x1-ox,y1-oy}};
            plat_draw_solid_quad(xy,color);
        }
        (*drawn)++;
    } else if(!strcmp(type,"SliderBar") || !strcmp(type,"ScaleBar")) {
        ui_widget *popup=ui_widget_child(w,"popup");
        if(popup){double range[2]={0,100};mp_numbers(w->src,"range",range,2);
            if(w->slider_range_set){range[0]=w->slider_min;range[1]=w->slider_max;}
            ui_widget *pinch=ui_widget_child(popup,"pinch");
            if(!strcmp(type,"ScaleBar") && !w->slider_range_set){range[0]=w->photo_min;range[1]=w->photo_max;}
            float value=!strcmp(type,"ScaleBar") && !w->slider_range_set?w->photo_value:(float)w->focus_index;
            float ratio=range[1]>range[0]?(float)((value-range[0])/(range[1]-range[0])):0;
            if(pinch)pinch->x=fminf(1,fmaxf(0,ratio))*(popup->w-pinch->w);}
        for(int i=0;i<w->nkids;i++) {
            if(w->kids[i]==popup && !w->option_open)continue;
            draw_widget(p,w->kids[i],f,inner,drawn,skipped);
        }
        set_clip(clip);
    } else if (!strcmp(type, "OptionMenu")) {
        if(w->option_open && w->focused && !p->under){p->option_popup=w;p->option_x=f.x;p->option_y=f.y;p->option_alpha=f.alpha;}
        /* Closed dropdown: backgrounds plus one value, not the popup list. */
        inner = clip_to(clip, f);
        set_clip(inner);
        for (int i = 0; i < w->nkids; i++)
            if (!w->kids[i]->name || strcmp(w->kids[i]->name, "popup"))
                draw_widget(p, w->kids[i], f, inner, drawn, skipped);
        const ui_widget *item=ui_option_item(w,w->focus_index);
        if(item) {
            frame base=f;base.x-=item->x;base.y-=item->y;
            const ui_widget *previous=p->option_value;p->option_value=w;
            draw_widget(p,item,base,inner,drawn,skipped);
            p->option_value=previous;
        }
        set_clip(clip);
    } else if (!strcmp(type, "SelectBox") || !strcmp(type,"ListBox")) {
        /* Authored spacing identifies a horizontal card strip. Keep the
           selected card centred and clip neighbouring cards to the viewport. */
        if (horizontal_strip(w)) {
            int k=w->focus_index<0?0:w->focus_index>=w->nkids?w->nkids-1:w->focus_index;
            float gap=(float)mp_number(n,"spacing",0);
            int select=!strcmp(type,"SelectBox");
            float target=select?select_offset(w,0):strip_target(w);
            float offset=w->strip_ready?w->strip_offset:target;
            float x=select?f.x+offset:f.x+f.w*0.5f-offset;
            clip_rect strip=clip_to(inner,f);set_clip(strip);
            for(int i=0;i<w->nkids;i++) {
                const ui_widget *it=w->kids[i];
                if(x+it->w>f.x && x<f.x+f.w) {
                    frame c=f;c.x=x-it->x;c.y+=(f.h-it->h)*0.5f-it->y;
                    float magnify=(float)mp_number(n,"magnify_ratio",1);
                    if(i==k && magnify>1 && it->nkids) {
                        /* Miniature car templates are flat image composites. */
                        for(int j=0;j<it->nkids;j++) {
                            ui_widget child=*it->kids[j];
                            if(!child.has_geometry && child.image_path) {
                                ui_image *im=image(p,root,child.image_path);
                                if(im){child.w=(float)im->w;child.h=(float)im->h;child.has_geometry=1;}
                            }
                            child.x=child.x*magnify-it->w*(magnify-1)*.5f;
                            child.y=child.y*magnify-it->h*(magnify-1);
                            child.w*=magnify;child.h*=magnify;
                            frame base=c;base.x+=it->x;base.y+=it->y;base.alpha*=it->opacity;
                            draw_widget(p,&child,base,strip,drawn,skipped);
                        }
                    } else draw_widget(p,it,c,strip,drawn,skipped);
                    if(i==k && f.alpha>0 && (w->focused || it->focused)) cursor_target(p,it,x,c.y+it->y,it->w,it->h);
                }
                x+=it->w+gap;
            }
            set_clip(inner);
        } else if (vertical_select(w)) {
            /* CORE 0x002D69C0/0x002D6B40: items stacked from the scroll offset
               at x = border_width; those within box/pitch of the focus are
               drawn, the focused one last. Faded neighbours come from the
               scripts' own actors (TopRoot.setButtonActor). */
            int k=w->focus_index<0?0:w->focus_index>=w->nkids?w->nkids-1:w->focus_index;
            float pitch=w->kids[0]->h+(float)mp_number(n,"spacing",0);
            float offset=w->strip_ready?w->strip_offset:select_offset(w,1);
            int reach=pitch>0?(int)(f.h/pitch):0;
            float x=f.x+(float)mp_number(n,"border_width",0);
            for(int pass=0;pass<2;pass++)
                for(int i=k-reach;i<=k+reach;i++) {
                    if(i<0 || i>=w->nkids || (i==k)!=(pass==1))continue;
                    const ui_widget *it=w->kids[i];
                    frame c=f;c.x=x-it->x;c.y=f.y+offset+i*pitch-it->y;
                    draw_widget(p,it,c,inner,drawn,skipped);
                    if(i==k && f.alpha>0 && (w->focused || it->focused))
                        cursor_target(p,it,x,c.y+it->y,it->w,it->h);
                }
        } else if (!strcmp(type,"ListBox") && (int)mp_number(n,"direction",0)==1 && w->nkids > 0) {
            /* a vertical ListBox (labomode.ad's replay and photo lists):
               items stacked from its top at their own height, scrolled so
               the focused one stays within it */
            int k=w->focus_index<0?0:w->focus_index>=w->nkids?w->nkids-1:w->focus_index;
            float pitch=w->kids[0]->h+(float)mp_number(n,"spacing",0),top=0;
            if(pitch>0 && (k+1)*pitch>f.h)top=(k+1)*pitch-f.h;
            for(int i=0;i<w->nkids;i++) {
                const ui_widget *it=w->kids[i];
                float y=f.y+i*pitch-top;
                if(y+pitch<=f.y || y>=f.y+f.h)continue;
                frame c=f;c.x=f.x-it->x;c.y=y-it->y;
                draw_widget(p,it,c,inner,drawn,skipped);
                if(i==k && f.alpha>0 && (w->focused || it->focused))cursor_target(p,it,f.x,y,it->w,it->h);
            }
        } else if (w->nkids > 0) {
            int k = w->focus_index < 0 ? 0 : w->focus_index >= w->nkids ? w->nkids - 1 : w->focus_index;
            const ui_widget *it = w->kids[k];
            frame c = f;
            c.x += (f.w - it->w) * 0.5f - it->x;
            c.y += (f.h - it->h) * 0.5f - it->y;
            draw_widget(p, it, c, inner, drawn, skipped);
        }
    } else {
        for (int i = 0; i < w->nkids; i++)
            draw_widget(p, w->kids[i], f, inner, drawn, skipped);
    }
    if (f.alpha>0 && w->focused && strcmp(type,"RootWindow") && strcmp(type,"ListBox") &&
        strcmp(type,"SelectBox") && (!w->parent || strcmp(w->parent->type,"SelectBox"))) {
        cursor_target(p,w,f.x,f.y,f.w,f.h);
    }
    if (clips) set_clip(clip);
}

/* FBox keeps child positions and grows around packed contents. Decorative
   faces without geometry fill the result, rather than contributing their old
   inherited size to it. Text changes are measured before drawing that frame. */
/* MMBox grid, func_002B82E8 (two columns by default), 002B85B0
   (requested size), 002B8778 (allocation). Column/row sizes are maxima;
   surplus or deficit is shared only by columns/rows requesting expansion.
   Full layout still needs an oracle covering nested text packing. */
static void layout_grid(ui_widget *w)
{
    if(!w->nkids)return;
    int columns=(int)mp_number(w->src,"column",2);
    if(columns<1 || columns>100)return;
    int count=0;
    for(int i=0;i<w->nkids;i++)if(w->kids[i]->visible && mp_number(w->kids[i]->src,"packing",0))count++;
    if(!count)return;
    int rows=(count+columns-1)/columns;
    float *sizes=calloc((size_t)(columns+rows),sizeof *sizes);
    unsigned char *expand=calloc((size_t)(columns+rows),1);
    if(!sizes || !expand){free(sizes);free(expand);return;}
    int k=0;
    for(int i=0;i<w->nkids;i++){
        ui_widget *c=w->kids[i];if(!c->visible || !mp_number(c->src,"packing",0))continue;
        int col=k%columns,row=columns+k/columns;k++;
        sizes[col]=fmaxf(sizes[col],c->w);sizes[row]=fmaxf(sizes[row],c->h);
        expand[col]|=mp_number(c->src,"pack_expand_x",0)!=0;
        expand[row]|=mp_number(c->src,"pack_expand_y",0)!=0;
    }
    float cg=(float)mp_number(w->src,"column_spacing",0),rg=(float)mp_number(w->src,"row_spacing",0);
    float pl=(float)mp_number(w->src,"pad_left",0),pr=(float)mp_number(w->src,"pad_right",0);
    float pt=(float)mp_number(w->src,"pad_top",0),pb=(float)mp_number(w->src,"pad_bottom",0);
    float width=pl+pr+(columns-1)*cg,height=pt+pb+(rows-1)*rg;int nx=0,ny=0;
    for(int i=0;i<columns;i++){width+=sizes[i];nx+=expand[i];}
    for(int i=0;i<rows;i++){height+=sizes[columns+i];ny+=expand[columns+i];}
    w->w=fmaxf(w->w,(float)mp_number(w->src,"minimum_width",0));
    w->h=fmaxf(height,(float)mp_number(w->src,"minimum_height",0));
    float dx=nx?(w->w-width)/nx:0,dy=ny?(w->h-height)/ny:0;
    for(int i=0;i<columns;i++)if(expand[i])sizes[i]+=dx;
    for(int i=0;i<rows;i++)if(expand[columns+i])sizes[columns+i]+=dy;
    k=0;float y=pt;
    for(int i=0;i<w->nkids;i++){
        ui_widget *c=w->kids[i];if(!c->visible || !mp_number(c->src,"packing",0))continue;
        int col=k%columns,row=columns+k/columns;k++;
        float x=pl;for(int j=0;j<col;j++)x+=sizes[j]+cg;
        float cw=c->w,ch=c->h;
        if(expand[col] && mp_number(c->src,"pack_fill_x",0))c->w=sizes[col];
        if(expand[row] && mp_number(c->src,"pack_fill_y",0))c->h=sizes[row];
        c->x=x+(expand[col] && !mp_number(c->src,"pack_fill_x",0)?(sizes[col]-cw)*.5f:0);
        c->y=y+(expand[row] && !mp_number(c->src,"pack_fill_y",0)?(sizes[row]-ch)*.5f:0);
        if(col==columns-1)y+=sizes[row]+rg;
    }
    free(expand);free(sizes);
}
static int is_face(const ui_widget *w){size_t n=strlen(w->type);return n>=4 && !strcmp(w->type+n-4,"Face");}
/* is_face is authored only as 0, on faces packed like boxes: a Face covers
   its box, a Composite (ProgressDialog's Bar) is packed */
static int packs_as_face(const ui_widget *c){return mp_number(c->src,"is_face",is_face(c))!=0;}
static void allocate_fbox(ui_project *p,ui_widget *w)
{
    if(strcmp(w->type,"FBox") || !mp_number(w->src,"packing",0))return;
    float left=(float)mp_number(w->src,"pad_left",0),top=(float)mp_number(w->src,"pad_top",0);
    float width=w->w-left-(float)mp_number(w->src,"pad_right",0);
    float height=w->h-top-(float)mp_number(w->src,"pad_bottom",0);
    for(int i=0;i<w->nkids;i++){
        ui_widget *c=w->kids[i];
        if(packs_as_face(c)){
            if(!c->has_geometry || mp_number(c->src,"packing",0)){c->x=c->y=0;c->w=w->w;c->h=w->h;}
            continue;
        }
        if(!mp_number(c->src,"packing",0))continue;
        c->x=left;c->y=top;
        if(mp_number(c->src,"pack_fill_x",0))c->w=width;
        if(mp_number(c->src,"pack_fill_y",0))c->h=height;
        /* expanding without filling: centred in the space, as authored
           (cursor.mproject ProgressDialog: Bar 280 at 12 and OK 140 at 82
           in 304) */
        if(mp_number(c->src,"pack_expand_x",0) && !mp_number(c->src,"pack_fill_x",0))c->x=left+(width-c->w)*.5f;
        if(mp_number(c->src,"pack_expand_y",0) && !mp_number(c->src,"pack_fill_y",0))c->y=top+(height-c->h)*.5f;
        if(!strcmp(c->type,"TextFace") && mp_number(c->src,"multiline",0) && mp_number(c->src,"pack_fill_x",0))ui_text_wrap(p,c,width);
    }
}
static void layout_packed(ui_project *p,ui_widget *w)
{
    for(int i=0;i<w->nkids;i++)layout_packed(p,w->kids[i]);
    /* A packing VBox stacks its visible packing children and takes their
       height: the arcade's SettingPopup (Title 24, rows of 32, Button 38)
       closes up over the hidden Difficulty/Open rows and comes out 158
       high on the PS2, below its authored minimum of 222. Faces (its
       shade; the option screens' frames, marked packing too) do not
       stack: they cover the result. */
    if(!strcmp(w->type,"VBox") && mp_number(w->src,"packing",0)){
        float y=(float)mp_number(w->src,"pad_top",0),gap=(float)mp_number(w->src,"spacing",0);int n=0;
        for(int i=0;i<w->nkids;i++){
            ui_widget *c=w->kids[i];if(!c->visible || !mp_number(c->src,"packing",0) || is_face(c))continue;
            c->y=y;y+=c->h+gap;n++;
        }
        if(!n)return;
        double authored[4];int sized=mp_numbers(w->src,"geometry",authored,4)==4;
        w->h=y-gap+(float)mp_number(w->src,"pad_bottom",0);
        for(int i=0;i<w->nkids;i++){   /* keeping their authored overhang */
            ui_widget *c=w->kids[i];double g[4];
            if(sized && (is_face(c) || !mp_number(c->src,"packing",0)) && mp_numbers(c->src,"geometry",g,4)==4)c->h=fmaxf(0,(float)(g[3]+w->h-authored[3]));
        }
        return;
    }
    if(!strcmp(w->type,"MBox") && mp_number(w->src,"packing",0)){
        layout_grid(w);for(int i=0;i<w->nkids;i++)allocate_fbox(p,w->kids[i]);return;
    }
    if(strcmp(w->type,"FBox") || !mp_number(w->src,"packing",0))return;
    float width=(float)mp_number(w->src,"minimum_width",0),height=(float)mp_number(w->src,"minimum_height",0);
    float horizontal=(float)mp_number(w->src,"pad_left",0)+(float)mp_number(w->src,"pad_right",0);
    float vertical=(float)mp_number(w->src,"pad_top",0)+(float)mp_number(w->src,"pad_bottom",0);
    for(int i=0;i<w->nkids;i++){
        ui_widget *c=w->kids[i];if(!c->visible || packs_as_face(c))continue;
        float size[2]={c->w,c->h};
        if(!strcmp(c->type,"TextFace")){
            if(mp_number(c->src,"multiline",0) && mp_number(c->src,"pack_fill_x",0) && width>horizontal)ui_text_wrap(p,c,width-horizontal);
            ui_text_size(p,c,size);
            c->w=fmaxf(size[0],(float)mp_number(c->src,"minimum_width",0));
            c->h=fmaxf(size[1],(float)mp_number(c->src,"minimum_height",0));
        }
        width=fmaxf(width,c->w+horizontal);height=fmaxf(height,c->h+vertical);
    }
    w->w=width;w->h=height;allocate_fbox(p,w);
}

void ui_draw_root(ui_project *p, int i, int *drawn, int *skipped)
{
    frame screen = { 0, 0, 640, 480, 1.0f };
    clip_rect none = { 0, 0, 0, 0, 0 };
    int d = 0, s = 0;
    ui_widget *w = ui_root_widget(p, i);
    p->cursor_visible=0;
    p->option_popup=NULL;
    if(!++p->draw_tick)p->draw_tick=1;
    if (w) {layout_packed(p,w);draw_widget(p, w, screen, none, &d, &s);}
    plat_set_clip(0, 0, 0, 0);
    if(p->option_popup) {
        const ui_widget *option=p->option_popup;
        int indices[512],count=0,selected_index=0,anchor_index=0;
        for(int k=0;k<ui_option_count(option) && count<512;k++) {
            if(!ui_option_item(option,k)->visible)continue;
            if(k==option->option_pending)selected_index=count;
            if(k==option->focus_index)anchor_index=count;
            indices[count++]=k;
        }
        /* the whole list, as the PS2's DUALSHOCK 2 page opens its 14
           choices (PCSX2 print); only the screen bounds it */
        int visible=count>19?19:count;
        float row=fmaxf(24,option->h),height=row*visible;
        float x=fminf(fmaxf(0,p->option_x),fmaxf(0,640-option->w));
        /* func_002C9098 negates the selected child's y (002C9254):
           the committed row starts at the closed control, not below it. */
        float y=p->option_y-anchor_index*row;
        y=fminf(fmaxf(4,y),fmaxf(4,466-height));
        ui_widget *popup=ui_widget_child(option,"popup");
        ui_widget *list=popup?ui_widget_child(popup,"vbox"):NULL;
        ui_widget *shadow=popup?ui_widget_child(popup,"shadow"):NULL;
        ui_widget *background=list?ui_widget_child(list,"bg"):NULL;
        frame overlay={x,y,option->w,height,p->option_alpha};
        clip_rect overlay_clip={0};
        if(shadow){ui_widget face=*shadow;face.w=option->w;face.h=height;draw_widget(p,&face,overlay,overlay_clip,&d,&s);}
        if(background){ui_widget face=*background;face.x=face.y=0;face.w=option->w;face.h=height;draw_widget(p,&face,overlay,overlay_clip,&d,&s);}
        int start=selected_index>=visible?selected_index-visible+1:0;
        clip_rect popup_clip={x,y,option->w,height,1};set_clip(popup_clip);
        for(int j=0;j<visible;j++) {
            ui_widget *item=ui_option_item(option,indices[start+j]);
            /* Draw authored highlight, committed-choice dot and label,
               with each item's own state mask, texture and RGBA. */
            frame base={x-item->x,y+j*row-item->y,option->w,row,
                        p->option_alpha*(item->insensitive?.4f:1)};
            draw_widget(p,item,base,popup_clip,&d,&s);
        }
        plat_set_clip(0,0,0,0);
        cursor_target(p,option,x,y+(selected_index-start)*row,option->w,row);
    }
    gt4_transition_motion *motion=&p->transition;
    if(motion->active && motion->kind==1 && motion->type==1 && p->capture) {
        /* 0022F6A0 +40..b8 suspends the context while the departed root is
           removed; 0028A5B8 +34..48 transforms only stage 2. Keep the
           outgoing surface through stage 0/1, rather than feeding a hidden
           page's black framebuffer back into the transition every field. */
        if(p->retained && (motion->stage<=1 || p->transition_presented==motion->frame)) {
            const uint8_t white[4]={255,255,255,255};
            plat_draw_quad(p->retained,0,0,640,480,0,1,1,0,white);
            if(drawn)*drawn=d;if(skipped)*skipped=s;return;
        }
        /* 0028A5B8 transforms pages; the engine draws its cursor separately. */
        p->transition_frame=p->capture(p->transition_frame,0,0,640,480);
        const uint8_t black[4]={0,0,0,255},white[4]={255,255,255,255};
        plat_draw_quad(NULL,0,0,640,480,0,0,1,1,black);
        if(p->retained)plat_draw_quad(p->retained,0,0,640,480,0,1,1,0,white);
        float scale=motion->stage==2?gt4_cross_scale(motion->value):1;
        uint8_t tint[4]={255,255,255,(uint8_t)(255*(motion->stage==2?1-motion->value:1))};
        plat_draw_quad(p->transition_frame,(640-640*scale)*.5f,(480-480*scale)*.5f,
                       640*scale,480*scale,0,1,1,0,tint);
    }
    /* The cursor belongs to the render context, not the button's paint pass.
       A focused popup still owns it while its actors fade/leave the viewport. */
    if(p->cursor_selection_set && p->cursor_selected && p->cursor_selected->root==i &&
       p->cursor_selected->focused && p->cursor_started) {
        int visible=1;
        for(const ui_widget *a=p->cursor_selected;a;a=a->parent)if(!a->visible){visible=0;break;}
        if(visible)p->cursor_visible=1;
    }
    /* Focus can be cleared while a page leaves. The render-context cursor
       survives that gap at its last position and fades with the scene. */
    int cursor_in_transition=p->cursor_started && (p->cursor_held || p->transition.active || (p->fade_period>0 &&
        (!p->fade_done || p->fade_to>0)));
    if(!p->cursor_visible && cursor_in_transition){
        p->cursor_tx=p->cursor_x;p->cursor_ty=p->cursor_y;p->cursor_ta=p->cursor_angle;p->cursor_focus=NULL;
    }
    if(!p->cursor_hidden && (p->cursor_visible || cursor_in_transition)) {
        ui_load_package(p,"/projects/cursor","CursorRoot");
        char cursor_path[96];snprintf(cursor_path,sizeof cursor_path,"image/cursor/%s.png",
            *p->cursor_icon?p->cursor_icon:"cursor");
        ui_image *cursor=image(p,i,cursor_path);
        if(!p->cursor_started){p->cursor_x=320;p->cursor_y=240;p->cursor_angle=0;p->cursor_started=1;}
        /* The arrow's tip is at (16,0) of the 32x32 sprite; it turns around it. */
        if(cursor && cursor->tex){const uint8_t white[4]={255,255,255,255};
            plat_draw_rotated(cursor->tex,p->cursor_x-16,p->cursor_y,32,32,p->cursor_x,p->cursor_y,p->cursor_angle,white);}
    }
    if(motion->active && motion->kind==2) {
        /* 0027B138 / 0027A1E8: old screen blocks cover the new page/cursor. */
        if(p->retained)for(int k=0;k<motion->columns*motion->rows;k++) {
            const gt4_transition_tile *t=&motion->tiles[k];
            if(t->alpha<=0)continue;
            uint8_t tint[4]={255,255,255,(uint8_t)(255*fminf(1,t->alpha))};
            plat_draw_quad(p->retained,t->x,t->y,t->w,t->h,t->source_x/640,
                1-t->source_y/480,(t->source_x+t->w)/640,1-(t->source_y+t->h)/480,tint);
        }
    } else if(motion->active && motion->kind==1) {
        if(motion->type==0 && p->retained && motion->value>0) {
            uint8_t tint[4]={255,255,255,(uint8_t)(255*motion->value)};
            plat_draw_quad(p->retained,0,0,640,480,0,1,1,0,tint);
        } else if(motion->type==1) {
            /* func_002182E0: magnify_ratio is the feedback blend factor,
               magnify_diff crops the source by that many pixels per edge.
               This pass follows the page AND the render-context cursor. */
            if(p->retained) {
                float diff=motion->magnify_diff;
                float ratio=fmaxf(0,fminf(1,motion->magnify_ratio));
                const uint8_t tint[4]={255,255,255,255};
                plat_draw_quad_blend(p->retained,0,0,640,480,diff/640,
                    1-diff/480,1-diff/640,diff/480,tint,(float)gt4_gs_alpha(ratio)/128.0f);
            }
            if(motion->with_black) {
                const uint8_t black[4]={0,0,0,255};
                plat_draw_quad_blend(NULL,0,0,640,480,0,0,1,1,black,
                    (float)gt4_gs_alpha(fminf(1,motion->black_opacity))/128.0f);
            }
            if(p->capture)p->retained=p->capture(p->retained,0,0,640,480);
            p->transition_presented=motion->frame;
        }
    }
    float opacity=p->fade_value;
    if(opacity>0) {
        const uint8_t black[4]={0,0,0,255};
        plat_draw_quad_blend(NULL,0,0,640,480,0,0,1,1,black,
                           (float)gt4_gs_alpha(fminf(1,opacity))/128.0f);
    }
    if (drawn) *drawn = d;
    if (skipped) *skipped = s;
}

void ui_draw_under(ui_project *p, int i)
{
    frame screen = { 0, 0, 640, 480, 1.0f };
    clip_rect none = { 0, 0, 0, 0, 0 };
    int d = 0, s = 0;
    ui_widget *w = ui_root_widget(p, i);
    if (!w) return;
    /* stamp with the tick the following ui_draw_root will use */
    unsigned tick = p->draw_tick;
    if (!++p->draw_tick) p->draw_tick = 1;
    p->under = 1;
    layout_packed(p, w); draw_widget(p, w, screen, none, &d, &s);
    p->under = 0; p->draw_tick = tick;
    plat_set_clip(0, 0, 0, 0);
}

unsigned ui_draw_tick(const ui_project *p){return p?p->draw_tick:0;}
void ui_set_movie_renderer(ui_project *p,ui_movie_draw_fn draw,void *user){p->draw_movie=draw;p->movie_user=user;}
void ui_set_model_renderer(ui_project *p,ui_model_renderer renderer){if(p && !p->nmeshes)p->models=renderer;}
void ui_set_car_renderer(ui_project *p,ui_car_draw_fn draw,void *user){if(p){p->draw_car=draw;p->car_user=user;}}
void ui_set_scene_renderer(ui_project *p,ui_scene_draw_fn draw,void *user){if(p){p->draw_scene=draw;p->scene_user=user;}}
void ui_set_photo_renderer(ui_project *p,ui_photo_draw_fn draw,void *user){if(p){p->draw_photo=draw;p->photo_user=user;}}

void ui_fade(ui_project *p,float from,float to,float seconds)
{
    p->transition.active=0;
    p->cursor_frozen=to>=from && to>0;
    if(to<=from && p->cursor_icon_reset){p->cursor_icon[0]=0;p->cursor_icon_reset=0;}
    p->fade_from=fminf(1,fmaxf(0,from));p->fade_to=fminf(1,fmaxf(0,to));
    p->fade_time=0;p->fade_period=isfinite(seconds)?fmaxf(0,seconds):0;
    p->fade_ticks=0;p->fade_value=p->fade_period>0?p->fade_from:p->fade_to;
    p->fade_done=p->fade_period<=0 || p->fade_from==p->fade_to;
    p->fade_step=p->fade_period>0?(p->fade_to-p->fade_from)/(p->fade_period*60.0f):0;
}
float ui_fade_remaining(const ui_project *p)
{
    if(p->fade_done || p->fade_step==0)return 0;
    /* Predict updates still required, including the strict crossing frame.
       Callers check completion again after presenting rather than a wall timer. */
    return (fabsf(p->fade_to-p->fade_value)/fabsf(p->fade_step)+1.0f)/60.0f;
}

void ui_set_capture_renderer(ui_project *p,plat_texture *(*capture)(plat_texture *,float,float,float,float))
{if(p)p->capture=capture;}
plat_texture *ui_capture_region(ui_project *p,plat_texture *t,float x,float y,float w,float h)
{return p && p->capture?p->capture(t,x,y,w,h):t;}
gt4_transition_motion *ui_transition_state(ui_project *p){return p?&p->transition:NULL;}
void ui_transition_start(ui_project *p,int kind,int operation,float period)
{
    p->fade_value=0;p->fade_period=0;p->fade_step=0;p->fade_done=1;
    if(p->transition.kind!=kind)gt4_transition_init(&p->transition,kind);
    gt4_transition_start(&p->transition,operation,period);
    p->cursor_frozen=period>0;
    p->transition_presented=UINT64_MAX;
    if(operation!=1 && p->capture)p->retained=p->capture(p->retained,0,0,640,480);
}
void ui_transition_reveal(ui_project *p)
{if(p){gt4_transition_release(&p->transition);
    if(p->transition.stage!=2 && p->transition.active)return;
    p->cursor_frozen=0;if(p->cursor_icon_reset){p->cursor_icon[0]=0;p->cursor_icon_reset=0;}}}
float ui_transition_remaining(const ui_project *p)
{
    const gt4_transition_motion *m=&p->transition;
    if(!m->active || m->stage!=2 || m->step<=0)return 0;
    float maximum=m->value;
    if(m->kind==2)for(int i=0;i<m->columns*m->rows;i++)maximum=fmaxf(maximum,m->tiles[i].alpha);
    return (fmaxf(0,maximum)/m->step+2)/60.0f;
}

void ui_car_prepare_draw(ui_widget *w)
{
    if(!w)return;
    gt4_service_clock(&w->car_camera_time,&w->car_camera_playing,w->car_camera!=NULL,0,
                     gt4_mot1_end(w->car_camera),gt4_mot1_loop(w->car_camera),w->car_camera_loop);
}
static void animate_strip(ui_widget *w,float seconds)
{
    if(!w)return;
    /* CarModel motion 2 also runs without Mot1 controllers: PresentRoot
       move_car (gtmode.ad L14653..14679), original update 00153BB8. */
    if(w->visible && isfinite(seconds) && seconds>0 && (w->car_motion==2 || w->car_motion_count || w->car_camera || w->car_crew_motion)){
        /* 00153BB8 +0x1cc..0x234 advances controllers at the US frame
           interval, bits 3C888888. Host delivery does not change the step. */
        w->car_service_ticks+=(double)seconds*60;
        while(w->car_service_ticks>=1.-1e-6){
            w->car_service_ticks-=1.;uint32_t bits=0x3c888888;float dt;memcpy(&dt,&bits,4);
            w->car_model_opacity=gt4_service_fade(w->car_model_opacity);
            /* 00153C38 is the normal CarFace model. The studio preview
               has its own conductor (00157378 -> 001D5B88). */
            if(!w->car_motion && w->car_normal)continue;
            if(w->car_motion==2)gt4_service_pose(w->car_position,w->model_angles,w->car_target_position,w->car_target_angles);
            for(int i=0;i<w->car_motion_count;i++){
                gt4_service_motion *track=&w->car_motions[i];
                gt4_service_advance(&track->time,track->playing,track->motion!=NULL,dt);
            }
            gt4_service_advance(&w->car_camera_time,w->car_camera_playing,w->car_camera!=NULL,dt);
            if(w->car_motion_count){
                /* Compatibility view of the first 001540C8 controller;
                   do not advance the same controller a second time. */
                w->car_crew_time=w->car_motions[0].time;
                w->car_crew_playing=w->car_motions[0].playing;
            }else gt4_service_advance(&w->car_crew_time,w->car_crew_playing,w->car_crew_motion!=NULL,dt);
        }
    }
    if(w->visible && !strcmp(w->type,"FlashFace") && isfinite(seconds) && seconds>0)w->flash_time+=seconds;
    /* Explicit poses used by event/license previews must still honor the
       script's turntable mode. Studio-only cars retain their renderer motion. */
    if(w->visible && !strcmp(w->type,"CarFace") && w->car_pose_set &&
       w->car_motion==1 && isfinite(seconds) && seconds>0){
        w->car_spin_ticks+=(double)seconds*60.0;
        while(w->car_spin_ticks>=1.0-1e-6){
            w->car_spin_ticks-=1.0;
            gt4_car_turn_step(&w->model_angles[1],&w->car_spin);
            if(w->car_position_easing)gt4_service_position(w->car_position,w->car_target_position,w->car_position_ratio);
        }
    }
    if(!strcmp(w->type,"ModelFace") && seconds>0) {
        double rest[3]={0};mp_numbers(w->src,"model_rotate",rest,3);
        for(int i=0;i<3;i++) {
            if(!w->model_target_mode)w->model_angles[i]=fmodf(w->model_angles[i]+w->model_velocity[i]*seconds*60,360);
            else {
                /* Return along the shortest arc. The easing rate is provisional;
                   target_mode/rotate_v are driven by the original map script. */
                float delta=remainderf((float)rest[i]-w->model_angles[i],360);
                w->model_angles[i]+=delta*(1-powf(.85f,seconds*60));
                if(fabsf(delta)<.01f)w->model_angles[i]=(float)rest[i];
            }
        }
    }
    if(horizontal_strip(w)) {
        float target=!strcmp(w->type,"SelectBox")?select_offset(w,0):strip_target(w),gap=(float)mp_number(w->src,"spacing",0);
        if(!w->strip_ready){w->strip_offset=target;w->strip_ready=1;}
        else w->strip_offset+=(target-w->strip_offset)*(1-expf(-18*seconds));
        if(fabsf(target-w->strip_offset)<0.05f)w->strip_offset=target;
        /* The authored track/thumb bar shares the card list's parent. Its
           thumb covers the visible fraction and follows the eased viewport. */
        ui_widget *bar=w->parent?ui_widget_child(w->parent,"bg"):NULL;
        ui_widget *thumb=w->parent?ui_widget_child(w->parent,"pinch"):NULL;
        if(bar && thumb && bar->w>0) {
            float total=-gap;
            for(int i=0;i<w->nkids;i++)total+=w->kids[i]->w+gap;
            float first=w->kids[0]->w*.5f,last=total-w->kids[w->nkids-1]->w*.5f;
            float fraction=last>first?(w->strip_offset-first)/(last-first):0;
            fraction=fminf(1,fmaxf(0,fraction));
            thumb->w=bar->w*fminf(1,w->w/fmaxf(total,1));
            thumb->x=bar->x+(bar->w-thumb->w)*fraction;
        }
    }
    if(vertical_select(w)) {
        /* Eased like the card strips; the original's per-frame ratio is
           not traced yet. */
        float target=select_offset(w,1);
        if(!w->strip_ready){w->strip_offset=target;w->strip_ready=1;}
        else w->strip_offset+=(target-w->strip_offset)*(1-expf(-18*seconds));
        if(fabsf(target-w->strip_offset)<0.1f)w->strip_offset=target;
    }
    if(!strcmp(w->type,"ToolTipFace")) {
        w->text_scroll+=seconds;
        if(w->visible)gt4_tooltip_advance(&w->tooltip_motion,seconds);
    }
    if(!strcmp(w->type,"ScrollWindow")) {
        /* Scroll bars (hbar/vbar > ScrollBase + ScrollPinch): the pinch covers
           the visible fraction of the scrolled space and sits at its offset. */
        ui_widget *clip=ui_widget_child(w,"clip"),*space=clip?ui_widget_child(clip,"space"):NULL;
        for(int axis=0;space && axis<2;axis++) {
            ui_widget *bar=ui_widget_child(w,axis?"vbar":"hbar");
            ui_widget *base=bar?ui_widget_child(bar,"base"):NULL,*pinch=bar?ui_widget_child(bar,"pinch"):NULL;
            if(!base || !pinch)continue;
            float view=axis?clip->h:clip->w,total=axis?space->h:space->w,pos=-(axis?space->y:space->x);
            float track=axis?base->h:base->w,size=total>view?track*view/total:track;
            float at=(axis?base->y:base->x)+(total>view?(track-size)*fminf(1,fmaxf(0,pos/(total-view))):0);
            if(axis){pinch->h=size;pinch->y=at;}else{pinch->w=size;pinch->x=at;}
            pinch->has_geometry=1;
            for(int i=0;i<pinch->nkids;i++){if(axis)pinch->kids[i]->h=size;else pinch->kids[i]->w=size;}
        }
    }
    if(w->scroll_set) {
        const ui_widget *owner=w->parent?w->parent->parent:NULL;
        float ratio=owner?(float)mp_number(owner->src,"interpolate_ratio",.15f):.15f;
        ratio=fminf(1,fmaxf(0,ratio));
        float blend=1-powf(1-ratio,seconds*60);
        w->x=fabsf(w->scroll_tx-w->x)<.5f?w->scroll_tx:w->x+(w->scroll_tx-w->x)*blend;
        w->y=fabsf(w->scroll_ty-w->y)<.5f?w->scroll_ty:w->y+(w->scroll_ty-w->y)*blend;
    }
    for(int i=0;i<w->nkids;i++)animate_strip(w->kids[i],seconds);
}
/* Native context focus, independent of the page paint/opacity. */
void ui_cursor_show(ui_project *p,int visible)
{if(p)p->cursor_hidden=!visible;}

void ui_cursor_hold(ui_project *p,int held)
{if(p)p->cursor_held=held;}

int ui_cursor_pause(ui_project *p,int paused)
{if(!p)return 0;int previous=p->cursor_paused;p->cursor_paused=paused;return previous;}

void ui_cursor_select(ui_project *p,const ui_widget *widget)
{
    if(!p)return;
    p->cursor_selection_set=1;
    if(p->cursor_selected!=widget){
        p->cursor_selected=widget;p->cursor_focus=NULL;
        if(widget && strcmp(widget->type,"ListBox") && strcmp(widget->type,"SelectBox")){
            p->cursor_focus=widget;p->cursor_rect_x=p->cursor_rect_y=0;
            p->cursor_rect_w=widget->w;p->cursor_rect_h=widget->h;
        }
    }
}

/* CursorRoot.setIcon, cursor.adc lines 42..50. Use the authored sprite;
   a default-icon request during deferred startPage commits with that page. */
void ui_cursor_icon(ui_project *p,const char *name)
{
    if(!p)return;
    if(!name || !*name || !strcmp(name,"cursor")) {
        if(p->cursor_frozen){p->cursor_icon_reset=1;return;}
        p->cursor_icon[0]=0;p->cursor_icon_reset=0;return;
    }
    if(strchr(name,'/') || strchr(name,'\\') || strstr(name,"..") || strlen(name)>=sizeof p->cursor_icon)return;
    snprintf(p->cursor_icon,sizeof p->cursor_icon,"%s",name);p->cursor_icon_reset=0;
}
void ui_cursor_speed(ui_project *p,float ratio)
{if(p && isfinite(ratio) && ratio>=0){p->cursor_speed_ratio=ratio;p->cursor_speed_set=1;}}
void ui_cursor_transfer(ui_project *destination,const ui_project *source)
{
    if(!destination || !source)return;
    memcpy(destination->cursor_icon,source->cursor_icon,sizeof destination->cursor_icon);
    destination->cursor_icon_reset=source->cursor_icon_reset;
    destination->cursor_frozen=source->cursor_frozen;
    destination->cursor_paused=source->cursor_paused;
    destination->cursor_held=source->cursor_held;
    /* A hidden/unpainted context has no cursor position to transfer. Copying
       its zero-initialized state reset a cached cursor to (320,240) on draw. */
    if(source->cursor_started){
        destination->cursor_started=1;
        destination->cursor_x=source->cursor_x;destination->cursor_y=source->cursor_y;
        destination->cursor_angle=source->cursor_angle;
        destination->cursor_tx=source->cursor_tx;destination->cursor_ty=source->cursor_ty;
        destination->cursor_ta=source->cursor_ta;
    }
    destination->cursor_speed_ratio=source->cursor_speed_ratio;
    destination->cursor_speed_set=source->cursor_speed_set;
    destination->cursor_ticks=source->cursor_ticks;
    destination->cursor_focus=NULL;destination->cursor_selected=NULL;destination->cursor_selection_set=0;
}

void ui_animate(ui_project *p,int root,float seconds)
{
    if(isfinite(seconds) && seconds>0) {
        p->fade_time=fminf(p->fade_period,p->fade_time+seconds);
        p->fade_ticks+=(double)seconds*60.0;
        while(p->fade_ticks>=1.0-1e-6) {
            p->fade_ticks-=1;
            gt4_transition_step(&p->transition);
            if(!p->fade_done)p->fade_done=gt4_color_step(&p->fade_value,p->fade_to,p->fade_step);
        }
    }
    animate_strip(ui_root_widget(p,root),seconds);
    if(p->cursor_started && isfinite(seconds) && seconds>0) {
        /* The original focus rectangle is taken after its actors update.
           Preserve the rendered layout offset while following live ancestors. */
        const ui_widget *focus=p->cursor_focus;
        const ui_widget *list=focus?focus->parent:NULL;
        int list_focus=list && list->focused && (horizontal_strip(list) || vertical_select(list)) &&
            list->focus_index>=0 && list->focus_index<list->nkids && list->kids[list->focus_index]==focus;
        if(focus && (focus->focused || list_focus)) {
            float x=0,y=0;
            cursor_origin(p->cursor_focus,&x,&y);
            cursor_target(p,p->cursor_focus,x+p->cursor_rect_x,y+p->cursor_rect_y,
                          p->cursor_rect_w,p->cursor_rect_h);
        }
        float ratio=p->cursor_speed_set?p->cursor_speed_ratio:1;
        p->cursor_ticks+=(double)seconds*60.0;
        while(p->cursor_ticks>=1.0-1e-6) {
            p->cursor_ticks-=1;
            gt4_cursor_update(&p->cursor_x,&p->cursor_y,&p->cursor_angle,
                              p->cursor_tx,p->cursor_ty,p->cursor_ta,ratio,
                              p->cursor_paused || p->cursor_held || p->cursor_frozen,1,1);
        }
    }
}

void ui_draw_caption(ui_project *p,int root,const char *text)
{
    ui_widget *r=ui_root_widget(p,root),*scroll=r?ui_widget_child(r,"CarScroll"):NULL;
    ui_widget *name=scroll?ui_widget_child(scroll,"Name"):NULL;
    if(!name)return;ui_widget label=*name;label.text=(char *)text;label.text_key=NULL;label.scale_x=label.scale_y=.62f;
    frame f={34,295,572,22,1};draw_text(p,&label,f);
}

/* Asset preparation never draws or advances focus/motion. Modal variants
   include hidden button states so the first selection change is warm too. */
static void prepare_widget(ui_project *p,ui_widget *w,int modal,void (*progress)(void *),void *user)
{
    if(!w || (!modal && !w->visible))return;
    if(progress)progress(user);
    if(!modal && !strcmp(w->type,"ModelFace")){model_mesh(p,w->resource_root,w->model_path?w->model_path:mp_string(w->src,"model_set_path"));model_environment(p,w->resource_root,w->src);}
    if(!strcmp(w->type,"TextFace") || !strcmp(w->type,"ToolTipFace"))font(p,text_font(w));
    if((!strcmp(w->type,"ImageFace") || !strcmp(w->type,"FrameImageFace")) && w->image_path)image(p,w->resource_root,w->image_path);
    if(!strcmp(w->type,"FrameImageFace")) {
        static const char *const edges[]={"tl_image_path","tr_image_path","bl_image_path","br_image_path","t_image_path","b_image_path","l_image_path","r_image_path"};
        for(int i=0;i<8;i++){const char *path=mp_string(w->src,edges[i]);if(path)image(p,w->resource_root,path);}
    }
    if(progress)progress(user);
    for(int i=0;i<w->nkids;i++)prepare_widget(p,w->kids[i],modal,progress,user);
}
void ui_prepare_root(ui_project *p,int root,void (*progress)(void *),void *user)
{if(p && root>=0 && root<p->nroots)prepare_widget(p,ui_root_widget(p,root),0,progress,user);}

void ui_prepare_dialogs(ui_project *p,void (*progress)(void *),void *user)
{
    if(!p)return;
    for(int i=0;i<p->nroots;i++) {
        ui_widget *root=ui_root_widget(p,i);const char *name=root->name;
        if(name && (strstr(name,"Popup") || strstr(name,"Dialog")))
            prepare_widget(p,root,1,progress,user);
    }
}

void ui_set_prepare_presenter(ui_project *p,void (*fn)(void *),void *user)
{if(p){p->prepare_presenter=fn;p->prepare_user=user;}}

void ui_set_album_renderer(ui_project *p,ui_photo_draw_fn draw,void *user){if(p){p->draw_album=draw;p->album_user=user;}}
void ui_set_native_renderer(ui_project *p,ui_photo_draw_fn draw,void *user){if(p){p->draw_native=draw;p->native_user=user;}}

/* A line of plain text in a disc face for native widgets (the Logger's
   labels, 00490B98 with its own scale): (x, y) the line's top left, or
   with right > 0 the line ending at x + right. */
float ui_string_height(ui_project *p,const char *face,float scale)
{
    ui_font *f=p?font(p,face):NULL;
    return f && f->tex?f->face.height*scale:0;
}
float ui_draw_string(ui_project *p,const char *face,float x,float y,float scale,float right,const uint8_t rgba[4],const char *text)
{
    ui_font *f=p && text?font(p,face):NULL;
    if(!f || !f->tex || scale<=0)return 0;
    const tex1_image *a=&f->face.atlases[0];
    float width=gt4_font_line_width(&f->face,text,0)*scale;
    if(right>0)x+=right-width;
    float base=y+f->face.baseline*scale;
    for(const char *c=text;*c;) {
        unsigned code=gt4_text_next(&c);
        if(code<32)continue;
        const gt4_glyph *g=gt4_font_glyph(&f->face,code);
        if(!g)g=gt4_font_glyph(&f->face,'?');
        if(!g)continue;
        if(g->w && g->h)
            plat_draw_glyph(f->tex,x+g->x*scale,base+g->y*scale,g->w*scale,g->h*scale,base,0,
                (float)g->u/a->width,(float)g->v/a->height,(float)(g->u+g->w)/a->width,(float)(g->v+g->h)/a->height,rgba);
        x+=g->advance*scale;
    }
    return width;
}
