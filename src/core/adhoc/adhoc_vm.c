#include "adhoc_vm.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ah_obj {
    uint8_t kind, mark, mkind, is_method;
    ah_obj *gc_next;
    /* AO_STRING */
    char   *s;
    size_t  len;
    /* AO_ARRAY, AO_MAP (k only for maps) */
    ah_val *v, *k;
    int     n, cap;
    /* AO_MODULE: module, class or instance */
    const char    *name;
    ah_obj        *parent;     /* enclosing module: name lookup continues there */
    ah_obj        *klass;      /* instance: its class; class: its base */
    const char   **msym;
    ah_val        *mval;
    int            mn, mcap;
    const char   **attrs;      /* class attributes, copied into each instance */
    int            nattrs;
    const ah_host *host;
    void          *hdata;
    /* AO_FUNC */
    ah_code *code;
    ah_obj  *module;
    /* AO_BOUND */
    ah_obj *self;
    ah_val  fn;
    /* AO_NATIVE */
    ah_native_fn nfn;
    void        *ndata;
    uint32_t     reference_mask;
    const char  *nname;
};

typedef struct {
    char  **s;
    size_t  n, cap;
} symtab;

struct ah_vm {
    ah_obj  *heap;
    size_t   nobjs, gc_at;
    ah_obj  *root;
    symtab   syms, seen;
    ah_val  *stack;
    int      sp, cap;
    ah_obj **mods;               /* modules entered with MODULE_DEFINE/CLASS_DEFINE */
    int      modsp, modcap;
    ah_val  *pins;
    int      npins, pincap;
    ah_code **codes;
    int      ncodes;
    int      depth;
    unsigned instructions_left, progress_ticks;
    void (*progress)(void *); void *progress_user;
    const char *where_src;       /* for messages */
    uint32_t    where_line;
    void (*log)(void *user, const char *msg);
    void *log_user;
    void *user;
    ah_val eval_ref;             /* the variable the last EVAL read, and where its value sits: */
    int    eval_sp;              /* `x.value` on a scalar static is x itself (labomode.ad's ejected_) */
};

#define MAX_IMPORTS 16

typedef struct {
    ah_code    *code;
    ah_obj     *module;
    ah_obj     *self;
    ah_val     *locals;
    ah_val      result;
    int         modbase;
    int         top;                    /* script top level: imports go to the module */
    const char *isym[MAX_IMPORTS];
    ah_val      ival[MAX_IMPORTS];
    int         nimp;
} frame;

/* ---------------------------------------------------------------- symbols */

static size_t hash(const char *s)
{
    size_t h = 2166136261u;
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

static const char *intern(symtab *t, const char *s, int *added)
{
    if (t->n * 2 >= t->cap) {
        size_t cap = t->cap ? t->cap * 2 : 1024;
        char **ns = calloc(cap, sizeof *ns);
        if (!ns) return NULL;
        for (size_t i = 0; i < t->cap; i++)
            if (t->s[i]) {
                size_t j = hash(t->s[i]) & (cap - 1);
                while (ns[j]) j = (j + 1) & (cap - 1);
                ns[j] = t->s[i];
            }
        free(t->s);
        t->s = ns;
        t->cap = cap;
    }
    size_t j = hash(s) & (t->cap - 1);
    while (t->s[j]) {
        if (!strcmp(t->s[j], s)) { if (added) *added = 0; return t->s[j]; }
        j = (j + 1) & (t->cap - 1);
    }
    t->s[j] = strdup(s);
    t->n++;
    if (added) *added = 1;
    return t->s[j];
}

static void symtab_free(symtab *t)
{
    for (size_t i = 0; i < t->cap; i++) free(t->s[i]);
    free(t->s);
}

const char *ah_sym(ah_vm *vm, const char *s) { return intern(&vm->syms, s ? s : "", NULL); }

/* ---------------------------------------------------------------- messages */

void ah_set_user(ah_vm *vm, void *user) { vm->user = user; }
void *ah_user(const ah_vm *vm) { return vm->user; }

void ah_set_log(ah_vm *vm, void (*log)(void *, const char *), void *user)
{
    vm->log = log;
    vm->log_user = user;
}

void ah_logf(ah_vm *vm, const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (vm->log) vm->log(vm->log_user, msg);
    else fprintf(stderr, "adhoc: %s\n", msg);
}

/* A problem the script survives (missing engine member, nil use): reported
   once per place, execution goes on with nil. */
static void warn(ah_vm *vm, const char *fmt, ...)
{
    char msg[400], full[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    snprintf(full, sizeof full, "%s:%u: %s", vm->where_src ? vm->where_src : "?", vm->where_line, msg);
    int added = 0;
    intern(&vm->seen, full, &added);
    if (added) ah_logf(vm, "%s", full);
}

int ah_error(ah_vm *vm, const char *fmt, ...)
{
    char msg[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    ah_logf(vm, "%s:%u: error: %s", vm->where_src ? vm->where_src : "?", vm->where_line, msg);
    return -1;
}

/* ---------------------------------------------------------------- values */

ah_val ah_nil(void) { ah_val v; memset(&v, 0, sizeof v); return v; }
ah_val ah_int(int64_t i) { ah_val v = ah_nil(); v.t = i == (int32_t)i ? AV_INT : AV_LONG; v.u.i = i; return v; }
ah_val ah_bool(int b) { ah_val v = ah_nil(); v.t = AV_BOOL; v.u.i = b != 0; return v; }
ah_val ah_float(double f) { ah_val v = ah_nil(); v.t = AV_FLOAT; v.u.f = (float)f; return v; }
ah_val ah_objv(ah_obj *o) { ah_val v = ah_nil(); if (o) { v.t = AV_OBJ; v.u.o = o; } return v; }
static ah_val dbl(double f) { ah_val v = ah_nil(); v.t = AV_DOUBLE; v.u.f = f; return v; }
static ah_val voidv(void) { ah_val v = ah_nil(); v.t = AV_VOID; return v; }

static ah_obj *alloc(ah_vm *vm, ah_okind kind)
{
    ah_obj *o = calloc(1, sizeof *o);
    if (!o) { fprintf(stderr, "adhoc: out of memory\n"); abort(); }
    o->kind = (uint8_t)kind;
    o->gc_next = vm->heap;
    vm->heap = o;
    vm->nobjs++;
    return o;
}

static ah_val strn(ah_vm *vm, const char *s, size_t len)
{
    ah_obj *o = alloc(vm, AO_STRING);
    o->s = malloc(len + 1);
    if (!o->s) abort();
    memcpy(o->s, s, len);
    o->s[len] = 0;
    o->len = len;
    return ah_objv(o);
}

ah_val ah_str(ah_vm *vm, const char *s) { return strn(vm, s ? s : "", s ? strlen(s) : 0); }

ah_val ah_native(ah_vm *vm, const char *name, ah_native_fn fn, void *data)
{
    ah_obj *o = alloc(vm, AO_NATIVE);
    o->nfn = fn;
    o->ndata = data;
    o->nname = ah_sym(vm, name);
    return ah_objv(o);
}

ah_val ah_native_refs(ah_vm *vm,const char *name,ah_native_fn fn,void *data,uint32_t mask){
    ah_val value=ah_native(vm,name,fn,data);value.u.o->reference_mask=mask;return value;
}

ah_val ah_bind(ah_vm *vm, ah_obj *self, ah_val fn)
{
    ah_obj *o = alloc(vm, AO_BOUND);
    o->self = self;
    o->fn = fn;
    return ah_objv(o);
}

ah_obj *ah_array_new(ah_vm *vm) { return alloc(vm, AO_ARRAY); }

static void grow(ah_obj *a, int need)
{
    if (need <= a->cap) return;
    int cap = a->cap ? a->cap * 2 : 8;
    while (cap < need) cap *= 2;
    a->v = realloc(a->v, (size_t)cap * sizeof *a->v);
    if (a->kind == AO_MAP) a->k = realloc(a->k, (size_t)cap * sizeof *a->k);
    if (!a->v || (a->kind == AO_MAP && !a->k)) abort();
    a->cap = cap;
}

void ah_array_push(ah_vm *vm, ah_obj *a, ah_val v)
{
    (void)vm;
    grow(a, a->n + 1);
    a->v[a->n++] = v;
}
int ah_array_set(ah_obj *a,int i,ah_val v){if(!a || a->kind!=AO_ARRAY || i<0 || i>=a->n)return 0;a->v[i]=v;return 1;}

int ah_array_size(const ah_obj *a) { return a && a->kind == AO_ARRAY ? a->n : 0; }
ah_val ah_array_get(const ah_obj *a, int i) { return a && i >= 0 && i < a->n ? a->v[i] : ah_nil(); }

ah_obj *ah_module_new(ah_vm *vm, const char *name, ah_obj *parent, const ah_host *host, void *hdata)
{
    ah_obj *o = alloc(vm, AO_MODULE);
    o->mkind = AM_MODULE;
    o->name = ah_sym(vm, name);
    o->parent = parent;
    o->host = host;
    o->hdata = hdata;
    return o;
}

void *ah_host_data(const ah_obj *o) { return o && o->kind == AO_MODULE ? o->hdata : NULL; }
const ah_host *ah_host_of(const ah_obj *o) { return o && o->kind == AO_MODULE ? o->host : NULL; }
const char *ah_module_name(const ah_obj *o) { return o && o->kind == AO_MODULE ? o->name : NULL; }
ah_obj *ah_module_parent(const ah_obj *o) { return o && o->kind == AO_MODULE ? o->parent : NULL; }

int ah_truthy(ah_val v)
{
    switch (v.t) {
    case AV_BOOL: case AV_INT: case AV_LONG: return v.u.i != 0;
    case AV_FLOAT: case AV_DOUBLE: return v.u.f != 0;
    case AV_OBJ: return 1;
    default: return 0;
    }
}

int ah_is_num(ah_val v) { return v.t == AV_BOOL || v.t == AV_INT || v.t == AV_LONG || v.t == AV_FLOAT || v.t == AV_DOUBLE; }
static int is_int(ah_val v) { return v.t == AV_BOOL || v.t == AV_INT || v.t == AV_LONG; }

double ah_num(ah_val v)
{
    if (is_int(v)) return (double)v.u.i;
    if (v.t == AV_FLOAT || v.t == AV_DOUBLE) return v.u.f;
    return 0;
}

const char *ah_cstr(ah_val v) { return v.t == AV_OBJ && v.u.o->kind == AO_STRING ? v.u.o->s : NULL; }
ah_obj *ah_as_obj(ah_val v, ah_okind kind) { return v.t == AV_OBJ && v.u.o->kind == kind ? v.u.o : NULL; }

const char *ah_to_text(ah_val v, char *buf, size_t len)
{
    switch (v.t) {
    case AV_NIL: snprintf(buf, len, "nil"); break;
    case AV_VOID: snprintf(buf, len, "void"); break;
    case AV_BOOL: snprintf(buf, len, "%s", v.u.i ? "true" : "false"); break;
    case AV_INT: case AV_LONG: snprintf(buf, len, "%lld", (long long)v.u.i); break;
    case AV_FLOAT: case AV_DOUBLE: snprintf(buf, len, "%g", v.u.f); break;
    case AV_OBJ:
        switch (v.u.o->kind) {
        case AO_STRING: return v.u.o->s;
        case AO_MODULE: snprintf(buf, len, "%s", v.u.o->name ? v.u.o->name : "module"); break;
        case AO_NATIVE: snprintf(buf, len, "native %s", v.u.o->nname); break;
        default: snprintf(buf, len, "object"); break;
        }
        break;
    default: snprintf(buf, len, "ref"); break;
    }
    return buf;
}

/* ---------------------------------------------------------------- members */

static int own_index(const ah_obj *o, const char *sym)
{
    for (int i = 0; i < o->mn; i++)
        if (o->msym[i] == sym) return i;
    return -1;
}

static void put_own(ah_obj *o, const char *sym, ah_val v)
{
    int i = own_index(o, sym);
    if (i >= 0) { o->mval[i] = v; return; }
    if (o->mn == o->mcap) {
        o->mcap = o->mcap ? o->mcap * 2 : 8;
        o->msym = realloc(o->msym, (size_t)o->mcap * sizeof *o->msym);
        o->mval = realloc(o->mval, (size_t)o->mcap * sizeof *o->mval);
        if (!o->msym || !o->mval) abort();
    }
    o->msym[o->mn] = sym;
    o->mval[o->mn++] = v;
}

static ah_val maybe_bind(ah_vm *vm, ah_obj *self, ah_val v)
{
    if (v.t == AV_OBJ && (v.u.o->kind == AO_FUNC || v.u.o->kind == AO_NATIVE) && v.u.o->is_method)
        return ah_bind(vm, self, v);
    return v;
}

/* func_00313900 -> 00315B78. Split on '{', use the decimal token
   before '}' as the zero-based argument index, then append its suffix.
   This is String.build, distinct from printf-style String.format. The
   adapter covers ordinary string/number arguments used by the disc UI;
   object conversion and malformed-token equivalence remain unvalidated. */
static int nat_string_build(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)data;*ret=ah_nil();if(argc<=0)return 0;
    size_t capacity=self->len+1;
    for(int i=0;i<argc;i++){
        char temp[64];const char *text=ah_to_text(argv[i],temp,sizeof temp);
        size_t n=strlen(text);
        /* A token may occur repeatedly. Allocate on demand below. */
        if(n>SIZE_MAX-capacity)return ah_error(vm,"build size overflow");
        capacity+=n;
    }
    char *out=malloc(capacity);if(!out)return ah_error(vm,"build allocation failed");
    size_t used=0;const char *part=self->s,*brace=strchr(part,'{');
    size_t prefix=brace?(size_t)(brace-part):strlen(part);
    memcpy(out,part,prefix);used=prefix;
    while(brace){
        part=brace+1;brace=strchr(part,'{');
        const char *end=brace?brace:part+strlen(part);
        const char *close=memchr(part,'}',(size_t)(end-part));
        char temp[64];long index=strtol(part,NULL,10);
        const char *text=index>=0 && index<argc?ah_to_text(argv[index],temp,sizeof temp):"";
        size_t value=strlen(text),suffix=close?(size_t)(end-close-1):0;
        if(value>SIZE_MAX-used-1 || suffix>SIZE_MAX-used-value-1){free(out);return ah_error(vm,"build size overflow");}
        size_t needed=used+value+suffix+1;
        if(needed>capacity){char *grown=realloc(out,needed);if(!grown){free(out);return ah_error(vm,"build allocation failed");}out=grown;capacity=needed;}
        memcpy(out+used,text,value);used+=value;
        if(suffix){memcpy(out+used,close+1,suffix);used+=suffix;}
    }
    out[used]=0;*ret=ah_str(vm,out);free(out);return 0;
}

static int builtin_member(ah_vm *vm, ah_obj *o, const char *sym, ah_val *out);
static int get_member(ah_vm *vm, ah_obj *o, const char *sym, ah_val *out);

static int nat_defined(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)d;
    const char *name = argc ? ah_cstr(argv[0]) : NULL;
    ah_val value;
    *ret = ah_bool(name && get_member(vm, self, ah_sym(vm, name), &value));
    return 0;
}

/* module.defineStatic(name, value): adds a static member by name. */
static int nat_define_static(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)d;
    char buf[64];
    if (argc >= 1) put_own(self, ah_sym(vm, ah_to_text(argv[0], buf, sizeof buf)), argc >= 2 ? argv[1] : ah_nil());
    *ret = argc >= 2 ? argv[1] : ah_nil();
    return 0;
}

/* Module.clearStatic: 00304820 +0x10 -> 003063F8 clears the module's
   own static table (+0x18), releasing its values. Keep its identity and
   class/host chain; the native VM's collector releases unreachable objects. */
static int nat_clear_static(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)d; (void)argv;
    if(argc)return ah_error(vm,"clearStatic takes no arguments");
    if(self->mn)memset(self->mval,0,(size_t)self->mn*sizeof *self->mval);
    self->mn=0;
    *ret=ah_nil();return 0;
}

/* Attribute read with the full chain: own members, engine hooks, class chain. */
static int get_member(ah_vm *vm, ah_obj *o, const char *sym, ah_val *out)
{
    if (o->kind != AO_MODULE) return builtin_member(vm, o, sym, out);
    int i = own_index(o, sym);
    if (i >= 0) { *out = maybe_bind(vm, o, o->mval[i]); return 1; }
    if (o->host && o->host->get && o->host->get(vm, o, sym, out)) return 1;
    for (ah_obj *k = o->klass; k; k = k->klass) {
        i = own_index(k, sym);
        if (i >= 0) { *out = maybe_bind(vm, o, k->mval[i]); return 1; }
        if (k->host && k->host->get && k->host->get(vm, o, sym, out)) return 1;
    }
    if (!strcmp(sym, "defineStatic")) {
        *out = ah_bind(vm, o, ah_native(vm, sym, nat_define_static, NULL));
        return 1;
    }
    if (!strcmp(sym, "clearStatic")) {
        *out = ah_bind(vm, o, ah_native(vm, sym, nat_clear_static, NULL));
        return 1;
    }
    if (!strcmp(sym, "defined")) {
        *out = ah_bind(vm, o, ah_native(vm, sym, nat_defined, NULL)); return 1;
    }
    return 0;
}

static void set_member(ah_vm *vm, ah_obj *o, const char *sym, ah_val v)
{
    if (o->kind != AO_MODULE) { warn(vm, "cannot set '%s' on a non-object", sym); return; }
    if (o->host && o->host->set && o->host->set(vm, o, sym, v)) return;
    for (ah_obj *k = o->klass; k; k = k->klass)
        if (k->host && k->host->set && k->host->set(vm, o, sym, v)) return;
    put_own(o, sym, v);
}

int ah_module_attach_host(ah_obj *o,const ah_host *host){
    if(!o || o->kind!=AO_MODULE || (o->host && o->host!=host))return 0;
    o->host=host;return 1;
}

void ah_set_member(ah_vm *vm, ah_obj *o, const char *name, ah_val v) { set_member(vm, o, ah_sym(vm, name), v); }

int ah_get_member(ah_vm *vm, ah_obj *o, const char *name, ah_val *out)
{
    *out = ah_nil();
    return o && get_member(vm, o, ah_sym(vm, name), out);
}

/* A member the module defines itself, not one it inherits */
int ah_get_own_member(ah_vm *vm, ah_obj *o, const char *name, ah_val *out)
{
    int i;
    *out = ah_nil();
    if (!o || o->kind != AO_MODULE) return 0;
    i = own_index(o, ah_sym(vm, name));
    if (i < 0) return 0;
    *out = o->mval[i];
    return 1;
}

void ah_copy_members(ah_vm *vm, ah_obj *dst, const ah_obj *src)
{
    (void)vm;
    for (int i = 0; i < src->mn; i++)
        if (own_index(dst, src->msym[i]) < 0) put_own(dst, src->msym[i], src->mval[i]);
    dst->klass = src->klass;
}

/* Built-in members of strings, arrays and maps (as far as GT4 scripts use them). */
static int nat_array_push(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)d;
    for (int i = 0; i < argc; i++) ah_array_push(vm, self, argv[i]);
    *ret = ah_objv(self);
    return 0;
}

/* array.sort(): in place, ascending (labomode.ad sortSelection orders the
   chosen list indices); numbers by value, strings by their bytes */
static int sort_before(ah_val a,ah_val b)
{
    const char *x=ah_cstr(a),*y=ah_cstr(b);
    if(x && y)return strcmp(x,y)<0;
    return ah_num(a)<ah_num(b);
}
static int nat_array_sort(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;(void)d;(void)argc;(void)argv;
    for (int i = 1; i < self->n; i++) {   /* stable: equal ones keep their order */
        ah_val v = self->v[i];
        int j = i;
        while (j > 0 && sort_before(v, self->v[j - 1])) { self->v[j] = self->v[j - 1]; j--; }
        self->v[j] = v;
    }
    *ret = ah_objv(self);
    return 0;
}

/* Scalar conversion methods are called by the authored attract timer. */
static int nat_scalar_convert(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)argv;ah_val value=ah_objv(self);if(self->kind==AO_MODULE)ah_get_member(vm,self,"value",&value);
    if(argc)return ah_error(vm,"conversion takes no arguments");
    const char *operation=data;char text[128];
    if(!strcmp(operation,"toString")){*ret=ah_str(vm,ah_to_text(value,text,sizeof text));return 0;}
    double number=ah_num(value);const char *string=ah_cstr(value);
    if(string && !strcmp(operation,"toInt")){
        /* Original string integer conversion (005A2F88) accepts a numeric
           prefix and returns zero for NONE; PresentRoot depends on this. */
        uint32_t bits=(uint32_t)strtoll(string,NULL,10);int32_t integer;
        memcpy(&integer,&bits,4);*ret=ah_int(integer);return 0;
    }
    if(string){char *end;number=strtod(string,&end);if(end==string || *end)return ah_error(vm,"invalid numeric string");}
    if(!isfinite(number))return ah_error(vm,"non-finite numeric conversion");
    if(!strcmp(operation,"toInt")){
        if(number < -9223372036854775808.0 || number >= 9223372036854775808.0)return ah_error(vm,"integer conversion overflow");
        *ret=ah_int((int64_t)number);
    }else *ret=ah_float(number);
    return 0;
}
static int nat_string_substr(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)data;if(argc<1 || argc>2 || !ah_is_num(argv[0]) || (argc==2 && !ah_is_num(argv[1])))return ah_error(vm,"substr requires start and optional length");
    double start=ah_num(argv[0]),length=argc==2?ah_num(argv[1]):(double)self->len;
    if(!isfinite(start) || !isfinite(length) || start<0 || length<0)return ah_error(vm,"invalid substring range");
    size_t offset=start>self->len?self->len:(size_t)start;
    size_t count=length>self->len-offset?self->len-offset:(size_t)length;
    char *text=malloc(count+1);if(!text)return ah_error(vm,"substring allocation failed");
    memcpy(text,self->s+offset,count);text[count]=0;*ret=ah_str(vm,text);free(text);return 0;
}

static int nat_string_case(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)data;(void)argv;if(argc)return ah_error(vm,"downcase takes no arguments");
    /* Native string library boundary. PhotoShoot Mode.Reset L1143 uses
       ASCII shutter labels; bytes outside ASCII remain unchanged. */
    char *text=malloc(self->len+1);if(!text)return ah_error(vm,"downcase allocation failed");
    for(size_t i=0;i<self->len;i++){unsigned char c=self->s[i];text[i]=(char)(c>='A' && c<='Z'?c+('a'-'A'):c);}
    text[self->len]=0;*ret=ah_str(vm,text);free(text);return 0;
}

static int nat_string_format(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)data;char out[4096];size_t used=0;int arg=0;
    for(const char *p=self->s;*p;p++) {
        if(*p!='%'){if(used+1>=sizeof out)return ah_error(vm,"format too long");out[used++]=*p;continue;}
        p++;if(*p=='%'){if(used+1>=sizeof out)return ah_error(vm,"format too long");out[used++]='%';continue;}
        int width=0,zero=*p=='0';
        while(*p>='0' && *p<='9'){width=width*10+*p++-'0';if(width>128)return ah_error(vm,"format width too large");}
        int precision=0;
        if(*p=='.'){p++;while(*p>='0' && *p<='9'){precision=precision*10+*p++-'0';if(precision>9)return ah_error(vm,"format precision too large");}}
        if(arg>=argc)return ah_error(vm,"format missing argument");
        char value[256];const char *text=value;
        if(*p=='d' || *p=='i')snprintf(value,sizeof value,zero?"%0*lld":"%*lld",width,(long long)ah_num(argv[arg]));
        else if(*p=='s')text=ah_to_text(argv[arg],value,sizeof value);
        /* 00490A40: %t is an integer scaled by 10^precision. quick-mt
           L167 uses %.2t for BestMaxSpeed, already in hundredths. */
        else if(*p=='t'){
            double number=ah_num(argv[arg]);if(!isfinite(number) || number<-9223372036854775808. || number>=9223372036854775808.)return ah_error(vm,"invalid fixed decimal");
            int64_t n=(int64_t)number;uint64_t a=n<0?0-(uint64_t)n:(uint64_t)n,scale=1;
            for(int k=0;k<precision;k++)scale*=10;
            if(precision)snprintf(value,sizeof value,"%s%llu.%0*llu",n<0?"-":"",(unsigned long long)(a/scale),precision,(unsigned long long)(a%scale));
            else snprintf(value,sizeof value,"%lld",(long long)n);
        }
        /* the PC layer's texts (ui_translate): %K a size in KB as KB, MB,
           GB or TB, %_ an argument dropped (a memory card's slot) */
        else if(*p=='K'){
            double kb=ah_num(argv[arg]);static const char *const unit[]={"KB","MB","GB","TB"};int u=0;
            while(kb>=1024 && u<3){kb/=1024;u++;}
            if(u)snprintf(value,sizeof value,"%.1f%s",kb,unit[u]);else snprintf(value,sizeof value,"%d%s",(int)kb,unit[0]);
        }
        else if(*p=='_')value[0]=0;
        else return ah_error(vm,"unsupported format conversion");
        arg++;size_t len=strlen(text);
        if(len>=sizeof out-used)return ah_error(vm,"format too long");
        memcpy(out+used,text,len);used+=len;
    }
    out[used]=0;*ret=ah_str(vm,out);return 0;
}

static int builtin_member(ah_vm *vm, ah_obj *o, const char *sym, ah_val *out)
{
    if (!strcmp(sym, "size") || !strcmp(sym, "length")) {
        *out = ah_int(o->kind == AO_STRING ? (int64_t)o->len : o->kind == AO_ARRAY || o->kind == AO_MAP ? o->n : 0);
        return 1;
    }
    if(o->kind==AO_STRING && !strcmp(sym,"build")){*out=ah_bind(vm,o,ah_native(vm,sym,nat_string_build,NULL));return 1;}
    if(o->kind==AO_STRING && !strcmp(sym,"substr")){*out=ah_bind(vm,o,ah_native(vm,sym,nat_string_substr,NULL));return 1;}
    if(o->kind==AO_STRING && !strcmp(sym,"downcase")){*out=ah_bind(vm,o,ah_native(vm,sym,nat_string_case,NULL));return 1;}
    if(o->kind==AO_STRING && (!strcmp(sym,"toInt") || !strcmp(sym,"toFloat") || !strcmp(sym,"toString"))){*out=ah_bind(vm,o,ah_native(vm,sym,nat_scalar_convert,(void *)sym));return 1;}
    if(o->kind==AO_STRING && !strcmp(sym,"format")) {
        *out=ah_bind(vm,o,ah_native(vm,"format",nat_string_format,NULL));return 1;
    }
    if (o->kind == AO_ARRAY && !strcmp(sym, "sort")) {
        *out = ah_bind(vm, o, ah_native(vm, "sort", nat_array_sort, NULL));
        return 1;
    }
    if (o->kind == AO_ARRAY && !strcmp(sym, "push")) {
        *out = ah_bind(vm, o, ah_native(vm, "push", nat_array_push, NULL));
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- stack */

static void push(ah_vm *vm, ah_val v)
{
    if (vm->sp == vm->cap) {
        vm->cap = vm->cap ? vm->cap * 2 : 256;
        vm->stack = realloc(vm->stack, (size_t)vm->cap * sizeof *vm->stack);
        if (!vm->stack) abort();
    }
    vm->stack[vm->sp++] = v;
}

static ah_val pop(ah_vm *vm)
{
    if (vm->sp == 0) { warn(vm, "stack underflow"); return ah_nil(); }
    return vm->stack[--vm->sp];
}

static void push_mod(ah_vm *vm, ah_obj *m)
{
    if (vm->modsp == vm->modcap) {
        vm->modcap = vm->modcap ? vm->modcap * 2 : 16;
        vm->mods = realloc(vm->mods, (size_t)vm->modcap * sizeof *vm->mods);
        if (!vm->mods) abort();
    }
    vm->mods[vm->modsp++] = m;
}

static ah_obj *cur_module(ah_vm *vm, frame *fr) { return vm->modsp > fr->modbase ? vm->mods[vm->modsp - 1] : fr->module; }

/* ---------------------------------------------------------------- references */

static int key_equal(uint8_t kt, const ah_val *k, ah_val key);
static ah_val elem_key(const ah_val *ref)
{
    ah_val k = ah_nil();
    k.t = ref->kt;
    k.u = ref->u;
    return k;
}

static int map_find(ah_obj *m, ah_val key)
{
    for (int i = 0; i < m->n; i++)
        if (key_equal(m->k[i].t, &m->k[i], key)) return i;
    return -1;
}

static ah_val deref(ah_vm *vm, ah_val v)
{
    ah_val out = ah_nil();
    switch (v.t) {
    case AV_REF_SLOT: return *v.u.slot;
    case AV_REF_MEMBER:
        if (!v.c) return out;
        if (v.c->kind == AO_MODULE && v.c->host && v.c->host->read && v.c->host->read(vm, v.c, v.sym, &out)) return out;
        if (!get_member(vm, v.c, v.sym, &out)) {
            warn(vm, "undefined '%s' in %s", v.sym, v.c->kind == AO_MODULE && v.c->name ? v.c->name : "value");
            out = ah_nil();
        }
        return out;
    case AV_REF_ELEM: {
        ah_val key = elem_key(&v);
        if (v.c->kind == AO_ARRAY) {
            int64_t i = (int64_t)ah_num(key);
            if (i >= 0 && i < v.c->n) return v.c->v[i];
            warn(vm, "array index %lld out of range (size %d)", (long long)i, v.c->n);
        } else if (v.c->kind == AO_MAP) {
            int i = map_find(v.c, key);
            if (i >= 0) return v.c->v[i];
        } else if (v.c->kind == AO_STRING) {
            int64_t i = (int64_t)ah_num(key);
            if (i >= 0 && (size_t)i < v.c->len) return strn(vm, v.c->s + i, 1);
        }
        return out;
    }
    default: return v;
    }
}

static void assign(ah_vm *vm, ah_val ref, ah_val v)
{
    switch (ref.t) {
    case AV_REF_SLOT: *ref.u.slot = v; break;
    case AV_REF_MEMBER:
        if (ref.c) set_member(vm, ref.c, ref.sym, v);
        else warn(vm, "assignment to a member of nil ('%s')", ref.sym);
        break;
    case AV_REF_ELEM: {
        ah_val key = elem_key(&ref);
        if (ref.c->kind == AO_ARRAY) {
            int64_t i = (int64_t)ah_num(key);
            if (i < 0 || i > 1000000) { warn(vm, "bad array index %lld", (long long)i); break; }
            grow(ref.c, (int)i + 1);
            while (ref.c->n <= i) ref.c->v[ref.c->n++] = ah_nil();
            ref.c->v[i] = v;
        } else if (ref.c->kind == AO_MAP) {
            int i = map_find(ref.c, key);
            if (i < 0) { grow(ref.c, ref.c->n + 1); i = ref.c->n++; ref.c->k[i] = key; }
            ref.c->v[i] = v;
        } else {
            warn(vm, "element assignment on a non-container");
        }
        break;
    }
    default: warn(vm, "assignment to a value that is not a variable");
    }
}

int ah_reference_set(ah_vm *vm,ah_val reference,ah_val value){
    if(reference.t!=AV_REF_SLOT && reference.t!=AV_REF_MEMBER && reference.t!=AV_REF_ELEM)return 0;
    assign(vm,reference,value);return 1;
}

static ah_val member_ref(ah_obj *c, const char *sym)
{
    ah_val r = ah_nil();
    r.t = AV_REF_MEMBER;
    r.c = c;
    r.sym = sym;
    return r;
}

static ah_val slot_ref(ah_val *slot)
{
    ah_val r = ah_nil();
    r.t = AV_REF_SLOT;
    r.u.slot = slot;
    return r;
}

static ah_val element_ref(ah_vm *vm, ah_val obj, ah_val key)
{
    if (obj.t != AV_OBJ) { warn(vm, "indexing nil or a number"); return member_ref(NULL, ah_sym(vm, "[]")); }
    ah_obj *o = obj.u.o;
    if (o->kind == AO_MODULE) {                      /* object["name"] = member */
        char buf[64];
        return member_ref(o, ah_sym(vm, ah_to_text(key, buf, sizeof buf)));
    }
    ah_val r = ah_nil();
    r.t = AV_REF_ELEM;
    r.c = o;
    r.kt = key.t;
    r.u = key.u;
    return r;
}

/* ---------------------------------------------------------------- operators */

static int str_eq(ah_val a, ah_val b)
{
    const char *x = ah_cstr(a), *y = ah_cstr(b);
    return x && y && a.u.o->len == b.u.o->len && !memcmp(x, y, a.u.o->len);
}

static int values_equal(ah_val a, ah_val b)
{
    if (ah_is_num(a) && ah_is_num(b)) return ah_num(a) == ah_num(b);
    if ((a.t == AV_NIL || a.t == AV_VOID) || (b.t == AV_NIL || b.t == AV_VOID))
        return (a.t == AV_NIL || a.t == AV_VOID) && (b.t == AV_NIL || b.t == AV_VOID);
    if (a.t == AV_OBJ && b.t == AV_OBJ) return a.u.o == b.u.o || str_eq(a, b);
    return 0;
}

static int key_equal(uint8_t kt, const ah_val *k, ah_val key)
{
    (void)kt;
    return values_equal(*k, key);
}

static ah_val concat(ah_vm *vm, ah_val a, ah_val b)
{
    char ba[64], bb[64];
    const char *x = ah_to_text(a, ba, sizeof ba), *y = ah_to_text(b, bb, sizeof bb);
    size_t lx = strlen(x), ly = strlen(y);
    char *buf = malloc(lx + ly + 1);
    if (!buf) abort();
    memcpy(buf, x, lx);
    memcpy(buf + lx, y, ly + 1);
    ah_val r = strn(vm, buf, lx + ly);
    free(buf);
    return r;
}

/* Result type of arithmetic: integers stay integers, float wins over them,
   double over float. Floats are single precision as on the PS2. */
static ah_val arith_result(ah_val a, ah_val b, double f, int64_t i)
{
    if (is_int(a) && is_int(b)) {
        ah_val r = ah_int(i);
        if (a.t == AV_LONG || b.t == AV_LONG) r.t = AV_LONG;
        return r;
    }
    if (a.t == AV_DOUBLE || b.t == AV_DOUBLE) return dbl(f);
    return ah_float(f);
}

static ah_val binop(ah_vm *vm, const char *op, ah_val a, ah_val b)
{
    if (!strcmp(op, "==")) return ah_bool(values_equal(a, b));
    if (!strcmp(op, "!=")) return ah_bool(!values_equal(a, b));
    if (!strcmp(op, "+") && (ah_cstr(a) || ah_cstr(b))) return concat(vm, a, b);
    /* Integer relational callbacks 002FD0B0 (+0x24..40, '<'),
       002FCF80 ('>'), 002FD148 ('<='), 002FD018 ('>=') dispatch the
       RHS toInteger slot +0x58. String 00314A40 (+0x30) calls decimal
       strtol (005A2F88 -> 005A7170), rather than rejecting a string.
       Car.getLength (0013E370) returns this decimal string; rejecting
       it silently disabled the authored compact-car length restriction. */
    if (a.t == AV_INT && ah_cstr(b) && op[0] && strchr("<>", op[0])) {
        /* 005A2F88 +0x14..20 truncates the strtol result to signed 32 bits. */
        int32_t lhs = (int32_t)a.u.i, rhs = (int32_t)strtoll(ah_cstr(b), NULL, 10);
        if (!strcmp(op, "<")) return ah_bool(lhs < rhs);
        if (!strcmp(op, "<=")) return ah_bool(lhs <= rhs);
        if (!strcmp(op, ">")) return ah_bool(lhs > rhs);
        if (!strcmp(op, ">=")) return ah_bool(lhs >= rhs);
    }
    if (ah_cstr(a) && ah_cstr(b) && op[0] && strchr("<>", op[0])) {
        int c = strcmp(ah_cstr(a), ah_cstr(b));
        if (!strcmp(op, "<")) return ah_bool(c < 0);
        if (!strcmp(op, "<=")) return ah_bool(c <= 0);
        if (!strcmp(op, ">")) return ah_bool(c > 0);
        if (!strcmp(op, ">=")) return ah_bool(c >= 0);
    }
    if (!ah_is_num(a) || !ah_is_num(b)) {
        warn(vm, "operator '%s' on non-numbers", op);
        return ah_nil();
    }
    double x = ah_num(a), y = ah_num(b);
    int64_t i = a.u.i, j = b.u.i;
    int ints = is_int(a) && is_int(b);
    if (!strcmp(op, "<")) return ah_bool(x < y);
    if (!strcmp(op, "<=")) return ah_bool(x <= y);
    if (!strcmp(op, ">")) return ah_bool(x > y);
    if (!strcmp(op, ">=")) return ah_bool(x >= y);
    if (!strcmp(op, "+")) return arith_result(a, b, x + y, ints ? i + j : 0);
    if (!strcmp(op, "-")) return arith_result(a, b, x - y, ints ? i - j : 0);
    if (!strcmp(op, "*")) return arith_result(a, b, x * y, ints ? i * j : 0);
    if (!strcmp(op, "/") || !strcmp(op, "%")) {
        if (ints && j == 0) { warn(vm, "integer division by zero"); return ah_nil(); }
        if (op[0] == '/') return arith_result(a, b, x / y, ints ? i / j : 0);
        return arith_result(a, b, fmod(x, y), ints ? i % j : 0);
    }
    if (!ints) { warn(vm, "operator '%s' needs integers", op); return ah_nil(); }
    if (!strcmp(op, "&")) return arith_result(a, b, 0, i & j);
    if (!strcmp(op, "|")) return arith_result(a, b, 0, i | j);
    if (!strcmp(op, "^")) return arith_result(a, b, 0, i ^ j);
    if (!strcmp(op, "<<")) return arith_result(a, b, 0, i << (j & 63));
    if (!strcmp(op, ">>")) return arith_result(a, b, 0, i >> (j & 63));
    warn(vm, "unknown operator '%s'", op);
    return ah_nil();
}

static ah_val unop(ah_vm *vm, const char *op, ah_val a)
{
    if (!strcmp(op, "!")) return ah_bool(!ah_truthy(a));
    if (!ah_is_num(a)) { warn(vm, "operator '%s' on a non-number", op); return ah_nil(); }
    if (!strcmp(op, "-@")) {
        if (is_int(a)) return ah_int(-a.u.i);
        a.u.f = -a.u.f;
        return a;
    }
    if (!strcmp(op, "+@")) return a;
    if (!strcmp(op, "~") && is_int(a)) return ah_int(~a.u.i);
    warn(vm, "unknown operator '%s'", op);
    return ah_nil();
}

/* ---------------------------------------------------------------- names */

/* A path list is the parts followed by the joined name, or one local name. */
static int path_parts(const ah_ins *in) { return in->npath > 1 ? in->npath - 1 : in->npath; }

static int find_in_chain(ah_vm *vm, frame *fr, const char *sym, ah_obj **container, ah_val *val)
{
    for (int i = 0; i < fr->nimp; i++)
        if (fr->isym[i] == sym) { *container = NULL; *val = slot_ref(&fr->ival[i]); return 2; }
    if (fr->self && get_member(vm, fr->self, sym, val)) { *container = fr->self; return 1; }
    for (ah_obj *m = cur_module(vm, fr); m; m = m->parent)
        if (get_member(vm, m, sym, val)) { *container = m; return 1; }
    return 0;
}

/* Reference for VARIABLE_PUSH/EVAL with a static (module) path. */
static ah_val static_ref(ah_vm *vm, frame *fr, const ah_ins *in)
{
    int n = path_parts(in);
    const char *first = ah_sym(vm, in->path[0]);
    ah_obj *c = NULL;
    ah_val v = ah_nil();
    int found = find_in_chain(vm, fr, first, &c, &v);
    if (n == 1) {
        if (found == 2) return v;                                /* import slot */
        return member_ref(found ? c : cur_module(vm, fr), first);
    }
    if (found == 2) v = *v.u.slot;
    if (!found) { warn(vm, "undefined '%s'", in->path[in->npath - 1]); return member_ref(NULL, first); }
    for (int i = 1; i < n; i++) {
        if (v.t != AV_OBJ) { warn(vm, "undefined '%s'", in->path[in->npath - 1]); return member_ref(NULL, first); }
        const char *s = ah_sym(vm, in->path[i]);
        if (i == n - 1) return member_ref(v.u.o, s);
        ah_val next;
        if (!get_member(vm, v.u.o, s, &next)) { warn(vm, "undefined '%s'", in->path[in->npath - 1]); return member_ref(NULL, s); }
        v = next;
    }
    return v;
}

static ah_val variable_ref(ah_vm *vm, frame *fr, const ah_ins *in)
{
    if (in->npath == 1 && in->i2 >= 0 && in->i2 < fr->code->slots)
        return slot_ref(&fr->locals[in->i2]);
    return static_ref(vm, fr, in);
}

/* MODULE_DEFINE: finds or creates the module path, starting in the current module. */
static ah_obj *enter_module(ah_vm *vm, frame *fr, const ah_ins *in)
{
    int n = path_parts(in);
    ah_obj *m = cur_module(vm, fr);
    for (int i = 0; i < n; i++) {
        const char *s = ah_sym(vm, in->path[i]);
        ah_val v = ah_nil();
        int k = own_index(m, s);
        if (k >= 0) v = m->mval[k];
        else if (i == 0 && n > 1) {
            for (ah_obj *p = m->parent; p && v.t == AV_NIL; p = p->parent)
                if ((k = own_index(p, s)) >= 0) v = p->mval[k];
        }
        if (v.t == AV_OBJ && v.u.o->kind == AO_MODULE) { m = v.u.o; continue; }
        ah_obj *nm = ah_module_new(vm, s, m, NULL, NULL);
        put_own(m, s, ah_objv(nm));
        m = nm;
    }
    return m;
}

/* ---------------------------------------------------------------- calls */

static int exec(ah_vm *vm, frame *fr, ah_val *ret);

static int call_code(ah_vm *vm, ah_obj *fn, ah_obj *self, int argc, const ah_val *argv, ah_val *ret)
{
    ah_code *code = fn->code;
    if (vm->depth > 200) return ah_error(vm, "call depth exceeded");
    frame fr;
    memset(&fr, 0, sizeof fr);
    fr.code = code;
    fr.module = fn->module;
    fr.self = self;
    fr.modbase = vm->modsp;
    fr.locals = calloc((size_t)(code->slots ? code->slots : 1), sizeof *fr.locals);
    if (!fr.locals) abort();
    for (int i = 0; i < argc && i < code->slots && i < code->nparams; i++) fr.locals[i] = argv[i];
    const char *src = vm->where_src;
    uint32_t line = vm->where_line;
    int stack_base = vm->sp;
    if (!vm->depth) vm->instructions_left = 1000000;
    vm->depth++;
    int rc = exec(vm, &fr, ret);
    vm->depth--;
    vm->sp = stack_base;
    vm->modsp = fr.modbase;
    vm->where_src = src;
    vm->where_line = line;
    free(fr.locals);
    return rc;
}

ah_obj *ah_native_class(ah_vm *vm,const char *name,ah_native_fn initialize,void *data)
{
    ah_obj *type=ah_module_new(vm,name,NULL,NULL,NULL);type->mkind=AM_CLASS;
    ah_val method=ah_native(vm,"__init__",initialize,data);method.u.o->is_method=1;
    ah_set_member(vm,type,"__init__",method);return type;
}

static int instantiate(ah_vm *vm, ah_obj *cls, int argc, const ah_val *argv, ah_val *ret)
{
    ah_obj *o = ah_module_new(vm, cls->name, cls->parent, NULL, NULL);
    o->mkind = AM_INSTANCE;
    o->klass = cls;
    for (ah_obj *k = cls; k; k = k->klass)
        for (int i = 0; i < k->nattrs; i++)
            if (own_index(o, k->attrs[i]) < 0) put_own(o, k->attrs[i], ah_nil());
    *ret = ah_objv(o);
    ah_val init, ignored;
    if (get_member(vm, o, ah_sym(vm, "__init__"), &init))
        if (ah_call(vm, init, argc, argv, &ignored) < 0) return -1;
    return 0;
}

static int call_value(ah_vm *vm, ah_val fn, int argc, const ah_val *argv, ah_val *ret);

int ah_call(ah_vm *vm, ah_val fn, int argc, const ah_val *argv, ah_val *ret)
{
    int sp = vm->sp, mods = vm->modsp;
    int rc = call_value(vm, fn, argc, argv, ret);
    if (rc < 0) { vm->sp = sp; vm->modsp = mods; }
    return rc;
}

static int call_value(ah_vm *vm, ah_val fn, int argc, const ah_val *argv, ah_val *ret)
{
    *ret = ah_nil();
    if (fn.t != AV_OBJ) { warn(vm, "call of nil (engine function not provided?)"); return 0; }
    ah_obj *o = fn.u.o, *self = NULL;
    if (o->kind == AO_BOUND) {
        self = o->self;
        fn = o->fn;
        if (fn.t != AV_OBJ) return 0;
        o = fn.u.o;
    }
    switch (o->kind) {
    case AO_FUNC: return call_code(vm, o, self, argc, argv, ret);
    case AO_NATIVE: {
        ah_val tmp[16];
        ah_val *args = argc <= 16 ? tmp : malloc((size_t)argc * sizeof *args);
        if (!args) abort();
        memcpy(args, argv, (size_t)argc * sizeof *args);       /* the VM stack may move */
        int rc = o->nfn(vm, self, o->ndata, argc, args, ret);
        if (args != tmp) free(args);
        return rc;
    }
    case AO_MODULE:
        if (o->mkind == AM_CLASS) return instantiate(vm, o, argc, argv, ret);
        warn(vm, "call of module '%s'", o->name);
        return 0;
    default:
        warn(vm, "call of a value that is not a function");
        return 0;
    }
}

int ah_call_member(ah_vm *vm, ah_obj *obj, const char *name, int argc, const ah_val *argv, ah_val *ret)
{
    ah_val fn;
    *ret = ah_nil();
    if (!obj || !get_member(vm, obj, ah_sym(vm, name), &fn)) return 0;
    int base = vm->sp, mods = vm->modsp;
    int rc = ah_call(vm, fn, argc, argv, ret);
    vm->sp = base;
    vm->modsp = mods;
    return rc < 0 ? -1 : 1;
}

int ah_run_module(ah_vm *vm, ah_code *code,ah_obj *module)
{
    vm->codes = realloc(vm->codes, (size_t)(vm->ncodes + 1) * sizeof *vm->codes);
    if (!vm->codes) abort();
    vm->codes[vm->ncodes++] = code;
    frame fr;
    memset(&fr, 0, sizeof fr);
    fr.code = code;
    fr.module = module;
    fr.top = 1;
    fr.modbase = vm->modsp;
    fr.locals = calloc((size_t)(code->slots ? code->slots : 1), sizeof *fr.locals);
    if (!fr.locals) abort();
    ah_val ret;
    int base = vm->sp;
    if(!vm->depth)vm->instructions_left = 1000000;
    vm->depth++;
    int rc = exec(vm, &fr, &ret);
    vm->depth--;
    vm->sp = base;
    vm->modsp = fr.modbase;
    free(fr.locals);
    return rc;
}

int ah_run_script(ah_vm *vm,ah_code *code){return ah_run_module(vm,code,vm->root);}

/* ---------------------------------------------------------------- interpreter */

static ah_val func_value(ah_vm *vm, ah_code *code, ah_obj *module, int method)
{
    ah_obj *f = alloc(vm, AO_FUNC);
    f->code = code;
    f->module = module;
    f->is_method = (uint8_t)method;
    return ah_objv(f);
}

static void do_import(ah_vm *vm, frame *fr, const ah_ins *in)
{
    int n = path_parts(in);
    ah_val v = ah_nil();
    ah_obj *c;
    if (!find_in_chain(vm, fr, ah_sym(vm, in->path[0]), &c, &v)) { warn(vm, "import: no module '%s'", in->path[0]); return; }
    v = deref(vm, v);
    for (int i = 1; i < n && v.t == AV_OBJ; i++) {
        ah_val next;
        if (!get_member(vm, v.u.o, ah_sym(vm, in->path[i]), &next)) next = ah_nil();
        v = next;
    }
    if (v.t != AV_OBJ || v.u.o->kind != AO_MODULE) { warn(vm, "import: '%s' is not a module", in->path[in->npath - 1]); return; }
    ah_obj *src = v.u.o;
    ah_obj *dst = cur_module(vm, fr);
    if (!strcmp(in->s, "*")) {
        for (int i = 0; i < src->mn; i++) {
            if (fr->top) put_own(dst, src->msym[i], src->mval[i]);
            else if (fr->nimp < MAX_IMPORTS) { fr->isym[fr->nimp] = src->msym[i]; fr->ival[fr->nimp++] = src->mval[i]; }
        }
        return;
    }
    const char *s = ah_sym(vm, in->s);
    ah_val val;
    if (!get_member(vm, src, s, &val)) { warn(vm, "import: no '%s' in '%s'", in->s, src->name); val = ah_nil(); }
    if (fr->top) put_own(dst, s, val);
    else if (fr->nimp < MAX_IMPORTS) { fr->isym[fr->nimp] = s; fr->ival[fr->nimp++] = val; }
    else warn(vm, "too many imports in one function");
}

unsigned ah_exchange_instruction_budget(ah_vm *vm,unsigned budget)
{unsigned previous=vm->instructions_left;vm->instructions_left=budget;return previous;}

static int exec(ah_vm *vm, frame *fr, ah_val *ret)
{
    ah_code *code = fr->code;
    int base = vm->sp;
    int ip = 0;
    fr->result = ah_nil();
    vm->where_src = code->source;
    for (;;) {
        if (ip < 0 || ip >= code->count) { *ret = fr->result; vm->sp = base; return 0; }
        if (!vm->instructions_left--) return ah_error(vm, "instruction budget exceeded (unsupported wait or script loop)");
        if(vm->progress && ++vm->progress_ticks>=1024){vm->progress_ticks=0;vm->progress(vm->progress_user);}
        const ah_ins *in = &code->ins[ip++];
        vm->where_line = in->line;
        switch (in->op) {
        case AH_NOP: case AH_SOURCE_FILE: case AH_LEAVE: case AH_LOCAL_DEFINE:
            break;
        case AH_INT_CONST: push(vm, ah_int((int32_t)in->i)); break;
        case AH_U_INT_CONST: { ah_val v = ah_int(in->i); v.t = AV_LONG; push(vm, v); break; }
        case AH_LONG_CONST: case AH_U_LONG_CONST: { ah_val v = ah_int(in->i); v.t = AV_LONG; push(vm, v); break; }
        case AH_FLOAT_CONST: push(vm, ah_float(in->f)); break;
        case AH_DOUBLE_CONST: push(vm, dbl(in->f)); break;
        case AH_BOOL_CONST: push(vm, ah_bool((int)in->i)); break;
        case AH_NIL_CONST: push(vm, ah_nil()); break;
        case AH_VOID_CONST: push(vm, voidv()); break;
        case AH_STRING_CONST: case AH_SYMBOL_CONST: push(vm, ah_str(vm, in->s)); break;
        case AH_FUNCTION_CONST: case AH_METHOD_CONST:
            push(vm, func_value(vm, in->frame, cur_module(vm, fr), in->op == AH_METHOD_CONST));
            break;

        case AH_VARIABLE_PUSH: push(vm, variable_ref(vm, fr, in)); break;
        case AH_VARIABLE_EVAL: push(vm, deref(vm, variable_ref(vm, fr, in))); break;
        case AH_EVAL: {
            ah_val v = pop(vm);
            vm->eval_ref = v; vm->eval_sp = vm->sp;
            push(vm, deref(vm, v));
            break;
        }
        case AH_ATTRIBUTE_PUSH: case AH_ATTRIBUTE_EVAL: {
            int at = vm->sp - 1;
            ah_val o = deref(vm, pop(vm));
            const char *s = ah_sym(vm, in->op == AH_ATTRIBUTE_PUSH ? in->s : in->path[0]);
            ah_val r;
            if (o.t != AV_OBJ && !strcmp(s, "value") && at == vm->eval_sp &&
                (vm->eval_ref.t == AV_REF_MEMBER || vm->eval_ref.t == AV_REF_SLOT || vm->eval_ref.t == AV_REF_ELEM))
                r = vm->eval_ref;
            else if(ah_is_num(o) && (!strcmp(s,"toFloat") || !strcmp(s,"toInt") || !strcmp(s,"toString"))) {
                ah_obj *box=ah_module_new(vm,"number",NULL,NULL,NULL);
                ah_set_member(vm,box,"value",o);
                r=ah_bind(vm,box,ah_native(vm,s,nat_scalar_convert,(void *)s));
            } else if (o.t != AV_OBJ) { warn(vm, "'%s' of nil", s); r = member_ref(NULL, s); }
            else r = member_ref(o.u.o, s);
            push(vm, in->op == AH_ATTRIBUTE_EVAL ? deref(vm, r) : r);
            break;
        }
        case AH_ELEMENT_PUSH: case AH_ELEMENT_EVAL: {
            ah_val k = deref(vm, pop(vm)), o = deref(vm, pop(vm));
            ah_val r = element_ref(vm, o, k);
            push(vm, in->op == AH_ELEMENT_EVAL ? deref(vm, r) : r);
            break;
        }

        case AH_ASSIGN_OLD: case AH_ASSIGN: case AH_ASSIGN_POP: {
            ah_val v = deref(vm, pop(vm)), r = pop(vm);
            assign(vm, r, v);
            if (in->op != AH_ASSIGN_POP) push(vm, v);
            break;
        }
        case AH_LIST_ASSIGN_OLD: case AH_LIST_ASSIGN: {
            ah_val v = deref(vm, pop(vm));
            int n = (int)in->i;
            if (n < 0 || n > vm->sp - base) return ah_error(vm, "bad list assignment");
            ah_obj *a = ah_as_obj(v, AO_ARRAY);
            for (int i = 0; i < n; i++)
                assign(vm, vm->stack[vm->sp - n + i], a ? ah_array_get(a, i) : ah_nil());
            vm->sp -= n;
            push(vm, v);
            break;
        }
        case AH_POP_OLD: fr->result = deref(vm, pop(vm)); break;
        case AH_POP: pop(vm); break;

        case AH_BINARY_OPERATOR: {
            ah_val b = deref(vm, pop(vm)), a = deref(vm, pop(vm));
            push(vm, !strcmp(in->s, "[]") ? element_ref(vm, a, b) : binop(vm, in->s, a, b));
            break;
        }
        case AH_UNARY_OPERATOR: push(vm, unop(vm, in->s, deref(vm, pop(vm)))); break;
        case AH_BINARY_ASSIGN_OPERATOR: {
            ah_val v = deref(vm, pop(vm)), r = pop(vm);
            ah_val nv = binop(vm, in->s, deref(vm, r), v);
            assign(vm, r, nv);
            push(vm, nv);
            break;
        }
        case AH_UNARY_ASSIGN_OPERATOR: {
            ah_val r = pop(vm), old = deref(vm, r);
            int inc = strstr(in->s, "++") != NULL;
            ah_val nv = binop(vm, inc ? "+" : "-", old, ah_int(1));
            assign(vm, r, nv);
            push(vm, in->s[0] == '@' ? old : nv);                 /* "@++" post, "++@" pre */
            break;
        }

        case AH_JUMP: ip = (int)in->i; break;
        case AH_JUMP_IF_TRUE: if (ah_truthy(deref(vm, pop(vm)))) ip = (int)in->i; break;
        case AH_JUMP_IF_FALSE: if (!ah_truthy(deref(vm, pop(vm)))) ip = (int)in->i; break;
        case AH_LOGICAL_AND_OLD: case AH_LOGICAL_AND: case AH_LOGICAL_OR_OLD: case AH_LOGICAL_OR: {
            /* the operand stays on the stack: kept as the result when jumping,
               dropped by the POP_OLD that follows otherwise */
            if (vm->sp == base) return ah_error(vm, "logical operator on an empty stack");
            ah_val v = deref(vm, vm->stack[vm->sp - 1]);
            vm->stack[vm->sp - 1] = v;
            int is_or = in->op == AH_LOGICAL_OR_OLD || in->op == AH_LOGICAL_OR;
            if (ah_truthy(v) == is_or) ip = (int)in->i;
            break;
        }

        case AH_CALL: case AH_CALL_OLD: {
            int n = (int)in->i;
            if (n < 0 || n + 1 > vm->sp - base) return ah_error(vm, "bad call argument count");
            ah_val fn = deref(vm, vm->stack[vm->sp - n - 1]);
            ah_obj *native=fn.t==AV_OBJ?fn.u.o:NULL;
            if(native && native->kind==AO_BOUND)native=native->fn.t==AV_OBJ?native->fn.u.o:NULL;
            uint32_t references=native && native->kind==AO_NATIVE?native->reference_mask:0;
            for (int i = vm->sp - n; i < vm->sp; i++){
                unsigned argument=(unsigned)(i-(vm->sp-n));
                if(argument>=32 || !(references&(1u<<argument)))vm->stack[i]=deref(vm,vm->stack[i]);
            }
            ah_val r;
            int start = vm->sp - n;
            int rc = ah_call(vm, fn, n, &vm->stack[start], &r);
            vm->sp = start - 1;
            vm->where_src = code->source;
            if (rc < 0) return -1;
            push(vm, r);
            break;
        }
        case AH_SET_STATE_OLD: case AH_SET_STATE:
            if (in->i == 0 && vm->modsp > fr->modbase) { vm->modsp--; break; }
            *ret = fr->result;
            vm->sp = base;
            return 0;

        case AH_ARRAY_CONST_OLD: case AH_ARRAY_CONST: {
            int n = in->op == AH_ARRAY_CONST ? 0 : (int)in->i;
            if (n < 0 || n > vm->sp - base) return ah_error(vm, "bad array size");
            ah_obj *a = ah_array_new(vm);
            for (int i = vm->sp - n; i < vm->sp; i++) ah_array_push(vm, a, deref(vm, vm->stack[i]));
            vm->sp -= n;
            push(vm, ah_objv(a));
            break;
        }
        case AH_ARRAY_PUSH: {
            ah_val v = deref(vm, pop(vm));
            ah_obj *a = vm->sp > base ? ah_as_obj(vm->stack[vm->sp - 1], AO_ARRAY) : NULL;
            if (a) ah_array_push(vm, a, v);
            break;
        }
        case AH_MAP_CONST_OLD: case AH_MAP_CONST: {
            int n = in->op == AH_MAP_CONST ? 0 : (int)in->i;
            if (n < 0 || 2 * n > vm->sp - base) return ah_error(vm, "bad map size");
            ah_obj *m = alloc(vm, AO_MAP);
            for (int i = vm->sp - 2 * n; i < vm->sp; i += 2) {
                ah_val r = element_ref(vm, ah_objv(m), deref(vm, vm->stack[i]));
                assign(vm, r, deref(vm, vm->stack[i + 1]));
            }
            vm->sp -= 2 * n;
            push(vm, ah_objv(m));
            break;
        }
        case AH_MAP_INSERT: {
            ah_val v = deref(vm, pop(vm)), k = deref(vm, pop(vm));
            if (vm->sp > base && ah_as_obj(vm->stack[vm->sp - 1], AO_MAP))
                assign(vm, element_ref(vm, vm->stack[vm->sp - 1], k), v);
            break;
        }
        case AH_STRING_PUSH: case AH_PRINT: {
            int n = (int)in->i;
            if (n < 0 || n > vm->sp - base) return ah_error(vm, "bad string count");
            ah_val s = ah_str(vm, "");
            for (int i = vm->sp - n; i < vm->sp; i++) s = concat(vm, s, deref(vm, vm->stack[i]));
            vm->sp -= n;
            if (in->op == AH_PRINT) { ah_logf(vm, "print: %s", ah_cstr(s)); push(vm, ah_nil()); }
            else push(vm, s);
            break;
        }

        case AH_MODULE_DEFINE: push_mod(vm, enter_module(vm, fr, in)); break;
        case AH_CLASS_DEFINE: {
            ah_obj *m = cur_module(vm, fr);
            const char *s = ah_sym(vm, in->s);
            int k = own_index(m, s);
            ah_obj *cls = k >= 0 ? ah_as_obj(m->mval[k], AO_MODULE) : NULL;
            if (!cls) {
                cls = ah_module_new(vm, s, m, NULL, NULL);
                cls->mkind = AM_CLASS;
                put_own(m, s, ah_objv(cls));
            }
            if (in->npath > 0) {
                ah_obj *c;
                ah_val b;
                if (find_in_chain(vm, fr, ah_sym(vm, in->path[0]), &c, &b)) {
                    b = deref(vm, b);
                    if (ah_as_obj(b, AO_MODULE) && b.u.o != cls) cls->klass = b.u.o;
                }
            }
            push_mod(vm, cls);
            break;
        }
        case AH_ATTRIBUTE_DEFINE: {
            ah_obj *m = cur_module(vm, fr);
            const char *s = ah_sym(vm, in->s);
            if (m->mkind == AM_CLASS) {
                m->attrs = realloc((void *)m->attrs, (size_t)(m->nattrs + 1) * sizeof *m->attrs);
                if (!m->attrs) abort();
                m->attrs[m->nattrs++] = s;
            } else if (own_index(m, s) < 0) {
                put_own(m, s, ah_nil());
            }
            break;
        }
        case AH_STATIC_DEFINE: {
            ah_obj *m = cur_module(vm, fr);
            const char *s = ah_sym(vm, in->s);
            if (own_index(m, s) < 0) put_own(m, s, ah_nil());
            break;
        }
        case AH_FUNCTION_DEFINE: case AH_METHOD_DEFINE: {
            ah_obj *m = cur_module(vm, fr);
            put_own(m, ah_sym(vm, in->s), func_value(vm, in->frame, m, in->op == AH_METHOD_DEFINE));
            break;
        }
        case AH_IMPORT: do_import(vm, fr, in); break;
        case AH_UNDEF: {
            ah_obj *m = cur_module(vm, fr);
            int k = in->npath ? own_index(m, ah_sym(vm, in->path[0])) : -1;
            if (k >= 0) { m->msym[k] = m->msym[m->mn - 1]; m->mval[k] = m->mval[--m->mn]; }
            break;
        }

        case AH_THROW: {
            char buf[64];
            return ah_error(vm, "script throw: %s", ah_to_text(deref(vm, pop(vm)), buf, sizeof buf));
        }
        case AH_TRY_CATCH:
            warn(vm, "try/catch not implemented; errors end the call");
            break;
        case AH_REQUIRE: pop(vm); warn(vm, "require not implemented"); break;
        case AH_OBJECT_SELECTOR: pop(vm); pop(vm); push(vm, ah_nil()); warn(vm, "OBJECT_SELECTOR not implemented"); break;
        case AH_MODULE_CONSTRUCTOR: warn(vm, "MODULE_CONSTRUCTOR not implemented"); break;
        default:
            return ah_error(vm, "opcode %s not implemented", ah_op_name(in->op));
        }
    }
}

/* ---------------------------------------------------------------- VM and GC */

/* Array(n): a new array of n nils. */
static int nat_array(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self; (void)d;
    ah_obj *a = ah_array_new(vm);
    int n = argc > 0 ? (int)ah_num(argv[0]) : 0;
    for (int i = 0; i < n && i < 1000000; i++) ah_array_push(vm, a, ah_nil());
    *ret = ah_objv(a);
    return 0;
}

ah_vm *ah_vm_new(void)
{
    ah_vm *vm = calloc(1, sizeof *vm);
    if (!vm) return NULL;
    vm->root = ah_module_new(vm, "", NULL, NULL, NULL);
    vm->gc_at = 20000;
    ah_set_member(vm, vm->root, "Array", ah_native(vm, "Array", nat_array, NULL));
    return vm;
}

ah_obj *ah_root(ah_vm *vm) { return vm->root; }

void ah_pin(ah_vm *vm, ah_val v)
{
    if (v.t != AV_OBJ) return;
    if (vm->npins == vm->pincap) {
        vm->pincap = vm->pincap ? vm->pincap * 2 : 16;
        vm->pins = realloc(vm->pins, (size_t)vm->pincap * sizeof *vm->pins);
        if (!vm->pins) abort();
    }
    vm->pins[vm->npins++] = v;
}

void ah_unpin(ah_vm *vm, ah_val v)
{
    for (int i = 0; i < vm->npins; i++)
        if (vm->pins[i].t == AV_OBJ && vm->pins[i].u.o == v.u.o) { vm->pins[i] = vm->pins[--vm->npins]; return; }
}

typedef struct { ah_obj **o; int n, cap; } gray;
static gray *g_gray;          /* set during a collection */

static void mark_obj(ah_obj *o)
{
    if (!o || o->mark) return;
    o->mark = 1;
    if (g_gray->n == g_gray->cap) {
        g_gray->cap = g_gray->cap ? g_gray->cap * 2 : 1024;
        g_gray->o = realloc(g_gray->o, (size_t)g_gray->cap * sizeof *g_gray->o);
        if (!g_gray->o) abort();
    }
    g_gray->o[g_gray->n++] = o;
}

void ah_mark(ah_vm *vm, ah_val v)
{
    (void)vm;
    if (v.t == AV_OBJ) mark_obj(v.u.o);
    else if (v.t == AV_REF_MEMBER || v.t == AV_REF_ELEM) mark_obj(v.c);
}

void ah_gc(ah_vm *vm)
{
    if (vm->depth || vm->nobjs < vm->gc_at) return;
    gray gs = { 0 };
    g_gray = &gs;
    mark_obj(vm->root);
    for (int i = 0; i < vm->npins; i++) ah_mark(vm, vm->pins[i]);
    for (int i = 0; i < vm->sp; i++) ah_mark(vm, vm->stack[i]);
    for (int i = 0; i < vm->modsp; i++) mark_obj(vm->mods[i]);
    while (gs.n) {
        ah_obj *o = gs.o[--gs.n];
        switch (o->kind) {
        case AO_ARRAY: for (int i = 0; i < o->n; i++) ah_mark(vm, o->v[i]); break;
        case AO_MAP: for (int i = 0; i < o->n; i++) { ah_mark(vm, o->k[i]); ah_mark(vm, o->v[i]); } break;
        case AO_MODULE:
            mark_obj(o->parent);
            mark_obj(o->klass);
            for (int i = 0; i < o->mn; i++) ah_mark(vm, o->mval[i]);
            if (o->host && o->host->mark) o->host->mark(vm, o);
            break;
        case AO_FUNC: mark_obj(o->module); break;
        case AO_BOUND: mark_obj(o->self); ah_mark(vm, o->fn); break;
        default: break;
        }
    }
    free(gs.o);
    g_gray = NULL;
    size_t live = 0;
    for (ah_obj **p = &vm->heap; *p;) {
        ah_obj *o = *p;
        if (o->mark) { o->mark = 0; live++; p = &o->gc_next; continue; }
        *p = o->gc_next;
        if (o->kind == AO_MODULE && o->host && o->host->finalize) o->host->finalize(o);
        free(o->s); free(o->v); free(o->k); free(o->msym); free(o->mval); free((void *)o->attrs);
        free(o);
    }
    vm->nobjs = live;
    vm->gc_at = live * 2 > 20000 ? live * 2 : 20000;
}

void ah_vm_free(ah_vm *vm)
{
    if (!vm) return;
    while (vm->heap) {
        ah_obj *o = vm->heap;
        vm->heap = o->gc_next;
        if (o->kind == AO_MODULE && o->host && o->host->finalize) o->host->finalize(o);
        free(o->s); free(o->v); free(o->k); free(o->msym); free(o->mval); free((void *)o->attrs);
        free(o);
    }
    for (int i = 0; i < vm->ncodes; i++) ah_free(vm->codes[i]);
    free(vm->codes);
    free(vm->stack);
    free(vm->mods);
    free(vm->pins);
    symtab_free(&vm->syms);
    symtab_free(&vm->seen);
    free(vm);
}

int ah_object_kind(const ah_obj *o){return o?o->kind:-1;}
int ah_object_count(const ah_obj *o){return !o?0:o->kind==AO_MODULE?o->mn:(o->kind==AO_ARRAY || o->kind==AO_MAP)?o->n:0;}
const char *ah_object_member_name(const ah_obj *o,int i){return o && o->kind==AO_MODULE && i>=0 && i<o->mn?o->msym[i]:NULL;}
ah_val ah_object_value(const ah_obj *o,int i){if(!o || i<0 || i>=ah_object_count(o))return ah_nil();return o->kind==AO_MODULE?o->mval[i]:o->v[i];}

void ah_remove_member(ah_vm *vm,ah_obj *o,const char *name){if(!o || o->kind!=AO_MODULE)return;int i=own_index(o,ah_sym(vm,name));if(i<0)return;memmove(o->msym+i,o->msym+i+1,(o->mn-i-1)*sizeof *o->msym);memmove(o->mval+i,o->mval+i+1,(o->mn-i-1)*sizeof *o->mval);o->mn--;}
ah_obj *ah_map_new(ah_vm *vm){return alloc(vm,AO_MAP);}
int ah_map_set(ah_vm *vm,ah_obj *o,ah_val key,ah_val value){(void)vm;if(!o || o->kind!=AO_MAP || map_find(o,key)>=0)return 0;grow(o,o->n+1);o->k[o->n]=key;o->v[o->n++]=value;return 1;}
ah_val ah_map_key(const ah_obj *o,int i){return o && o->kind==AO_MAP && i>=0 && i<o->n?o->k[i]:ah_nil();}

void ah_set_progress(ah_vm *vm,void (*fn)(void *),void *user)
{vm->progress=fn;vm->progress_user=user;vm->progress_ticks=0;}
