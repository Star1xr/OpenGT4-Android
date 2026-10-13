# Architecture

Goal: a native port that is readable, moddable, and easy to move to other platforms.

## Layers

```
core (game logic, physics, AI)      src/core
  |  only via
platform interfaces                 include/platform/platform.h
  |
backends (SDL, Vulkan/GL, ...)      src/platform/<name>
```

## Rules

- Core code never calls OS, GPU, audio or file APIs directly.
- No dependence on PS2 memory layout or 32-bit pointers.
- Renderer sits behind an interface so backends can be swapped.
- Game data is read at runtime from the user's own extracted files. Nothing from the game is embedded in this repository.

## Order of work

1. Reproduce original behavior (reference for everything else).
2. Get one track and one car running.
3. Only then change physics, force feedback, telemetry and other mods.
