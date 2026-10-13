#include "mproject.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *p, *end;
    int         line;
    char       *err;
    size_t      err_len;
    int         failed;
} lexer;

static void fail(lexer *L, const char *what)
{
    if (!L->failed && L->err)
        snprintf(L->err, L->err_len, "line %d: %s", L->line, what);
    L->failed = 1;
}

static void skip_space(lexer *L)
{
    while (L->p < L->end && isspace((unsigned char)*L->p)) {
        if (*L->p == '\n') L->line++;
        L->p++;
    }
}

static char *dup_range(const char *a, const char *b)
{
    char *s = malloc((size_t)(b - a) + 1);
    if (s) { memcpy(s, a, (size_t)(b - a)); s[b - a] = 0; }
    return s;
}

static int is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int is_ident(char c) { return isalnum((unsigned char)c) || c == '_'; }

static char *read_ident(lexer *L)
{
    const char *a = L->p;
    while (L->p < L->end && is_ident(*L->p)) L->p++;
    return dup_range(a, L->p);
}

static mp_node *new_node(mp_kind k)
{
    mp_node *n = calloc(1, sizeof *n);
    if (n) n->kind = k;
    return n;
}

static int push(mp_node *parent, mp_node *child)
{
    mp_node **it = realloc(parent->items, (size_t)(parent->count + 1) * sizeof *it);
    if (!it) return 0;
    parent->items = it;
    parent->items[parent->count++] = child;
    return 1;
}

static int peek_is(lexer *L, char c) { skip_space(L); return L->p < L->end && *L->p == c; }

static void parse_items(lexer *L, mp_node *into, int depth);

/* After '{' has been consumed: items until the matching '}'. */
static void parse_block(lexer *L, mp_node *into, int depth)
{
    parse_items(L, into, depth + 1);
    if (!L->failed) {
        if (peek_is(L, '}')) L->p++;
        else fail(L, "expected '}'");
    }
}

static void parse_items(lexer *L, mp_node *into, int depth)
{
    if (depth > 64) { fail(L, "nesting too deep"); return; }
    for (;;) {
        skip_space(L);
        if (L->failed || L->p >= L->end || *L->p == '}') return;
        char c = *L->p;
        mp_node *n = NULL;
        if (c == '"') {
            const char *a = ++L->p;
            while (L->p < L->end && *L->p != '"') { if (*L->p == '\n') L->line++; L->p++; }
            if (L->p >= L->end) { fail(L, "unterminated string"); return; }
            n = new_node(MP_STRING);
            if (n) n->string = dup_range(a, L->p);
            L->p++;
        } else if (c == '-' || c == '+' || c == '.' || isdigit((unsigned char)c)) {
            char *e;
            double v = strtod(L->p, &e);
            if (e == L->p) { fail(L, "bad number"); return; }
            L->p = e;
            n = new_node(MP_NUMBER);
            if (n) n->number = v;
        } else if (is_ident_start(c)) {
            char *id = read_ident(L);
            skip_space(L);
            if (L->p < L->end && *L->p == '{') {                 /* Type { ... } */
                L->p++;
                n = new_node(MP_OBJECT);
                if (n) { n->name = id; id = NULL; parse_block(L, n, depth); }
            } else if (L->p < L->end && *L->p == '[') {          /* key [n] { ... } */
                while (L->p < L->end && *L->p != ']') L->p++;
                if (L->p < L->end) L->p++;
                if (!peek_is(L, '{')) { free(id); fail(L, "expected '{' after list size"); return; }
                L->p++;
                n = new_node(MP_LIST);
                if (n) { n->name = id; id = NULL; parse_block(L, n, depth); }
            } else if (L->p < L->end && is_ident_start(*L->p)) { /* key type { ... } */
                char *type = read_ident(L);
                if (!peek_is(L, '{')) { free(id); free(type); fail(L, "expected '{' after property type"); return; }
                L->p++;
                n = new_node(MP_PROPERTY);
                if (n) { n->name = id; n->type = type; id = NULL; type = NULL; parse_block(L, n, depth); }
                free(type);
            } else {
                free(id);
                fail(L, "unexpected token after name");
                return;
            }
            free(id);
        } else {
            fail(L, "unexpected character");
            return;
        }
        if (!n || !push(into, n)) { mp_free(n); fail(L, "out of memory"); return; }
    }
}

mp_node *mp_parse(const char *text, size_t len, char *err, size_t err_len)
{
    lexer L = { text, text + len, 1, err, err_len, 0 };
    mp_node *top = new_node(MP_LIST);
    if (!top) return NULL;
    parse_items(&L, top, 0);
    skip_space(&L);
    if (!L.failed && L.p < L.end) fail(&L, "unexpected '}'");
    if (L.failed || top->count != 1 || top->items[0]->kind != MP_OBJECT) {
        if (!L.failed) fail(&L, "expected one top-level object");
        mp_free(top);
        return NULL;
    }
    mp_node *root = top->items[0];
    free(top->items);
    free(top);
    return root;
}

void mp_free(mp_node *n)
{
    if (!n) return;
    for (int i = 0; i < n->count; i++) mp_free(n->items[i]);
    free(n->items);
    free(n->name);
    free(n->type);
    free(n->string);
    free(n);
}

const mp_node *mp_prop(const mp_node *obj, const char *key)
{
    if (!obj) return NULL;
    for (int i = 0; i < obj->count; i++) {
        const mp_node *c = obj->items[i];
        if ((c->kind == MP_PROPERTY || c->kind == MP_LIST) && !strcmp(c->name, key))
            return c;
    }
    return NULL;
}

const char *mp_string(const mp_node *obj, const char *key)
{
    const mp_node *p = mp_prop(obj, key);
    for (int i = 0; p && i < p->count; i++)
        if (p->items[i]->kind == MP_STRING) return p->items[i]->string;
    return NULL;
}

/* Numbers of a property, or of the first object inside a list/property
   (e.g. text_color RGBA {1 2 3 4} is stored as a property of numbers). */
int mp_numbers(const mp_node *obj, const char *key, double *out, int max)
{
    const mp_node *p = mp_prop(obj, key);
    int k = 0;
    for (int i = 0; p && i < p->count && k < max; i++) {
        const mp_node *value=p->items[i];
        if (value->kind == MP_NUMBER) out[k++] = value->number;
        /* Authored lists use typed values: range [2] { digit {-5} digit {5} }. */
        else if(value->kind==MP_OBJECT)
            for(int j=0;j<value->count && k<max;j++)
                if(value->items[j]->kind==MP_NUMBER)out[k++]=value->items[j]->number;
    }
    return k;
}

double mp_number(const mp_node *obj, const char *key, double fallback)
{
    double v;
    return mp_numbers(obj, key, &v, 1) == 1 ? v : fallback;
}

int mp_children(const mp_node *obj, const mp_node *const **out)
{
    const mp_node *p = mp_prop(obj, "children");
    if (!p || p->kind != MP_LIST) { *out = NULL; return 0; }
    *out = (const mp_node *const *)p->items;
    return p->count;
}

const mp_node *mp_find_named(const mp_node *root, const char *type, const char *name)
{
    if (!root) return NULL;
    if (root->kind == MP_OBJECT && (!type || !strcmp(root->name, type))) {
        const char *n = mp_string(root, "name");
        if (n && !strcmp(n, name)) return root;
    }
    for (int i = 0; i < root->count; i++) {
        const mp_node *f = mp_find_named(root->items[i], type, name);
        if (f) return f;
    }
    return NULL;
}
