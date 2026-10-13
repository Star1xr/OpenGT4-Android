/*
 * GT4 compiled Adhoc scripts (.adc, "ADCH005"): loaded into memory as code
 * frames. The instruction set and operand layouts are described in
 * docs/ADHOC.md (from the GT Modding Hub and GTAdhocToolchain, MIT).
 */
#ifndef OPENGT4_ADHOC_CODE_H
#define OPENGT4_ADHOC_CODE_H

#include <stddef.h>
#include <stdint.h>

enum {
    AH_ARRAY_CONST_OLD = 0, AH_ASSIGN_OLD = 1, AH_ATTRIBUTE_DEFINE = 2, AH_ATTRIBUTE_PUSH = 3,
    AH_BINARY_ASSIGN_OPERATOR = 4, AH_BINARY_OPERATOR = 5, AH_CALL = 6, AH_CLASS_DEFINE = 7,
    AH_EVAL = 8, AH_FLOAT_CONST = 9, AH_FUNCTION_DEFINE = 10, AH_IMPORT = 11, AH_INT_CONST = 12,
    AH_JUMP = 13, AH_JUMP_IF_TRUE = 14, AH_JUMP_IF_FALSE = 15, AH_LIST_ASSIGN_OLD = 16,
    AH_LOCAL_DEFINE = 17, AH_LOGICAL_AND_OLD = 18, AH_LOGICAL_OR_OLD = 19, AH_METHOD_DEFINE = 20,
    AH_MODULE_DEFINE = 21, AH_NIL_CONST = 22, AH_NOP = 23, AH_POP_OLD = 24, AH_PRINT = 25,
    AH_REQUIRE = 26, AH_SET_STATE_OLD = 27, AH_STATIC_DEFINE = 28, AH_STRING_CONST = 29,
    AH_STRING_PUSH = 30, AH_THROW = 31, AH_TRY_CATCH = 32, AH_UNARY_ASSIGN_OPERATOR = 33,
    AH_UNARY_OPERATOR = 34, AH_UNDEF = 35, AH_VARIABLE_PUSH = 36, AH_ATTRIBUTE_EVAL = 37,
    AH_VARIABLE_EVAL = 38, AH_SOURCE_FILE = 39, AH_FUNCTION_CONST = 40, AH_METHOD_CONST = 41,
    AH_MAP_CONST_OLD = 42, AH_LONG_CONST = 43, AH_ASSIGN = 44, AH_LIST_ASSIGN = 45,
    AH_CALL_OLD = 46, AH_OBJECT_SELECTOR = 47, AH_SYMBOL_CONST = 48, AH_LEAVE = 49,
    AH_ARRAY_CONST = 50, AH_ARRAY_PUSH = 51, AH_MAP_CONST = 52, AH_MAP_INSERT = 53, AH_POP = 54,
    AH_SET_STATE = 55, AH_VOID_CONST = 56, AH_ASSIGN_POP = 57, AH_U_INT_CONST = 58,
    AH_U_LONG_CONST = 59, AH_DOUBLE_CONST = 60, AH_ELEMENT_PUSH = 61, AH_ELEMENT_EVAL = 62,
    AH_LOGICAL_AND = 63, AH_LOGICAL_OR = 64, AH_BOOL_CONST = 65, AH_MODULE_CONSTRUCTOR = 66,
    AH_OPCODE_COUNT = 67
};

typedef struct ah_code ah_code;

typedef struct {
    uint32_t line;
    uint8_t  op;
    int64_t  i;         /* integer operand (jump target, count, int/long const, state) */
    int32_t  i2;        /* second integer (variable slot, leave rewind) */
    double   f;         /* float/double const */
    char    *s;         /* symbol operand (name, string const, operator) */
    char   **path;      /* symbol list operand (module path, variable path) */
    int      npath;
    ah_code *frame;     /* nested code frame (function/method definition or const) */
} ah_ins;

struct ah_code {
    char    *source;
    char   **params;
    int      nparams;
    int      stack_size, local_count;
    int      slots;     /* local storage needed: params, locals and every slot index used */
    ah_ins  *ins;
    int      count;
};

/* Parses a whole .adc file. NULL on error with a message in err. */
ah_code *ah_load(const uint8_t *data, size_t size, char *err, size_t err_len);
void     ah_free(ah_code *c);
const char *ah_op_name(int op);

#endif /* OPENGT4_ADHOC_CODE_H */
