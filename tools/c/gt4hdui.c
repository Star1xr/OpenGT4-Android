/*
 * gt4hdui: builds the local HD cache of the menu images (docs/UI_HD.md).
 * Every image of the disc's UI packages (.gpb) and of /piece is decoded,
 * upscaled by Real-ESRGAN (downloaded by the user, not part of OpenGT4) and
 * stored in the cache directory as <content hash>.hdi. Nothing leaves the
 * user's machine; the cache is regenerated from the disc.
 *
 *   gt4hdui <disc.iso> <cache dir> <realesrgan dir> [--model name] [--limit n]
 */
#include "gpb.h"
#include "tex1.h"
#include "ui_hd.h"
#include "vol.h"

#include "miniz.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_one(p) _mkdir(p)
#else
#include <dirent.h>
#define mkdir_one(p) mkdir(p, 0755)
#endif

typedef struct { char (*paths)[160]; int n, cap; } list;
static void push(list *l, const char *path)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 256;
        l->paths = realloc(l->paths, (size_t)l->cap * sizeof *l->paths);
        if (!l->paths) exit(1);
    }
    snprintf(l->paths[l->n++], 160, "%s", path);
}

typedef struct { gt4_vol *v; list *gpbs, *imgs; const char *dir; } walk_ctx;
static void walk(gt4_vol *v, const char *dir, list *gpbs, list *imgs);
static void visit(void *user, const char *name, int is_dir, uint32_t size)
{
    (void)size;
    walk_ctx *c = user;
    char path[160];
    snprintf(path, sizeof path, "%s/%s", strcmp(c->dir, "/") ? c->dir : "", name);
    if (is_dir) { walk(c->v, path, c->gpbs, c->imgs); return; }
    const char *ext = strrchr(name, '.');
    if (ext && !strcmp(ext, ".gpb")) push(c->gpbs, path);
    else if (ext && !strcmp(ext, ".img") && !strncmp(path, "/piece/", 7)) push(c->imgs, path);
}
static void walk(gt4_vol *v, const char *dir, list *gpbs, list *imgs)
{
    walk_ctx c = { v, gpbs, imgs, dir };
    gt4_vol_list(v, dir, visit, &c);
}

/* Transparent texels take their nearest opaque neighbour's colour, so the
   upscaler does not pull black into the edges (dark fringes). */
static void bleed(uint8_t *rgba, int w, int h)
{
    uint8_t *known = calloc((size_t)w * h, 1), *next = malloc((size_t)w * h);
    if (!known || !next) { free(known); free(next); return; }
    for (int i = 0; i < w * h; i++) known[i] = rgba[i * 4 + 3] >= 16;   /* near-clear texels often hold stray colours */
    for (int pass = 0; pass < 8; pass++) {
        int changed = 0;
        memcpy(next, known, (size_t)w * h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int i = y * w + x, sum[3] = { 0, 0, 0 }, n = 0;
                if (known[i]) continue;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= w || yy >= h || !known[yy * w + xx]) continue;
                        for (int k = 0; k < 3; k++) sum[k] += rgba[(yy * w + xx) * 4 + k];
                        n++;
                    }
                if (n) { for (int k = 0; k < 3; k++) rgba[i * 4 + k] = (uint8_t)(sum[k] / n); next[i] = 1; changed = 1; }
            }
        memcpy(known, next, (size_t)w * h);
        if (!changed) break;
    }
    free(known); free(next);
}

static int write_png(const char *path, const uint8_t *rgba, int w, int h)
{
    size_t len = 0;
    void *png = tdefl_write_image_to_png_file_in_memory(rgba, w, h, 4, &len);
    FILE *f = png ? fopen(path, "wb") : NULL;
    int ok = f && fwrite(png, 1, len, f) == len;
    if (f) fclose(f);
    mz_free(png);
    return ok;
}

/* Minimal PNG reader for the upscaler's output: 8-bit RGB or RGBA,
   not interlaced. */
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static uint8_t *read_png(const char *path, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return NULL; }
    fclose(f);
    uint8_t *idat = NULL, *out = NULL, *raw = NULL;
    size_t idat_n = 0;
    int width = 0, height = 0, type = -1;
    if (n < 8 || memcmp(d, "\x89PNG\r\n\x1a\n", 8)) goto done;
    for (size_t at = 8; at + 12 <= (size_t)n;) {
        uint32_t len = be32(d + at);
        if (at + 12 + len > (size_t)n) break;
        const uint8_t *type_at = d + at + 4, *body = d + at + 8;
        if (!memcmp(type_at, "IHDR", 4) && len >= 13) {
            width = (int)be32(body); height = (int)be32(body + 4);
            if (body[8] != 8 || body[12] != 0) goto done;
            type = body[9];
        } else if (!memcmp(type_at, "IDAT", 4)) {
            uint8_t *grown = realloc(idat, idat_n + len);
            if (!grown) goto done;
            idat = grown; memcpy(idat + idat_n, body, len); idat_n += len;
        } else if (!memcmp(type_at, "IEND", 4)) break;
        at += 12 + len;
    }
    int ch = type == 6 ? 4 : type == 2 ? 3 : 0;
    if (!ch || width <= 0 || height <= 0 || !idat) goto done;
    size_t raw_n = 0, stride = (size_t)width * ch;
    raw = tinfl_decompress_mem_to_heap(idat, idat_n, &raw_n, TINFL_FLAG_PARSE_ZLIB_HEADER);
    if (!raw || raw_n < (stride + 1) * height) goto done;
    out = malloc((size_t)width * height * 4);
    uint8_t *prev = calloc(stride, 1), *cur = malloc(stride);
    if (!out || !prev || !cur) { free(out); out = NULL; free(prev); free(cur); goto done; }
    for (int y = 0; y < height; y++) {
        const uint8_t *line = raw + y * (stride + 1);
        int filter = line[0];
        for (size_t i = 0; i < stride; i++) {
            int a = i >= (size_t)ch ? cur[i - ch] : 0, b = prev[i], c = i >= (size_t)ch ? prev[i - ch] : 0, x = line[1 + i];
            if (filter == 1) x += a;
            else if (filter == 2) x += b;
            else if (filter == 3) x += (a + b) / 2;
            else if (filter == 4) {
                int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
                x += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
            }
            cur[i] = (uint8_t)x;
        }
        for (int x = 0; x < width; x++) {
            uint8_t *o = out + ((size_t)y * width + x) * 4;
            memcpy(o, cur + (size_t)x * ch, 3);
            o[3] = ch == 4 ? cur[(size_t)x * ch + 3] : 255;
        }
        memcpy(prev, cur, stride);
    }
    free(prev); free(cur);
    *w = width; *h = height;
done:
    free(d); free(idat); mz_free(raw);
    return out;
}

/* Box-reduces an image by an integer factor (the upscaler works at 4x;
   large images are kept at 2x to bound memory). */
static uint8_t *reduce(const uint8_t *rgba, int w, int h, int f, int *ow, int *oh)
{
    *ow = w / f; *oh = h / f;
    uint8_t *o = malloc((size_t)*ow * *oh * 4);
    if (!o) return NULL;
    for (int y = 0; y < *oh; y++)
        for (int x = 0; x < *ow; x++)
            for (int k = 0; k < 4; k++) {
                int s = 0;
                for (int j = 0; j < f; j++) for (int i = 0; i < f; i++) s += rgba[((size_t)(y * f + j) * w + x * f + i) * 4 + k];
                o[((size_t)y * *ow + x) * 4 + k] = (uint8_t)(s / (f * f));
            }
    return o;
}

/* The PNG file names in a directory. */
static void list_pngs(const char *dir, list *out)
{
#ifdef _WIN32
    char pattern[700];
    snprintf(pattern, sizeof pattern, "%s/*.png", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do push(out, fd.cFileName); while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL) { const char *x = strrchr(e->d_name, '.'); if (x && !strcmp(x, ".png")) push(out, e->d_name); }
    if (d) closedir(d);
#endif
}

static int exists(const char *path) { struct stat st; return stat(path, &st) == 0; }
static int gt4_ui_hd_write(const char *path, const uint8_t *rgba, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t zn = 0;
    void *z = tdefl_compress_mem_to_heap(rgba, (size_t)w * h * 4, &zn, TDEFL_DEFAULT_MAX_PROBES);   /* raw deflate */
    if (!z) { fclose(f); remove(path); return 0; }
    uint8_t head[12] = { 'H', 'D', 'I', '2', (uint8_t)w, (uint8_t)(w >> 8), (uint8_t)(w >> 16), (uint8_t)(w >> 24),
                         (uint8_t)h, (uint8_t)(h >> 8), (uint8_t)(h >> 16), (uint8_t)(h >> 24) };
    int ok = fwrite(head, 1, 12, f) == 12 && fwrite(z, 1, zn, f) == zn;
    mz_free(z);
    fclose(f);
    if (!ok) remove(path);
    return ok;
}

/* The upscaler's alpha is unreliable (faint noise where the original is
   clear, which shows the bled colours as speckles; near zero on some logos,
   which vanish): keep its colours, take the alpha from the original,
   resampled bilinearly. */
static void original_alpha(uint8_t *hd, int w, int h, const uint8_t *orig, int ow, int oh)
{
    for (int y = 0; y < h; y++) {
        float sy = (y + .5f) * oh / h - .5f;
        int y0 = sy < 0 ? 0 : (int)sy, y1 = y0 + 1 < oh ? y0 + 1 : oh - 1;
        float fy = sy - y0; if (fy < 0) fy = 0;
        for (int x = 0; x < w; x++) {
            float sx = (x + .5f) * ow / w - .5f;
            int x0 = sx < 0 ? 0 : (int)sx, x1 = x0 + 1 < ow ? x0 + 1 : ow - 1;
            float fx = sx - x0; if (fx < 0) fx = 0;
            float a = (orig[((size_t)y0 * ow + x0) * 4 + 3] * (1 - fx) + orig[((size_t)y0 * ow + x1) * 4 + 3] * fx) * (1 - fy) +
                      (orig[((size_t)y1 * ow + x0) * 4 + 3] * (1 - fx) + orig[((size_t)y1 * ow + x1) * 4 + 3] * fx) * fy;
            hd[((size_t)y * w + x) * 4 + 3] = (uint8_t)(a + .5f);
        }
    }
}

/* Does an upscaled image still look like its original? Its colours,
   reduced back to the original size, against the original's where it is
   fairly opaque. A GPU fault (device lost while another program uses the
   GPU) can leave wrong colours or noise that are not blank; those fail. */
static int looks_like(const uint8_t *hd, int w, int h, const uint8_t *orig, int ow, int oh)
{
    double err = 0, weight = 0;
    int fx = w / ow, fy = h / oh;
    if (fx < 1 || fy < 1) return 0;
    for (int y = 0; y < oh; y++)
        for (int x = 0; x < ow; x++) {
            const uint8_t *o = orig + ((size_t)y * ow + x) * 4;
            if (o[3] < 64) continue;
            double c[3] = { 0, 0, 0 };
            for (int j = 0; j < fy; j++)
                for (int i = 0; i < fx; i++)
                    for (int k = 0; k < 3; k++) c[k] += hd[((size_t)(y * fy + j) * w + x * fx + i) * 4 + k];
            double a = o[3] / 255.0;
            for (int k = 0; k < 3; k++) err += a * fabs(c[k] / (fx * fy) - o[k]);
            weight += 3 * a;
        }
    return weight < 1 || err / weight < 24;
}

/* Upscaled PNGs in out_dir -> <cache>/<hash>.hdi: 4x for images up to 256
   texels, 2x for larger ones (they already have detail; this bounds the
   cache). A converted image's source leaves src_dir. Returns the count. */
static int convert_outputs(const char *cache, const char *src_dir, const char *out_dir)
{
    int stored = 0;
    char path[800], src[800];
    list outs = { 0 };
    list_pngs(out_dir, &outs);
    for (int i = 0; i < outs.n; i++) {
        unsigned long long hash = 0;
        int w = 0, h = 0, rw = 0, rh = 0;
        snprintf(path, sizeof path, "%s/%s", out_dir, outs.paths[i]);
        uint8_t *px = sscanf(outs.paths[i], "%16llx", &hash) == 1 ? read_png(path, &w, &h) : NULL;
        remove(path);
        if (!px) continue;
        /* A lost GPU device leaves blank output: not cached, retried. */
        int blank = 1;
        for (size_t k = 0; k < (size_t)w * h * 4 && blank; k++) blank = px[k] == 0;
        if (blank) { free(px); continue; }
        uint8_t *keep = px;
        if (w > 1024 || h > 1024) { keep = reduce(px, w, h, 2, &rw, &rh); w = rw; h = rh; }
        snprintf(path, sizeof path, "%s/%016llx.hdi", cache, hash);
        snprintf(src, sizeof src, "%s/%s", src_dir, outs.paths[i]);
        int sw = 0, sh = 0, good = 0;
        uint8_t *orig = keep ? read_png(src, &sw, &sh) : NULL;
        if (orig && looks_like(keep, w, h, orig, sw, sh)) { original_alpha(keep, w, h, orig, sw, sh); good = 1; }
        free(orig);
        if (good && gt4_ui_hd_write(path, keep, w, h)) { stored++; remove(src); }
        if (keep != px) free(keep);
        free(px);
    }
    free(outs.paths);
    return stored;
}

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: %s <disc.iso> <cache dir> <realesrgan dir> [--model name] [--limit n]\n", argv[0]); return 2; }
    const char *cache = argv[2], *tool = argv[3], *model = "realesr-animevideov3";
    int limit = 0, fix_alpha = 0, verify = 0;
    for (int i = 4; i < argc; i++) { if (!strcmp(argv[i], "--fix-alpha")) fix_alpha = 1; if (!strcmp(argv[i], "--verify")) verify = 1; }
    for (int i = 4; i + 1 < argc; i++) {
        if (!strcmp(argv[i], "--model")) model = argv[++i];
        else if (!strcmp(argv[i], "--limit")) limit = atoi(argv[++i]);
    }
    const char *err = NULL;
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "cannot open volume: %s\n", err); return 1; }
    char in_dir[600], out_dir[600], path[800];
    snprintf(in_dir, sizeof in_dir, "%s/work_in", cache);
    snprintf(out_dir, sizeof out_dir, "%s/work_out", cache);
    mkdir_one(cache); mkdir_one(in_dir); mkdir_one(out_dir);

    list gpbs = { 0 }, imgs = { 0 };
    walk(v, "/", &gpbs, &imgs);
    printf("%d packages, %d piece images\n", gpbs.n, imgs.n);
    /* Pass 1: every distinct image not in the cache yet goes to work_in. */
    int queued = 0, seen = 0;
    for (int g = 0; g <= gpbs.n + imgs.n && (!limit || queued < limit); g++) {
        size_t size = 0;
        uint8_t *pkg = g < gpbs.n ? gt4_vol_read(v, gpbs.paths[g], &size) : NULL;
        int count = pkg ? gpb_count(pkg, size) : g < gpbs.n + imgs.n ? 1 : 0;
        for (int i = 0; i < count && (!limit || queued < limit); i++) {
            size_t n = 0;
            uint8_t *data = pkg ? gpb_load(pkg, size, gpb_name(pkg, size, i), &n) : gt4_vol_read(v, imgs.paths[g - gpbs.n], &n);
            tex1_image *ti = NULL;
            int k = data ? tex1_decode(data, n, &ti) : -1;
            free(data);
            if (k > 0 && ti[0].width >= 8 && ti[0].height >= 8 && ti[0].width <= 1024 && ti[0].height <= 1024) {
                seen++;
                uint64_t hash = gt4_ui_hd_hash(ti[0].rgba, ti[0].width, ti[0].height);
                snprintf(path, sizeof path, "%s/%016llx.hdi", cache, (unsigned long long)hash);
                if (verify) {
                    /* --verify: drop cached images that no longer look like their
                       original (a later run makes them again) */
                    int hw = 0, hh = 0;
                    uint8_t *hd = gt4_ui_hd_read(path, &hw, &hh);
                    if (hd && !looks_like(hd, hw, hh, ti[0].rgba, ti[0].width, ti[0].height)) { remove(path); queued++; }
                    free(hd);
                } else if (fix_alpha) {
                    /* --fix-alpha: redo the alpha of a cached image from its original */
                    int hw = 0, hh = 0;
                    uint8_t *hd = gt4_ui_hd_read(path, &hw, &hh);
                    if (hd) { original_alpha(hd, hw, hh, ti[0].rgba, ti[0].width, ti[0].height); if (gt4_ui_hd_write(path, hd, hw, hh)) queued++; free(hd); }
                } else if (!exists(path)) {
                    snprintf(path, sizeof path, "%s/%016llx.png", in_dir, (unsigned long long)hash);
                    if (!exists(path)) {
                        int empty = 1;
                        for (size_t z = 0; z < (size_t)ti[0].width * ti[0].height * 4 && empty; z++) empty = ti[0].rgba[z] == 0;
                        if (empty) { tex1_free(ti, k); continue; }
                        bleed(ti[0].rgba, ti[0].width, ti[0].height);
                        if (write_png(path, ti[0].rgba, ti[0].width, ti[0].height)) queued++;
                    }
                }
            }
            tex1_free(ti, k);
        }
        free(pkg);
    }
    gt4_vol_close(v);
    if (verify) { printf("%d images, %d bad cached images removed\n", seen, queued); return 0; }
    if (fix_alpha) { printf("%d images, %d alpha channels redone\n", seen, queued); return 0; }
    printf("%d images, %d to upscale\n", seen, queued);
    /* Batches of 32 through Real-ESRGAN, each converted at once; a batch
       the GPU dropped (device lost: another program's load, e.g. a game)
       leaves blank output and is simply run again, three tries at most. */
    char batch_dir[600];
    snprintf(batch_dir, sizeof batch_dir, "%s/work_batch", cache);
    mkdir_one(batch_dir);
    int stored = 0;
    for (int tries = 0; tries < 3; tries++) {
        list pending = { 0 };
        list_pngs(in_dir, &pending);
        if (!pending.n) { free(pending.paths); break; }
        if (tries) printf("retrying %d images\n", pending.n);
        for (int b = 0; b < pending.n; b += 32) {
            for (int i = b; i < b + 32 && i < pending.n; i++) {
                char from[800], to[800];
                snprintf(from, sizeof from, "%s/%s", in_dir, pending.paths[i]);
                snprintf(to, sizeof to, "%s/%s", batch_dir, pending.paths[i]);
                rename(from, to);
            }
            char cmd[2400];
            snprintf(cmd, sizeof cmd, "\"\"%s/realesrgan-ncnn-vulkan.exe\" -i \"%s\" -o \"%s\" -n %s -s 4 -t 128 -f png -m \"%s/models\" > nul 2>&1\"",
                     tool, batch_dir, out_dir, model, tool);
            printf("upscaling %d-%d of %d (%s)\n", b + 1, b + 32 < pending.n ? b + 32 : pending.n, pending.n, model);
            fflush(stdout);
            system(cmd);
            stored += convert_outputs(cache, batch_dir, out_dir);
            /* what did not convert goes back for the next try */
            list left = { 0 };
            list_pngs(batch_dir, &left);
            for (int i = 0; i < left.n; i++) {
                char from[800], to[800];
                snprintf(from, sizeof from, "%s/%s", batch_dir, left.paths[i]);
                snprintf(to, sizeof to, "%s/%s", in_dir, left.paths[i]);
                rename(from, to);
            }
            free(left.paths);
        }
        free(pending.paths);
    }
    printf("stored %d HD images in %s\n", stored, cache);
    free(gpbs.paths); free(imgs.paths);
    return 0;
}
