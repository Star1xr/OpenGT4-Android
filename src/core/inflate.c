#include "inflate.h"

#include "miniz.h"

/* Streaming inflate in raw mode (negative window bits = no zlib header).
   GT4's index pages are not terminated streams: a stored block with exactly
   the page's bytes, then no final block. So filling the output exactly counts
   as success, not only reaching the end of the stream. */
size_t core_inflate_raw(const void *src, size_t src_len, void *dst, size_t dst_len)
{
    mz_stream s = {0};
    int rc;
    if (mz_inflateInit2(&s, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
        return (size_t)-1;
    s.next_in = (const unsigned char *)src;
    s.avail_in = (unsigned int)src_len;
    s.next_out = (unsigned char *)dst;
    s.avail_out = (unsigned int)dst_len;
    /* SYNC_FLUSH, not FINISH: with FINISH, miniz reports input that ends before
       a final block as a data error, and these streams end with a sync-flush
       marker (an empty stored block), never a final block. */
    rc = mz_inflate(&s, MZ_SYNC_FLUSH);
    size_t out = dst_len - s.avail_out;
    mz_inflateEnd(&s);
    if (rc == MZ_STREAM_END || (out == dst_len && (rc == MZ_OK || rc == MZ_BUF_ERROR)))
        return out;
    return (size_t)-1;
}
