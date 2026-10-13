/*
 * GT4 "gpb2" packages: named assets (mostly Tex1 images) used by the UI
 * projects. Layout: magic "gpb2", pair count at 0x0c, then 16-byte pairs
 * {name offset, data offset, size, 0} from 0x10. Checked against the boot
 * project's packages (docs/UI_FORMAT.md).
 */
#ifndef OPENGT4_GPB_H
#define OPENGT4_GPB_H

#include <stddef.h>
#include <stdint.h>

/* Finds an asset by name (e.g. "image/boot/lang_bg_4.png"). Returns a pointer
   into pkg (not a copy) and its size, or NULL. */
const uint8_t *gpb_find(const uint8_t *pkg, size_t pkg_size, const char *name, size_t *size);

/* Same lookup, returning a malloc'd copy with packed (compressed) assets
   inflated. Free with free(). */
uint8_t *gpb_load(const uint8_t *pkg, size_t pkg_size, const char *name, size_t *size);

/* Number of assets, and the name of asset i (or NULL). */
int         gpb_count(const uint8_t *pkg, size_t pkg_size);
const char *gpb_name(const uint8_t *pkg, size_t pkg_size, int i);

#endif /* OPENGT4_GPB_H */
