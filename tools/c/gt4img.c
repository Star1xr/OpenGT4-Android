/*
 * gt4img: export a GT4 image to PNG, using the port's own decoder.
 *
 *   gt4img <disc.iso> <volume path> <out prefix>
 *       e.g. /advertise/us/main.img  -> out_0.png, out_1.png, ...
 *   gt4img <disc.iso> <volume path of a .gpb> <asset name> <out prefix>
 *       e.g. /projects/boot/US/LanguageRoot.gpb image/boot/lang_bg_4.png
 *   gt4img --file <local .img> <out prefix>
 *       a Tex1 file already on disk (e.g. one embedded in the boot ELF)
 */
#include "gpb.h"
#include "tex1.h"
#include "vol.h"

#include "miniz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int save_png(const char *path, const tex1_image *im)
{
    size_t len = 0;
    void *png = tdefl_write_image_to_png_file_in_memory(im->rgba, im->width, im->height, 4, &len);
    FILE *f = png ? fopen(path, "wb") : NULL;
    if (!f) { mz_free(png); return 0; }
    fwrite(png, 1, len, f);
    fclose(f);
    mz_free(png);
    return 1;
}

static int export_all(const uint8_t *img, size_t img_size, const char *prefix)
{
    tex1_image *ims = NULL;
    int count = tex1_decode(img, img_size, &ims);
    if (count < 0) { fprintf(stderr, "not a Tex1 image\n"); return 0; }
    for (int i = 0; i < count; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s_%d.png", prefix, i);
        printf("%s: %dx%d (texture %dx%d)\n", path, ims[i].width, ims[i].height, ims[i].tex_w, ims[i].tex_h);
        if (!save_png(path, &ims[i])) fprintf(stderr, "cannot write %s\n", path);
    }
    tex1_free(ims, count);
    return 1;
}

int main(int argc, char **argv)
{
    const char *err = NULL;
    if (argc == 4 && !strcmp(argv[1], "--file")) {
        FILE *f = fopen(argv[2], "rb");
        if (!f) { fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *data = n > 0 && n < (64l << 20) ? malloc((size_t)n) : NULL;
        int ok = data && fread(data, 1, (size_t)n, f) == (size_t)n;
        fclose(f);
        ok = ok && export_all(data, (size_t)n, argv[3]);
        free(data);
        return ok ? 0 : 1;
    }
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s <disc.iso> <path.img> <out_prefix>\n"
                        "       %s <disc.iso> <path.gpb> <asset> <out_prefix>\n", argv[0], argv[0]);
        return 2;
    }
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "cannot open volume: %s\n", err); return 1; }
    size_t n = 0;
    uint8_t *file = gt4_vol_read(v, argv[2], &n);
    if (!file) { fprintf(stderr, "cannot read %s\n", argv[2]); gt4_vol_close(v); return 1; }
    const uint8_t *img = file;
    size_t img_size = n;
    const char *prefix = argv[3];
    if (argc == 5) {
        uint8_t *asset = gpb_load(file, n, argv[3], &img_size);
        if (asset) { free(file); file = asset; }
        img = asset;
        prefix = argv[4];
        if (!img) {
            fprintf(stderr, "no asset %s; the package has:\n", argv[3]);
            for (int i = 0; i < gpb_count(file, n); i++) fprintf(stderr, "  %s\n", gpb_name(file, n, i));
            free(file); gt4_vol_close(v); return 1;
        }
    }
    int ok = export_all(img, img_size, prefix);
    free(file);
    gt4_vol_close(v);
    return ok ? 0 : 1;
}
