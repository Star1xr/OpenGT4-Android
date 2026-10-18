# OpenGT4 — Android (ARM64)

Bu dizin, OpenGT4'nin **Android portunu** anlatır. Port resmi OpenGT4 değildir:
GPLv3 §7 kapsamında **değiştirilmiş sürüm** olarak işaretlenir ve her yerde
"**Based on OpenGT4**" atfı korunur (bkz. [Lisans](#lisans)).

- **Hedef:** arm64-v8a, `minSdk 26`, OpenGL ES 3.2 (yoksa ES 3.0 fallback + uyarı)
- **Disc:** kullanıcının kendi Gran Turismo 4 disc'i; **yerinde okunur**, kopyalanmaz/açılmaz
- **Yasak:** FSR/upscale (saf port), oyun verisi (ISO/dump/BIOS/asset/save) repoda bulunmaz

---

## 1. Durum

| Faz | İş | Durum |
|---|---|---|
| 0 | Depo/doküman/lisans temizliği, senkron script'i | ✅ |
| 1 | Gradle + Compose iskeleti, JNI köprüsü, GLES 3.2 boot doğrulaması | ✅ |
| 2 | Giriş/çıkış: SAF ile ISO (fd sondası hazır), save dizini, config/INI köprüsü | 🚧 |
| 3 | GLES 3.2 render yolu (`render3d`, `showroom`, `race_view`, shader prologu) | ⏳ |
| 4 | Dokunmatik PS2 kontrolleri (GLES overlay → `plat_pad_state`) | ⏳ |
| 5 | Launcher olgunlaştırma (video/denetim ayarları, doğrulama, ön bellek) | ⏳ |
| 6 | Gömülü FFmpeg (LGPL, yalnız MPEG-2) ile `.pss` filmler | ⏳ |
| 7 | Menülerde doğrudan dokunma (hit-test), native mobil his | ⏳ |
| 8 | Release: imzalama, AAB/APK, cihaz matrisi | ⏳ |

Windows/Linux yolu **bozulmaz**: `_WIN32` kod (harici ffmpeg yolu, Win32 launcher)
olduğu gibi kalır, Android yalnız kendi dosyalarını ekler.

---

## 2. Kararlar

| Konu | Karar |
|---|---|
| Runtime | **SDL2** (Android desteği hazır; `plat_sdl.c` + `pad_sdl.c` yeniden kullanılır) |
| Grafik | **OpenGL ES 3.2**; shader'lar tek kaynak, hem `#version 330 core` hem `#version 300 es` |
| AMD FSR | **Yok.** `third_party/fsr1`, `fsr.c`, `fsr_source.c` Android build'ine girmez |
| Launcher | **Jetpack Compose + Material 3** (yalnız launcher Compose; oyun SDLSurface'te) |
| ISO | **yerinde**: SAF `openFileDescriptor` → `/proc/self/fd/<n>` yolu oyunun `plat_file_open`'una verilir |
| Filmler | **gömülü FFmpeg libavcodec (LGPL)**, yalnız `mpeg2video`; sistem `ffmpeg` çağrılmaz |
| Dosya API'si | `src/platform/common` zaten tam (`plat_file_*`, `plat_disk_free`, `plat_png_read`, `plat_jpeg_png`) → yeni backend yok |
| D-pad | Oyun içinde dokunmatik pad; **menülerde D-pad yok**, basılan yere basılır |

`/proc/self/fd/<n>` yöntemi: `GameActivity` seçilen `content://` URI'yi
`ParcelFileDescriptor` olarak açar, fd'yi süreç yaşadıkça açık tutar ve oyun
`argv[1]` olarak bu yolu alır. `open()`/`pread()` doğrudan çalışır, **hiçbir kopya
oluşmaz**. Sağlayıcı aranabilir (seekable) bir fd vermezse (bulut sağlayıcıları
tipik olarak pipe döner) launcher açık bir hata verir: "dosyayı cihaz depolamasına
kopyalayın".

---

## 3. Kaynak düzeni

```
android/                          Gradle projesi (Compose launcher + GameActivity + JNI)
  app/src/main/java/org/opengt4/android/
    LauncherActivity.kt           ISO seçimi, ayarlar, Play
    GameActivity.kt               SDLActivity türevi; getLibraries/getArguments
    bridge/NativeBridge.kt        JNI: disc doğrulama, config oku/yaz, GLES bilgisi
    ui/…                          Material 3 ekranlar
  app/src/main/java/org/libsdl/app/
    *.java                        SDL2 2.30.9 Java glue (vendor'lı, zlib lisans)
  app/src/main/cpp/CMakeLists.txt Android native hedefi (SDL2 + core + physics + android backend)
  third_party/SDL-LICENSE.txt     SDL zlib lisansı (vendor'lı Java için)
src/platform/android/             Android backend (platform sınırının arkasında)
  android_platform.h              Android'e özel iç API
  android_jni.c                   JNI giriş noktaları
  android_boot.c                  Faz 1: GLES 3.2 bağlam doğrulama ikilisi (geçici)
  android_video.c                 GLES bağlam/çerçeve yönetimi (Faz 3'te tamamlanır)
  android_touch.c                 dokunmatik → pad durumu (Faz 4)
docs/ANDROID.md                   bu dosya
```

`src/core/**` hiçbir zaman OS/GPU/dosya API'si çağırmaz; yalnız
`include/platform/platform.h`. Android'e özel her şey `src/platform/android/**`.

---

## 4. Build

**Bu depoda/analiz ortamında build çalıştırılmaz.** Build yalnızca CI'da koşar:

| Workflow | Ne yapar | Durum |
|---|---|---|
| `.github/workflows/android-ci.yml` | her push: native derleme + `:app:assembleDebug` + APK artefaktı | ✅ (yalnız özel repoda koşar) |
| `.github/workflows/android-release.yml` | etiket: imzalı APK + AAB, `dist/` artefaktı | ⏳ Faz 8 |
| `.github/workflows/desktop-ci.yml` | Windows/Linux yolu bozulmadı mı (asset'siz testler) | ⏳ Faz 2 |

`android-ci` işi `github.repository == 'Star1xr/opengt4privaterepo'` koşuluyla
korunur: public ayna aynı commit'i taşır ama Actions dakikası harcamaz.

Neden CI'da: NDK/SDK/Gradle indirmesi ve tam build, geliştirme ortamında ~GB'lar
ve uzun süre ister; tekrarlanabilirlik için tek yer CI'dır.

`./gradlew` **yok** (wrapper jar'ı binary'dir ve bu ortamda üretilemez); CI
`gradle/actions/setup-gradle` ile Gradle'ı indirip `gradle :app:assembleDebug`
çalıştırır. Yerelde Gradle kullanacak geliştirici `gradle wrapper` ile üretebilir.

Sabitlenen sürümler (tekrarlanabilirlik): Gradle 8.9, AGP 8.7.2, Kotlin 2.0.20,
Compose BOM 2024.09.02, `compileSdk 35` / `minSdk 26` / `targetSdk 34`,
NDK 27.0.12077973, CMake 3.22.1, SDL 2.30.9 (FetchContent).

Artefaktlar: `android/app/build/outputs/apk/debug/app-debug.apk` (debug),
`dist/opengt4-android-<version>-arm64.apk` (release).

İmzalama: `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`,
`ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD` — **yalnız özel reponun Actions
secret'larında**, repoda asla.

---

## 5. Cihazda çalıştırma

1. `adb install -r app-debug.apk`
2. Launcher'da **Disc seç** → `.iso` dosyasını seç (SAF), **Oynat**.
3. Loglar: `adb logcat -s OpenGT4:V SDL:V AndroidRuntime:E`

Faz 1 doğrulaması: uygulama açılır, GLES sürüm satırını loglar (`OpenGT4: GLES
3.2 …`), ekranı temizleyip çıkar. Bu, SDL2 + EGL + ES 3.2 + JNI + SAF yolunun
uçtan uca çalıştığını kanıtlar.

---

## 6. Lisans

- Kod: **GPLv3** (bkz. `LICENSE`), ek şartlar: `NOTICE` §7.
- Android sürümü **resmi OpenGT4 değildir**; doküman ve uygulama içinde
  "Based on OpenGT4" atfı ve "resmi sürüm değildir" ibaresi korunur.
- Vendor'lı SDL2 Java glue: zlib lisansı (`android/third_party/SDL-LICENSE.txt`).
- Gömülü FFmpeg (Faz 6): **LGPL** build, dinamik bağlanır, kaynağı ve lisansı
  `private-assets` branch'inde dağıtılır; yalnız `mpeg2video` dekoderi derlenir.
- Oyun verisi hiçbir zaman dağıtılmaz; kullanıcı kendi disc'ini kullanır.
