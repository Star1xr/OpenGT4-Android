/*
 * GT4.VOL reader. Format notes and how each constant was checked:
 * docs/VOL_FORMAT.md.
 */
#include "vol.h"

#include "inflate.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <string.h>

#define PAGE          0x800u
#define RoFS_MAGIC    0xACB990ADu     /* bitwise NOT of "RoFS" */
#define GT4_TOC_PAGE  0x2159u         /* real index; page 0 holds a decoy GT3 index */
#define OFFSET_KEY    0x14ac327au     /* index page offsets: v ^ (i*KEY + KEY) */
#define TOC_XOR       0x55u           /* index pages: inflate, then XOR every byte */
#define DATA_MAGIC    0xFFF7EEC5u     /* compressed file: magic, -size, raw deflate */

typedef struct {
    uint32_t parent;       /* node ID of the parent directory */
    char    *name;
    uint8_t  type;         /* 0 directory, 1 file, 2 compressed file */
    uint32_t node;         /* directories: own node ID */
    uint32_t page;         /* files: first page, relative to the data area */
    uint32_t size;         /* files: real size */
    uint32_t csize;        /* compressed files: size on disc */
} vol_entry;

struct gt4_vol {
    plat_file *f;
    char *source;
    uint64_t   base;       /* offset of GT4.VOL inside the file */
    uint64_t   data;       /* offset of the data area */
    vol_entry *e;
    size_t     n, cap;
};

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static int read_at(gt4_vol *v, uint64_t off, void *dst, size_t n)
{
    return plat_file_read_at(v->f, v->base + off, dst, n) == n;
}

/* A file in the ISO 9660 root directory of the disc's first or second layer. */
static int find_in_iso(plat_file *f, int second_layer, const char *name, uint64_t *offset, uint32_t *file_size)
{
    uint8_t pvd[2048], dir[2048 * 4];
    if (plat_file_read_at(f, 16 * 2048, pvd, sizeof pvd) != sizeof pvd || memcmp(pvd + 1, "CD001", 5))
        return 0;
    uint64_t layer=0;
    size_t name_len=strlen(name);
    if(second_layer) {
        uint32_t sectors=le32(pvd+80);
        if(sectors<16)return 0;
        layer=(uint64_t)sectors-16;
        if(plat_file_read_at(f,(layer+16)*2048,pvd,sizeof pvd)!=sizeof pvd ||
           memcmp(pvd+1,"CD001",5))return 0;
    }
    uint32_t lba = le32(pvd + 156 + 2), size = le32(pvd + 156 + 10);
    if (size > sizeof dir)
        size = sizeof dir;
    if (plat_file_read_at(f, (layer+lba) * 2048, dir, size) != size)
        return 0;
    for (uint32_t pos = 0; pos < size;) {
        uint8_t len = dir[pos];
        if (!len) { pos = (pos / 2048 + 1) * 2048; continue; }
        if (len>=33 && pos+len<=size && dir[pos + 32] >= name_len &&
            name_len<=len-33 && !memcmp(dir + pos + 33, name, name_len)) {
            *offset = (layer+le32(dir + pos + 2)) * 2048;
            *file_size = le32(dir + pos + 10);
            return 1;
        }
        pos += len;
    }
    return 0;
}

/* A disc image: find GT4.VOL in the ISO 9660 root directory. */
static int locate_in_iso(gt4_vol *v, int movies)
{
    uint32_t size;
    return find_in_iso(v->f, movies, movies ? "GT4L1.VOL" : "GT4.VOL", &v->base, &size);
}

uint8_t *gt4_disc_read_root(const char *path, const char *name, size_t *size)
{
    plat_file *f = plat_file_open(path);
    uint64_t offset = 0;
    uint32_t n = 0;
    uint8_t *data = NULL;
    if (f && find_in_iso(f, 0, name, &offset, &n) && n && n <= 64u << 20 && (data = malloc(n))) {
        if (plat_file_read_at(f, offset, data, n) != n) { free(data); data = NULL; }
    }
    if (f) plat_file_close(f);
    if (data && size) *size = n;
    return data;
}

/* A file in a directory of the ISO 9660 tree, "IRX/LIBSD.IRX": each
   component looked up in its directory's records, the file's version
   (";1") ignored. */
uint8_t *gt4_disc_read_file(const char *path, const char *name, size_t *size)
{
    plat_file *f = plat_file_open(path);
    uint8_t pvd[2048], *dir = NULL, *data = NULL;
    if (!f) return NULL;
    if (plat_file_read_at(f, 16 * 2048, pvd, sizeof pvd) != sizeof pvd || memcmp(pvd + 1, "CD001", 5)) goto done;
    uint32_t lba = le32(pvd + 156 + 2), length = le32(pvd + 156 + 10);
    for (const char *part = name; *part;) {
        const char *end = strchr(part, '/');
        size_t n = end ? (size_t)(end - part) : strlen(part);
        if (!length || length > 1u << 20 || !(dir = malloc(length)) ||
            plat_file_read_at(f, (uint64_t)lba * 2048, dir, length) != length) goto done;
        int found = 0;
        for (uint32_t pos = 0; pos < length;) {
            uint8_t len = dir[pos];
            if (!len) { pos = (pos / 2048 + 1) * 2048; continue; }
            if (len >= 33 && pos + len <= length && dir[pos + 32] >= n && n <= (size_t)len - 33 &&
                !memcmp(dir + pos + 33, part, n) && (dir[pos + 32] == n || dir[pos + 33 + n] == ';')) {
                lba = le32(dir + pos + 2);
                length = le32(dir + pos + 10);
                found = 1;
                break;
            }
            pos += len;
        }
        free(dir);
        dir = NULL;
        if (!found) goto done;
        part += n + (end != NULL);
    }
    if (length && length <= 64u << 20 && (data = malloc(length)) &&
        plat_file_read_at(f, (uint64_t)lba * 2048, data, length) != length) { free(data); data = NULL; }
    if (data && size) *size = length;
done:
    free(dir);
    plat_file_close(f);
    return data;
}

static int add(gt4_vol *v, const vol_entry *e)
{
    if (v->n == v->cap) {
        size_t cap = v->cap ? v->cap * 2 : 1024;
        vol_entry *ne = realloc(v->e, cap * sizeof *ne);
        if (!ne) return 0;
        v->e = ne; v->cap = cap;
    }
    v->e[v->n++] = *e;
    return 1;
}

/* One decompressed index page: header, records, and at the page end a table of
   u32 (offset | length << 16) read backwards. Entry pages pair (value, key):
   key = parent node (big-endian u32) + name; value = type byte + little-endian fields. */
static int parse_page(gt4_vol *v, const uint8_t *p)
{
    int is_index = (int16_t)le16(p);
    unsigned count = le16(p + 2);
    if (is_index)
        return 1;                         /* index pages only speed up lookups */
    if (count % 2 || count * 4 > PAGE)
        return 0;
    for (unsigned i = 0; i < count; i += 2) {
        uint32_t dv = le32(p + PAGE - 4 * (i + 1)), dk = le32(p + PAGE - 4 * (i + 2));
        const uint8_t *val = p + (dv & 0xffff), *key = p + (dk & 0xffff);
        unsigned vlen = dv >> 16, klen = dk >> 16;
        if ((dv & 0xffff) + vlen > PAGE || (dk & 0xffff) + klen > PAGE || klen < 4 || vlen < 1)
            return 0;
        vol_entry e = {0};
        e.parent = be32(key);
        e.name = malloc(klen - 4 + 1);
        if (!e.name) return 0;
        memcpy(e.name, key + 4, klen - 4);
        e.name[klen - 4] = 0;
        e.type = val[0];
        if (e.type == 0 && vlen >= 5) {
            e.node = le32(val + 1);
        } else if (e.type == 1 && vlen >= 13) {
            e.page = le32(val + 1); e.size = le32(val + 9);
        } else if (e.type == 2 && vlen >= 17) {
            e.page = le32(val + 1); e.csize = le32(val + 9); e.size = le32(val + 13);
        } else {
            free(e.name);
            return 0;
        }
        if (!add(v, &e)) { free(e.name); return 0; }
    }
    return 1;
}

static gt4_vol *open_volume(const char *path, const char **err,int movies)
{
    static const char *dummy;
    uint8_t h[0x40];
    if (!err) err = &dummy;
    gt4_vol *v = calloc(1, sizeof *v);
    if (!v) { *err = "out of memory"; return NULL; }
    v->f = plat_file_open(path);
    if (!v->f) { *err = "cannot open file"; free(v); return NULL; }
    v->source = malloc(strlen(path)+1);
    if (!v->source) { *err = "out of memory"; gt4_vol_close(v); return NULL; }
    strcpy(v->source,path);
    if (!locate_in_iso(v,movies))
        v->base = 0;                      /* assume a bare GT4.VOL */

    uint64_t toc = (uint64_t)GT4_TOC_PAGE * PAGE;
    if (!read_at(v, toc, h, sizeof h) || le32(h) != RoFS_MAGIC || le16(h + 0x10) != PAGE) {
        *err = "no GT4 RoFS header at page 0x2159 (not an NTSC-U GT4 volume?)";
        gt4_vol_close(v); return NULL;
    }
    uint32_t toc_pages = le16(h + 0x12), header_pages = le32(h + 0x0c);
    v->data = toc + (uint64_t)header_pages * PAGE;

    uint32_t *offs = malloc(4u * (toc_pages + 1));
    uint8_t *page = malloc(PAGE);
    if (!offs || !page || !read_at(v, toc + 0x40, offs, 4u * (toc_pages + 1))) {
        *err = "cannot read index offsets"; free(offs); free(page); gt4_vol_close(v); return NULL;
    }
    for (uint32_t i = 0; i <= toc_pages; i++)
        offs[i] = le32((uint8_t *)&offs[i]) ^ (i * OFFSET_KEY + OFFSET_KEY);

    uint8_t *packed = NULL;
    for (uint32_t i = 0; i < toc_pages; i++) {
        uint32_t len = offs[i + 1] - offs[i];
        uint8_t *np = realloc(packed, len);
        if (offs[i + 1] < offs[i] || !np || !read_at(v, toc + offs[i], np, len)
            || core_inflate_raw(np, len, page, PAGE) != PAGE) {
            if (np) packed = np;
            *err = "broken index page";
            free(packed); free(offs); free(page); gt4_vol_close(v); return NULL;
        }
        packed = np;
        for (uint32_t k = 0; k < PAGE; k++)
            page[k] ^= TOC_XOR;
        if (!parse_page(v, page)) {
            *err = "unexpected index page layout";
            free(packed); free(offs); free(page); gt4_vol_close(v); return NULL;
        }
    }
    free(packed); free(offs); free(page);
    return v;
}

void gt4_vol_close(gt4_vol *v)
{
    if (!v) return;
    for (size_t i = 0; i < v->n; i++)
        free(v->e[i].name);
    free(v->e);
    if (v->f) plat_file_close(v->f);
    free(v->source);
    free(v);
}

const char *gt4_vol_source(const gt4_vol *v) { return v?v->source:NULL; }

size_t gt4_vol_count(const gt4_vol *v) { return v->n; }

static const vol_entry *child(const gt4_vol *v, uint32_t parent, const char *name, size_t len)
{
    for (size_t i = 0; i < v->n; i++)
        if (v->e[i].parent == parent && strlen(v->e[i].name) == len && !memcmp(v->e[i].name, name, len))
            return &v->e[i];
    return NULL;
}

/* Root directory is node 1 (the "." entry under parent 0). */
static const vol_entry *lookup(const gt4_vol *v, const char *path, uint32_t *dir_node)
{
    uint32_t node = 1;
    const vol_entry *e = NULL;
    while (*path) {
        while (*path == '/') path++;
        if (!*path) break;
        const char *end = strchr(path, '/');
        size_t len = end ? (size_t)(end - path) : strlen(path);
        if (e && e->type != 0) return NULL;
        e = child(v, node, path, len);
        if (!e) return NULL;
        if (e->type == 0) node = e->node;
        path += len;
    }
    if (dir_node) *dir_node = node;
    return e;
}

uint8_t *gt4_vol_read(gt4_vol *v, const char *path, size_t *size)
{
    const vol_entry *e = lookup(v, path, NULL);
    if (!e || e->type == 0) return NULL;
    uint8_t *out = malloc(e->size ? e->size : 1);
    if (!out) return NULL;
    uint64_t at = v->data + (uint64_t)e->page * PAGE;
    if (e->type == 1) {
        if (!read_at(v, at, out, e->size)) { free(out); return NULL; }
    } else {
        uint8_t *packed = malloc(e->csize);
        if (!packed || e->csize < 8 || !read_at(v, at, packed, e->csize)
            || le32(packed) != DATA_MAGIC || (uint32_t)(-(int32_t)le32(packed + 4)) != e->size
            || core_inflate_raw(packed + 8, e->csize - 8, out, e->size) != e->size) {
            free(packed); free(out); return NULL;
        }
        free(packed);
    }
    if (size) *size = e->size;
    return out;
}

int gt4_vol_list(gt4_vol *v, const char *dir, gt4_vol_list_fn fn, void *user)
{
    uint32_t node = 1;
    const vol_entry *d = lookup(v, dir, &node);
    if (d && d->type != 0) return 0;
    for (size_t i = 0; i < v->n; i++)
        if (v->e[i].parent == node && strcmp(v->e[i].name, "."))
            fn(user, v->e[i].name, v->e[i].type == 0, v->e[i].size);
    return 1;
}

gt4_vol *gt4_vol_open(const char *path,const char **err){return open_volume(path,err,0);}
gt4_vol *gt4_vol_open_movies(const char *path,const char **err){return open_volume(path,err,1);}
