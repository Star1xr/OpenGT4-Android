# Android project

The Compose launcher and the SDL2 / OpenGL ES 3.2 game for arm64 devices.

    cd android
    gradle --no-daemon :app:assembleDebug

Needs JDK 17, Android SDK 35, NDK 27.0.12077973 and CMake 3.22.1. There is no
gradlew script, because the wrapper jar is a binary and is not kept here. Run
gradle wrapper if you want one.

The launcher, GameActivity, DiscHolder and the JNI bridge live in
app/src/main/java/org/opengt4/android/, the native target is
app/src/main/cpp/CMakeLists.txt, and the SDL Java glue under
app/src/main/java/org/libsdl/app is vendored (zlib). The Android backend itself
is in ../src/platform/android/.

See ../docs/ANDROID.md.
