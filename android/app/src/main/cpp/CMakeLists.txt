# OpenGT4 Android native target (arm64-v8a).
#
# Phase 1: SDL2 + the platform layers (time/audio/input/files) + the phase 1 boot
#          binary. Scope: the SDL2 Android context, the ES 3.2 gate and proving
#          the SAF disc path on a device.
# Phase 3: opengt4_core and the shared GLES render sources are added here and
#          src/app/main.c replaces android_boot.c. The source lists move to
#          cmake/opengt4_sources.cmake first so they are not duplicated.
# FSR/upscale sources (third_party/fsr1, fsr.c, fsr_source.c) are deliberately
# absent from this build.
cmake_minimum_required(VERSION 3.22)
project(opengt4_android C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT DEFINED OPENGT4_ANDROID_VERSION)
    set(OPENGT4_ANDROID_VERSION "0.0.0-local")
endif()

# android/app/src/main/cpp -> repository root (five levels up)
get_filename_component(OPENGT4_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
message(STATUS "OpenGT4 root: ${OPENGT4_ROOT}")

# ---- SDL2: Android support lives in SDL's own CMakeLists ----
include(FetchContent)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL2)

# ---- OpenGT4 ----
add_library(opengt4 SHARED
    ${OPENGT4_ROOT}/src/platform/android/android_boot.c
    ${OPENGT4_ROOT}/src/platform/android/android_jni.c
    ${OPENGT4_ROOT}/src/platform/android/android_video.c
    ${OPENGT4_ROOT}/src/platform/sdl/plat_sdl.c
    ${OPENGT4_ROOT}/src/platform/sdl/pad_sdl.c
    ${OPENGT4_ROOT}/src/platform/common/file_stdio.c)

target_include_directories(opengt4 PRIVATE
    ${OPENGT4_ROOT}/include
    ${OPENGT4_ROOT}/src/core
    ${OPENGT4_ROOT}/src/platform/sdl
    ${OPENGT4_ROOT}/src/platform/android)

target_compile_definitions(opengt4 PRIVATE
    OPENGT4_ANDROID_VERSION="${OPENGT4_ANDROID_VERSION}")

# SDL2main is empty on Android (SDL_android_main.c): SDLActivity calls "SDL_main"
# inside libopengt4.so instead.
target_link_libraries(opengt4 PRIVATE
    SDL2::SDL2
    GLESv3
    log
    android
    m)
