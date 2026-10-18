/*
 * Android entry preparation.
 *
 * The Compose launcher already selected the disc and wrote video.ini, and the
 * game receives the PC command line (src/app/main.c): the ISO first, then
 * --save-dir and --config-file. The PC's preparation window (launcher.c, Win32
 * GDI) has no Android counterpart, so an ISO on the command line means "run the
 * game"; anything else is a failure because there is nothing else to show.
 *
 * Android drops the game's stdout and stderr, and adb is not always at hand, so
 * both are sent to <filesDir>/logs/opengt4.log next to the profiles; the
 * launcher has a Share log button for it. The session before stays as
 * opengt4.log.prev, so a crash does not take the older one with it.
 */
#include "launcher.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define LOG_DIR   "/logs"
#define LOG_NAME  "/opengt4.log"
#define LOG_PREV  ".prev"
#define LOG_MAX_BYTES (1024 * 1024)

/* <save-dir> is <filesDir>/saves; the log lives in <filesDir>/logs. */
static int log_paths(const char *save_dir, char *dir, size_t dir_size,
                     char *path, size_t path_size)
{
    if (!save_dir || !*save_dir) return 0;
    const char *slash = strrchr(save_dir, '/');
    if (!slash || slash == save_dir) return 0;
    size_t n = (size_t)(slash - save_dir);
    if (n + sizeof LOG_DIR > dir_size) return 0;
    memcpy(dir, save_dir, n);
    memcpy(dir + n, LOG_DIR, sizeof LOG_DIR);
    if (strlen(dir) + sizeof LOG_NAME > path_size) return 0;
    snprintf(path, path_size, "%s%s", dir, LOG_NAME);
    return 1;
}

static void rotate_log(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size <= LOG_MAX_BYTES) return;
    char previous[1100];
    if (snprintf(previous, sizeof previous, "%s%s", path, LOG_PREV) >= (int)sizeof previous) return;
    remove(previous);
    rename(path, previous);
}

static void open_session_log(const char *save_dir)
{
    char dir[1024], path[1100];
    if (!log_paths(save_dir, dir, sizeof dir, path, sizeof path)) return;
    mkdir(dir, 0770);   /* the first run makes it; later ones find it */
    rotate_log(path);
    if (!freopen(path, "a", stdout)) return;
    freopen(path, "a", stderr);
    /* Line buffered: a crash keeps every line written before it. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    time_t now = time(NULL);
    struct tm tm;
    char stamp[32] = "?";
    if (localtime_r(&now, &tm)) strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", &tm);
    printf("\n== OpenGT4 session %s ==\n", stamp);
    fflush(stdout);
}

int launcher_bootstrap(int *argc, char ***argv, launcher_startup *startup)
{
    (void)startup;
    const char *save_dir = NULL;
    for (int i = 1; i + 1 < *argc; i++)
        if (!strcmp((*argv)[i], "--save-dir")) save_dir = (*argv)[i + 1];
    open_session_log(save_dir);
    if (*argc > 1 && strcmp((*argv)[1], "--launcher")) return 1;
    return -1;
}
