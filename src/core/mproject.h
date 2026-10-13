/*
 * GT4 UI project files (.mproject): plain text describing each screen as a
 * tree of widgets. Three forms (docs/UI_FORMAT.md):
 *
 *   Type { ... }                  object (a widget, or a value like RGBA)
 *   key type { values }           property, e.g. geometry rectangle {0 0 640 360}
 *   key [n] { ... }               list, e.g. children [8] { ... }
 *
 * Values are numbers or "strings". The parser builds a generic tree; the UI
 * code interprets it.
 */
#ifndef OPENGT4_MPROJECT_H
#define OPENGT4_MPROJECT_H

#include <stddef.h>

typedef enum { MP_OBJECT, MP_PROPERTY, MP_LIST, MP_NUMBER, MP_STRING } mp_kind;

typedef struct mp_node mp_node;
struct mp_node {
    mp_kind   kind;
    char     *name;      /* object type, property/list key; NULL for values */
    char     *type;      /* property value type ("string", "digit", ...) */
    double    number;    /* MP_NUMBER */
    char     *string;    /* MP_STRING */
    mp_node **items;     /* contents of an object, property or list */
    int       count;
};

/* Parses the whole file. Returns the top object (Project) or NULL; on error
   *err points to a message with the line number. Free with mp_free. */
mp_node *mp_parse(const char *text, size_t len, char *err, size_t err_len);
void     mp_free(mp_node *n);

/* Helpers on an object. */
const mp_node *mp_prop(const mp_node *obj, const char *key);         /* property or list */
const char    *mp_string(const mp_node *obj, const char *key);       /* first string value */
int            mp_numbers(const mp_node *obj, const char *key, double *out, int max);
double         mp_number(const mp_node *obj, const char *key, double fallback);
/* Children objects (from the "children" list), or NULL/0. */
int            mp_children(const mp_node *obj, const mp_node *const **out);
/* Finds a RootWindow (or any object) by its name property, depth-first. */
const mp_node *mp_find_named(const mp_node *root, const char *type, const char *name);

#endif /* OPENGT4_MPROJECT_H */
