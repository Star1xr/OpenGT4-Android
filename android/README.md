# OpenGT4 — Android

Compose (Material 3) launcher + SDL2/GLES 3.2 native oyun, arm64-v8a.

- Üst düzey anlatım: [`../docs/ANDROID.md`](../docs/ANDROID.md)
- Build **yalnızca CI'da** (özel repo Actions): `.github/workflows/android-ci.yml`
- `./gradlew` bilerek **yok**: wrapper jar'ı binary'dir ve bu ortamda üretilemez.
  CI `gradle/actions/setup-gradle` ile Gradle'ı indirir. Yerelde isteyen
  `gradle wrapper` çalıştırıp üretebilir.

## Dizinler

| Yol | İçerik |
|---|---|
| `app/src/main/java/org/opengt4/android/` | launcher, GameActivity, JNI köprüsü, Compose ekranları |
| `app/src/main/java/org/libsdl/app/` | SDL2 2.30.9 Java glue (vendor'lı, zlib — `third_party/SDL-LICENSE.txt`) |
| `app/src/main/cpp/CMakeLists.txt` | native hedef: SDL2 + `src/platform/{android,common,sdl}` |
| `../src/platform/android/` | Android backend (platform sınırının arkasında) |

## Yerel build (isteğe bağlı, desteklenen yol CI'dır)

```sh
cd android
gradle --no-daemon :app:assembleDebug   # Android SDK + NDK 27 gerekir
```

Gerekli ortam: JDK 17, Android SDK (platform 35, build-tools 35), NDK
`27.0.12077973`, CMake `3.22.1`, ve `ANDROID_HOME`/`ANDROID_SDK_ROOT`.
