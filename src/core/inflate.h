/* Raw deflate (no zlib header), as used by GT4's compressed files. */
#ifndef OPENGT4_INFLATE_H
#define OPENGT4_INFLATE_H

#include <stddef.h>

/* Returns the number of bytes written to dst, or (size_t)-1 on a broken stream. */
size_t core_inflate_raw(const void *src, size_t src_len, void *dst, size_t dst_len);

#endif
