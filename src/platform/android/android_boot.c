/*
 * Faz 1 boot ikilisi (GECICI).
 *
 * Amac: SDL2 + EGL/GLES 3.2 + SAF fd zincirinin uctan uca calistigini cihazda
 * kanitlamak. Faz 3'te yerini gercek oyuna birakir: bu dosya silinir, ayni
 * hedefi src/app/main.c + paylasilan GLES render yolu alir.
 *
 * main() SDL tarafindan SDL_main'e yeniden adlandirilir (SDL_main.h); SDLActivity
 * bu sembolu libopengt4.so icinden cagirir ve main donunce Activity kapanir.
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
        ALOGW("disc yolu bos: launcher'dan iso secilmedi");
        return;
    }
    plat_file *file = plat_file_open(path);
    if (!file) {
        ALOGE("disc acilamadi: %s", path);
        return;
    }
    uint64_t size = plat_file_size(file);
    uint8_t head[16];
    size_t got = plat_file_read_at(file, 0, head, sizeof head);
    plat_file_close(file);

    ALOGI("disc: %s (%llu bayt, ilk okuma %zu bayt)", path, (unsigned long long)size, got);
    if (size == GT4_V200_BYTES)
        ALOGI("disc boyutu v2.00 ile uyumlu");
    else
        ALOGW("disc boyutu v2.00'dan farkli: %llu != %llu",
              (unsigned long long)size, (unsigned long long)GT4_V200_BYTES);
}

int main(int argc, char **argv)
{
    ALOGI("%s", plat_android_banner());
    for (int i = 0; i < argc; i++) ALOGI("argv[%d] = %s", i, argv[i]);

    const char *disc = argc > 1 ? argv[1] : NULL;
    for (int i = 2; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--save-dir")) ALOGI("save dizini: %s", argv[i + 1]);

    report_disc(disc);

    plat_video_desc desc = { 640, 448, 1, 1, 0 }; /* upscale 0: Android'de FSR yok */
    if (plat_video_init(&desc) != 0) {
        ALOGE("video baslatilamadi");
        return 1;
    }
    ALOGI("gl info: %s", plat_android_gl_info());
    ALOGI("ES 3.2 baglami: %s", plat_android_es32() ? "evet" : "hayir (3.0 fallback)");
    plat_video_set_view(640.0f, 448.0f);

    /* 3 saniyelik olcum: swap zinciri ve baglam gercekten akiyor mu? */
    uint64_t start = plat_time_ns();
    int frames = 0;
    while (plat_time_ns() - start < (uint64_t)BOOT_PROBE_MS * 1000000ULL && !plat_should_quit()) {
        plat_video_begin_frame();
        plat_video_end_frame();
        plat_sleep_ms(16);
        frames++;
    }
    double seconds = (double)(plat_time_ns() - start) / 1e9;
    ALOGI("olcum: %d kare / %.2f s (%.1f fps)", frames, seconds, seconds > 0 ? frames / seconds : 0.0);
    ALOGI("Faz 1 dogrulandi: SDL2 + GLES + SAF fd yolu calisiyor");

    plat_video_shutdown();
    ALOGW("Faz 1 boot ikilisi: gercek oyun henuz baglanmadi (Faz 3); uygulama kapaniyor");
    return 0;
}
