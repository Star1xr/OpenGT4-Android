/*
 * gt4vol: list or extract files from GT4.VOL, straight from the user's disc
 * image. Uses the same reader as the port (src/core/vol.c).
 *
 *   gt4vol <disc.iso|GT4.VOL> ls [dir]
 *   gt4vol <disc.iso|GT4.VOL> cat <path> <out_file>
 */
#include "vol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_entry(void *user, const char *name, int is_dir, uint32_t size)
{
    (void)user;
    if (is_dir)
        printf("%12s  %s/\n", "", name);
    else
        printf("%12u  %s\n", size, name);
}

int main(int argc, char **argv)
{
    const char *err = NULL;
    if (argc < 3) {
        fprintf(stderr, "usage: %s <disc.iso|GT4.VOL> ls [dir] | cat <path> <out>\n", argv[0]);
        return 2;
    }
    int movies=argc>3 && !strcmp(argv[argc-1],"--movies");
    if(movies)argc--;
    gt4_vol *v = movies?gt4_vol_open_movies(argv[1],&err):gt4_vol_open(argv[1], &err);
    if (!v) {
        fprintf(stderr, "cannot open volume: %s\n", err);
        return 1;
    }
    int rc = 0;
    if (!strcmp(argv[2], "ls")) {
        printf("%zu entries in the volume\n", gt4_vol_count(v));
        if (!gt4_vol_list(v, argc > 3 ? argv[3] : "/", print_entry, NULL)) {
            fprintf(stderr, "not a directory\n");
            rc = 1;
        }
    } else if (!strcmp(argv[2], "cat") && argc == 5) {
        size_t n = 0;
        uint8_t *d = gt4_vol_read(v, argv[3], &n);
        FILE *f = d ? fopen(argv[4], "wb") : NULL;
        if (!d || !f) {
            fprintf(stderr, "cannot read %s\n", argv[3]);
            rc = 1;
        } else {
            fwrite(d, 1, n, f);
            printf("%s: %zu bytes\n", argv[3], n);
        }
        if (f) fclose(f);
        free(d);
    } else {
        fprintf(stderr, "unknown command\n");
        rc = 2;
    }
    gt4_vol_close(v);
    return rc;
}
