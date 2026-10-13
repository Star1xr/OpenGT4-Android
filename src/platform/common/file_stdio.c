/* plat_file on the C standard library, with 64-bit offsets. */
#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct plat_file {
    FILE    *fp;
    uint64_t size;
};

#ifdef _WIN32
#define SEEK64(f, o) _fseeki64((f), (long long)(o), SEEK_SET)
#define END64(f)     _fseeki64((f), 0, SEEK_END)
#define TELL64(f)    ((uint64_t)_ftelli64(f))
#else
#define SEEK64(f, o) fseeko((f), (off_t)(o), SEEK_SET)
#define END64(f)     fseeko((f), 0, SEEK_END)
#define TELL64(f)    ((uint64_t)ftello(f))
#endif

plat_file *plat_file_open(const char *path)
{
    plat_file *f = calloc(1, sizeof *f);
    if (!f) return NULL;
#ifdef _WIN32
    /* P: file selectors return UTF-8; use Windows wide paths for ISO names. */
    wchar_t wide[32768];
    f->fp = path && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,32768)?_wfopen(wide,L"rb"):NULL;
#else
    f->fp = path?fopen(path, "rb"):NULL;
#endif
    if (!f->fp) { free(f); return NULL; }
    END64(f->fp);
    f->size = TELL64(f->fp);
    return f;
}

uint64_t plat_file_size(plat_file *f) { return f->size; }

size_t plat_file_read_at(plat_file *f, uint64_t offset, void *dst, size_t n)
{
    if (offset > f->size || SEEK64(f->fp, offset))
        return 0;
    return fread(dst, 1, n, f->fp);
}

void plat_file_close(plat_file *f)
{
    if (!f) return;
    fclose(f->fp);
    free(f);
}

#ifndef _WIN32
#include <sys/statvfs.h>
#endif

int plat_disk_free(const char *path, uint64_t *bytes)
{
#ifdef _WIN32
    ULARGE_INTEGER avail;
    if (!path || !bytes || !GetDiskFreeSpaceExA(path, &avail, NULL, NULL)) return 0;
    *bytes = avail.QuadPart;
#else
    struct statvfs s;
    if (!path || !bytes || statvfs(path, &s)) return 0;
    *bytes = (uint64_t)s.f_bavail * s.f_frsize;
#endif
    return 1;
}
