# OpenGT4 — Android project

Compose (Material 3) launcher plus the SDL2 / OpenGL ES 3.2 native game, arm64-v8a.

- High-level documentation: [`../docs/ANDROID.md`](../docs/ANDROID.md)
- Build: `gradle --no-daemon :app:assembleDebug` (see below)
- There is deliberately no `./gradlew`: the wrapper jar is a binary and is not
  kept in the repository. Run `gradle wrapper` once to generate one locally.

## Directories

| Path | Content |
|---|---|
| `app/src/main/java/org/opengt4/android/` | launcher, GameActivity, JNI bridge, Compose screens |
| `app/src/main/java/org/libsdl/app/` | SDL2 2.30.9 Java glue (vendored, zlib — `third_party/SDL-LICENSE.txt`) |
| `app/src/main/cpp/CMakeLists.txt` | native target: SDL2 + `src/platform/{android,common,sdl}` |
| `../src/platform/android/` | Android backend, behind the platform boundary |

## Build

```sh
cd android
gradle --no-daemon :app:assembleDebug    # debug APK
gradle --no-daemon :app:assembleRelease  # release APK (signing is separate)
```

Required environment: JDK 17, Android SDK (`platforms;android-35`,
`build-tools;35.0.0`), NDK `27.0.12077973`, CMake `3.22.1`, and
`ANDROID_HOME`/`ANDROID_SDK_ROOT` pointing at the SDK.
