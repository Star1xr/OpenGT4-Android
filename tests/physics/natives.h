#ifndef OPENGT4_PHYS_NATIVES_H
#define OPENGT4_PHYS_NATIVES_H
#include "ee.h"
/* Rewritten functions to use: NULL none, "all", or addresses ("0x357700,...") */
void phys_natives_select(const char *list);
/* Hooks, memory base, FPU mode and the bridge for this context. */
void phys_natives_install(ee_ctx *c);
void phys_natives_report(void);
/* Calls per original function when allocated (0x520000 / 4 counters from 0x100000). */
extern uint32_t *phys_coverage;
/* Asked first for each function (e.g. the controller read of a driving demo). */
extern ee_fn (*phys_extra_hook)(uint32_t addr);
/* Rows for the native car record (NULL: the tables in the game's memory). */
#include "car.h"
extern const gt4_specdb_src *phys_record_src;
extern const gt4_specdb_src phys_memory_src;   /* the tables in the game's memory */
/* the last functions entered (to stderr) */
void phys_print_recent(void);
#endif
