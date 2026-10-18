# OpenGT4 on Android

An unofficial Android port of OpenGT4 for arm64 devices. It is a modified
version, not the official OpenGT4 release, and it keeps the credits of the PC
project (GPLv3, see NOTICE).

## What it is

- arm64-v8a, minSdk 26, OpenGL ES 3.2 with a fallback to ES 3.0.
- Your own Gran Turismo 4 disc image is read where it is. Nothing is copied or extracted.
- 3D scenes are drawn at the Resolution setting of video.ini (1280x960 by default) and scaled up to the screen, without multisampling. Raise it for sharpness, lower it for speed.
- SDL2 for the window, input and audio, and a Jetpack Compose launcher for disc selection.
- No game data and no upscaling code in the repository.

## Status

Work in progress. The launcher runs and checks the disc, and the game starts on
the shared GLES renderer: the 2D UI, the menus and the 3D scenes draw, the last
of them through the same shaders as the desktop build, ported to the ES GLSL
dialect. Touch controls, films, save transfer and the performance pass are still
missing.

## Building

You need JDK 17, the Android SDK with platform 35 and build-tools 35,
NDK 27.0.12077973 and CMake 3.22.1. Versions are pinned: Gradle 8.9, AGP 8.7.2,
Kotlin 2.0.20, Compose BOM 2024.09.02 and SDL 2.30.9.

    cd android
    gradle --no-daemon :app:assembleDebug

The APK is written to android/app/build/outputs/apk/debug/. There is no gradlew
script, because the wrapper jar is a binary and is not kept here.

Release builds read ANDROID_KEYSTORE_BASE64, ANDROID_KEYSTORE_PASSWORD,
ANDROID_KEY_ALIAS and ANDROID_KEY_PASSWORD from the environment. Keystores and
passwords are never committed.

## Running it

    adb install -r app-debug.apk

Choose the disc image in the launcher, then press Play. The log names the GLES
version, the renderer, and the disc when it is accepted ("Gran Turismo 4 -
NTSC-U / SCUS-97328 / v2.00") or the reason it was rejected.

Android sends the game's output nowhere, so the game writes it to
`logs/opengt4.log` in its own files directory. The launcher's Share log button
sends that file, and the session before it stays as `logs/opengt4.log.prev`, so
a crash does not take the older one with it. The log holds the GLES version, the
driver, the shader messages, the size 3D scenes are drawn at and, every five
seconds, the frame rate with the worst frame of that window.

## Reading the disc

The launcher keeps the descriptor of the file the user picked open for the
lifetime of the process. The game receives it as /proc/self/fd/<n> and reads it
with 64-bit offsets, so the image stays where it is and no storage permission is
needed. Providers that cannot seek, which is most cloud providers, are rejected
with a message asking the user to copy the file to the device first.

## Layout

    android/               Gradle project: Compose launcher, GameActivity, JNI bridge
    src/platform/android/  Android backend, behind the platform layer
    src/platform/common/   shared file access, reused as is
    src/platform/sdl/      shared time, audio, input and the GL/GLES renderer, reused as is

Everything that touches the operating system sits in src/platform/android/, so
src/core stays portable and the Windows build is untouched.

## Licence

GPLv3, with the extra terms in NOTICE. The SDL Java glue in
android/app/src/main/java/org/libsdl/app is zlib licensed, see
android/third_party/SDL-LICENSE.txt. Movies will use an LGPL FFmpeg build with
only the mpeg2video decoder, distributed separately.
