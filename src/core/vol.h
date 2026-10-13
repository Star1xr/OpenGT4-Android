/*
 * GT4.VOL reader: the game's data volume ("RoFS"), read directly from the
 * user's disc image or from a copied GT4.VOL. Replaces the PS2 path
 * (fileio/CDVD over the IOP) with plain file reads. Format: docs/VOL_FORMAT.md.
 */
#ifndef OPENGT4_VOL_H
#define OPENGT4_VOL_H

#include <stddef.h>
#include <stdint.h>

typedef struct gt4_vol gt4_vol;

/* path: a disc image (.iso) or a GT4.VOL file. NULL on error; *err says why. */
gt4_vol *gt4_vol_open(const char *path, const char **err);
gt4_vol *gt4_vol_open_movies(const char *path, const char **err);
void     gt4_vol_close(gt4_vol *v);
/* Borrowed source filename, for assets stored outside GT4.VOL (CORE.GT4). */
const char *gt4_vol_source(const gt4_vol *v);

/* Number of entries (files and directories) in the volume. */
size_t   gt4_vol_count(const gt4_vol *v);

/* Reads a whole file, e.g. "/advertise/us/main.img". Returns a malloc'd
   buffer (free with free()) and its size, or NULL if not found or broken. */
uint8_t *gt4_vol_read(gt4_vol *v, const char *path, size_t *size);

/* Reads a file from the ISO 9660 root of a disc image, outside GT4.VOL
   (e.g. "SCUS_973.28", the boot executable). NULL if the path is not a
   disc image or the file is missing. Free with free(). */
uint8_t *gt4_disc_read_root(const char *path, const char *name, size_t *size);
/* The same for a file in a directory of the disc, e.g. "IRX/LIBSD.IRX". */
uint8_t *gt4_disc_read_file(const char *path, const char *name, size_t *size);

/* Calls fn for each entry under directory `dir` ("/" for the root). */
typedef void (*gt4_vol_list_fn)(void *user, const char *name, int is_dir, uint32_t size);
int      gt4_vol_list(gt4_vol *v, const char *dir, gt4_vol_list_fn fn, void *user);

#endif /* OPENGT4_VOL_H */
