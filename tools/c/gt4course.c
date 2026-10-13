/*
 * gt4course: inspects a course file (/crs/<name>) from the user's disc with
 * the port's own readers: converts the world model set and reads the runway
 * (grid start positions, road surface). Prints counts and positions only.
 *
 *   gt4course <disc.iso> <course>        e.g. gt4course disc.iso nurburgring
 *
 * Layouts cross-checked with Nenkai's MIT PDTools / format templates
 * (CourseDataFileGT4, GT4Runway.bt); see docs/COURSES.md.
 */
#include "course.h"
#include "model.h"
#include "vol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *err = NULL;
    if (argc < 3) { fprintf(stderr, "usage: %s <disc.iso> <course> [modelset-header-offset]\n", argv[0]); return 2; }
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "cannot open volume: %s\n", err); return 1; }
    char path[128];
    size_t size = 0;
    if (argv[2][0] == '/') {   /* a car model (e.g. /car/menu/fdeu0006): its commands and callbacks */
        gt4_model_stats st;
        uint8_t *bytes = gt4_vol_read(v, argv[2], &size);
        memset(&st, 0, sizeof st);
        gt4_model *m = bytes ? gt4_model_decode_stats(bytes, size, 0, argc > 3 ? atoi(argv[3]) : 0, &st) : NULL;
        printf("%s: %zu bytes, %d triangles\ncommands:", argv[2], size, m ? m->count : -1);
        for (int k = 0; k < 256; k++) if (st.ops[k]) printf(" %d:%u", k, st.ops[k]);
        printf("\ncallbacks:");
        for (int k = 0; k < 256; k++) if (st.callbacks[k]) printf(" %d:%u", k, st.callbacks[k]);
        printf("\n");
        for (int i = 0, run = 0; m && i < m->count; i++) {   /* the draws in order: kind, texture, blend state */
            const gt4_triangle *t = &m->triangles[i], *n = i + 1 < m->count ? &m->triangles[i + 1] : NULL;
            run++;
            if (n && n->kind == t->kind && n->texture == t->texture && n->blend_enabled == t->blend_enabled &&
                !memcmp(&n->state, &t->state, sizeof t->state))
                continue;
            printf("  %5d tris kind %u tex %d abe %d blend 0x%02x fix %d zw %d at %d af %d ar %d fail %d date %d/%d mask %08x rgb %.2f %.2f %.2f a %.2f\n",
                   run, t->kind, t->texture, t->blend_enabled, t->state.blend, t->state.fixed_alpha, t->state.depth_write,
                   t->state.alpha_test, t->state.alpha_func, t->state.alpha_ref, t->state.alpha_fail, t->state.destination_test,t->state.destination_bit,(unsigned)t->state.color_mask,
                   t->v[0].rgb[0], t->v[0].rgb[1], t->v[0].rgb[2], t->v[0].alpha);
            {   /* the run's UV range */
                float lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f };
                for (int j = i - run + 1; j <= i; j++)
                    for (int k = 0; k < 3; k++)
                        for (int c = 0; c < 2; c++) {
                            float x = m->triangles[j].v[k].uv[c];
                            if (x < lo[c]) lo[c] = x;
                            if (x > hi[c]) hi[c] = x;
                        }
                printf("        uv u %.3f..%.3f v %.3f..%.3f\n", lo[0], hi[0], lo[1], hi[1]);
            }
            run = 0;
        }
        if (m && m->count) {   /* the vertices' alpha and height */
            float alo = 1e9f, ahi = -1e9f, ylo = 1e9f, yhi = -1e9f;
            for (int i = 0; i < m->count; i++)
                for (int k = 0; k < 3; k++) {
                    const gt4_vertex *p = &m->triangles[i].v[k];
                    if (p->alpha < alo) alo = p->alpha;
                    if (p->alpha > ahi) ahi = p->alpha;
                    if (p->p[1] < ylo) ylo = p->p[1];
                    if (p->p[1] > yhi) yhi = p->p[1];
                }
            printf("vertex alpha %.3f..%.3f, y %.3f..%.3f\n", alo, ahi, ylo, yhi);
        }
        if (m && argc > 5)   /* the triangles of one texture: centre, normal, UVs */
            for (int i = 0; i < m->count; i++) {
                const gt4_triangle *t = &m->triangles[i];
                if (t->texture != atoi(argv[5])) continue;
                printf("  tri %d p %.2f %.2f %.2f n %.2f %.2f %.2f uv %.3f,%.3f %.3f,%.3f %.3f,%.3f\n", i,
                       (t->v[0].p[0] + t->v[1].p[0] + t->v[2].p[0]) / 3, (t->v[0].p[1] + t->v[1].p[1] + t->v[2].p[1]) / 3,
                       (t->v[0].p[2] + t->v[1].p[2] + t->v[2].p[2]) / 3, t->v[0].normal[0], t->v[0].normal[1], t->v[0].normal[2],
                       t->v[0].uv[0], t->v[0].uv[1], t->v[1].uv[0], t->v[1].uv[1], t->v[2].uv[0], t->v[2].uv[1]);
            }
        if (m && argc > 4) {   /* the textures to <dir>/tNN.ppm (colour) and tNN_a.pgm (the GS alpha, 128 = 1) */
            for (int t = 0; t < m->ntextures; t++) {
                const tex1_image *im = &m->textures[t];
                char name[512];
                FILE *o;
                if (!im->rgba) continue;
                printf("  texture %d: %dx%d (%dx%d) tcc %d tfx %d mag %d min %d wrap %d %d u %d..%d v %d..%d\n", t, im->width, im->height, im->tex_w, im->tex_h,
                       im->color_component, im->texture_function, im->magnify, im->minify, im->wrap_u, im->wrap_v, im->min_u, im->max_u, im->min_v, im->max_v);
                {   /* the UVs of the triangles that use it */
                    float lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f };
                    int n = 0;
                    for (int i = 0; i < m->count; i++)
                        if (m->triangles[i].texture == t)
                            for (int k = 0; k < 3; k++, n++)
                                for (int j = 0; j < 2; j++) {
                                    float x = m->triangles[i].v[k].uv[j];
                                    if (x < lo[j]) lo[j] = x;
                                    if (x > hi[j]) hi[j] = x;
                                }
                    if (n) printf("    uv u %.3f..%.3f v %.3f..%.3f\n", lo[0], hi[0], lo[1], hi[1]);
                }
                snprintf(name, sizeof name, "%s/t%02d.ppm", argv[4], t);
                if ((o = fopen(name, "wb"))) {
                    fprintf(o, "P6 %d %d 255\n", im->width, im->height);
                    for (int p = 0; p < im->width * im->height; p++) fwrite(im->rgba + p * 4, 1, 3, o);
                    fclose(o);
                }
                snprintf(name, sizeof name, "%s/t%02d_a.pgm", argv[4], t);
                if ((o = fopen(name, "wb"))) {
                    fprintf(o, "P5 %d %d 255\n", im->width, im->height);
                    for (int p = 0; p < im->width * im->height; p++) fputc(im->native_alpha ? im->native_alpha[p] : im->rgba[p * 4 + 3], o);
                    fclose(o);
                }
            }
        }
        gt4_model_free(m);
        free(bytes);
        gt4_vol_close(v);
        return m ? 0 : 1;
    }
    snprintf(path, sizeof path, "/crs/%s", argv[2]);
    uint8_t *data = gt4_vol_read(v, path, &size);
    if (!data) { fprintf(stderr, "cannot read %s\n", path); gt4_vol_close(v); return 1; }

    gt4_course c;
    if (!gt4_course_open(&c, data, size)) { fprintf(stderr, "not a GT4 course file\n"); free(data); gt4_vol_close(v); return 1; }
    printf("%s: %zu bytes, world model set at 0x%zx, runway at 0x%zx\n", path, size, c.world, c.runway);

    gt4_model_stats st;
    size_t model_offset=c.world;
    if (argc > 3) {
        char *end; unsigned long field = strtoul(argv[3], &end, 0);
        if (!*argv[3] || *end || field > 0xfc || (field & 3)) {
            free(data); gt4_vol_close(v); return 2;
        }
        uint32_t off; memcpy(&off, data + field, 4); model_offset = off;
        if (!off || off >= size) { free(data); gt4_vol_close(v); return 1; }
        printf("selected model set at 0x%zx (header +0x%lx)\n", model_offset, field);
    }
    gt4_model *world = gt4_model_decode_world(data + model_offset, size - model_offset, &st);
    if (world) {
        printf("world: %d triangles, %d textures, %d unsupported\n", world->count, world->ntextures, world->unsupported);
        float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
        for (int i = 0; i < world->count; i++)
            for (int k = 0; k < 3; k++)
                for (int j = 0; j < 3; j++) {
                    float x = world->triangles[i].v[k].p[j];
                    if (x < lo[j]) lo[j] = x;
                    if (x > hi[j]) hi[j] = x;
                }
        printf("world bounds: (%.0f, %.0f, %.0f) - (%.0f, %.0f, %.0f)\n", lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
        {   /* the colours as converted: vertex (1 = the GS's 128) and texels */
            double vs[3] = { 0 }, ts[3] = { 0 }, tn = 0;
            for (int i = 0; i < world->count; i++)
                for (int k = 0; k < 3; k++)
                    for (int j = 0; j < 3; j++) vs[j] += world->triangles[i].v[k].rgb[j];
            for (int t = 0; t < world->ntextures; t++) {
                const tex1_image *im = &world->textures[t];
                for (int p = 0; im->rgba && p < im->width * im->height; p++, tn++)
                    for (int j = 0; j < 3; j++) ts[j] += im->rgba[p * 4 + j];
            }
            {   /* the draw states and how many triangles each */
                enum { MAXS = 64 };
                gt4_draw_state st[MAXS]; int be[MAXS], tex[MAXS], cnt[MAXS], ns = 0;
                for (int i = 0; i < world->count; i++) {
                    const gt4_triangle *t = &world->triangles[i];
                    int k = 0, hast = t->texture >= 0;
                    for (; k < ns; k++)
                        if (!memcmp(&st[k], &t->state, sizeof st[k]) && be[k] == t->blend_enabled && tex[k] == hast) break;
                    if (k == ns) {
                        if (ns == MAXS) continue;
                        st[ns] = t->state; be[ns] = t->blend_enabled; tex[ns] = hast; cnt[ns] = 0; ns++;
                    }
                    cnt[k]++;
                }
                for (int k = 0; k < ns; k++)
                    printf("  state: %6d tris, tex %d abe %d blend 0x%02x fix %d zw %d zt %d zf %d at %d af %d ar %d fail %d date %d/%d cull %d mask %08x fog %d (%d %d %d)\n",
                           cnt[k], tex[k], be[k], st[k].blend, st[k].fixed_alpha, st[k].depth_write, st[k].depth_test, st[k].depth_func,
                           st[k].alpha_test, st[k].alpha_func, st[k].alpha_ref, st[k].alpha_fail, st[k].destination_test,
                           st[k].destination_bit, st[k].cull, (unsigned)st[k].color_mask,
                           st[k].fog_override, st[k].fog_rgb[0], st[k].fog_rgb[1], st[k].fog_rgb[2]);
            }
            for (int t = 0; t < world->ntextures && t < 12; t++) {
                const tex1_image *im = &world->textures[t];
                int mx[3] = { 0 }, n = im->rgba ? im->width * im->height : 0;
                for (int p = 0; p < n; p++)
                    for (int j = 0; j < 3; j++) if (im->rgba[p * 4 + j] > mx[j]) mx[j] = im->rgba[p * 4 + j];
                {   /* the GS alpha (128 is one) of the texture */
                    int amin = 255, amax = 0;
                    double asum = 0;
                    for (int q = 0; q < n && im->native_alpha; q++) {
                        int a = im->native_alpha[q];
                        if (a < amin) amin = a;
                        if (a > amax) amax = a;
                        asum += a;
                    }
                    printf("  texture %d: %dx%d, max texel %d %d %d, tcc %d tfx %d, alpha %d..%d mean %.1f\n", t, im->width, im->height,
                           mx[0], mx[1], mx[2], im->color_component, im->texture_function, n && im->native_alpha ? amin : -1, amax, n ? asum / n : 0);
                }
            }
            if (world->count) {
                double amin = 9, amax = -9;
                for (int i = 0; i < world->count; i++)
                    for (int j = 0; j < 3; j++) {
                        double a = world->triangles[i].v[j].alpha;
                        if (a < amin) amin = a;
                        if (a > amax) amax = a;
                    }
                printf("vertex alpha %.3f..%.3f\n", amin, amax);
            }
            if (world->count)
                printf("mean vertex colour %.3f %.3f %.3f, mean texel %.0f %.0f %.0f\n",
                       vs[0] / (world->count * 3.0), vs[1] / (world->count * 3.0), vs[2] / (world->count * 3.0),
                       tn ? ts[0] / tn : 0, tn ? ts[1] / tn : 0, tn ? ts[2] / tn : 0);
        }
        printf("commands:");
        for (int k = 0; k < 256; k++) if (st.ops[k]) printf(" %d:%u", k, st.ops[k]);
        printf("\ncallbacks:");
        for (int k = 0; k < 256; k++) if (st.callbacks[k]) printf(" %d:%u", k, st.callbacks[k]);
        printf("\n");
        for (int k = 0; k < st.nsamples; k++) {
            int op = (int)st.samples[k][0], n = op == 15 || op == 16 ? 16 : op == 19 ? 4 : op >= 20 ? 1 : 3;
            printf("  op %d:", op);
            for (int j = 1; j <= n; j++) printf(" %g", st.samples[k][j]);
            printf("\n");
        }
    }
    else printf("world: conversion failed (op %d at 0x%zx, model.c line %d)\n", st.fail_op, st.fail_offset, st.fail_line);
    int success = world != NULL;
    gt4_model_free(world);

    printf("runway: length %.1f m, %d grid positions, %d road vertices, %d road triangles\n",
           c.length, c.grid_count, c.road_vertex_count, c.road_tri_count);
    for (int i = 0; i < c.grid_count && i < 6; i++) {
        gt4_grid g = gt4_course_grid(&c, i);
        float h = 0;
        int on_road = gt4_course_height(&c, g.pos[0], g.pos[2], g.pos[1] + 2, &h);
        printf("  grid %d: (%.2f, %.2f, %.2f) heading %.3f rad, road height %s %.2f\n",
               i, g.pos[0], g.pos[1], g.pos[2], g.heading, on_road ? "" : "(none)", h);
    }
    free(data);
    gt4_vol_close(v);
    return success ? 0 : 1;
}
