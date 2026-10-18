/*
 * Android entry preparation.
 *
 * The Compose launcher already selected the disc and wrote video.ini, and the
 * game receives the PC command line (src/app/main.c): the ISO first, then
 * --save-dir and --config-file. The PC's preparation window (launcher.c, Win32
 * GDI) has no Android counterpart, so an ISO on the command line means "run the
 * game"; anything else is a failure because there is nothing else to show.
 */
#include "launcher.h"

#include <string.h>

int launcher_bootstrap(int *argc, char ***argv, launcher_startup *startup)
{
    (void)startup;
    if (*argc > 1 && strcmp((*argv)[1], "--launcher")) return 1;
    return -1;
}
