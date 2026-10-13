/*
 * GT4's converted Flash with its scripts (strobe_player.h). Layout of STRB
 * version 0 (offsets from the file's start, little endian):
 *   +0x10/+0x14 size, +0x1C frames per second; then (offset, count) pairs:
 *   +0x30 sprites (0x24: places, frame actions, labels, -, -, frames),
 *   +0x38 shapes (24: bounds, fill groups), +0x50 fonts (0x18: name),
 *   +0x60 edit texts (0x34), +0x68 fills (8), +0x98 bitmap matrices (64),
 *   +0xA0 buttons (0x14), +0xA8 actions (offset, size), +0xB0 bitmaps
 *   (16: a Tex1 file), +0xB8 extrender entries (0x18: bounds, name).
 * A placement (0x28): type (0 shape, 2 sprite, 5 edit text, 6 button,
 * 7 extrender), first and end frame, character, depth, clip depth, clip
 * event flags, clip actions (event, action), parameters per frame (0x24:
 * rotation, -, x and y scale, x, y, colour RGBA with 0x80 for one) and its
 * name. Edit texts: bounds, colour, font, height, alignment (+0x1B: 0 left,
 * 1 right, 2 centred) and the initial text (an rtext key).
 */
#include "strobe_player.h"
#include "font.h"
#include "font_layout.h"
#include "glyph_sdf.h"
#include "tex1.h"
#include "platform/platform.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct inst inst;
struct inst {
    int sprite, frame, playing, visible, fresh;
    inst *parent;
    int place;             /* its placement in the parent's sprite */
    inst **kids;           /* per placement of its sprite, when it is a live sprite */
    char (*text)[96];      /* per placement: an edit text's text, set by a script */
    uint8_t *has_text;
    struct { char name[24]; strb_value v; } vars[16];
    int nvars;
    int has_x;             /* the host's own _x (func_0047DA90), in the parent's units */
    float x;
    inst *next_dead;       /* in the player's list to free after the scripts */
};

typedef struct {
    char name[32];
    gt4_font face;
    plat_texture *ink, *outline;
    int palette, ok;
    float sx, sy;
} strb_font;

typedef struct solid_fill solid_fill;
struct solid_fill {size_t key;unsigned count;int supported;float (*xy)[4][2];solid_fill *next;};

struct strb_player {
    uint8_t *d;
    size_t n;
    gt4_vol *vol;
    const gt4_rtext *rt;
    char page[32];
    strb_host host;
    float rate;
    double acc;
    inst *root;
    plat_texture **bitmaps;
    int *bw, *bh, *btw, *bth;
    unsigned nbitmaps;
    strb_font *fonts;
    unsigned nfonts;
    solid_fill *solids; /* resident tessellation of immutable straight paths */
    int depth;             /* script nesting */
    int key;               /* the key going down, for Key.getCode() */
    int buttons_on, nav_all;   /* the buttons' navigation (strb_navigate) */
    inst *focus;           /* the focused button: its timeline and placement */
    int focus_shown;       /* the first focus's IdleToOverUp ran */
    inst *dead;            /* instances let go while a script ran */
    int focus_place;
    struct strb_array *arrays;   /* the scripts' arrays, kept to the close */
};

/* an Array (InitArray), as a value: STRB_OBJ with place -3 */
typedef struct strb_array {
    struct strb_array *next;
    int n;
    strb_value v[32];
} strb_array;

static uint32_t u32(const strb_player *p, size_t o) { return o + 4 <= p->n ? p->d[o] | (uint32_t)p->d[o + 1] << 8 | (uint32_t)p->d[o + 2] << 16 | (uint32_t)p->d[o + 3] << 24 : 0; }
static unsigned u16(const strb_player *p, size_t o) { return o + 2 <= p->n ? p->d[o] | (unsigned)p->d[o + 1] << 8 : 0; }
static int s16(const strb_player *p, size_t o) { return (int16_t)u16(p, o); }
static float f32(const strb_player *p, size_t o) { uint32_t v = u32(p, o); float f; memcpy(&f, &v, 4); return f; }
static const char *cstr(const strb_player *p, size_t o) { return o < p->n && memchr(p->d + o, 0, p->n - o) ? (const char *)p->d + o : ""; }

/* ------------------------------------------------------------ sprites */

static size_t sprite_at(const strb_player *p, int s) { return u32(p, 0x30) + (size_t)s * 0x24; }
static unsigned sprite_places(const strb_player *p, int s) { return u32(p, sprite_at(p, s) + 4); }
static size_t place_at(const strb_player *p, int s, unsigned i) { return u32(p, sprite_at(p, s)) + (size_t)i * 0x28; }
static int sprite_frames(const strb_player *p, int s) { int f = (int)u32(p, sprite_at(p, s) + 32); return f > 0 ? f : 1; }
static int place_live(const strb_player *p, size_t pl, int frame) { return frame >= s16(p, pl + 2) && frame < s16(p, pl + 4); }
static const char *place_name(const strb_player *p, size_t pl) { return u32(p, pl + 0x24) ? cstr(p, u32(p, pl + 0x20)) : ""; }

static inst *inst_new(strb_player *p, int sprite, inst *parent, int place)
{
    inst *k = calloc(1, sizeof *k);
    unsigned n = sprite_places(p, sprite);
    if (!k) return NULL;
    k->sprite = sprite;
    k->parent = parent;
    k->place = place;
    k->playing = 1;
    k->visible = 1;
    k->fresh = 1;
    k->kids = calloc(n ? n : 1, sizeof *k->kids);
    k->text = calloc(n ? n : 1, sizeof *k->text);
    k->has_text = calloc(n ? n : 1, 1);
    return k;
}

static void inst_free(strb_player *p, inst *k)
{
    if (!k) return;
    /* a script still running may be on it (a page change from a button's
       own timeline): kept until the outermost script ends */
    if (p->depth > 0) {
        k->parent = NULL;
        k->next_dead = p->dead;
        p->dead = k;
        return;
    }
    for (unsigned i = 0; i < sprite_places(p, k->sprite); i++) inst_free(p, k->kids[i]);
    free(k->kids);
    free(k->text);
    free(k->has_text);
    free(k);
}

static void run_action(strb_player *p, inst *self, unsigned index);

static void frame_actions(strb_player *p, inst *k)
{
    size_t s = sprite_at(p, k->sprite), list = u32(p, s + 8);
    unsigned n = u32(p, s + 12);
    for (unsigned i = 0; i < n; i++)
        if (s16(p, list + i * 4) == k->frame) run_action(p, k, u16(p, list + i * 4 + 2));
}

/* the placements of k's frame: sprites made (at their first frame, its
   actions run) or let go, scripted texts forgotten with their placement */
static void sync_kids(strb_player *p, inst *k)
{
    unsigned n = sprite_places(p, k->sprite);
    for (unsigned i = 0; i < n; i++) {
        size_t pl = place_at(p, k->sprite, i);
        int live = place_live(p, pl, k->frame);
        if (!live) {
            if (k->kids[i]) { inst_free(p, k->kids[i]); k->kids[i] = NULL; }
            k->has_text[i] = 0;
            continue;
        }
        /* a button shows its "up" state's sprite (record +0) until focused */
        if (s16(p, pl) == 6 && !k->kids[i] && (unsigned)s16(p, pl + 6) < u32(p, 0xa4)) {
            unsigned up = u16(p, u32(p, 0xa0) + (size_t)s16(p, pl + 6) * 0x14);
            if (up < u32(p, 0x34)) k->kids[i] = inst_new(p, (int)up, k, (int)i);
            if (k->kids[i]) { sync_kids(p, k->kids[i]); frame_actions(p, k->kids[i]); }
        }
        if (s16(p, pl) == 2 && !k->kids[i]) {
            k->kids[i] = inst_new(p, s16(p, pl + 6), k, (int)i);
            if (k->kids[i]) {
                sync_kids(p, k->kids[i]);
                frame_actions(p, k->kids[i]);
            }
        }
    }
}

static void go_to(strb_player *p, inst *k, int frame, int play)
{
    int n = sprite_frames(p, k->sprite);
    k->frame = frame < 0 ? 0 : frame >= n ? n - 1 : frame;
    k->playing = play;
    sync_kids(p, k);
    frame_actions(p, k);
}

static void clip_events(strb_player *p, inst *k, unsigned event)
{
    unsigned n = sprite_places(p, k->sprite);
    for (unsigned i = 0; i < n; i++) {
        size_t pl = place_at(p, k->sprite, i), list = u32(p, pl + 0x10);
        unsigned m = u32(p, pl + 0x14);
        inst *kid = k->kids[i];
        if (!kid) continue;
        for (unsigned j = 0; j < m; j++)
            if (u32(p, list + j * 8) == event) run_action(p, kid, u32(p, list + j * 8 + 4));
    }
}

/* one frame of k: on to the next frame when playing (looping), its
   placements, its frame actions, then the older children's frames and
   their enterFrame (event 2) actions */
static void step(strb_player *p, inst *k)
{
    unsigned n = sprite_places(p, k->sprite);
    int entered = 0;
    for (unsigned i = 0; i < n; i++) if (k->kids[i]) k->kids[i]->fresh = 0;
    if (k->playing) {
        int frames = sprite_frames(p, k->sprite);
        if (frames > 1) {
            k->frame = k->frame + 1 >= frames ? 0 : k->frame + 1;
            entered = 1;
        }
    }
    sync_kids(p, k);
    if (entered) frame_actions(p, k);
    for (unsigned i = 0; i < n; i++)
        if (k->kids[i] && !k->kids[i]->fresh) step(p, k->kids[i]);
    clip_events(p, k, 2);
}

/* ------------------------------------------------------------- scripts */

static strb_value num(double v) { strb_value r; memset(&r, 0, sizeof r); r.t = STRB_NUM; r.n = v; return r; }
static strb_value boolean(int v) { strb_value r = num(v != 0); r.t = STRB_BOOL; return r; }
static strb_value str(const char *s) { strb_value r; memset(&r, 0, sizeof r); r.t = STRB_STR; snprintf(r.s, sizeof r.s, "%s", s); return r; }
static strb_value obj(inst *k, int place) { strb_value r; memset(&r, 0, sizeof r); r.t = k ? STRB_OBJ : STRB_UNDEF; r.obj = k; r.place = place; return r; }
static double to_num(const strb_value *v) { return v->t == STRB_NUM || v->t == STRB_BOOL ? v->n : v->t == STRB_STR ? atof(v->s) : 0; }
/* a string compared with a number: its number, NaN when it is not one
   ("CONTRAST" == 0 is false, as in Flash) */
static double as_number(const strb_value *v)
{
    if (v->t == STRB_STR) {
        char *end;
        double d = strtod(v->s, &end);
        while (*end == ' ') end++;
        return v->s[0] && !*end ? d : NAN;
    }
    return to_num(v);
}
static int truthy(const strb_value *v) { return v->t == STRB_OBJ || v->t == STRB_FUNC || (v->t == STRB_STR ? v->s[0] != 0 : to_num(v) != 0); }
static void to_text(const strb_value *v, char *out, size_t n)
{
    if (v->t == STRB_STR) snprintf(out, n, "%s", v->s);
    else if (v->t == STRB_BOOL) snprintf(out, n, "%s", v->n ? "true" : "false");
    else if (v->t == STRB_NUM) snprintf(out, n, "%.15g", v->n);
    else if (n) out[0] = 0;
}

static int same_name(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return !*a && !*b;
}

/* a child of k by its placement's name (names in these movies ignore case) */
static strb_value child(strb_player *p, inst *k, const char *name)
{
    unsigned n = sprite_places(p, k->sprite);
    for (unsigned i = 0; i < n; i++) {
        size_t pl = place_at(p, k->sprite, i);
        if (!place_live(p, pl, k->frame) || !same_name(place_name(p, pl), name)) continue;
        if (k->kids[i] && s16(p, pl) == 2) return obj(k->kids[i], -1);
        if (s16(p, pl) == 5) return obj(k, (int)i);
    }
    return obj(NULL, -1);
}

/* a frame label of k's sprite (12 bytes each: frame, name, its length) */
static int label(strb_player *p, inst *k, const char *name)
{
    size_t s = sprite_at(p, k->sprite), list = u32(p, s + 16);
    unsigned n = u32(p, s + 20);
    char buf[64];
    for (unsigned i = 0; i < n; i++) {
        size_t e = list + i * 12, len = u32(p, e + 8);
        if (len >= sizeof buf || u32(p, e + 4) + len > p->n) continue;
        memcpy(buf, p->d + u32(p, e + 4), len);
        buf[len] = 0;
        if (same_name(buf, name)) return (int)u32(p, e);
    }
    return -1;
}

static strb_value get_member(strb_player *p, const strb_value *o, const char *name)
{
    inst *k = o->obj;
    if (o->t == STRB_OBJ && o->place == -2) {       /* Key's constants */
        static const struct { const char *name; int code; } keys[] = {
            { "LEFT", 37 }, { "UP", 38 }, { "RIGHT", 39 }, { "DOWN", 40 }, { "ENTER", 13 },
            { "SPACE", 32 }, { "ESCAPE", 27 } };
        for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
            if (same_name(name, keys[i].name)) return num(keys[i].code);
        return obj(NULL, -1);
    }
    if (o->t != STRB_OBJ || !k) return obj(NULL, -1);
    if (o->place == -3) {                            /* an array: its length, an element */
        const strb_array *a = o->obj;
        char *end;
        long i = strtol(name, &end, 10);
        if (same_name(name, "length")) return num(a->n);
        if (*name && !*end && i >= 0 && i < a->n) return a->v[i];
        return obj(NULL, -1);
    }
    if (o->place >= 0) {
        if (same_name(name, "text")) {
            if (k->has_text[o->place]) return str(k->text[o->place]);
            return str("");
        }
        return obj(NULL, -1);
    }
    if (same_name(name, "_visible")) return boolean(k->visible);
    if (same_name(name, "_parent")) return obj(k->parent, -1);
    if (same_name(name, "_name")) return str(k->parent ? place_name(p, place_at(p, k->parent->sprite, (unsigned)k->place)) : "");
    if (same_name(name, "_currentframe")) return num(k->frame + 1);
    if (same_name(name, "_totalframes")) return num(sprite_frames(p, k->sprite));
    for (int i = 0; i < k->nvars; i++) if (same_name(k->vars[i].name, name)) return k->vars[i].v;
    return child(p, k, name);
}

static void set_member(strb_value *o, const char *name, const strb_value *v)
{
    inst *k = o->obj;
    char buf[96];
    if (o->t != STRB_OBJ || !k) return;
    if (o->place == -3) {
        strb_array *a = o->obj;
        char *end;
        long i = strtol(name, &end, 10);
        if (*name && !*end && i >= 0 && i < 32) { a->v[i] = *v; if (a->n <= i) a->n = (int)i + 1; }
        return;
    }
    if (o->place >= 0) {
        if (same_name(name, "text")) {
            to_text(v, buf, sizeof buf);
            snprintf(k->text[o->place], sizeof k->text[o->place], "%s", buf);
            k->has_text[o->place] = 1;
        }
        return;
    }
    if (same_name(name, "_visible")) { k->visible = truthy(v); return; }
    for (int i = 0; i < k->nvars; i++)
        if (same_name(k->vars[i].name, name)) { k->vars[i].v = *v; return; }
    if (k->nvars < (int)(sizeof k->vars / sizeof k->vars[0])) {
        snprintf(k->vars[k->nvars].name, sizeof k->vars[0].name, "%s", name);
        k->vars[k->nvars++].v = *v;
    }
}

/* one call's locals, registers and the timeline its names resolve on */
typedef struct {
    inst *self;
    struct { char name[24]; strb_value v; } local[8];
    int nlocal;
    strb_value reg[4];
} frame_scope;

/* a name or a dotted path (_root.a.b, _parent.x) */
static strb_value get_var(strb_player *p, frame_scope *f, const char *name)
{
    char part[96];
    strb_value cur = obj(f->self, -1);
    const char *q = name;
    int first = 1;
    if (same_name(name, "Key")) { strb_value r = obj(NULL, -2); r.t = STRB_OBJ; return r; }
    for (int i = 0; i < f->nlocal; i++) if (same_name(f->local[i].name, name)) return f->local[i].v;
    while (*q) {
        size_t n = strcspn(q, ".");
        snprintf(part, sizeof part, "%.*s", (int)n, q);
        if (first && same_name(part, "this")) cur = obj(f->self, -1);
        else if (same_name(part, "_root")) cur = obj(p->root, -1);
        else cur = get_member(p, &cur, part);
        first = 0;
        q += n;
        if (*q == '.') q++;
    }
    return cur;
}

static void set_var(strb_player *p, frame_scope *f, const char *name, const strb_value *v)
{
    const char *dot = strrchr(name, '.');
    for (int i = 0; i < f->nlocal; i++) if (same_name(f->local[i].name, name)) { f->local[i].v = *v; return; }
    if (dot) {
        char path[96];
        snprintf(path, sizeof path, "%.*s", (int)(dot - name), name);
        strb_value o = get_var(p, f, path);
        set_member(&o, dot + 1, v);
        return;
    }
    strb_value me = obj(f->self, -1);
    set_member(&me, name, v);
}

static void define_local(frame_scope *f, const char *name, const strb_value *v)
{
    for (int i = 0; i < f->nlocal; i++) if (same_name(f->local[i].name, name)) { f->local[i].v = *v; return; }
    if (f->nlocal < 8) {
        snprintf(f->local[f->nlocal].name, sizeof f->local[0].name, "%s", name);
        f->local[f->nlocal++].v = *v;
    }
}

static strb_value exec(strb_player *p, frame_scope *f, size_t at, size_t end, size_t pool_at);

/* a script function: its parameters as locals, `this` the timeline it
   was made on unless called as a method */
static strb_value call_function(strb_player *p, const strb_value *fn, inst *self, const strb_value *args, int nargs)
{
    frame_scope f;
    size_t at = fn->code, q;
    unsigned np, size;
    memset(&f, 0, sizeof f);
    f.self = self ? self : fn->obj;
    if (!f.self || at + 2 > p->n) return obj(NULL, -1);
    q = at + 3 + strlen(cstr(p, at + 3)) + 1;      /* past op, length, name */
    np = u16(p, q);
    q += 2;
    for (unsigned i = 0; i < np && q < p->n; i++) {
        strb_value v = i < (unsigned)nargs ? args[i] : obj(NULL, -1);
        define_local(&f, cstr(p, q), &v);
        q += strlen(cstr(p, q)) + 1;
    }
    size = u16(p, q);
    q = at + 3 + u16(p, at + 1);                   /* the body follows the record */
    return exec(p, &f, q, q + size, fn->pool);
}

static void method(strb_player *p, strb_value *o, const char *name, strb_value *args, int nargs, strb_value *ret)
{
    inst *k = o->obj;
    *ret = obj(NULL, -1);
    if (o->t == STRB_OBJ && o->place == -2) {       /* Key */
        if (same_name(name, "getCode")) *ret = num(p->key);
        return;
    }
    if (o->t == STRB_OBJ && o->place == -3) {       /* Array.push */
        strb_array *a = o->obj;
        if (same_name(name, "push")) for (int i = 0; i < nargs && a->n < 32; i++) a->v[a->n++] = args[i];
        *ret = num(a->n);
        return;
    }
    if (o->t != STRB_OBJ || !k || o->place >= 0) return;
    {
        strb_value m = get_member(p, o, name);
        if (m.t == STRB_FUNC) { *ret = call_function(p, &m, k, args, nargs); return; }
    }
    if (same_name(name, "play")) k->playing = 1;
    else if (same_name(name, "stop")) k->playing = 0;
    else if ((same_name(name, "gotoAndStop") || same_name(name, "gotoAndPlay")) && nargs > 0) {
        int frame = args[0].t == STRB_STR ? label(p, k, args[0].s) : (int)to_num(&args[0]) - 1;
        if (frame >= 0) go_to(p, k, frame, same_name(name, "gotoAndPlay"));
    } else if (same_name(name, "nextFrame")) go_to(p, k, k->frame + 1, 0);
    else if (same_name(name, "prevFrame")) go_to(p, k, k->frame - 1, 0);
    /* the movie class's own: a pressed button turns the buttons off until
       the next NavigateVertical / NavigateAll */
    else if (same_name(name, "inactivateButton")) p->buttons_on = 0;
    else if (same_name(name, "activateButton")) p->buttons_on = 1;
}

/* GetProperty / SetProperty indices: 4 _currentframe, 5 _totalframes,
   7 _visible, 13 _name */
static strb_value target_of(strb_player *p, frame_scope *f, const strb_value *t)
{
    if (t->t == STRB_OBJ) return *t;
    if (t->t == STRB_STR && t->s[0]) return get_var(p, f, t->s);
    return obj(f->self, -1);
}

static strb_value exec(strb_player *p, frame_scope *f, size_t at, size_t end, size_t pool_at)
{
    enum { DEPTH = 64 };
    strb_value stack[DEPTH], result = obj(NULL, -1);
    int sp = 0;
    const char *pool[64];
    unsigned npool = 0;
    size_t current_pool = 0;
    if (end > p->n || p->depth > 16) return result;
    p->depth++;
#define POP() (sp > 0 ? stack[--sp] : obj(NULL, -1))
#define PUSH(v) do { if (sp < DEPTH) stack[sp++] = (v); } while (0)
    /* a function's body: the constant pool in force where it was made */
    if (pool_at && pool_at + 5 <= p->n && p->d[pool_at] == 0x88) {
        unsigned c = u16(p, pool_at + 3);
        size_t q = pool_at + 5, stop = pool_at + 3 + u16(p, pool_at + 1);
        for (unsigned i = 0; i < c && q < stop; i++) {
            const char *s = cstr(p, q);
            if (npool < 64) pool[npool++] = s;
            q += strlen(s) + 1;
        }
        current_pool = pool_at;
    }
    while (at < end) {
        size_t start = at;
        unsigned op = p->d[at++], len = 0;
        size_t next;
        if (op & 0x80) { len = u16(p, at); at += 2; }
        next = at + len;
        switch (op) {
        case 0x00: goto done;                                     /* End */
        case 0x04: go_to(p, f->self, f->self->frame + 1, 0); break;   /* NextFrame */
        case 0x05: go_to(p, f->self, f->self->frame - 1, 0); break;   /* PrevFrame */
        case 0x06: f->self->playing = 1; break;                   /* Play */
        case 0x07: f->self->playing = 0; break;                   /* Stop */
        case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f: case 0x10: case 0x11:
        case 0x3f: case 0x47: case 0x48: case 0x66: case 0x67: case 0x21: {
            strb_value b = POP(), a = POP();
            double x = to_num(&a), y = to_num(&b);
            switch (op) {
            case 0x0a: PUSH(num(x + y)); break;                   /* Add */
            case 0x0b: PUSH(num(x - y)); break;                   /* Subtract */
            case 0x0c: PUSH(num(x * y)); break;                   /* Multiply */
            case 0x0d: PUSH(num(y != 0 ? x / y : 0)); break;      /* Divide */
            case 0x0e: PUSH(boolean(x == y)); break;              /* Equals */
            case 0x0f: case 0x48: PUSH(boolean(x < y)); break;    /* Less, Less2 */
            case 0x67: PUSH(boolean(x > y)); break;               /* Greater */
            case 0x10: PUSH(boolean(truthy(&a) && truthy(&b))); break;
            case 0x11: PUSH(boolean(truthy(&a) || truthy(&b))); break;
            case 0x3f: PUSH(num(y != 0 ? fmod(x, y) : 0)); break; /* Modulo */
            case 0x47: case 0x21:                                 /* Add2, StringAdd */
                if (op == 0x21 || a.t == STRB_STR || b.t == STRB_STR) {
                    char sa[96], sb[96], out[192];
                    to_text(&a, sa, sizeof sa);
                    to_text(&b, sb, sizeof sb);
                    snprintf(out, sizeof out, "%s%s", sa, sb);
                    PUSH(str(out));
                } else PUSH(num(x + y));
                break;
            case 0x66:                                            /* StrictEquals */
                PUSH(boolean(a.t == b.t && (a.t == STRB_STR ? !strcmp(a.s, b.s)
                                           : a.t == STRB_OBJ || a.t == STRB_FUNC ? a.obj == b.obj && a.place == b.place && a.code == b.code
                                           : a.t == STRB_UNDEF || x == y)));
                break;
            }
            break;
        }
        case 0x12: { strb_value v = POP(); PUSH(boolean(!truthy(&v))); break; }   /* Not */
        case 0x17: (void)POP(); break;                            /* Pop */
        case 0x1c: { strb_value v = POP(); char t[96]; to_text(&v, t, sizeof t); PUSH(get_var(p, f, t)); break; }   /* GetVariable */
        case 0x1d: { strb_value v = POP(), name = POP(); char t[96]; to_text(&name, t, sizeof t); set_var(p, f, t, &v); break; }
        case 0x22: {                                              /* GetProperty */
            strb_value idx = POP(), t = POP(), o = target_of(p, f, &t);
            int i = (int)to_num(&idx);
            PUSH(i == 13 ? get_member(p, &o, "_name") : i == 4 ? get_member(p, &o, "_currentframe")
                 : i == 5 ? get_member(p, &o, "_totalframes") : i == 7 ? get_member(p, &o, "_visible") : obj(NULL, -1));
            break;
        }
        case 0x23: {                                              /* SetProperty */
            strb_value v = POP(), idx = POP(), t = POP(), o = target_of(p, f, &t);
            if ((int)to_num(&idx) == 7) set_member(&o, "_visible", &v);
            break;
        }
        case 0x3c: { strb_value v = POP(), name = POP(); define_local(f, name.s, &v); break; }   /* DefineLocal */
        case 0x41: { strb_value name = POP(), u = obj(NULL, -1); define_local(f, name.s, &u); break; }   /* DefineLocal2 */
        case 0x3d: {                                              /* CallFunction */
            strb_value name = POP(), cnt = POP(), args[8], r = obj(NULL, -1), fn;
            int nargs = (int)to_num(&cnt);
            if (nargs < 0) nargs = 0;
            for (int i = 0; i < nargs; i++) { strb_value a = POP(); if (i < 8) args[i] = a; }
            if (nargs > 8) nargs = 8;
            fn = get_var(p, f, name.s);
            if (fn.t == STRB_FUNC) r = call_function(p, &fn, NULL, args, nargs);
            else if (!p->host.call || !p->host.call(p->host.user, name.s, args, nargs, &r)) r = obj(NULL, -1);
            PUSH(r);
            break;
        }
        case 0x3e: result = POP(); goto done;                     /* Return */
        case 0x44: {                                              /* TypeOf */
            strb_value v = POP();
            PUSH(str(v.t == STRB_NUM ? "number" : v.t == STRB_STR ? "string" : v.t == STRB_BOOL ? "boolean"
                     : v.t == STRB_FUNC ? "function" : v.t == STRB_OBJ ? "movieclip" : "undefined"));
            break;
        }
        case 0x49: {                                              /* Equals2 */
            strb_value b = POP(), a = POP();
            int eq = a.t == STRB_STR && b.t == STRB_STR ? !strcmp(a.s, b.s)
                   : a.t == STRB_OBJ || b.t == STRB_OBJ ? a.obj == b.obj && a.place == b.place
                   : (a.t == STRB_UNDEF) != (b.t == STRB_UNDEF) ? 0
                   : as_number(&a) == as_number(&b);
            PUSH(boolean(eq));
            break;
        }
        case 0x4a: { strb_value v = POP(); PUSH(num(to_num(&v))); break; }   /* ToNumber */
        case 0x4b: { strb_value v = POP(); char t[96]; to_text(&v, t, sizeof t); PUSH(str(t)); break; }   /* ToString */
        case 0x4c: { strb_value v = POP(); PUSH(v); PUSH(v); break; }   /* PushDuplicate */
        case 0x4d: { strb_value b = POP(), a = POP(); PUSH(b); PUSH(a); break; }   /* StackSwap */
        case 0x4e: { strb_value name = POP(), o = POP(); char t[96]; to_text(&name, t, sizeof t); PUSH(get_member(p, &o, t)); break; }
        case 0x4f: { strb_value v = POP(), name = POP(), o = POP(); char t[96]; to_text(&name, t, sizeof t); set_member(&o, t, &v); break; }
        case 0x50: { strb_value v = POP(); PUSH(num(to_num(&v) + 1)); break; }   /* Increment */
        case 0x51: { strb_value v = POP(); PUSH(num(to_num(&v) - 1)); break; }   /* Decrement */
        case 0x42: {                                              /* InitArray */
            strb_value cnt = POP();
            strb_array *a = calloc(1, sizeof *a);
            int n = (int)to_num(&cnt);
            for (int i = 0; i < n; i++) { strb_value e = POP(); if (a && i < 32) a->v[a->n++] = e; }
            if (a) { a->next = p->arrays; p->arrays = a; }
            {
                strb_value r = obj(NULL, -1);
                if (a) { r.t = STRB_OBJ; r.obj = a; r.place = -3; }
                PUSH(r);
            }
            break;
        }
        case 0x52: {                                              /* CallMethod */
            strb_value name = POP(), o = POP(), cnt = POP(), args[8], r;
            int nargs = (int)to_num(&cnt);
            if (nargs < 0) nargs = 0;
            for (int i = 0; i < nargs; i++) { strb_value a = POP(); if (i < 8) args[i] = a; }
            if (nargs > 8) nargs = 8;
            if (name.t == STRB_STR && name.s[0]) method(p, &o, name.s, args, nargs, &r);
            else r = o.t == STRB_FUNC ? call_function(p, &o, NULL, args, nargs) : obj(NULL, -1);
            PUSH(r);
            break;
        }
        case 0x81: go_to(p, f->self, s16(p, at), 0); break;       /* GotoFrame (stopped, as Flash) */
        case 0x87: if (p->d[at] < 4 && sp > 0) f->reg[p->d[at]] = stack[sp - 1]; break;   /* StoreRegister */
        case 0x88: {                                              /* ConstantPool */
            unsigned c = u16(p, at);
            size_t q = at + 2;
            npool = 0;
            for (unsigned i = 0; i < c && q < next; i++) {
                const char *s = cstr(p, q);
                if (npool < 64) pool[npool++] = s;
                q += strlen(s) + 1;
            }
            current_pool = start;
            break;
        }
        case 0x8c: {                                              /* GotoLabel */
            int frame = label(p, f->self, cstr(p, at));
            if (frame >= 0) go_to(p, f->self, frame, 0);
            break;
        }
        case 0x96: {                                              /* Push */
            size_t q = at;
            while (q < next) {
                unsigned t = p->d[q++];
                strb_value v = obj(NULL, -1);
                switch (t) {
                case 0: v = str(cstr(p, q)); q += strlen(cstr(p, q)) + 1; break;
                case 1: v = num(f32(p, q)); q += 4; break;
                case 2: case 3: break;
                case 4: if (p->d[q] < 4) v = f->reg[p->d[q]]; q += 1; break;
                case 5: v = boolean(p->d[q] != 0); q += 1; break;
                case 6: {   /* a double, its high word first */
                    uint64_t bits = (uint64_t)u32(p, q) << 32 | u32(p, q + 4);
                    double dv;
                    memcpy(&dv, &bits, 8);
                    v = num(dv);
                    q += 8;
                    break;
                }
                case 7: v = num((int32_t)u32(p, q)); q += 4; break;
                case 8: v = str(p->d[q] < npool ? pool[p->d[q]] : ""); q += 1; break;
                case 9: v = str(u16(p, q) < npool ? pool[u16(p, q)] : ""); q += 2; break;
                default: q = next; break;
                }
                PUSH(v);
            }
            break;
        }
        case 0x99: next += (size_t)(ptrdiff_t)s16(p, at); break;   /* Jump */
        case 0x9b: {                                              /* DefineFunction */
            strb_value fn = obj(f->self, -1);
            const char *name = cstr(p, at);
            size_t q = at + strlen(name) + 1;
            unsigned np = u16(p, q);
            q += 2;
            for (unsigned i = 0; i < np && q < next; i++) q += strlen(cstr(p, q)) + 1;
            fn.t = STRB_FUNC;
            fn.code = start;
            fn.pool = current_pool;
            if (*name) set_var(p, f, name, &fn);
            else PUSH(fn);
            next += u16(p, q);                                    /* past its body */
            break;
        }
        case 0x9d: { strb_value c = POP(); if (truthy(&c)) next += (size_t)(ptrdiff_t)s16(p, at); break; }   /* If */
        case 0x9f: {                                              /* GotoFrame2 */
            strb_value v = POP();
            int frame = v.t == STRB_STR ? label(p, f->self, v.s) : (int)to_num(&v) - 1;
            if (frame >= 0) go_to(p, f->self, frame, p->d[at] & 1);
            break;
        }
        default: break;
        }
        at = next;
    }
done:
#undef POP
#undef PUSH
    p->depth--;
    while (!p->depth && p->dead) {
        inst *k = p->dead;
        p->dead = k->next_dead;
        inst_free(p, k);
    }
    return result;
}

static void run_action(strb_player *p, inst *self, unsigned index)
{
    size_t table = u32(p, 0xa8), at;
    frame_scope f;
    if (index >= u32(p, 0xac) || !self) return;
    memset(&f, 0, sizeof f);
    f.self = self;
    at = u32(p, table + index * 8);
    exec(p, &f, at, at + u32(p, table + index * 8 + 4), 0);
}

/* ------------------------------------------------------------- drawing */

typedef struct { float a, d, x, y; float col[4],add[4]; } xform;

static plat_texture *glyph_texture(const tex1_image *a)
{
    int big = a->width > a->height ? a->width : a->height, k = big > 0 ? 2048 / big : 1, w = 0, h = 0;
    uint8_t *sdf;
    plat_texture *t;
    if (k > 4) k = 4;
    if (k < 1) k = 1;
    sdf = gt4_glyph_sdf(a->rgba, a->width, a->height, k, &w, &h);
    if (!sdf) return plat_texture_create(a->width, a->height, a->rgba);
    t = plat_texture_create(w, h, sdf);
    free(sdf);
    return t;
}

/* a font as the movie names it ("hb_24#0*125,125"): #1 the ink alone,
   #0 or none the ink over its outline; *x,y the size in percent */
static strb_font *font(strb_player *p, unsigned i)
{
    strb_font *f;
    const char *spec, *q;
    char base[32];
    size_t n;
    if (i > p->nfonts) return NULL;
    f = &p->fonts[i];
    if (f->name[0]) return f->ok ? f : NULL;
    spec = i==p->nfonts?"Gaiji":cstr(p, u32(p, u32(p, 0x50) + i * 0x18));
    snprintf(f->name, sizeof f->name, "%s", *spec ? spec : "?");
    n = strcspn(spec, "#*");
    snprintf(base, sizeof base, "%.*s", (int)n, spec);
    f->sx = f->sy = 1;
    f->palette = p->host.palette;
    for (q = spec + n; *q;) {
        if (*q == '#') f->palette = atoi(++q);
        else if (*q == '*') {
            int x = 100, y = 100;
            sscanf(++q, "%d,%d", &x, &y);
            f->sx = (float)x / 100;
            f->sy = (float)y / 100;
        }
        q += strcspn(q, "#*");
    }
    {
        static const char *const files[] = { "/font/system.fnt", "/font/race.fnt", "/font/menu.fnt" };
        for (unsigned k = 0; k < 3 && !f->ok; k++) {
            size_t size = 0;
            uint8_t *data = gt4_vol_read(p->vol, files[k], &size);
            if (data && gt4_font_decode(data, size, base, &f->face) == 1) f->ok = 1;
            free(data);
        }
    }
    if (!f->ok) { fprintf(stderr, "strb: no font %s\n", spec); return NULL; }
    f->ink = glyph_texture(&f->face.atlases[f->face.natlases > 1 ? 1 : 0]);
    f->outline = glyph_texture(&f->face.atlases[0]);
    return f;
}

static void color_of(const xform *m, const uint8_t rgba[4], uint8_t out[4])
{
    for (int i = 0; i < 4; i++) {
        float v = ((float)rgba[i]*m->col[i]+m->add[i])*(i==3?2:1);
        out[i] = (uint8_t)(v > 255 ? 255 : v < 0 ? 0 : v + 0.5f);
    }
}

static void draw_embedded_text(strb_player *,const xform *,size_t,size_t,const char *);
static void draw_text(strb_player *p, const xform *m, unsigned id, const char *s)
{
    size_t e = u32(p, 0x60) + (size_t)id * 0x34;
    strb_font *f;
    float l = f32(p, e), t = f32(p, e + 4), r = f32(p, e + 8), x, y, sx, sy, w, base;
    uint8_t color[4];
    int align = p->d[e + 0x1b];
    if (id >= u32(p, 0x64) || !*s) return;
    unsigned fi=u16(p,e+0x14);size_t embedded=u32(p,0x50)+(size_t)fi*24;
    if(fi<p->nfonts && u32(p,embedded+4)){draw_embedded_text(p,m,e,embedded,s);return;}
    if(!(f=font(p,fi)))return;
    color_of(m, p->d + e + 16, color);
    if (!color[3]) return;
    /* the face at the field's height (+0x16, twips) against its own size
       (the number in its name, "hb_24"), then the name's percentages */
    {
        const char *u = strchr(f->name, '_');
        float size = u ? (float)atof(u + 1) : 0, k = size > 0 ? u16(p, e + 0x16) / 20.f / size : 1;
        /* the host's choice (strb_host.field_size): the field's height
           already holds the face's percentages (racemonitor.strb: 14 = 20 x
           70%, the user's PCSX2 Race Monitor print); the pause and results
           movies, matched to their prints, take them again. A: the text
           engine's own sizing (00473820 +0x28) not followed through */
        if (p->host.field_size && f->sy > 0) k /= f->sy;
        sx = f->sx * k * m->a;
        sy = f->sy * k * m->d;
    }
    /* the game's text engine puts a unit between letters (as the menus'
       letter_spacing default) */
    /* 00492978 proportional FT01 placement, shared with quick menus. */
    /* 004940E8: %B<c> consumes a Gaiji button glyph, not literal text. */
    char plain[2048];size_t np=0;float button_width=0;
    for(const char *q=s;*q && np<sizeof plain-1;){
        if(q[0]=='%' && q[1]=='B' && q[2]){strb_font *b=font(p,p->nfonts);const gt4_glyph *g=b?gt4_font_glyph(&b->face,(unsigned char)q[2]):NULL;if(g)button_width+=g->advance*sx;q+=3;}
        else {if(q[0]=='%' && q[1]=='%')q++;plain[np++]=*q++;}
    }plain[np]=0;w=gt4_font_original_width(&f->face,plain,sx)+button_width;
    /* 00479CA0 +0xbc..0x128 -> 00473900/004738A0/00473A40:
       native-font edit text uses the bounds to align, without calling
       0048F790 (the quick-menu abbreviation helper). 00490700 writes
       alignment bounds +0x54..0x60, not a clip rectangle; 00492978
       +0x36c..0x3d0 clips only to the screen. A narrow authored timer
       field therefore keeps the full string, rather than "0'1...". */
    /* the field's own bounds, without Flash's two-unit gutter (the PCSX2
       results: the texts against the bitmaps) */
    x = m->a * (align == 1 ? r : align == 2 ? (l + r) * 0.5f : l) + m->x;
    y = m->d * t + m->y;
    if (align == 1) x -= w;
    else if (align == 2) x -= w * 0.5f;
    base = y + f->face.baseline * sy;
    for (int pass = f->palette == 1 ? 1 : 0; pass < 2; pass++) {
        static const uint8_t black[4] = { 0, 0, 0, 255 };
        const tex1_image *a = &f->face.atlases[0];
        const char *q = s;
        float pen = x;
        unsigned previous=0;
        uint8_t c[4];
        memcpy(c, pass ? color : black, 4);
        c[3] = color[3];
        while (*q) {
            int button=q[0]=='%' && q[1]=='B' && q[2];unsigned code;
            strb_font *face=f;if(button){code=(unsigned char)q[2];q+=3;face=font(p,p->nfonts);}
            else {if(q[0]=='%' && q[1]=='%')q++;code=gt4_text_next(&q);}
            const gt4_glyph *g=face?gt4_font_glyph(&face->face,code):NULL;if(!g)continue;
            float ink=0,step=button?g->advance*sx:gt4_font_original_step(&face->face,previous,code,sx,&ink);
            const tex1_image *atlas=button?&face->face.atlases[0]:a;uint8_t tint[4];memcpy(tint,c,4);
            if(button && pass){static const uint8_t rgb[4][3]={{0,0x99,0x9b},{0xf7,0x1c,0x54},{0x51,0x72,0xb3},{0xf7,0x68,0xa1}};
                for(int k=0;k<3;k++)tint[k]=code>='c' && code<='f'?rgb[code-'c'][k]:0x64;
            }
            if(g->w && g->h)plat_draw_glyph(pass?face->ink:face->outline,pen+ink,base+g->y*sy,g->w*sx,g->h*sy,base,0,
                (float)g->u/atlas->width,(float)g->v/atlas->height,(float)(g->u+g->w)/atlas->width,(float)(g->v+g->h)/atlas->height,tint);
            pen+=step;previous=button || code==' '?0:code;
        }
    }
}

/* /menu/US/result_license.strb +0x3294: four solid fills spell FAIL.
   Each +4/+8 pair names an array of 20-byte contours, not just one path.
   A at +0x32b8 has two contours (outer +3490, hole +34a4). Edge flags
   zero store a straight endpoint at +12/+16; start is contour +4/+8.
   Keep these authored segments, including slopes/concavity and the hole.
   Native scan-band tessellation is cached; GS raster/curves remain A. */
typedef struct {float x0,y0,x1,y1;} solid_edge;
static int ascending_float(const void *a,const void *b)
{float x=*(const float *)a,y=*(const float *)b;return x<y?-1:x>y;}
static float edge_x(const solid_edge *e,float y)
{return e->x0+(y-e->y0)*(e->x1-e->x0)/(e->y1-e->y0);}
static solid_fill *solid_geometry_mode(strb_player *p,size_t fs,int curved)
{
    for(solid_fill *s=p->solids;s;s=s->next)if(s->key==(fs+(curved?p->n:0)))return s;
    solid_fill *s=calloc(1,sizeof *s);if(!s)return NULL;
    s->key=fs+(curved?p->n:0);s->next=p->solids;p->solids=s;
    solid_edge edge[256];float ys[512];unsigned ne=0,ny=0;
    size_t strokes=u32(p,fs+4);unsigned contours=u32(p,fs+8);
    if(!contours || contours>64 || strokes>p->n || contours*20>p->n-strokes)return s;
    for(unsigned j=0;j<contours;j++){
        size_t stroke=strokes+j*20,path=u32(p,stroke+12);unsigned n=u32(p,stroke+16);
        if(!n || ne>=256 || n+1>256-ne || path>p->n || n*20>p->n-path)return s;
        float x=f32(p,stroke+4),y=f32(p,stroke+8);ys[ny++]=y;
        for(unsigned i=0;i<n;i++){
            float x1=f32(p,path+i*20+12),y1=f32(p,path+i*20+16);
            if(u32(p,path+i*20)){
                if(!curved || ne+8>=256 || ny+8>=512)return s;
                /* A: eight quadratic segments replace GS curve rasterization.
                   Control/end points are authored in the STRB, never fitted. */
                float cx=f32(p,path+i*20+4),cy=f32(p,path+i*20+8),startx=x,starty=y;
                for(int k=1;k<=8;k++){float q=k/8.f,z=1-q;
                    float nx=z*z*startx+2*z*q*cx+q*q*x1,ny1=z*z*starty+2*z*q*cy+q*q*y1;
                    edge[ne++]=(solid_edge){x,y,nx,ny1};ys[ny++]=ny1;x=nx;y=ny1;
                }continue;
            }
            if(!isfinite(x+y+x1+y1))return s;
            edge[ne++]=(solid_edge){x,y,x1,y1};ys[ny++]=y1;x=x1;y=y1;
        }
        float x1=f32(p,stroke+4),y1=f32(p,stroke+8);
        if(x!=x1 || y!=y1)edge[ne++]=(solid_edge){x,y,x1,y1};
    }
    qsort(ys,ny,sizeof *ys,ascending_float);unsigned distinct=0;
    for(unsigned i=0;i<ny;i++)if(!distinct || ys[i]!=ys[distinct-1])ys[distinct++]=ys[i];
    /* At most ne/2 spans per band. Allocate only on first use; no
       tessellation, sorting or allocation on subsequent movie frames. */
    s->xy=malloc((size_t)distinct*(ne/2)*sizeof *s->xy);if(!s->xy)return s;
    for(unsigned b=0;b+1<distinct;b++){
        float mid=(ys[b]+ys[b+1])*.5f,xs[256];unsigned ids[256],nx=0;
        for(unsigned i=0;i<ne;i++)if((edge[i].y0<mid)!=(edge[i].y1<mid)){
            float x=edge_x(edge+i,mid);unsigned k=nx;
            while(k && xs[k-1]>x){xs[k]=xs[k-1];ids[k]=ids[k-1];k--;}
            xs[k]=x;ids[k]=i;nx++;
        }
        if(nx&1){free(s->xy);s->xy=NULL;s->count=0;return s;}
        for(unsigned i=0;i+1<nx;i+=2){
            float (*q)[2]=s->xy[s->count++];const solid_edge *left=edge+ids[i],*right=edge+ids[i+1];
            q[0][0]=edge_x(left,ys[b]);q[0][1]=ys[b];
            q[1][0]=edge_x(right,ys[b]);q[1][1]=ys[b];
            q[2][0]=edge_x(left,ys[b+1]);q[2][1]=ys[b+1];
            q[3][0]=edge_x(right,ys[b+1]);q[3][1]=ys[b+1];
        }
    }
    s->supported=1;return s;
}
static solid_fill *solid_geometry(strb_player *p,size_t fs){return solid_geometry_mode(p,fs,0);}
/* 00479CA0 +138..188 scales embedded glyphs by height/1024 and
   places baseline at font+0x12 * 0.05 * scale. The asset supplies glyph
   outlines and advances (12-byte entries); rasterization remains A. */
static void draw_embedded_text(strb_player *p,const xform *m,size_t e,size_t f,const char *text){
 unsigned count=u32(p,f+4);size_t table=u32(p,f);if(count>65536 || table>p->n || count*12>p->n-table)return;
 float scale=u16(p,e+0x16)/1024.f,width=0;uint8_t color[4];color_of(m,p->d+e+16,color);
 for(const char *q=text;*q;q++)for(unsigned k=0;k<count;k++)if(u16(p,table+k*12+8)==(unsigned char)*q){width+=s16(p,table+k*12+10)*.05f*scale;break;}
 float x=f32(p,e),y=f32(p,e+4)+s16(p,f+0x12)*.05f*scale;
 int align=p->d[e+0x1b];if(align==1)x=f32(p,e+8)-width;else if(align==2)x=(x+f32(p,e+8)-width)*.5f;
 for(const char *q=text;*q;q++)for(unsigned k=0;k<count;k++){
  size_t g=table+k*12;if(u16(p,g+8)!=(unsigned char)*q)continue;
  xform c=*m;c.a*=scale;c.d*=scale;c.x+=m->a*x;c.y+=m->d*y;
  size_t paths=u32(p,g);unsigned n=u32(p,g+4);if(paths<=p->n && n<64 && n*12<=p->n-paths)for(unsigned j=0;j<n;j++){
   solid_fill *fill=solid_geometry_mode(p,paths+j*12,1);if(!fill || !fill->supported)continue;
   for(unsigned b=0;b<fill->count;b++){float xy[4][2];for(int v=0;v<4;v++){xy[v][0]=c.a*fill->xy[b][v][0]+c.x;xy[v][1]=c.d*fill->xy[b][v][1]+c.y;}plat_draw_solid_quad(xy,color);}
  }
  x+=s16(p,g+10)*.05f*scale;break;
 }
}
static int fill_solid(strb_player *p,const xform *m,size_t fs,const uint8_t color[4])
{
    solid_fill *s=solid_geometry(p,fs);if(!s || !s->supported)return 0;
    for(unsigned i=0;i<s->count;i++){
        float xy[4][2];for(unsigned k=0;k<4;k++){
            xy[k][0]=m->a*s->xy[i][k][0]+m->x;xy[k][1]=m->d*s->xy[i][k][1]+m->y;
        }plat_draw_solid_quad(xy,color);
    }return 1;
}

/* Gradient fills (types 1 and 2): +0x78 the gradients (12 bytes: their
   stops' offset and count, the kind, 9 linear, 10 radial), each stop a
   ratio and a colour (8 bytes, 0x80 alpha for one); the fill's matrix
   takes the shape's point to the gradient's u = m0 x + m4 y + m12 (the
   bitmaps' layout), its ratio along a linear one (held at the ends), the
   distance from its middle for a radial one. /menu/common/racemonitor.strb
   paints its backgrounds so (0x666666 to 0x1A1A1A down the header). A:
   the GS's own shading not compared; drawn as a grid of corner colours. */
static void gradient_at(strb_player *p, const xform *m, unsigned grad, size_t mx, float x, float y, uint8_t out[4])
{
    size_t g = u32(p, 0x78) + (size_t)grad * 12, stops = u32(p, g);
    unsigned n = u32(p, g + 4), kind = u16(p, g + 8);
    float u = f32(p, mx) * x + f32(p, mx + 16) * y + f32(p, mx + 48);
    float v = f32(p, mx + 4) * x + f32(p, mx + 20) * y + f32(p, mx + 52);
    float t = kind == 10 ? sqrtf((u - .5f) * (u - .5f) + (v - .5f) * (v - .5f)) * 2 : u;
    uint8_t a[4], c[4];
    if (!n || stops + n * 8 > p->n) { memset(out, 0, 4); return; }
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    unsigned i = 0;
    while (i + 1 < n && f32(p, stops + (i + 1) * 8) < t) i++;
    if (i + 1 >= n || t <= f32(p, stops + i * 8)) { color_of(m, p->d + stops + i * 8 + 4, out); return; }
    {
        float t0 = f32(p, stops + i * 8), t1 = f32(p, stops + (i + 1) * 8), k = t1 > t0 ? (t - t0) / (t1 - t0) : 0;
        for (int j = 0; j < 4; j++)
            c[j] = (uint8_t)(p->d[stops + i * 8 + 4 + j] + (p->d[stops + (i + 1) * 8 + 4 + j] - p->d[stops + i * 8 + 4 + j]) * k + 0.5f);
        memcpy(a, c, 4);
        color_of(m, a, out);
    }
}
static void fill_gradient(strb_player *p, const xform *m, size_t fs, size_t fo, float l, float t, float r, float b)
{
    unsigned grad = u16(p, fo + 2), mi = u16(p, fo + 4);
    size_t mx = u32(p, 0x98) + (size_t)mi * 64;
    solid_fill *sf = solid_geometry(p, fs);
    if (grad >= u32(p, 0x7c) || mi >= u32(p, 0x9c)) return;
    /* each band of the outline (or its box) cut into a grid */
    unsigned bands = sf && sf->supported ? sf->count : 1;
    for (unsigned q = 0; q < bands; q++) {
        float x0 = l, y0 = t, x1 = r, y1 = b;
        if (sf && sf->supported) {
            x0 = fminf(sf->xy[q][0][0], sf->xy[q][2][0]); x1 = fmaxf(sf->xy[q][1][0], sf->xy[q][3][0]);
            y0 = sf->xy[q][0][1]; y1 = sf->xy[q][2][1];
        }
        const int nx = 8, ny = 8;
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++) {
                float ax = x0 + (x1 - x0) * i / nx, bx = x0 + (x1 - x0) * (i + 1) / nx;
                float ay = y0 + (y1 - y0) * j / ny, by = y0 + (y1 - y0) * (j + 1) / ny;
                uint8_t c[4][4];
                gradient_at(p, m, grad, mx, ax, ay, c[0]);
                gradient_at(p, m, grad, mx, bx, ay, c[1]);
                gradient_at(p, m, grad, mx, ax, by, c[2]);
                gradient_at(p, m, grad, mx, bx, by, c[3]);
                if (c[0][3] | c[1][3] | c[2][3] | c[3][3])
                    plat_draw_quad4(NULL, m->a * ax + m->x, m->d * ay + m->y, m->a * (bx - ax), m->d * (by - ay), 0, 0, 1, 1, c);
            }
    }
}

/* a shape: each fill's area (the outline's corners) in its colour or its
   bitmap through the bitmap matrix (to the stored texture's 0..1) */
static void draw_shape(strb_player *p, const xform *m, unsigned id)
{
    size_t sh = u32(p, 0x38) + (size_t)id * 24, groups = u32(p, sh + 16);
    if (id >= u32(p, 0x3c)) return;
    for (unsigned g = 0; g < u32(p, sh + 20); g++) {
        size_t fs = u32(p, groups + g * 8);
        for (unsigned k = 0; k < u32(p, groups + g * 8 + 4); k++, fs += 12) {
            unsigned fill = u32(p, fs) & 0xffff;
            size_t fo = u32(p, 0x68) + (size_t)fill * 8, stroke = u32(p, fs + 4), path = u32(p, stroke + 12);
            unsigned edges = u32(p, stroke + 16), type = u16(p, fo);
            float l = f32(p, sh), t = f32(p, sh + 4), r = f32(p, sh + 8), b = f32(p, sh + 12);
            uint8_t c[4];
            if (fill >= u32(p, 0x6c)) continue;
            if (edges && edges < 64) {
                l = t = 1e30f; r = b = -1e30f;
                for (unsigned i = 0; i < edges; i++) {
                    float x = f32(p, path + i * 20 + 12), y = f32(p, path + i * 20 + 16);
                    l = fminf(l, x); r = fmaxf(r, x); t = fminf(t, y); b = fmaxf(b, y);
                }
            }
            if (!(r > l && b > t)) continue;
            {
                float x0 = m->a * l + m->x, y0 = m->d * t + m->y, w = m->a * (r - l), h = m->d * (b - t);
                if (type == 0) {
                    color_of(m, p->d + fo + 4, c);
                    if (c[3] && !fill_solid(p,m,fs,c))
                        plat_draw_quad(NULL, x0, y0, w, h, 0, 0, 1, 1, c);
                } else if (type == 1 || type == 2) {
                    fill_gradient(p, m, fs, fo, l, t, r, b);
                } else if (type == 4) {
                    unsigned bm = u16(p, fo + 2), mi = u16(p, fo + 4);
                    size_t mx = u32(p, 0x98) + (size_t)mi * 64;
                    float su, sv;
                    if (bm >= p->nbitmaps || !p->bitmaps[bm] || mi >= u32(p, 0x9c)) continue;
                    su = (float)p->btw[bm] / p->bw[bm];
                    sv = (float)p->bth[bm] / p->bh[bm];
                    for (int i = 0; i < 4; i++) {
                        float v = 255 * m->col[i] + m->add[i]*(i==3?2:1);
                        c[i] = (uint8_t)(v > 255 ? 255 : v < 0 ? 0 : v + 0.5f);
                    }
                    if (c[3])
                        plat_draw_quad(p->bitmaps[bm], x0, y0, w, h,
                                       (l * f32(p, mx) + f32(p, mx + 48)) * su, (t * f32(p, mx + 20) + f32(p, mx + 52)) * sv,
                                       (r * f32(p, mx) + f32(p, mx + 48)) * su, (b * f32(p, mx + 20) + f32(p, mx + 52)) * sv, c);
                }
            }
        }
    }
}

static int by_depth(const void *a, const void *b) { return ((const int *)a)[1] - ((const int *)b)[1]; }

static void draw_inst(strb_player *p, inst *k, const xform *m)
{
    unsigned n = sprite_places(p, k->sprite), live = 0;
    int (*order)[2];
    if (!k->visible || !n) return;
    order = malloc(n * sizeof *order);
    if (!order) return;
    for (unsigned i = 0; i < n; i++) {
        size_t pl = place_at(p, k->sprite, i);
        if (!place_live(p, pl, k->frame)) continue;
        order[live][0] = (int)i;
        order[live][1] = (int)u16(p, pl + 8);
        live++;
    }
    qsort(order, live, sizeof *order, by_depth);
    for (unsigned j = 0; j < live; j++) {
        unsigned i = (unsigned)order[j][0];
        size_t pl = place_at(p, k->sprite, i), prm = u32(p, pl + 0x18);
        unsigned nprm = u32(p, pl + 0x1c), at = (unsigned)(k->frame - s16(p, pl + 2));
        xform c = *m;
        int type = s16(p, pl), id = s16(p, pl + 6);
        if (nprm) {
            size_t q;
            float sx, sy, x, y;
            size_t q2;
            float f = 0;
            if (at >= nprm) at = nprm - 1;
            q = prm + (size_t)at * 0x24;
            /* between frames the PS2 blends a placement's parameters with
               its next frame's by the movie's progress toward that frame
               (func_00475CA0 -> func_004791C0 keeps movie +0x1C; func_0047D660
               -> func_00474E08 mixes matrix, colour and +0x20) */
            q2 = q;
            if (k->playing && at + 1 < nprm) { q2 = q + 0x24; f = (float)(p->acc * p->rate); f = f < 0 ? 0 : f > 1 ? 1 : f; }
            /* the race screens place everything upright: the rotation
               (+0) and the second word (+4) are not taken */
#define MIX(o) (f32(p, q + (o)) * (1 - f) + f32(p, q2 + (o)) * f)
            sx = MIX(8); sy = MIX(12); x = MIX(16); y = MIX(20);
#undef MIX
            if ((type == 2 || type == 6) && k->kids[i] && k->kids[i]->has_x) x = k->kids[i]->x;
            c.a = m->a * sx;
            c.d = m->d * sy;
            c.x = m->a * x + m->x;
            c.y = m->d * y + m->y;
            /* 00474E08 -> 0047E5F8 interpolates both four-byte colors:
               +0x18 multiplier, +0x1c additive. The latter colors the
               license timer; ignoring it made that text black. A: the
               existing float interpolation differs from 0047DE58's
               quantized 1/256 interpolation; retain that limitation. */
            for(int ch=0;ch<4;ch++){
                c.col[ch]=m->col[ch]*(p->d[q+24+ch]*(1-f)+p->d[q2+24+ch]*f)/128.f;
                c.add[ch]=m->add[ch]+m->col[ch]*(p->d[q+28+ch]*(1-f)+p->d[q2+28+ch]*f);
            }
        }
        switch (type) {
        case 0: draw_shape(p, &c, (unsigned)id); break;
        case 2: case 6: if (k->kids[i]) draw_inst(p, k->kids[i], &c); break;
        case 5: {
            const char *s = k->has_text[i] ? k->text[i] : NULL;
            char key[96];
            if (!s) {
                size_t e = u32(p, 0x60) + (size_t)id * 0x34;
                const char *t = NULL;
                snprintf(key, sizeof key, "%s", u32(p, e + 0x24) ? cstr(p, u32(p, e + 0x24)) : "");
                if (p->rt && key[0]) t = gt4_rtext_get(p->rt, p->page, key);
                s = t ? t : key;
            }
            draw_text(p, &c, (unsigned)id, s);
            break;
        }
        case 7: {
            size_t e = u32(p, 0xb8) + (size_t)id * 0x18;
            uint8_t col[4];
            static const uint8_t one[4] = { 255, 255, 255, 128 };
            float l = f32(p, e), t = f32(p, e + 4), r = f32(p, e + 8), b = f32(p, e + 12);
            const char *name = k->parent ? place_name(p, place_at(p, k->parent->sprite, (unsigned)k->place)) : "";
              /* ResultLicense's 003E0E38 reads the extrender's own
                 name at +0x10. Preserve named car-icon instances, and
                 use the asset name for anonymous placements. */
              if(!*name)name=cstr(p,u32(p,e+0x10));
            color_of(&c, one, col);
            if (p->host.ext && (unsigned)id < u32(p, 0xbc) && col[3])
                p->host.ext(p->host.user, name, c.a * l + c.x, c.d * t + c.y, c.a * (r - l), c.d * (b - t), col);
            if (p->host.ext_path && (unsigned)id < u32(p, 0xbc) && col[3]) {
                /* the named instances from the root down to it, then the
                   extrender's own name ("BAR3.chip") */
                char path[160] = "";
                const inst *chain[16];
                int n = 0;
                for (const inst *q = k; q && q->parent && n < 16; q = q->parent) chain[n++] = q;
                while (n-- > 0) {
                    const char *nm = place_name(p, place_at(p, chain[n]->parent->sprite, (unsigned)chain[n]->place));
                    if (!*nm) continue;
                    size_t at = strlen(path);
                    snprintf(path + at, sizeof path - at, "%s%s", at ? "." : "", nm);
                }
                {
                    size_t at = strlen(path);
                    snprintf(path + at, sizeof path - at, "%s%s", at ? "." : "", cstr(p, u32(p, e + 0x10)));
                }
                p->host.ext_path(p->host.user, path, c.a * l + c.x, c.d * t + c.y, c.a * (r - l), c.d * (b - t), col);
            }
            break;
        }
        default: break;
        }
    }
    free(order);
}

/* ------------------------------------------------------------- the API */

strb_player *strb_open(gt4_vol *vol, const char *path, const gt4_rtext *rt, const char *page, const strb_host *host)
{
    strb_player *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->vol = vol;
    p->rt = rt;
    snprintf(p->page, sizeof p->page, "%s", page ? page : "");
    if (host) p->host = *host;
    p->d = gt4_vol_read(vol, path, &p->n);
    if (!p->d || p->n < 0xc0 || memcmp(p->d, "STRB", 4) || u32(p, 4) != 0 || !u32(p, 0x34)) {
        fprintf(stderr, "strb: cannot read %s\n", path);
        strb_close(p);
        return NULL;
    }
    p->rate = f32(p, 0x1c);
    if (!(p->rate > 0 && p->rate <= 240)) p->rate = 30;
    p->nbitmaps = u32(p, 0xb4);
    if (p->nbitmaps > 256) p->nbitmaps = 0;
    p->bitmaps = calloc(p->nbitmaps + 1, sizeof *p->bitmaps);
    p->bw = calloc(p->nbitmaps + 1, sizeof(int));
    p->bh = calloc(p->nbitmaps + 1, sizeof(int));
    p->btw = calloc(p->nbitmaps + 1, sizeof(int));
    p->bth = calloc(p->nbitmaps + 1, sizeof(int));
    for (unsigned i = 0; i < p->nbitmaps; i++) {
        size_t e = u32(p, 0xb0) + i * 16, off = u32(p, e), len = u32(p, e + 4);
        tex1_image *im = NULL;
        int count;
        if (off > p->n || len > p->n - off) continue;
        count = tex1_decode(p->d + off, len, &im);
        if (count > 0) {
            p->bitmaps[i] = plat_texture_create(im[0].width, im[0].height, im[0].rgba);
            p->bw[i] = im[0].width; p->bh[i] = im[0].height;
            p->btw[i] = im[0].tex_w; p->bth[i] = im[0].tex_h;
        }
        tex1_free(im, count);
    }
    p->nfonts = u32(p, 0x54);
    if (p->nfonts > 64) p->nfonts = 0;
    p->fonts = calloc(p->nfonts + 1, sizeof *p->fonts);
    p->root = inst_new(p, 0, NULL, -1);
    if (p->root) {
        sync_kids(p, p->root);
        frame_actions(p, p->root);
    }
    return p;
}

void strb_close(strb_player *p)
{
    if (!p) return;
    p->depth = 0;
    if (p->root) inst_free(p, p->root);
    while (p->arrays) { strb_array *a = p->arrays; p->arrays = a->next; free(a); }
    while (p->dead) { inst *k = p->dead; p->dead = k->next_dead; inst_free(p, k); }
    for (unsigned i = 0; i < p->nbitmaps; i++) if (p->bitmaps && p->bitmaps[i]) plat_texture_destroy(p->bitmaps[i]);
    for (unsigned i = 0; p->fonts && i <= p->nfonts; i++) {
        strb_font *f = &p->fonts[i];
        if (f->ink) plat_texture_destroy(f->ink);
        if (f->outline) plat_texture_destroy(f->outline);
        if (f->ok) gt4_font_free(&f->face);
    }
    while(p->solids){solid_fill *s=p->solids;p->solids=s->next;free(s->xy);free(s);}
    free(p->fonts);
    free(p->bitmaps); free(p->bw); free(p->bh); free(p->btw); free(p->bth);
    free(p->d);
    free(p);
}

static void focus_buttons(strb_player *p, int dir);
void strb_tick(strb_player *p, double dt)
{
    if (!p || !p->root) return;
    p->acc += dt;
    if (p->acc > 1) p->acc = 1;
    while (p->acc >= 1.0 / p->rate) {
        p->acc -= 1.0 / p->rate;
        step(p, p->root);
        focus_buttons(p, 0);
    }
}

/* a button's conditions (record +12, +16: 4 bytes each, the condition
   bits and the action): bit 0 the focus coming (Flash's IdleToOverUp),
   bit 2 the press (OverUpToOverDown); actions run on its timeline */
static void button_run(strb_player *p, inst *k, unsigned place, unsigned bits)
{
    size_t pl = place_at(p, k->sprite, place), b, list;
    if ((unsigned)s16(p, pl + 6) >= u32(p, 0xa4)) return;
    b = u32(p, 0xa0) + (size_t)s16(p, pl + 6) * 0x14;
    list = u32(p, b + 12);
    for (unsigned j = 0; j < u32(p, b + 16); j++)
        if (u16(p, list + j * 4) & bits) run_action(p, k, u16(p, list + j * 4 + 2));
}

static void press(strb_player *p, inst *k)
{
    unsigned n = sprite_places(p, k->sprite);
    for (unsigned i = 0; i < n; i++) {
        size_t pl = place_at(p, k->sprite, i);
        if (!place_live(p, pl, k->frame)) continue;
        if (s16(p, pl) == 6) button_run(p, k, i, 4);
        else if (k->kids[i]) press(p, k->kids[i]);
    }
}

/* the buttons on screen, their places and whether inside an instance
   named DEFAULT (the first focus) */
typedef struct { inst *k; int place; float x, y; int is_default; } button_at;
static void collect_buttons(strb_player *p, inst *k, float ox, float oy, float sx, float sy, int in_default, button_at *out, int *n)
{
    unsigned count = sprite_places(p, k->sprite);
    if (!k->visible) return;
    for (unsigned i = 0; i < count && *n < 32; i++) {
        size_t pl = place_at(p, k->sprite, i), prm = u32(p, pl + 0x18);
        unsigned nprm = u32(p, pl + 0x1c), at = (unsigned)(k->frame - s16(p, pl + 2));
        float x = ox, y = oy, ax = sx, ay = sy;
        if (!place_live(p, pl, k->frame)) continue;
        if (nprm) {
            size_t q;
            if (at >= nprm) at = nprm - 1;
            q = prm + (size_t)at * 0x24;
            ax = sx * f32(p, q + 8); ay = sy * f32(p, q + 12);
            x = ox + sx * f32(p, q + 16); y = oy + sy * f32(p, q + 20);
        }
        if (s16(p, pl) == 6) {
            out[*n].k = k; out[*n].place = (int)i; out[*n].x = x; out[*n].y = y; out[*n].is_default = in_default;
            (*n)++;
        } else if (s16(p, pl) == 2 && k->kids[i])
            collect_buttons(p, k->kids[i], x, y, ax, ay, in_default || same_name(place_name(p, pl), "DEFAULT"), out, n);
    }
}

/* the focused button over (record +2), the others up (+0) */
static void show_button(strb_player *p, inst *k, unsigned place, int over)
{
    size_t pl = place_at(p, k->sprite, place);
    unsigned want;
    if ((unsigned)s16(p, pl + 6) >= u32(p, 0xa4)) return;
    want = u16(p, u32(p, 0xa0) + (size_t)s16(p, pl + 6) * 0x14 + (over ? 2 : 0));
    if (want >= u32(p, 0x34)) {   /* no sprite for the state (pitmenu's items: 0xFFFF up) */
        if (k->kids[place]) { inst_free(p, k->kids[place]); k->kids[place] = NULL; }
        return;
    }
    if (k->kids[place] && k->kids[place]->sprite == (int)want) return;
    inst_free(p, k->kids[place]);
    k->kids[place] = inst_new(p, (int)want, k, (int)place);
    if (k->kids[place]) { sync_kids(p, k->kids[place]); frame_actions(p, k->kids[place]); }
}

/* keep the focus on a button that is there (else the DEFAULT one, else
   the first), drawing it over; dir moves it: 1 down, 2 up, 3 right, 4 left
   (the nearest button that way, by its place; A: the game's own rule for
   the nearest one is not read) */
static void focus_buttons(strb_player *p, int dir)
{
    button_at b[32];
    int n = 0, cur = -1, best = -1;
    float bd = 1e30f;
    if (!p->root) return;
    collect_buttons(p, p->root, 0, 0, 1, 1, 0, b, &n);
    for (int i = 0; i < n; i++) if (b[i].k == p->focus && b[i].place == p->focus_place) cur = i;
    if (cur < 0) {
        for (int i = 0; i < n && cur < 0; i++) if (b[i].is_default) cur = i;
        if (cur < 0 && n) cur = 0;
    } else if (dir && p->buttons_on) {
        for (int i = 0; i < n; i++) {
            float dx = b[i].x - b[cur].x, dy = b[i].y - b[cur].y, d;
            if (i == cur) continue;
            if ((dir == 1 && dy <= 0.5f) || (dir == 2 && dy >= -0.5f) || (dir == 3 && dx <= 0.5f) || (dir == 4 && dx >= -0.5f)) continue;
            d = dir <= 2 ? fabsf(dy) * 4 + fabsf(dx) : fabsf(dx) * 4 + fabsf(dy);
            if (d < bd) { bd = d; best = i; }
        }
        if (best >= 0) {
            cur = best;
            p->focus = b[cur].k;
            p->focus_place = b[cur].place;
            button_run(p, b[cur].k, (unsigned)b[cur].place, 1);
        }
    }
    p->focus = cur >= 0 ? b[cur].k : NULL;
    p->focus_place = cur >= 0 ? b[cur].place : -1;
    /* the first focus once navigation is on is a focus coming too: its
       IdleToOverUp runs (the pause's buttons play "cursor" as it opens) */
    if (p->buttons_on && p->focus && !p->focus_shown) {
        p->focus_shown = 1;
        button_run(p, p->focus, (unsigned)p->focus_place, 1);
    }
    for (int i = 0; i < n; i++) show_button(p, b[i].k, (unsigned)b[i].place, i == cur && p->buttons_on);
}

static void key_down(strb_player *p, inst *k)
{
    unsigned n = sprite_places(p, k->sprite);
    clip_events(p, k, 0x40);       /* Flash's keyDown */
    for (unsigned i = 0; i < n; i++) if (k->kids[i]) key_down(p, k->kids[i]);
}

void strb_key(strb_player *p, int code)
{
    if (!p || !p->root) return;
    p->key = code;
    key_down(p, p->root);
    if (!p->buttons_on) return;
    if (code == 13) {
        focus_buttons(p, 0);
        if (p->focus) button_run(p, p->focus, (unsigned)p->focus_place, 4);
        focus_buttons(p, 0);
        return;
    }
    if (code == 40 || code == 38) focus_buttons(p, code == 40 ? 1 : 2);
    else if (p->nav_all && (code == 39 || code == 37)) focus_buttons(p, code == 39 ? 3 : 4);
}

void strb_navigate(strb_player *p, int all)
{
    if (!p) return;
    p->nav_all = all;
    p->buttons_on = 1;
}

void strb_press(strb_player *p) { if (p && p->root) press(p, p->root); }

/* ------------------------------------------------- the host's own access */

/* a placement by a dotted path from the root ("BAR1.NAME"), as the game's
   native code finds an instance by name (func_00480878) */
static strb_value find_path(strb_player *p, const char *path)
{
    strb_value cur = obj(p ? p->root : NULL, -1);
    char part[64];
    while (path && *path && cur.t == STRB_OBJ && cur.obj && cur.place < 0) {
        const char *dot = strchr(path, '.');
        size_t n = dot ? (size_t)(dot - path) : strlen(path);
        if (n >= sizeof part) return obj(NULL, -1);
        memcpy(part, path, n);
        part[n] = 0;
        cur = child(p, cur.obj, part);
        path = dot ? dot + 1 : NULL;
    }
    return path && *path ? obj(NULL, -1) : cur;
}
int strb_goto(strb_player *p, const char *path, int frame)
{
    strb_value v = path && *path ? find_path(p, path) : obj(p ? p->root : NULL, -1);
    if (v.t != STRB_OBJ || !v.obj || v.place >= 0) return 0;
    if (((inst *)v.obj)->frame != frame) go_to(p, v.obj, frame, 0);
    else ((inst *)v.obj)->playing = 0;
    return 1;
}
int strb_text(strb_player *p, const char *path, const char *text)
{
    strb_value v = find_path(p, path), t = str(text ? text : "");
    if (v.t != STRB_OBJ || !v.obj || v.place < 0) return 0;
    set_member(&v, "text", &t);
    return 1;
}
/* func_003DA668 -> 0048F790: a text cut to its edit text's width with
   "..." when longer (the face at the field's height as draw_text) */
int strb_text_fit(strb_player *p, const char *path, const char *text)
{
    strb_value v = find_path(p, path);
    char buf[96];
    if (v.t != STRB_OBJ || !v.obj || v.place < 0 || !text) return 0;
    {
        inst *k = v.obj;
        size_t pl = place_at(p, k->sprite, (unsigned)v.place), e;
        unsigned id = (unsigned)s16(p, pl + 6);
        strb_font *f;
        float width, sx;
        if (id >= u32(p, 0x64)) return strb_text(p, path, text);
        e = u32(p, 0x60) + (size_t)id * 0x34;
        if (!(f = font(p, u16(p, e + 0x14)))) return strb_text(p, path, text);
        {
            const char *u = strchr(f->name, '_');
            float size = u ? (float)atof(u + 1) : 0, k = size > 0 ? u16(p, e + 0x16) / 20.f / size : 1;
            if (p->host.field_size && f->sy > 0) k /= f->sy;   /* as draw_text */
            sx = f->sx * k;
        }
        width = f32(p, e + 8) - f32(p, e);
        snprintf(buf, sizeof buf, "%s", text);
        if (gt4_font_original_width(&f->face, buf, sx) > width) {
            size_t n = strlen(buf);
            while (n > 0) {
                char cut[100];
                n--;
                while (n > 0 && ((unsigned char)buf[n] & 0xc0) == 0x80) n--;
                snprintf(cut, sizeof cut, "%.*s...", (int)n, buf);
                if (gt4_font_original_width(&f->face, cut, sx) <= width) { snprintf(buf, sizeof buf, "%s", cut); break; }
            }
        }
    }
    return strb_text(p, path, buf);
}
int strb_set_x(strb_player *p, const char *path, float x)
{
    strb_value v = find_path(p, path);
    if (v.t != STRB_OBJ || !v.obj || v.place >= 0) return 0;
    ((inst *)v.obj)->has_x = 1;
    ((inst *)v.obj)->x = x;
    return 1;
}
int strb_visible(strb_player *p, const char *path, int on)
{
    strb_value v = find_path(p, path), b = boolean(on);
    if (v.t != STRB_OBJ || !v.obj || v.place >= 0) return 0;
    set_member(&v, "_visible", &b);
    return 1;
}

void strb_draw(strb_player *p) { strb_draw_faded(p, 1); }
void strb_draw_faded(strb_player *p, float alpha)
{
    xform m = { 1, 1, 0, 0, { 1, 1, 1, 1 } };
    m.col[3] = alpha;
    if (!p || !p->root) return;
    plat_video_set_view(640, 480);
    draw_inst(p, p->root, &m);
    plat_video_set_view(640, 448);
}
