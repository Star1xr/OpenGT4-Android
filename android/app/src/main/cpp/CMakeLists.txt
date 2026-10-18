# OpenGT4 Android native hedefi (arm64-v8a).
#
# Faz 1: SDL2 + platform katmani (time/audio/input/dosya) + Faz 1 boot ikilisi.
#        Boyu: SDL2 Android baglami, ES 3.2 kapi kontrolu ve SAF fd yolunun
#        cihazda dogrulanmasi.
# Faz 3: buraya opengt4_core + paylasilan GLES render kaynaklari eklenir ve
#        android_boot.c yerine src/app/main.c baglanir. Kaynak listeleri iki
#        yerde kopyalanmasin diye once cmake/opengt4_sources.cmake cikarilir.
# FSR/upscale kaynaklari (third_party/fsr1, fsr.c, fsr_source.c) BILEREK yok.
cmake_minimum_required(VERSION 3.22)
project(opengt4_android C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT DEFINED OPENGT4_ANDROID_VERSION)
    set(OPENGT4_ANDROID_VERSION "0.0.0-local")
endif()

# android/app/src/main/cpp -> depo koku (5 ust dizin)
get_filename_component(OPENGT4_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
message(STATUS "OpenGT4 root: ${OPENGT4_ROOT}")

# ---- SDL2: Android destegi SDL'in kendi CMakeLists'inde ----
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

# SDL2main Android'de bos (SDL_android_main.c): main SDLActivity tarafindan
# libopengt4.so icinde "SDL_main" olarak cagrilir.
target_link_libraries(opengt4 PRIVATE
    SDL2::SDL2
    GLESv3
    log
    android
    m)
