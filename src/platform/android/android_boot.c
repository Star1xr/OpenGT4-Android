/*
 * Phase 1 boot binary (temporary).
 *
 * Purpose: prove on real hardware that SDL2 + EGL/GLES 3.2 + the SAF disc path
 * work end to end. Phase 3 replaces it with the real game: this file is deleted
 * and the same target is built from src/app/main.c plus the shared GLES render
 * path.
 *
 * SDL renames main() to SDL_main (SDL_main.h); SDLActivity calls that symbol
 * inside libopengt4.so and finishes the Activity when it returns.
 */
#include "platform/platform.h"
#include "android_platform.h"

#include <SDL.h>
#include <stdlib.h>
#include <string.h>

/* v2.00 NTSC-U (SCUS-97328): config/supported_discs.json */
#define GT4_V200_BYTES 5314478080ULL

#define BOOT_PROBE_MS 3000

static void report_disc(const char *path)
{
    if (!path || !*path) {
        ALOGW("no disc path given: nothing was selected in the launcher");
        return;
    }
    plat_file *file = plat_file_open(path);
    if (!file) {
        ALOGE("cannot open the disc: %s", path);
        return;
    }
    uint64_t size = plat_file_size(file);
    uint8_t head[16];
    size_t got = plat_file_read_at(file, 0, head, sizeof head);
    plat_file_close(file);

    ALOGI("disc: %s (%llu bytes, first read %zu bytes)", path, (unsigned long long)size, got);
    if (size == GT4_V200_BYTES)
        ALOGI("disc size matches v2.00");
    else
        ALOGW("disc size differs from v2.00: %llu != %llu",
              (unsigned long long)size, (unsigned long long)GT4_V200_BYTES);
}

int main(int argc, char **argv)
{
    ALOGI("%s", plat_android_banner());
    for (int i = 0; i < argc; i++) ALOGI("argv[%d] = %s", i, argv[i]);

    const char *disc = argc > 1 ? argv[1] : NULL;
    for (int i = 2; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--save-dir")) ALOGI("save directory: %s", argv[i + 1]);

    report_disc(disc);

    plat_video_desc desc = { 640, 448, 1, 1, 0 }; /* upscale 0: no FSR on Android */
    if (plat_video_init(&desc) != 0) {
        ALOGE("video init failed");
        return 1;
    }
    ALOGI("gl info: %s", plat_android_gl_info());
    ALOGI("ES 3.2 context: %s", plat_android_es32() ? "yes" : "no (ES 3.0 fallback)");
    plat_video_set_view(640.0f, 448.0f);

    /* Three seconds of measurement: is the swap chain really running? */
    uint64_t start = plat_time_ns();
    int frames = 0;
    while (plat_time_ns() - start < (uint64_t)BOOT_PROBE_MS * 1000000ULL && !plat_should_quit()) {
        plat_video_begin_frame();
        plat_video_end_frame();
        plat_sleep_ms(16);
        frames++;
    }
    double seconds = (double)(plat_time_ns() - start) / 1e9;
    ALOGI("measured: %d frames / %.2f s (%.1f fps)", frames, seconds, seconds > 0 ? frames / seconds : 0.0);
    ALOGI("phase 1 verified: SDL2 + GLES + SAF disc path are working");

    plat_video_shutdown();
    ALOGW("phase 1 boot binary: the real game is not wired up yet (phase 3); closing");
    return 0;
}
