/*
 * gt4model_survey: converts every car model of /car/menu with the port's
 * converter (src/core/model.c) and reports which ones fail, and which model
 * commands and callbacks the disc actually uses. Evidence for generic fixes;
 * prints names and counts only, no game data.
 *
 *   gt4model_survey <disc.iso> [--each [name_prefix]]
 *   gt4model_survey <disc.iso> --labels LABEL...    (showroom label -> model file)
 */
#include "model.h"
#include "mot1.h"
#include "specdb.h"
#include "vol.h"

#include "miniz.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char (*names)[64]; int n, cap; } list;

static void collect(void *user, const char *name, int is_dir, uint32_t size)
{
    (void)size;
    list *l = user;
    if (is_dir) return;
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 1024;
        l->names = realloc(l->names, (size_t)l->cap * sizeof *l->names);
        if (!l->names) exit(1);
    }
    snprintf(l->names[l->n++], 64, "%s", name);
}

int main(int argc, char **argv)
{
    const char *err = NULL;
    if (argc < 2) { fprintf(stderr, "usage: %s <disc.iso> [--each]\n", argv[0]); return 2; }
    int each = argc > 2 && !strcmp(argv[2], "--each");
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "cannot open volume: %s\n", err); return 1; }
    if (argc > 2 && !strcmp(argv[2], "--labels")) {
        /* Same tables as the showroom: GENERIC_CAR label -> variation -> model. */
        size_t n[4] = { 0 };
        uint8_t *t[4];
        const char *names[4] = { "GENERIC_CAR.idi", "CAR_VARIATION_american.dbt", "VARIATIONamerican.dbt", "american_StrDB.sdb" };
        for (int k = 0; k < 4; k++) {
            char path[96];
            snprintf(path, sizeof path, "/specdb/GT4_US2560/%s", names[k]);
            t[k] = gt4_vol_read(v, path, &n[k]);
        }
        for (int i = 3; i < argc; i++) {
            uint32_t id;
            uint8_t r[256];
            const char *model = NULL;
            if (t[0] && t[1] && t[2] && t[3] && gt4_specdb_label(t[0], n[0], argv[i], &id)
                && gt4_specdb_row(t[1], n[1], id, r, sizeof r) == 8) {
                uint32_t var = r[0] | r[1] << 8 | r[2] << 16 | (uint32_t)r[3] << 24;
                if (gt4_specdb_row(t[2], n[2], var, r, sizeof r) == 52)
                    model = gt4_specdb_string(t[3], n[3], r[0] | r[1] << 8 | r[2] << 16 | (uint32_t)r[3] << 24);
            }
            printf("%s %s\n", argv[i], model ? model : "-");
        }
        for (int k = 0; k < 4; k++) free(t[k]);
        gt4_vol_close(v);
        return 0;
    }
    if (argc > 3 && !strcmp(argv[2], "--variations")) {
        /* The VARIATION rows (52 bytes) of one model file: raw words. */
        size_t vs = 0, ss = 0;
        uint8_t *var = gt4_vol_read(v, "/specdb/GT4_US2560/VARIATIONamerican.dbt", &vs);
        uint8_t *str = gt4_vol_read(v, "/specdb/GT4_US2560/american_StrDB.sdb", &ss);
        unsigned count = var && vs >= 16 ? (var[8] | var[9] << 8 | var[10] << 16 | (unsigned)var[11] << 24) : 0;
        for (unsigned i = 0; i < count; i++) {
            uint8_t r[256];
            if (gt4_specdb_row_at(var, vs, i, r, sizeof r) != 52) continue;
            const char *model = gt4_specdb_string(str, ss, r[0] | r[1] << 8 | r[2] << 16 | (unsigned)r[3] << 24);
            if (!model || strcmp(model, argv[3])) continue;
            const char *name = gt4_specdb_string(str, ss, r[12] | r[13] << 8 | r[14] << 16 | (unsigned)r[15] << 24);
            printf("%-24s", name ? name : "?");
            for (int k = 0; k < 52; k++) printf("%s%02x", k % 4 ? "" : " ", r[k]);
            printf("\n");
        }
        free(var); free(str);
        gt4_vol_close(v);
        return 0;
    }
    if (argc > 3 && !strcmp(argv[2], "--wheel")) {
        /* The wheel parts of /car/menu/<file>: rim (offset at +32) and
           tires (+44, +48); per texture its triangles, kinds and blending. */
        char path[128];
        size_t size = 0;
        snprintf(path, sizeof path, "/car/menu/%s", argv[3]);
        uint8_t *data = gt4_vol_read(v, path, &size);
        static const int at[3] = { 32, 44, 48 };
        for (int piece = 0; data && piece < 3; piece++) {
            size_t off = data[at[piece]] | data[at[piece] + 1] << 8 | data[at[piece] + 2] << 16 | (size_t)data[at[piece] + 3] << 24;
            if (off >= size) continue;
            gt4_model *m = gt4_model_decode_part(data + off, size - off, 0, 0);
            if (!m) { printf("piece %d: failed\n", piece); continue; }
            printf("piece %d: %d triangles, %d textures\n", piece, m->count, m->ntextures);
            for (int t = -2; t < m->ntextures; t++) {
                int n = 0, kinds[8] = { 0 }, blend = 0, cull = -1, atest = 0;
                float rgb = 0, xlo = 1e9f, xhi = -1e9f;
                for (int i = 0; i < m->count; i++) {
                    const gt4_triangle *tr = &m->triangles[i];
                    if (tr->texture != t) continue;
                    n++; kinds[tr->kind & 7]++; blend |= tr->blend_enabled; cull = tr->state.cull; atest |= tr->state.alpha_test;
                    for (int j = 0; j < 3; j++) { rgb += tr->v[j].rgb[0]; xlo = fminf(xlo, tr->v[j].p[0]); xhi = fmaxf(xhi, tr->v[j].p[0]); }
                }
                if (!n) continue;
                printf("  texture %d: %d tris kinds", t, n);
                for (int k = 0; k < 8; k++) if (kinds[k]) printf(" %d:%d", k, kinds[k]);
                printf(" rgb %.2f x %.3f..%.3f blend %d cull %d atest %d\n", rgb / (3 * n), xlo, xhi, blend, cull, atest);
            }
            for (int t = 0; argc > 4 && t < m->ntextures; t++) {
                char png_path[512];
                size_t len = 0;
                snprintf(png_path, sizeof png_path, "%s_%d_%d.png", argv[4], piece, t);
                void *png = tdefl_write_image_to_png_file_in_memory(m->textures[t].rgba, m->textures[t].width, m->textures[t].height, 4, &len);
                FILE *f = png ? fopen(png_path, "wb") : NULL;
                if (f) { fwrite(png, 1, len, f); fclose(f); }
                mz_free(png);
            }
            gt4_model_free(m);
        }
        free(data);
        gt4_vol_close(v);
        return 0;
    }
    if (argc > 4 && !strcmp(argv[2], "--raw")) {
        /* local inspection only: copies one volume file to disk */
        size_t size = 0;
        uint8_t *data = gt4_vol_read(v, argv[3], &size);
        FILE *f = data ? fopen(argv[4], "wb") : NULL;
        if (f) { fwrite(data, 1, size, f); fclose(f); }
        free(data);
        gt4_vol_close(v);
        return f ? 0 : 1;
    }
    if (argc > 3 && !strcmp(argv[2], "--mot")) {
        /* A keyframe Mot1 (e.g. /menu/select/light.mot): timeline and samples. */
        size_t size = 0;
        uint8_t *data = gt4_vol_read(v, argv[3], &size);
        gt4_mot1 *m = data ? gt4_mot1_open(data, size) : NULL;
        free(data);
        if (!m) { fprintf(stderr, "not a supported Mot1: %s\n", argv[3]); gt4_vol_close(v); return 1; }
        printf("end %.4f loop %.4f\n", gt4_mot1_end(m), gt4_mot1_loop(m));
        static const char *names[] = { "Alpha0", "Alpha1", "Alpha2", "Alpha3", "ColorScale", "BlackLevel", "WhiteLevel",
                                       "shadowDarkness0", "shadowBlur0", "bottomShadow" };
        for (float t = 0; t <= 26; t += t < 1.1f ? .25f : 2) {
            printf("t=%5.2f (%5.2f)", t, gt4_mot1_wrap(m, t));
            for (int i = 0; i < (int)(sizeof names / sizeof *names); i++) {
                float value;
                if (gt4_mot1_value(m, names[i], gt4_mot1_wrap(m, t), &value)) printf(" %s=%.2f", names[i], value);
            }
            float c[4];
            if (gt4_mot1_light(m, 0x0F, 0, gt4_mot1_wrap(m, t), c) == 3) printf(" light0=(%.2f %.2f %.2f)", c[0], c[1], c[2]);
            printf("\n");
            for (int light = 0; light < 4; light++) {
                printf("   light %d:", light);
                for (int kind = 0x0C; kind <= 0x0F; kind++) {
                    int n = gt4_mot1_light(m, kind, light, gt4_mot1_wrap(m, t), c);
                    if (n) printf(" %02X=(%.2f %.2f %.2f%s%.2f)", kind, c[0], c[1], c[2], n > 3 ? " " : "", n > 3 ? c[3] : 0.f);
                }
                printf("\n");
            }
            float look[9], fov;
            if (gt4_mot1_camera(m, gt4_mot1_wrap(m, t), look, &fov))
                printf("   camera eye (%.3f %.3f %.3f) target (%.3f %.3f %.3f) up (%.3f %.3f %.3f) fov %.4f\n",
                       look[0], look[1], look[2], look[3], look[4], look[5], look[6], look[7], look[8], fov);
        }
        gt4_mot1_close(m);
        gt4_vol_close(v);
        return 0;
    }
    if (argc > 3 && !strcmp(argv[2], "--mdl")) {
        /* One MDLS file (e.g. /menu/select/bg.mdl): each model index, and the world path. */
        size_t size = 0;
        uint8_t *data = gt4_vol_read(v, argv[3], &size);
        if (!data) { fprintf(stderr, "cannot read %s\n", argv[3]); gt4_vol_close(v); return 1; }
        for (int index = 0; index < 8; index++) {
            gt4_model_stats st;
            gt4_model *m = gt4_model_decode_stats(data, size, 0, index, &st);
            if (m) printf("model %d: %d triangles, %d textures, %d unsupported\n", index, m->count, m->ntextures, m->unsupported);
            for (int t = -2; m && t < m->ntextures; t++) {
                /* per texture: triangle count, bounds and mean vertex colour */
                float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f }, rgb[3] = { 0 };
                int n = 0, blend = 0, normals = 0;float uvlo[2]={1e9f,1e9f},uvhi[2]={-1e9f,-1e9f};
                for (int i = 0; i < m->count; i++) {
                    const gt4_triangle *tr = &m->triangles[i];
                    if (tr->texture != t) continue;
                    n++; blend = tr->blend_enabled;
                    for(int j=0;j<3;j++){const float *v=tr->v[j].normal;if(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]>1e-12f)normals++;}
                    for(int j=0;j<3;j++)for(int k=0;k<2;k++){uvlo[k]=fminf(uvlo[k],tr->v[j].uv[k]);uvhi[k]=fmaxf(uvhi[k],tr->v[j].uv[k]);}
                    for (int j = 0; j < 3; j++) for (int k = 0; k < 3; k++) {
                        lo[k] = fminf(lo[k], tr->v[j].p[k]); hi[k] = fmaxf(hi[k], tr->v[j].p[k]); rgb[k] += tr->v[j].rgb[k];
                    }
                }
                if (n) printf("  texture %d: %d tris, x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f rgb %.2f %.2f %.2f blend %d normals %d/%d\n", t, n,
                              lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], rgb[0] / (3 * n), rgb[1] / (3 * n), rgb[2] / (3 * n), blend,normals,3*n);
                if(n && t>=0){const tex1_image *im=&m->textures[t];printf("    uv %.4f..%.4f %.4f..%.4f image %dx%d GS %dx%d wrap %u/%u region %u..%u %u..%u\n",uvlo[0],uvhi[0],uvlo[1],uvhi[1],im->width,im->height,im->tex_w,im->tex_h,im->wrap_u,im->wrap_v,im->min_u,im->max_u,im->min_v,im->max_v);}
            }
            for (int t = 0; m && argc > 4 && t < m->ntextures; t++) {
                /* optional local inspection: <prefix>_<model>_<texture>.png */
                char path[512];
                size_t len = 0;
                snprintf(path, sizeof path, "%s_%d_%d.png", argv[4], index, t);
                void *png = tdefl_write_image_to_png_file_in_memory(m->textures[t].rgba, m->textures[t].width, m->textures[t].height, 4, &len);
                FILE *f = png ? fopen(path, "wb") : NULL;
                if (f) { fwrite(png, 1, len, f); fclose(f); }
                mz_free(png);
            }
            if (!m) printf("model %d: failed (op %d at 0x%zx, line %d)\n", index, st.fail_op, st.fail_offset, st.fail_line);
            printf("  commands:");
            for (int k = 0; k < 256; k++) if (st.ops[k]) printf(" %d:%u", k, st.ops[k]);
            printf("\n  callbacks:");
            for (int k = 0; k < 256; k++) if (st.callbacks[k]) printf(" %d:%u", k, st.callbacks[k]);
            printf("\n");
            gt4_model_free(m);
        }
        gt4_model_stats ws;
        gt4_model *w = gt4_model_decode_world(data, size, &ws);
        if (w) printf("world: %d triangles, %d textures\n", w->count, w->ntextures);
        else printf("world: failed (op %d at 0x%zx, line %d)\n", ws.fail_op, ws.fail_offset, ws.fail_line);
        gt4_model_free(w);
        free(data);
        gt4_vol_close(v);
        return 0;
    }
    list l = { 0 };
    gt4_vol_list(v, "/car/menu", collect, &l);

    unsigned ops_cars[256] = { 0 }, cb_cars[256] = { 0 }, fail_ops[258] = { 0 };
    int ok = 0, failed = 0, notcar = 0;
    long long tris = 0, unsupported = 0;
    for (int i = 0; i < l.n; i++) {
        char path[128];
        size_t size = 0;
        snprintf(path, sizeof path, "/car/menu/%s", l.names[i]);
        if (argc > 3 && strncmp(l.names[i], argv[3], strlen(argv[3]))) continue;   /* name prefix */
        uint8_t *data = gt4_vol_read(v, path, &size);
        if (!data || size < 4 || memcmp(data, "CAR4", 4)) { notcar++; free(data); continue; }
        gt4_model_stats st;
        gt4_model *m = gt4_model_decode_stats(data, size, 0, 0, &st);
        for (int k = 0; k < 256; k++) {
            if (st.ops[k]) ops_cars[k]++;
            if (st.callbacks[k]) cb_cars[k]++;
        }
        if (m) {
            ok++;
            tris += m->count;
            unsupported += m->unsupported;
            if (each) printf("%-12s ok   %6d triangles  %d unsupported\n", l.names[i], m->count, m->unsupported);
            for (int k = 0; each && k < st.nsamples; k++) {
                int op = (int)st.samples[k][0], n = op == 15 || op == 16 ? 16 : op == 19 ? 4 : op >= 20 ? 1 : 3;
                printf("    op %d:", op);
                for (int j = 1; j <= n; j++) printf(" %g", st.samples[k][j]);
                printf("\n");
            }
        } else {
            failed++;
            fail_ops[st.fail_op + 2]++;
            printf("%-12s FAIL op %d at 0x%zx\n", l.names[i], st.fail_op, st.fail_offset);
        }
        gt4_model_free(m);
        free(data);
    }
    printf("\n%d converted, %d failed, %d not CAR4; %lld triangles, %lld unsupported strips/textures\n",
           ok, failed, notcar, tris, unsupported);
    printf("failure causes:");
    for (int k = 0; k < 258; k++)
        if (fail_ops[k]) printf(" %s%d:%u", k == 0 ? "budget" : k == 1 ? "bounds" : "op", k - 2, fail_ops[k]);
    printf("\ncommands (cars using each):");
    for (int k = 0; k < 256; k++)
        if (ops_cars[k]) printf(" %d:%u", k, ops_cars[k]);
    printf("\ncallback ids (cars using each):");
    for (int k = 0; k < 256; k++)
        if (cb_cars[k]) printf(" %d:%u", k, cb_cars[k]);
    printf("\n");
    free(l.names);
    gt4_vol_close(v);
    return 0;
}
