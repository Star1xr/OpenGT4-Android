#include "adhoc_code.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Operand layout per opcode (version 5):
   i = int32, I = uint32, q = int64, Q = uint64, b = byte, f = float, d = double,
   s = symbol (int16 length + bytes), S = symbol list (uint32 count + symbols),
   v = int32 into i2 (variable slot / second int), F = nested code frame. */
static const char *const layout[AH_OPCODE_COUNT] = {
    "i", "", "s", "s", "s", "s", "i", "sS", "", "f", "sF", "Ss", "i", "i", "i", "i", "i",
    "s", "i", "i", "sF", "S", "", "", "", "i", "", "b", "s", "s", "i", "", "i", "s", "s",
    "S", "Sv", "S", "Sv", "s", "F", "F", "i", "q", "", "i", "i", "", "s", "iv", "I", "",
    "", "", "", "b", "", "", "I", "Q", "d", "", "", "i", "i", "b", "" };

static const char *const names[AH_OPCODE_COUNT] = {
    "ARRAY_CONST_OLD", "ASSIGN_OLD", "ATTRIBUTE_DEFINE", "ATTRIBUTE_PUSH", "BINARY_ASSIGN_OPERATOR",
    "BINARY_OPERATOR", "CALL", "CLASS_DEFINE", "EVAL", "FLOAT_CONST", "FUNCTION_DEFINE", "IMPORT",
    "INT_CONST", "JUMP", "JUMP_IF_TRUE", "JUMP_IF_FALSE", "LIST_ASSIGN_OLD", "LOCAL_DEFINE",
    "LOGICAL_AND_OLD", "LOGICAL_OR_OLD", "METHOD_DEFINE", "MODULE_DEFINE", "NIL_CONST", "NOP",
    "POP_OLD", "PRINT", "REQUIRE", "SET_STATE_OLD", "STATIC_DEFINE", "STRING_CONST", "STRING_PUSH",
    "THROW", "TRY_CATCH", "UNARY_ASSIGN_OPERATOR", "UNARY_OPERATOR", "UNDEF", "VARIABLE_PUSH",
    "ATTRIBUTE_EVAL", "VARIABLE_EVAL", "SOURCE_FILE", "FUNCTION_CONST", "METHOD_CONST",
    "MAP_CONST_OLD", "LONG_CONST", "ASSIGN", "LIST_ASSIGN", "CALL_OLD", "OBJECT_SELECTOR",
    "SYMBOL_CONST", "LEAVE", "ARRAY_CONST", "ARRAY_PUSH", "MAP_CONST", "MAP_INSERT", "POP",
    "SET_STATE", "VOID_CONST", "ASSIGN_POP", "U_INT_CONST", "U_LONG_CONST", "DOUBLE_CONST",
    "ELEMENT_PUSH", "ELEMENT_EVAL", "LOGICAL_AND", "LOGICAL_OR", "BOOL_CONST", "MODULE_CONSTRUCTOR" };

const char *ah_op_name(int op) { return op >= 0 && op < AH_OPCODE_COUNT ? names[op] : "?"; }

typedef struct {
    const uint8_t *d;
    size_t         pos, size;
    int            bad;
    int            depth;
} reader;

static int need(reader *r, size_t n)
{
    if (r->bad || n > r->size - r->pos) { r->bad = 1; return 0; }
    return 1;
}

static uint32_t u32(reader *r)
{
    if (!need(r, 4)) return 0;
    const uint8_t *p = r->d + r->pos;
    r->pos += 4;
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t u64(reader *r) { uint64_t lo = u32(r); return lo | (uint64_t)u32(r) << 32; }

static uint8_t u8(reader *r)
{
    if (!need(r, 1)) return 0;
    return r->d[r->pos++];
}

/* Symbols are EUC-JP in version 5; kept as raw bytes (ASCII in practice). */
static char *sym(reader *r)
{
    if (!need(r, 2)) return NULL;
    int16_t n = (int16_t)(r->d[r->pos] | r->d[r->pos + 1] << 8);
    r->pos += 2;
    if (n < 0 || !need(r, (size_t)n)) { r->bad = 1; return NULL; }
    char *s = malloc((size_t)n + 1);
    if (!s) { r->bad = 1; return NULL; }
    memcpy(s, r->d + r->pos, (size_t)n);
    s[n] = 0;
    r->pos += (size_t)n;
    return s;
}

static char **syms(reader *r, int *count)
{
    uint32_t n = u32(r);
    if (r->bad || n > 4096) { r->bad = 1; *count = 0; return NULL; }
    char **list = calloc(n ? n : 1, sizeof *list);
    if (!list) { r->bad = 1; return NULL; }
    for (uint32_t i = 0; i < n; i++) list[i] = sym(r);
    *count = (int)n;
    return list;
}

static ah_code *frame(reader *r);

static void operands(reader *r, ah_ins *in)
{
    for (const char *f = layout[in->op]; *f && !r->bad; f++) {
        switch (*f) {
        case 'i': in->i = (int32_t)u32(r); break;
        case 'I': in->i = u32(r); break;
        case 'q': in->i = (int64_t)u64(r); break;
        case 'Q': in->i = (int64_t)u64(r); break;
        case 'b': in->i = u8(r); break;
        case 'v': in->i2 = (int32_t)u32(r); break;
        case 'f': { uint32_t b = u32(r); float v; memcpy(&v, &b, 4); in->f = v; break; }
        case 'd': { uint64_t b = u64(r); double v; memcpy(&v, &b, 8); in->f = v; break; }
        case 's': in->s = sym(r); break;
        case 'S': in->path = syms(r, &in->npath); break;
        case 'F': in->frame = frame(r); if (!in->frame) r->bad = 1; break;
        }
    }
}

static ah_code *frame(reader *r)
{
    if (++r->depth > 64) { r->bad = 1; return NULL; }
    ah_code *c = calloc(1, sizeof *c);
    if (!c) { r->bad = 1; return NULL; }
    c->source = sym(r);
    c->params = syms(r, &c->nparams);
    c->stack_size = (int32_t)u32(r);
    c->local_count = (int32_t)u32(r);
    uint32_t n = u32(r);
    if (r->bad || n > (r->size - r->pos) / 5) { r->bad = 1; ah_free(c); r->depth--; return NULL; }
    c->ins = calloc(n ? n : 1, sizeof *c->ins);
    if (!c->ins) { r->bad = 1; ah_free(c); r->depth--; return NULL; }
    for (uint32_t k = 0; k < n && !r->bad; k++) {
        ah_ins *in = &c->ins[k];
        in->line = u32(r);
        in->op = u8(r);
        c->count = (int)k + 1;
        if (in->op >= AH_OPCODE_COUNT) { r->bad = 1; break; }
        operands(r, in);
    }
    r->depth--;
    if (r->bad) { ah_free(c); return NULL; }
    c->slots = c->local_count > c->nparams ? c->local_count : c->nparams;
    for (int k = 0; k < c->count; k++) {
        const ah_ins *in = &c->ins[k];
        if ((in->op == AH_VARIABLE_PUSH || in->op == AH_VARIABLE_EVAL) && in->npath == 1
            && in->i2 >= c->slots && in->i2 < 65536)
            c->slots = in->i2 + 1;
    }
    return c;
}

ah_code *ah_load(const uint8_t *data, size_t size, char *err, size_t err_len)
{
    const uint8_t *nul = memchr(data, 0, size < 16 ? size : 16);
    if (!nul || memcmp(data, "ADCH", 4) || atoi((const char *)data + 4) != 5) {
        snprintf(err, err_len, "not an ADCH005 script");
        return NULL;
    }
    reader r = { data, (size_t)(nul - data) + 1, size, 0, 0 };
    ah_code *top = frame(&r);
    if (!top)
        snprintf(err, err_len, "broken script near offset %#zx", r.pos);
    return top;
}

void ah_free(ah_code *c)
{
    if (!c) return;
    for (int k = 0; k < c->count; k++) {
        ah_ins *in = &c->ins[k];
        free(in->s);
        for (int j = 0; j < in->npath; j++) free(in->path[j]);
        free(in->path);
        ah_free(in->frame);
    }
    for (int j = 0; j < c->nparams; j++) free(c->params[j]);
    free(c->params);
    free(c->ins);
    free(c->source);
    free(c);
}
