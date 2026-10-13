/*
 * Native Adhoc interpreter: runs GT4's compiled menu scripts (ADCH005) the
 * way the game engine does, with engine objects (widgets, main::game, sound,
 * ...) supplied by the port through host hooks and native functions.
 *
 * Semantics follow the instruction descriptions of the GT Modding Hub and
 * GTAdhocToolchain; what was inferred from GT4's own code is marked in
 * docs/ADHOC.md.
 */
#ifndef OPENGT4_ADHOC_VM_H
#define OPENGT4_ADHOC_VM_H

#include "adhoc_code.h"

#include <stdint.h>

typedef struct ah_vm  ah_vm;
typedef struct ah_obj ah_obj;

typedef enum {
    AV_NIL, AV_VOID, AV_BOOL, AV_INT, AV_LONG, AV_FLOAT, AV_DOUBLE, AV_OBJ,
    AV_REF_SLOT, AV_REF_MEMBER, AV_REF_ELEM            /* only on the VM stack */
} ah_type;

typedef struct ah_val ah_val;
struct ah_val {
    uint8_t t;
    uint8_t kt;            /* AV_REF_ELEM: type of the key held in u */
    union {
        int64_t i;
        double  f;
        ah_obj *o;
        ah_val *slot;
    } u;
    ah_obj     *c;         /* AV_REF_MEMBER / AV_REF_ELEM: container */
    const char *sym;       /* AV_REF_MEMBER: interned member name */
};

typedef enum { AO_STRING, AO_ARRAY, AO_MAP, AO_MODULE, AO_FUNC, AO_BOUND, AO_NATIVE } ah_okind;
typedef enum { AM_MODULE, AM_CLASS, AM_INSTANCE } ah_mkind;

/* Native function. self is the bound object (or NULL). Returns 0, or -1
   after ah_error() to raise a script error. */
typedef int (*ah_native_fn)(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret);

/* Hooks for engine objects (widgets, actors, main::game...). get/set return
   1 when they handled the member, 0 to fall back to script members. */
typedef struct {
    const char *type_name;
    int  (*get)(ah_vm *vm, ah_obj *self, const char *sym, ah_val *out);
    int  (*set)(ah_vm *vm, ah_obj *self, const char *sym, ah_val v);
    void (*mark)(ah_vm *vm, ah_obj *self);       /* values kept in hdata */
    void (*finalize)(ah_obj *self);
    unsigned persist_members; /* Save only owned data; never host pointers/state. */
    /* A script's read of the member, before its own members: an engine
       attribute shown differently from the value the engine keeps
       (main::game.money, a string, over the numeric balance). */
    int  (*read)(ah_vm *vm, ah_obj *self, const char *sym, ah_val *out);
} ah_host;

/* ---- VM ---- */
ah_vm  *ah_vm_new(void);
void    ah_vm_free(ah_vm *vm);
ah_obj *ah_root(ah_vm *vm);
void    ah_set_user(ah_vm *vm, void *user);             /* engine context for hooks */
void   *ah_user(const ah_vm *vm);
void    ah_set_log(ah_vm *vm, void (*log)(void *user, const char *msg), void *user);
void    ah_logf(ah_vm *vm, const char *fmt, ...);
int     ah_error(ah_vm *vm, const char *fmt, ...);     /* always returns -1 */

/* Runs a script's top-level code in the root module; the VM keeps the code. */
int ah_run_script(ah_vm *vm, ah_code *code);
int ah_run_module(ah_vm *vm,ah_code *code,ah_obj *module);
/* Calls a function value (function, bound method, native, class). */
int ah_call(ah_vm *vm, ah_val fn, int argc, const ah_val *argv, ah_val *ret);
/* obj.name(args...) if the member exists and is callable; 1 = called, 0 = absent. */
int ah_call_member(ah_vm *vm, ah_obj *obj, const char *name, int argc, const ah_val *argv, ah_val *ret);

/* Mark-and-sweep; only between top-level calls. Pinned values are roots. */
void ah_gc(ah_vm *vm);
void ah_pin(ah_vm *vm, ah_val v);
void ah_unpin(ah_vm *vm, ah_val v);
void ah_mark(ah_vm *vm, ah_val v);

/* Give a nested platform event frame its own bounded execution budget,
   restoring the suspended script's budget afterwards. GC still sees the
   active call depth and cannot collect its stack-local frames. */
/* Presentation pump only: must not re-enter the VM. */
void ah_set_progress(ah_vm *vm,void (*fn)(void *),void *user);
unsigned ah_exchange_instruction_budget(ah_vm *vm,unsigned budget);

/* ---- values ---- */
const char *ah_sym(ah_vm *vm, const char *s);        /* interned */
ah_val ah_nil(void);
ah_val ah_int(int64_t i);
ah_val ah_bool(int b);
ah_val ah_float(double f);
ah_val ah_objv(ah_obj *o);
ah_val ah_str(ah_vm *vm, const char *s);
ah_val ah_native(ah_vm *vm, const char *name, ah_native_fn fn, void *data);
/* Opt-in output-reference arguments for native script adapters. */
ah_val ah_native_refs(ah_vm *vm,const char *name,ah_native_fn fn,void *data,uint32_t reference_mask);
int ah_reference_set(ah_vm *vm,ah_val reference,ah_val value);
ah_val ah_bind(ah_vm *vm, ah_obj *self, ah_val fn);
ah_obj *ah_array_new(ah_vm *vm);
void    ah_array_push(ah_vm *vm, ah_obj *a, ah_val v);
int     ah_array_size(const ah_obj *a);
ah_val  ah_array_get(const ah_obj *a, int i);
int     ah_array_set(ah_obj *a,int i,ah_val v);

ah_obj *ah_native_class(ah_vm *vm,const char *name,ah_native_fn initialize,void *data);
ah_obj *ah_module_new(ah_vm *vm, const char *name, ah_obj *parent, const ah_host *host, void *hdata);
/* Attach hooks to an existing restored module; refuses a different host. */
int     ah_module_attach_host(ah_obj *o,const ah_host *host);
void   *ah_host_data(const ah_obj *o);
const ah_host *ah_host_of(const ah_obj *o);
const char *ah_module_name(const ah_obj *o);
ah_obj *ah_module_parent(const ah_obj *o);
void    ah_set_member(ah_vm *vm, ah_obj *o, const char *name, ah_val v);
int     ah_get_member(ah_vm *vm, ah_obj *o, const char *name, ah_val *out);
int     ah_get_own_member(ah_vm *vm, ah_obj *o, const char *name, ah_val *out);   /* not inherited */
/* Copies script members (not host state) from one module to another. */
void    ah_copy_members(ah_vm *vm, ah_obj *dst, const ah_obj *src);
/* Read-only owned data iteration for native save snapshots. */
void ah_remove_member(ah_vm *vm,ah_obj *o,const char *name);
ah_obj *ah_map_new(ah_vm *vm);
int ah_map_set(ah_vm *vm,ah_obj *o,ah_val key,ah_val value);
ah_val ah_map_key(const ah_obj *o,int index);
int ah_object_kind(const ah_obj *o);
int ah_object_count(const ah_obj *o);
const char *ah_object_member_name(const ah_obj *o,int index);
ah_val ah_object_value(const ah_obj *o,int index);

int         ah_truthy(ah_val v);
double      ah_num(ah_val v);                          /* 0 for non-numbers */
int         ah_is_num(ah_val v);
const char *ah_cstr(ah_val v);                         /* string contents or NULL */
ah_obj     *ah_as_obj(ah_val v, ah_okind kind);         /* NULL if not that kind */
/* Converts any value to text (numbers, nil...) into buf. */
const char *ah_to_text(ah_val v, char *buf, size_t len);

#endif /* OPENGT4_ADHOC_VM_H */
