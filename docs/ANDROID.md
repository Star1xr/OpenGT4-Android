# OpenGT4 on Android

An unofficial Android port of OpenGT4 for arm64 devices. It is a modified
version, not the official OpenGT4 release, and it keeps the credits of the PC
project (GPLv3, see NOTICE).

## What it is

- arm64-v8a, minSdk 26, OpenGL ES 3.2 with a fallback to ES 3.0.
- Your own Gran Turismo 4 disc image is read where it is. Nothing is copied or extracted.
- SDL2 for the window, input and audio, and a Jetpack Compose launcher for disc selection.
- No game data and no upscaling code in the repository.

## Building

You need JDK 17, the Android SDK with platform 35 and build-tools 35,
NDK 27.0.12077973 and CMake 3.22.1.

    cd android
    gradle --no-daemon :app:assembleDebug

The APK is written to android/app/build/outputs/apk/debug/. There is no gradlew
script, because the wrapper jar is a binary and is not kept here. Run
gradle wrapper if you want one.

Versions are pinned: Gradle 8.9, AGP 8.7.2, Kotlin 2.0.20, Compose BOM
2024.09.02 and SDL 2.30.9.

## Running it

    adb install -r app-debug.apk
    adb logcat -s OpenGT4:V SDL:V

Choose the disc image in the launcher, then press Play. The port logs the GLES
version, the renderer and whether noperspective interpolation is available, so
the log is the quickest way to see what a device supports.

## Reading the disc

The launcher opens the file the user picked and keeps that descriptor open for
the lifetime of the process. The game receives it as /proc/self/fd/<n> and reads
it with 64-bit offsets, so the image stays where it is and no storage permission
is needed. Providers that cannot seek, which is most cloud providers, are
rejected with a message asking the user to copy the file to the device first.

## Layout

    android/               Gradle project: Compose launcher, GameActivity, JNI bridge
    src/platform/android/  Android backend, behind the platform layer
    src/platform/common/   shared file access, reused as is
    src/platform/sdl/      shared time, audio and input, reused as is

Everything that touches the operating system sits in src/platform/android/, so
src/core stays portable and the Windows build is untouched.

## State

Phase 1 works: the app starts, creates a GLES 3.2 context, checks the disc and
draws for a few seconds before it closes. Next are the real render path, touch
controls, launcher options, menu movies with an embedded FFmpeg build, and
tapping directly on menu items.

## Signing

Release builds read ANDROID_KEYSTORE_BASE64, ANDROID_KEYSTORE_PASSWORD,
ANDROID_KEY_ALIAS and ANDROID_KEY_PASSWORD from the environment. Keystores and
passwords are never committed.

## Licence

GPLv3, with the extra terms in NOTICE. The SDL Java glue in
android/app/src/main/java/org/libsdl/app is zlib licensed, see
android/third_party/SDL-LICENSE.txt. Movies will use an LGPL FFmpeg build with
only the mpeg2video decoder, distributed separately.
