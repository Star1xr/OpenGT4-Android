# Native car-on-course preview

2026-10-01: races now draw native sky, scenery and distant model sets, cull
off-screen batches conservatively and align contact shadows to an indexed
RNW4 road surface. Course CPU geometry is freed after GPU upload. See
`docs/RENDERER.md` for measurements and validation. Course lighting/fog and
reflection maps remain incomplete; the preview notes below are historical.

The first course scene is a **stationary grid preview**, not a playable race.
The native converter reads the user's `/crs/<label>` and places the selected
showroom car at runway grid entry 0. No EE/VU execution is involved.

## Open and capture

In the Arcade car selection screen, **F5** loads the selected course and car.
The current paint selection is retained. Left/right orbit the camera; up/down
change its distance. **Esc** returns to selection, or cancels a pending load.
Menu music stops in the preview; engine/race audio is not implemented.

```powershell
build/Release/opengt4.exe $disc --race-course tsukuba --race-car ka_01
build/Release/opengt4.exe $disc --race-course tsukuba --race-car ka_01 --shot build/race.png
# Use menu selection instead of explicit labels:
build/Release/opengt4.exe $disc arcade TopRoot --preview-courses --input accept,accept,accept,accept,down,accept,right --race-preview --shot build/race_menu.png
```

Local convenience launcher: `build/Carro na pista.cmd` (Ka at Tsukuba).
Use `--color N` to select a paint in a direct CLI capture.

`showroom_request_course` performs disc reading and model conversion on a
separate SDL worker with its own volume handle. `showroom_poll_course` transfers
ownership on the rendering thread, or discards a cancelled result. Shutdown
joins both loaders. A failed load leaves the current scene intact. CPU
rasterization remains synchronous and is not yet fast enough for a smooth race.

## Data and conversion

Course header `+0x04` points to the world MDLS; `+0xdc` to RNW4. Runway:

| Offset | Meaning |
|---|---|
| `0x14` | Course length, float metres |
| `0x3a` | Grid count, u16 |
| `0x46`, `0x48` | Road vertex and triangle counts, u16 |
| `0x5c` | Grid table offset relative to RNW4; 16-byte XYZ/heading records |
| `0x74` | Road vertices; 16-byte records, XYZ floats |
| `0x78` | Road triangles; 12-byte records, first three u16 values are indices |

Bounds, vertex indices and finite coordinates are validated. Ground height
uses barycentric interpolation in XZ, selecting the highest surface below a
ceiling so bridges do not put the car on the upper road accidentally. It is
computed once when installing the scene, not for every stationary frame.

World conversion: uncompressed packets carry float positions; "compressed"
packets (GT4) carry signed fixed point, cars at 1/4096 and course worlds at
1/500 (2 mm). A compressed course packet sends its vertices in several VIF
unpacks at consecutive VU addresses (the unpack's address is the first
vertex): runs of 16-bit deltas broken by single 32-bit jumps, all in STMOD 2,
where the VIF adds each value to its row register (set by STROW) and keeps
the sum. Matrix commands and authored draw states are retained.

The scale and the batches were found by comparing the converted world with
the runway's road triangles (for each road triangle, is there a world
triangle straight under its centre within 0.6 m?). With 1/512 and only the
first batch read, 76% of the Nürburgring road, 50% of Deep Forest's and 24%
of Suzuka's had no asphalt drawn over it, and what there was drifted with the
distance from the origin (tens of metres at Nürburgring). Now 98 of the 110
course files cover all of their road and the rest leave at most 1% (small
street and slalom pieces). Car models convert exactly as before.

The generic view uses a
40-degree vertical FOV and a camera initially 6.5 m behind, 2.2 m above ground.
These camera settings are provisional, not recovered original race behavior.

Layout references are the local research copies of Nenkai's MIT PDTools
`CourseDataFileGT4.cs`, `PGLUshape.cs`, and format template `GT4Runway.bt` /
`GT4CourseData.bt` in ignored `build/reference`. No reference/game data is bundled.

## Verified and still missing

- Tsukuba: car, track and grandstand rendered in the main application.
- Nürburgring, Suzuka, Deep Forest: the world set draws the whole road (see
  above); checked on screen at the Nürburgring Time Trial start against a
  PCSX2 capture.
- `gt4course disc course [modelset-header-offset]` can inspect other model sets
  (e.g. `0x34`, a reduced copy of the road). Only the world set is rendered.
- Sky is a generated gradient. Original sky/environment/far sets, course
  lighting and reflections are not integrated. Car lighting/reflection still
  comes from the showroom. Contact shadow is approximate; slope alignment,
  physics, steering, tire animation, collision and opponents are not implemented.

## Checks

```powershell
cmake --build build --config Release --target opengt4 showroom_probe course_tests model_tests gt4course
ctest --test-dir build -C Release --output-on-failure
.venv/Scripts/python.exe tests/ui/run_race_smoke.py $disc
.venv/Scripts/python.exe tests/ui/run_car_smoke.py $disc
```

15 CTest tests, 5 race smoke cases and 9 menu/car smoke cases passed. The course
unit fixture checks a sloped surface, overlapping road levels, table truncation,
invalid indices and nonfinite positions. Captures/logs are under
`build/race_smoke` and `build/fb`.

The showroom probe now redraws after resetting angle even when requested time
is zero. Previously a zero-second capture still showed the last loading frame;
old `_0.png` baselines need regeneration. The Ka at 8 seconds matches the
existing `build/sr_base/ka_01_8.png` with zero changed pixels.
