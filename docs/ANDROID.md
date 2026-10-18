# OpenGT4 — Android (ARM64)

This document describes the Android port of OpenGT4. The port is **not** the
official OpenGT4 release: under GPLv3 §7 it is marked as a modified version and
keeps the **"Based on OpenGT4"** attribution in the docs and in the UI (see
[Licensing](#7-licensing)).

- **Target:** arm64-v8a, `minSdk 26`, OpenGL ES 3.2 (ES 3.0 fallback with a warning)
- **Disc:** the user's own Gran Turismo 4 disc, read **in place**; never copied or extracted
- **Not part of this build:** FSR/upscaling (pure port) and any game data
  (ISO, dump, BIOS, asset, save file)

---

## 1. Status

| Phase | Work | State |
|---|---|---|
| 0 | Repo/docs/licensing cleanup, sync script | ✅ |
| 1 | Gradle + Compose skeleton, JNI bridge, GLES 3.2 boot probe | ✅ |
| 2 | I/O: ISO over SAF (fd probe done), save dir, config/INI bridge | 🚧 |
| 3 | GLES 3.2 render path (`render3d`, `showroom`, `race_view`, shader prologue) | ⏳ |
| 4 | Touch PS2 controls (GLES overlay → `plat_pad_state`) | ⏳ |
| 5 | Launcher: video/audio/pad options, disc validation details, thumbnail cache | ⏳ |
| 6 | Menu movies (`.pss`) with embedded FFmpeg (LGPL, MPEG-2 only) | ⏳ |
| 7 | Direct touch in menus (hit-test), native mobile feel | ⏳ |
| 8 | Release: signing, APK/AAB, device matrix | ⏳ |

The Windows/Linux path is **not** broken: `_WIN32` code (external ffmpeg path,
Win32 launcher) stays as it is, and Android only adds its own files.

---

## 2. Decisions

| Topic | Decision |
|---|---|
| Runtime | **SDL2** (Android support included; `plat_sdl.c` + `pad_sdl.c` are reused) |
| Graphics | **OpenGL ES 3.2**; single shader source valid for both `#version 330 core` and `#version 300 es` |
| AMD FSR | **Not present.** `third_party/fsr1`, `fsr.c`, `fsr_source.c` are not in the Android build |
| Launcher | **Jetpack Compose + Material 3** (launcher only; the game runs in its own Activity/SDLSurface) |
| Disc | **In place:** SAF `openFileDescriptor` → `/proc/self/fd/<n>` handed to the game's `plat_file_open` |
| Movies | Embedded **FFmpeg libavcodec (LGPL)**, `mpeg2video` only; no system `ffmpeg` binary |
| File API | `src/platform/common` already provides everything (`plat_file_*`, `plat_disk_free`, `plat_png_read`, `plat_jpeg_png`) → no new backend |
| Menus | Touch pad in game; **no D-pad in menus**, tapping activates the item under the finger |

`/proc/self/fd/<n>`: `GameActivity` opens the chosen `content://` URI as a
`ParcelFileDescriptor`, keeps it open for the process lifetime and passes that
path as `argv[1]`. The kernel reopens the same dentry, so no copy is made and no
storage permission is needed. Providers that hand out a non-seekable fd (cloud
providers usually return a pipe) are rejected with a clear message: *copy the
file to this device's storage and select it again*.

---

## 3. Layout

```
android/                          Gradle project (Compose launcher + GameActivity + JNI)
  app/src/main/java/org/opengt4/android/
    LauncherActivity.kt           disc selection, validation, Play
    DiscHolder.kt                 keeps the SAF fd alive, probes seekability
    GameActivity.kt               SDLActivity subclass; getLibraries/getArguments
    bridge/NativeBridge.kt        JNI: version banner, disc fd probe, GLES info
    ui/…                          Material 3 screens and theme
  app/src/main/java/org/libsdl/app/
    *.java                        SDL2 2.30.9 Java glue (vendored, zlib)
  app/src/main/cpp/CMakeLists.txt Android native target (SDL2 + platform layers)
  third_party/SDL-LICENSE.txt     SDL zlib license (for the vendored Java glue)
src/platform/android/             Android backend, behind the platform boundary
  android_platform.h              internal Android API + logcat macros
  android_video.c                 SDL2 window, EGL/GLES context, ES 3.2 gate
  android_jni.c                   JNI entry points, fd probe, version banner
  android_boot.c                  Phase 1 boot probe (temporary)
docs/ANDROID.md                   this file
```

`src/core/**` never calls an OS, GPU, audio or file API; it only uses
`include/platform/platform.h`. Everything Android-specific lives in
`src/platform/android/**`.

Phase 3 will move the shared GL code into `src/platform/gl/` (one copy of
`render3d.c`, `showroom.c`, `race_view.c`, …) and replace `android_boot.c` with
`src/app/main.c`. To avoid two copies of the source lists, the lists move to
`cmake/opengt4_sources.cmake` first.

---

## 4. Build

Requirements: JDK 17, Android SDK (`platforms;android-35`, `build-tools;35.0.0`),
NDK `27.0.12077973`, CMake `3.22.1`, and `ANDROID_HOME` (or `ANDROID_SDK_ROOT`)
pointing at the SDK.

```sh
cd android
gradle --no-daemon :app:assembleDebug     # debug APK
gradle --no-daemon :app:assembleRelease   # release APK (signing is separate)
```

There is deliberately no `./gradlew`: the wrapper jar is a binary and is not
kept in the repository. Run `gradle wrapper` once if you want one.

Artifacts: `android/app/build/outputs/apk/debug/app-debug.apk` (debug) and
`android/app/build/outputs/apk/release/` (release).

Pinned versions, for reproducibility: Gradle 8.9, AGP 8.7.2, Kotlin 2.0.20,
Compose BOM 2024.09.02, `compileSdk 35` / `minSdk 26` / `targetSdk 34`,
NDK 27.0.12077973, CMake 3.22.1, SDL 2.30.9 (FetchContent).

---

## 5. Running on a device

1. `adb install -r app-debug.apk`
2. In the launcher: **Select disc image** → pick your `.iso` (SAF), then **Play**.
3. Logs: `adb logcat -s OpenGT4:V SDL:V AndroidRuntime:E`

Phase 1 probe: the app logs the GLES lines (`OpenGT4: GLES 3.2 | GLSL …`), checks
the disc size through `plat_file_read_at`, clears the screen for three seconds and
returns, which closes the Activity. That proves SDL2 + EGL + ES 3.2 + JNI + SAF
end to end on real hardware.

---

## 6. Licensing

- Code: **GPLv3** (see `LICENSE`); additional terms in `NOTICE` §7.
- The Android build is **not the official OpenGT4**: the "Based on OpenGT4"
  attribution and the "unofficial" marker are kept in the docs and in the app.
- Vendored SDL2 Java glue: zlib license (`android/third_party/SDL-LICENSE.txt`).
- Embedded FFmpeg (phase 6): **LGPL** build, dynamically linked, with its source
  and license distributed alongside; only the `mpeg2video` decoder is compiled in.
- Game data is never distributed; the user supplies their own disc.

---

## 7. Release and signing

Release builds read the keystore from environment variables, never from the repo:

| Variable | Meaning |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | base64 of the `.jks` keystore |
| `ANDROID_KEYSTORE_PASSWORD` | keystore password |
| `ANDROID_KEY_ALIAS` | key alias |
| `ANDROID_KEY_PASSWORD` | key password |

APKs and AABs are written to `dist/` and are never committed; the keystore and
its passwords live outside the repository.

---

## 8. Risks

| # | Risk | Mitigation |
|---|---|---|
| R1 | Driver without ES 3.2 | ES 3.0 fallback, warning in the log; `noperspective` falls back to `smooth` |
| R2 | Cloud provider returns a pipe | fd seekability probe, explicit error message |
| R3 | PS2 memory-layout assumptions on a phone | later: memory budget pass; assets keep streaming from the disc |
| R4 | Movies need FFmpeg | phase 6; until then menu videos are skipped |
| R5 | Touch latency | overlay is drawn in the same GLES frame, no Java views in the game path |
| R6 | Android 14+ scoped storage quirks | in-place fd path avoids path permissions entirely |
