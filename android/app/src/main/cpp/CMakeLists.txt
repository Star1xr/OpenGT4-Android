# OpenGT4 Android native target (arm64-v8a).
#
# The game program (src/app/main.c) runs on SDL2 with a GLES 3.2 context. The
# 2D renderer, the scene target and the 3D scene come from the shared sources
# under src/platform/sdl/ (render_gl.c, render3d.c, gl.c), the same files the
# desktop build compiles; only the context and the present live in
# src/platform/android/android_video.c.
#
# The Windows-only parts are absent by design: src/app/launcher.c (Win32 GDI,
# replaced by android_launcher.c), src/app/launcher_photos.c, and the external
# FFmpeg playback in src/platform/sdl/movie.c (replaced by android_movie.c
# until the films are ported). FSR/upscale (third_party/fsr1, fsr.c,
# fsr_source.c) is deliberately absent too.
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

# ---- game code: the root project's libraries, without its SDL setup ----
set(OPENGT4_LIBS_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory(${OPENGT4_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/opengt4_libs)

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
    ${OPENGT4_ROOT}/src/platform/android/android_jni.c
    ${OPENGT4_ROOT}/src/platform/android/android_launcher.c
    ${OPENGT4_ROOT}/src/platform/android/android_movie.c
    ${OPENGT4_ROOT}/src/platform/android/android_video.c
    ${OPENGT4_ROOT}/src/platform/sdl/plat_sdl.c
    ${OPENGT4_ROOT}/src/platform/sdl/pad_sdl.c
    ${OPENGT4_ROOT}/src/platform/sdl/gl.c
    ${OPENGT4_ROOT}/src/platform/sdl/render_gl.c
    ${OPENGT4_ROOT}/src/platform/sdl/render3d.c
    ${OPENGT4_ROOT}/src/platform/sdl/car_model.c
    ${OPENGT4_ROOT}/src/platform/sdl/race_view.c
    ${OPENGT4_ROOT}/src/platform/sdl/race_lights.c
    ${OPENGT4_ROOT}/src/platform/sdl/showroom.c
    ${OPENGT4_ROOT}/src/platform/sdl/license_scene.c
    ${OPENGT4_ROOT}/src/app/main.c
    ${OPENGT4_ROOT}/src/app/launcher_config.c
    ${OPENGT4_ROOT}/src/app/launcher_gallery.c
    ${OPENGT4_ROOT}/src/app/race_game.c
    ${OPENGT4_ROOT}/src/app/race_audio.c
    ${OPENGT4_ROOT}/src/app/loading_screen.c
    ${OPENGT4_ROOT}/src/app/pregrid_audio.c
    ${OPENGT4_ROOT}/src/app/photo_travel.c
    ${OPENGT4_ROOT}/src/app/video_settings.c
    ${OPENGT4_ROOT}/src/app/save_ui.c)

target_include_directories(opengt4 PRIVATE
    ${OPENGT4_ROOT}/include
    ${OPENGT4_ROOT}/src/app
    ${OPENGT4_ROOT}/src/core
    ${OPENGT4_ROOT}/src/platform/sdl
    ${OPENGT4_ROOT}/src/platform/android)

target_compile_definitions(opengt4 PRIVATE
    OPENGT4_ANDROID_VERSION="${OPENGT4_ANDROID_VERSION}")

# SDL2main is empty on Android (SDL_android_main.c): SDLActivity calls "SDL_main"
# inside libopengt4.so instead. opengt4_platform_common brings the portable file
# API; only what the linked objects need is pulled out of it.
target_link_libraries(opengt4 PRIVATE
    SDL2::SDL2
    opengt4_race_sound
    opengt4_replay_state
    opengt4_career_vehicle
    opengt4_core
    opengt4_physics
    opengt4_platform_common
    miniz
    GLESv3
    log
    android
    m)
