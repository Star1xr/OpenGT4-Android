/*
 * gt4adhoc: lists the instructions of a compiled Adhoc script (.adc) read
 * from the disc, for studying menu behaviour. Prints only the listing.
 *
 *   gt4adhoc <disc.iso> <path.adc> [function name filter]
 */
#include "adhoc/adhoc_code.h"
#include "vol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int contains(const ah_code *c, const char *name)
{
    for (int i = 0; c && i < c->count; i++) {
        const ah_ins *x = &c->ins[i];
        if ((x->op == AH_FUNCTION_DEFINE || x->op == AH_METHOD_DEFINE) && x->s && !strcmp(x->s, name)) return 1;
        if (x->frame && contains(x->frame, name)) return 1;
    }
    return 0;
}

static void dump(const ah_code *c, int depth, const char *filter, int show)
{
    for (int i = 0; c && i < c->count; i++) {
        const ah_ins *x = &c->ins[i];
        int defines = (x->op == AH_FUNCTION_DEFINE || x->op == AH_METHOD_DEFINE || x->op == AH_MODULE_DEFINE
                       || x->op == AH_CLASS_DEFINE);
        int here = show || (defines && x->s && filter && !strcmp(x->s, filter)) || !filter;
        if (here) {
            printf("%*s%4d %-22s", depth * 2, "", i, ah_op_name(x->op));
            if (x->op == AH_FLOAT_CONST || x->op == AH_DOUBLE_CONST) printf(" %g", x->f);
            else if (x->op == AH_INT_CONST || x->op == AH_U_INT_CONST || x->op == AH_LONG_CONST || x->op == AH_JUMP
                     || x->op == AH_JUMP_IF_TRUE || x->op == AH_JUMP_IF_FALSE || x->op == AH_CALL || x->op == AH_BOOL_CONST)
                printf(" %lld", (long long)x->i);
            if (x->s) printf(" %s", x->s);
            for (int k = 0; k < x->npath; k++) printf("%s%s", k ? "::" : " ", x->path[k]);
            if (getenv("GT4ADHOC_LINES")) printf("  @L%u", x->line);   /* the source line the VM's errors name */
            if (x->frame && x->frame->nparams && (x->op == AH_FUNCTION_DEFINE || x->op == AH_METHOD_DEFINE)) {
                printf("  (");
                for (int k = 0; k < x->frame->nparams; k++) printf("%s%s", k ? ", " : "", x->frame->params[k]);
                printf(")");
            }
            printf("\n");
        }
        if (x->frame && (here || (filter && contains(x->frame, filter)) || !filter))
            dump(x->frame, depth + 1, filter, here && (show || defines));
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <disc.iso> <path.adc> [function]\n", argv[0]); return 2; }
    const char *err = NULL;
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "cannot open volume: %s\n", err); return 1; }
    size_t size = 0;
    uint8_t *data = gt4_vol_read(v, argv[2], &size);
    char msg[256] = "";
    ah_code *c = data ? ah_load(data, size, msg, sizeof msg) : NULL;
    if (!c) { fprintf(stderr, "cannot load %s: %s\n", argv[2], msg); free(data); gt4_vol_close(v); return 1; }
    dump(c, 0, argc > 3 ? argv[3] : NULL, 0);
    ah_free(c);
    free(data);
    gt4_vol_close(v);
    return 0;
}
