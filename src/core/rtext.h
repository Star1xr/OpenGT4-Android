#ifndef OPENGT4_RTEXT_H
#define OPENGT4_RTEXT_H
#include <stddef.h>
#include <stdint.h>
typedef struct { const uint8_t *data; size_t size; unsigned pages; } gt4_rtext;
/* Validates the entire RT03 table. The caller keeps input bytes alive. */
int gt4_rtext_open(gt4_rtext *rt, const uint8_t *data, size_t size);
const char *gt4_rtext_get(const gt4_rtext *rt, const char *page, const char *key);
/* RT03 on the supported US disc stores UTF-8. Invalid input consumes one
   byte and yields U+FFFD; this never reads beyond a terminating NUL. */
unsigned gt4_text_next(const char **text);
#endif
