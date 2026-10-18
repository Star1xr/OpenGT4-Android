# Renderer

## Video backend (OpenGL 3.3)

`src/platform/sdl/video_gl.c` owns the window and an OpenGL 3.3 core
context (`gl.c` loads the entry points through SDL). Every frame is drawn
into an offscreen framebuffer the size of the window's drawable and copied
to the window at the end, so captures read the same pixels whether the
window is visible or hidden (tests).

The menus keep their 640x480 virtual screen: `plat_video_set_view` sets it,
it is letterboxed into the window, and all 2D drawing (`plat_draw_quad*`,
glyphs, rotated quads, clipping) is batched into textured triangles. The
window size sets the resolution: `opengt4 ... --window 1920x1080` or
`--fullscreen`. Menu captures under SDL_Renderer and OpenGL match.

## 3D (render3d.c)

Car and course models are converted once (`src/core/model.c`) and uploaded
as vertex buffers and textures. Consecutive triangles with the same state
form one draw. The shaders reproduce the software rasterizer of
`showroom.c`, which remains the reference (`OPENGT4_RENDERER=software`):

| PS2 rule | GPU implementation |
|----------|--------------------|
| Blend `(A-B)*C+D`, A/B/D in {Cs, Cd, 0}, C in {As, Ad, FIX} | the coefficient of Cs and Cd (a*C+b) mapped to GL factors; subtract / reverse-subtract when one is negative; `1+C` (not expressible) approximated by ONE |
| ABE bit per primitive | blending enabled only for those draws |
| Alpha as a coefficient (128 = 1), quantized | computed in the shader; the framebuffer alpha holds min(coefficient, 1) |
| Alpha test with fail action (KEEP, FB_ONLY, ZB_ONLY, RGB_ONLY) | two passes: passing fragments, then failing ones with the fail action's masks |
| Destination alpha test (DATE) | destination alpha copied to a texture before such draws, compared in the shader |
| Texture addressing REPEAT/CLAMP/REGION_CLAMP/REGION_REPEAT | per-tap addressing and bilinear filtering in the shader (texelFetch) |
| Colour masks, depth test/function/write, culling | GL masks; original API-to-ZTST mapping (004A26F0 / D_00629DD8), identical for writable/read-only depth; no estimated polygon offset |
| CarFace lighting, environment reflection (`env0`) | vertex shader lighting; per-pixel spherical mapping |

Faces kept by the reference (counter-clockwise on its y-down screen) are
counter-clockwise in GL.

A group only issues the calls it needs (2026-10-18): blend, depth, cull, the
colour mask, the unit 0 texture and the per-group uniforms are compared with
the previous group's values and skipped when they are the same. A course mesh
is a thousand groups and neighbouring groups usually share a material, so the
repeated calls were most of the submission cost. The Android frame log line
also reports what the last frame drew: `last frame N draws, M triangles, K
groups culled`.

## Validation

`showroom_probe` captures the same scene with either renderer (it restarts
the rotation after loading, so captures are reproducible);
`tests/ui/png_stats.py` measures the difference. At 640x448:

| Scene | Pixels differing | Mean | > 48 |
|-------|-----------------:|-----:|-----:|
| Ka, Cobra, R8, Model T | 37k-50k | 3.4-3.9 | 72-316 |
| Sports 800, MP5 | 28k-30k | 4.9-5.7 | ~490 |
| M Coupe | 44k | 8.0 | 1808 (side vent chrome slightly less contrasted) |
| Tsukuba / Nurburgring race view | 107k-225k | 4.8-13.2 | 1.9k-3.1k (grid markings drawn crisper on the GPU) |

Most differences are rounding and edge rules. The M Coupe vent and the
course decals are the open differences; no PS2 capture yet says which
renderer is right there. Frame time: ~0.3 ms (showroom) against ~18 ms in
software; course views 7-17 ms including the first upload.

## Texture upscaling (FSR 1)

`src/platform/sdl/fsr.c` runs AMD FidelityFX Super Resolution 1 (EASU,
then RCAS sharpening at 0.2 stops) once, when a texture is loaded; the
shaders are built from the official headers in `third_party/fsr1` (MIT,
licence kept there), embedded at build time. Needs an OpenGL 4.x context
(4.3 is requested, 3.3 is the fallback without upscaling).

- Menu images: by the integer factor that covers the window scale (3x at
  1080p), colours premultiplied by alpha for the filter. Textures updated
  every frame (movies) are not upscaled.
- 3D textures: 4x up to 128x128 texels, 2x above; addressing modes are
  applied in the upscaled texels. Region-repeat textures are left as they
  are (their bit masks do not scale).
- Default on; `--upscale off` disables it. `showroom_probe` stays without
  upscaling unless `OPENGT4_UPSCALE` is set, so reference comparisons are
  unchanged (`OPENGT4_WINDOW=WxH` sets its resolution).

Everything happens in video memory from the user's own disc; nothing is
written out. Expectations: edges get firmer and halos go away; detail that
is not in the original pixels does not appear (the Ka's rear plate logo is
a few dozen texels magnified about 9x at 1080p, and stays soft).

# Renderer improvements — 2026-10-01

Follow-up: compressed cars now retain their authored signed V3-8 smooth normals
at VU addresses `8080` (lit) and `8040` (reflection). Previously only float
normals were decoded, so the GPU/CPU fallback supplied one normal per triangle.
This made the entire S2000 body look faceted even though its reflection geometry
had the same 1,476 triangles in menu and open assets. The fix restores asset
data at load time, retaining the existing vertex layout and draw count. The
S2000 '99 Tsukuba capture now has smooth body lighting/reflections. Synthetic
stream regressions check signed values and both normal addresses.

Rigid pilot/vehicle shape positions also use their authored scale (`0049D328`
-> `0049E548`), fixing the oversized helmet. Shape kind 4 is a specular-only
mask (`0049D1A0` -> `0049EB90`); diffuse lighting on that additive pass had
whitened S2000 seats. Both GPU and CPU showroom paths now preserve that
distinction. See `docs/DRIVER_RENDERING.md` for pilot integration and remaining
animation limitations.

The shared GPU path now restores wheel-frame components, draws native course
sky/scenery, filters ordinary textures with mipmaps and up to 8x anisotropy,
and casts the showroom silhouette on the GPU. Original assets stay on the
user's disc. The earlier bring-up notes above describe previous stages.

- Car callbacks 11–14 select the visible branch and retain wheel-frame tags.
  Evidence: retail `CarModel::Callback` at `0x00453698`, wheel transform at
  `0x00454110`. This restores the authored brake meshes rather than fabricating
  discs or excluding low shape indices. Focus RS gains 1,452 triangles.
  Calipers follow steering/suspension independently of tire rotation. Axle
  placement still uses the existing SpecDB wheelbase/tread approximation.
  Follow-up: the first implementation inverted the lateral half-width
  translation and buried brakes behind the body hull. `0x004537A4` and
  `0x00454154..0x00454178` confirm native side 1 uses +width/2, side 0
  -width/2. Corrected and checked visually behind Focus ST170 wheel spokes;
  a factory-coordinate regression covers both axles and sides.
- Tire radial deformation now transforms normals by its inverse transpose.
- Course header `+0x14`, `+0x54`, `+0x58` supplies scenery, sky and far scenery.
  The sky follows camera translation and retains its authored angular size.
  Optional malformed/missing sets are skipped. Course lighting, fog and
  reflection maps remain incomplete; car reflections still use CarFace env0.
- Contact shadows align to RNW4 road height/normal, independently of chassis
  pitch. A hashed 16 m road index keeps queries local and handles bridges with
  a height ceiling. Oversized triangles use a bounded fallback chain. The
  2 cm shadow lift is a native raster bias, not a recovered PS2 constant.
- Studio shadow targets/shaders persist; the GPU draws one silhouette and
  two blur passes at 320x224. Light position, darkness and blur come from
  `light.mot`. The five-tap Gaussian is an approximation of PS2 jittered
  projection; the CPU reference remains available via `OPENGT4_SHADOW_CPU=1`.
- Ordered batches have conservative model-space bounds and at most 128
  triangles. Off-screen batches are rejected before state changes or DATE
  copies. No material sorting changes native mask/transparency order.
- Body/brake meshes share reference-counted GPU textures. Environment pixels
  update existing storage. Vertex uploads use a 467 KB scratch buffer instead
  of another full expanded model (228 MB per million triangles). Decoded
  course geometry/textures are released after successful GPU upload; RNW4 and
  the course buffer remain available to physics. Group allocation grows with
  actual batches rather than allocating one group per triangle.
- REPEAT/CLAMP textures use hardware linear/trilinear filtering; region clamp
  and region repeat retain manual per-tap GS addressing. Mipmaps/anisotropy
  improve distance stability and are native enhancements, not GS parity.

Measured locally at 640x448, 300 steady frames after 10 warm-up frames, VSync
and menu pacing disabled, with `glFinish` included in elapsed time:

| Scene / controlled comparison | Reference | Updated |
| --- | ---: | ---: |
| Focus RS studio, CPU vs GPU silhouette shadow | 6.259 ms | 3.833 ms |
| Tsukuba Ka grid, full submission vs bounds culling | 4.588 ms | 2.675 ms |
| Tsukuba submitted triangles, including repeated alpha passes | 157,270 | 50,111 |
| Tsukuba draw calls | 3,325 | 963 |

These are scene measurements, not whole-game FPS guarantees. Tsukuba and Nurburgring captures
with culling enabled/disabled are pixel-identical. Reproduce with
`OPENGT4_PROBE_FRAMES=300` and `showroom_probe`; set `OPENGT4_COURSE=tsukuba`
for the track, `OPENGT4_DISABLE_CULL=1` for the comparison. Captures/research
remain local in `extracted/renderer/`.

Validation: 807/807 CAR4 models convert; 19 CTest tests pass, including 100,000
frustum differential cases, attachment branch/stack tests, and indexed-road
comparisons with sloped road, bridges, negative cells and oversized triangles.
Visual checks include Focus RS/ST170 showroom, Tsukuba/Suzuka/Nurburgring grid and a
moving Ka with five opponents at Tsukuba. A Focus/Suzuka dynamic CLI capture
did not produce a usable physics pose; that combination is not verified.

## Vintage wheels and open cabins — 2026-10-01 follow-up

The shared car loader incorrectly read every TIRESIZE diameter as tenths of
an inch. Retail `0x0034BA30` uses whole inches below 61, divides larger
multiples of 10 by ten, and uses tenths for the remaining values. Width is
in 5 mm units, with a zero-width fallback of 10 mm. This restores the
authored 26/41-inch Patent Motor Wagen rims and the 34/45-inch Motor Carriage
rims. Their thin tires were also incorrectly rejected by the preview's
arbitrary minimum size guards. No model-name exceptions or synthetic wheels
are used. Zero front tread retains the single front tire and two rim faces.

The S2000's black roof came from compositing model 1 as additional opaque
bodywork. This model is an auxiliary closed volume (untextured kind 5,
black vertices, no depth writes), not the detailed open cabin in model 0.
Native model-1 submission at `0x004528A4` is a separate pass; the existing
standalone black draw omitted that context. The visible loader now uses
model 0 plus wheel components and model 2 contact shadows, leaving model 1
unloaded. This also removes the unnecessary CPU decoding/GPU upload/draw.
The remaining native auxiliary-pass behavior is not claimed to be recovered.

Validation: 19 CTest tests pass, including all three diameter encodings,
the 60/61 boundary, zero-width fallback and vintage dimensions.
`gt4car_survey disc.iso` audits the assembled loader rather than only MDLS
conversion: 722/722 catalog labels loaded, none without wheel assemblies,
none with nonfinite vertices. `--labels LABEL ...` checks a selected set.
This is an assembly audit, not a guarantee of visual correctness for every
vehicle. Visual captures cover both 1886 vehicles, S2000 '99/'04/Type V,
Spider Duetto and Focus ST170; Tsukuba grid checks cover S2000 and Patent,
and a live S2000 race capture verifies the posed-wheel path with five rivals.
The showroom/culling performance numbers above were measured before this
vehicle-loader follow-up. Axle-center placement remains approximate.

## Driver foundation — 2026-10-01

The original character skin stream, rig metadata and optional GPU palette
path are now decoded/tested. The 722-car driver audit confirms open S2000,
Patent and Motor Carriage pilot selection. Race integration and the original
pose/steering/shift animation are still pending; the bind diagnostic is not
a seated or animated race driver. See [DRIVER_RENDERING.md](DRIVER_RENDERING.md)
for the full audit, native function evidence, validation and remaining work.

## Course colour and sets against PCSX2 savestates — 2026-10-02

The savestates in `captures/physics/*.p2s` carry PCSX2's own screenshot of
the moment (`Screenshot.png`). `OPENGT4_RACE_SNAPSHOT=<capture>.eeMemory.bin`
loads that moment's guest memory into the native race (conductor found by
its session table), takes the captured lineup's models and freezes it, with
no HUD and no start camera, so the same frame can be compared side by side:

```bash
OPENGT4_RACE_SNAPSHOT=captures/physics/s03.eeMemory.bin OPENGT4_RACE_VIEW=0 \
  build/Release/opengt4.exe "$ISO" --drive --race-course laguna --window 640x480 --shot s03.png --shot-delay 8
```

`s01`..`s06` and `roof` are Laguna Seca / Nürburgring Arcade races (RACE
+0x78 is the COURSE key), `a_parado`..`e_reta2` Tsukuba time trials.
`gt4course disc course <header offset>` prints each set's draw states,
mean vertex colour and texel ranges.

- Course textures are authored at half intensity (texels up to 127) and the
  course comes out doubled: GS colour (textured texel * colour / 128,
  untextured the colour itself) times two. Laguna's textured sky matches at
  2.00x per channel, Tsukuba's untextured sky at twice its vertex colours.
- The view is world +0x04, sky +0x54 (behind everything) and far +0x58.
  Environment +0x14 / +0x5C (what cars reflect), reflection +0x24 with its
  mask +0x34 (the road's sheen, not done yet) and mirror sky +0x60 are not
  drawn in it (field names from Nenkai's PDTools CourseDataFileGT4).


## Race paint, rims and MSAA masks — 2026-10-02

Three independent faults affected the Ka's race model, without requiring
per-vehicle exceptions:

- `0049D328` submits shape+0x10 VIF packets unchanged. Signed UV unpacks
  at 8040+n are fixed-point V2-32/16/8 with STROW/STMOD; the unsigned
  C040 V2-8 stream is strip restart metadata. The previous V2-8 mask
  ignored the USN bit and consumed the strip stream, dropping tire faces.
  Subsequent signed UV batches (e.g. 8054/806F in `/car/lod/fdeu0006`)
  also lost their offsets. The decoder now retains their authored UVs.
- Race rim MDLS uses WheelContext, not CarModel's callback table:
  `00453028` → `00455598`, vtable `006885F0`, callback `00453E68`.
  Its argument selects a Tex1 bank and returns branch zero; `004568F0`
  binds that bank to external texture slot 2. Skipping this callback
  left only the tire; following it without the texture produced a solid
  polygon. The dedicated wheel decoder preserves the authored static
  bank (ctx+8=0). The adjacent authored blur bank (ctx+8=1) is now implemented below;
  dynamic accumulation for ctx+8>1 remains pending.
  Ka/S2000 race wheel geometry rises from 496 to 944 triangles per car.
- DATE/DATM from `00455708` commands 29/31 must read alpha written by
  preceding geometry. The main app uses an MSAA scene, where
  `glCopyTexSubImage2D` is invalid; the isolated single-sample probe did
  not expose it. A framebuffer blit now resolves the current mask into
  a resident single-sample texture before DATE draws, preserving draw
  order and framebuffer bindings. No CPU readback, per-frame texture
  allocation, mesh rebuild or removal of lamp geometry. MSAA edge alpha
  is resolved rather than tested per sample; original depth tolerance
  and environment approximation remain as documented above.

Validation uses independent synthetic data: `model_stream_tests` covers
all signed UV sizes, row modes, offsets, unsigned restarts, WheelContext
branches and selected external Tex1 banks. `model_mask_probe` covers
8 DATE/DATM cases on 1x/4x targets, framebuffer restoration and GL errors.
Both have deliberately wrong expected-result controls (`--negative`).
Restoring the old mask-copy code locally also makes `model_mask_probe`
fail at the GL-error check; restoring the fix passes all eight cases.
The Release build and all 22 CTest cases passed, alongside the mask probe
and its negative control. OpenGL's restriction is documented by
[Khronos](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/gl4/glCopyTexSubImage2D.xml).
Neither is a PCSX2 image oracle. The local Ka comparison sheet is
`extracted/renderer/ka-lamps-comparison.png`: native before/after plus the
user's original rear view (`u29`). The extra rectangle is gone (V).
Paint/rim rendering remains A pending fuller original-camera coverage
and the remaining close-up defects; no newly guessed vehicle constants were added.

`gt4car_survey disc.iso --race=1` / `--race=6` audits the race loader;
omitting `--race` audits the showroom. All three catalog runs loaded
722/722 vehicles with wheel assemblies and finite vertices. This does
not guarantee every component or every car's appearance.

F3 toggles the requested native inspection camera (P), seeded from the
active view including its projection. WASD moves, Q/E descends/ascends,
arrows turn, Shift accelerates; the camera ignores collisions, and F3
restores the race view. Player commands become neutral while inspecting;
the race continues. Speeds 3/15 m/s, turn 1.5 rad/s and pitch limit 1.55
are deliberate PC controls, not PS2 constants. Real frame time controls
movement independently of `OPENGT4_RACE_SPEED`.

`race_inspect_probe disc.iso output-prefix [--negative]` verifies the
actual app camera/keyboard code without sending OS input, and captures
front/rear/side diagnostic views. It checks pose seeding, travel distance,
orthonormal axes, vertical unclipped movement, neutral driving commands,
toggling back and cleanup. PNGs contain disc assets and stay local.


## Wheel exposure and native tire fitting — 2026-10-02

Wheel blur follows `003D73A0` -> `00344E48`: interpolate previous/current
rotations at presentation t and t+1, subtract, multiply by course Env0+EA0
exposure and radians-to-degrees constant 0x42652EE0. `00453E68` mode 1
compares absolute radians with 0x3DB2B8C1 and binds the adjacent authored
bank to external slot 2. Both banks stay resident; no per-frame uploads,
allocations or extra draws. Barrel/environment passes keep their bindings.
Mode >1 uses `00453BB0` accumulation, still pending.

Old fitting normalized the rim by its mesh extent and fitted the tire bead
to that scale. Original rim/prepared tire instead receive uniform nominal
rim radius (`0045314C..58`, `00453848..68`). `004588C0` reads radial min/max
and max absolute axial coordinate over every authored shape. `00458AB8`
maps the bead to 1, tread to total radius/rim radius and axial coordinates
to width/rim radius. It retains authored normals. This replaces the earlier
estimated stretch/inverse-transpose normals, at asset load only.

`wheel_motion_oracle extracted/core.bin`: 50,000 angular intervals bit-exact
and bank choices (E), 50,000 deformation samples within absolute 0.000001
(C). `--negative`/`--geometry-negative` exit 1 with deliberately wrong outputs.
`model_stream_tests` covers bank attachment and bounded all-shape scanning.
All 22 CTest cases pass after affected model targets are rebuilt. Showroom
and race=1/6 catalogs: 722/722, no missing wheels/nonfinite models.
`race_inspect_probe ... prefix --wheel-test`: 17,201 changed pixels, unchanged
draw/triangle counts; `--wheel-negative` fails as intended.

Local evidence: `extracted/renderer/ka-wheel-fit-{sharp,blurred,side}.png`,
`ka-wheel-close-comparison.png`, `wheel-fit-survey-{menu,race}.log`.
`b_reta` snapshot: original Ka CAR4 01AEAC00, rim 01B4DB40 (4 models/9 shapes),
front tire 01B55CC0 (2 models/4 shapes), CarModel 01BE2D68. Same authored
race MDLS as disc parts; all 244 rim position entries are byte-identical
to the captured PS2 streams. First tire shape: 132 positions agree with native
preparation within 0.0000002 local units. No invented tessellation/menu swap.
The cutouts reported in that close are corrected by the submission-order
fix below; comparable original projection/lighting remains pending (A).

## Wheel lip cutouts: tire before rim — 2026-10-02

The transparent face of the race rim also writes depth. Drawing the tire
after it left tire pixels blocked by the rim's polygon, exposing notches
and a polygonal-looking lip. `00453028 +0x16C` submits the tire MDLS before
`+0x198` submits the rim. The race body commands similarly call tire
callbacks 15..18 before nested rim callbacks 36..39. The shared loader
previously appended the rim first; it now appends the prepared tire first
in the assembled showroom model and in each animated race wheel.
Geometry, textures and draw counts are retained; only load-time order changes.

`wheel_order_oracle extracted/core.bin disc.iso` runs original `00453028`
unchanged, recording submissions at the renderer boundary with resource
queries supplied as fixtures. For all four slots it returns tire then rim.
That independent order drives comparison of 24 actual native Ka/S2000
assemblies (showroom, race=1/6), including triangle kinds, UVs and texture
bank indices. All pass (E for order); `--negative` reverses the expected
order and fails. The oracle does not compare GS rasterization or lighting.
Release builds, all 22 CTest cases and 1x/4x DATE mask probes pass.
Race=6 catalog: 722/722, no failed loads/missing wheels/nonfinite vertices.

The user's new original close and dark front view are stored locally as
`extracted/renderer/ka-wheel-reference-{close,front}.png`. The comparison
sheet `ka-wheel-order-comparison.png` shows the native notch removal and
continuous rim lip alongside the original close at another angle.
`ka-wheel-order-blur-comparison.png` shows sharp and authored blurred banks;
the real race probe changes 17,471 pixels with unchanged draws/triangles.
Overall rim appearance remains A: camera/light differ and dynamic blur
mode >1 remains pending. Authored polygonal tire segments are preserved;
no guessed smoothing or showroom-rim substitution was introduced.

## Course fog and road sheen — 2026-10-02

Both read from the game, then compared with the PCSX2 screenshots inside
the savestates (`OPENGT4_RACE_SNAPSHOT`, above). `OPENGT4_NO_SHEEN=1`
turns the sheen off for a before/after.

**Fog.** `func_00397CE0` (the environment applied before the scene) calls
`func_00397DA0` on Env0 +0x60: `func_004A40B0(distance +0x70, floor +0x74)`
and the colour through `func_004A3F98`, scaled by `D_0062150C`
(`func_00397DF8`: 1 / (+0x38 * +0x3C), a half, while the scene is drawn).
`func_004A40B0` leaves three numbers in the graphics context
(0x70002FAC/FBC/FCC): 255 - max*255, max*255 + 0.1 and the distance; they
reach VU1 as the w of the viewport scale, of the viewport offset and of the
clip vector (vf25, vf26, vf24). The vertex loops (micro memory of a
savestate, e.g. 0x3D8) do `MINI vf6, vf3, vf24` (w limited to the
distance), times Q = 1/w, `MADD ... vf25` on the offset, `FTOI4`: so
F = floor(max*255 + 0.1 + (255 - max*255) * min(w, d) / w), w the view
depth. Nothing is fogged nearer than the distance; far away the colour
keeps `max` of itself. The vertex GIF tags in the captures have FGE set
(PRIM 0x3C / 0x7C). The sky is drawn by `func_00396B80` with the fog at
(0, 1): none. MDLS commands 33/34/35 (`00455708`) set, keep and restore
the fog colour (`004A4050`); the course's additive shapes (blend 0x58)
set it to black, so that their light fades instead of turning into fog.

Laguna Seca: colour 0.70 0.68 0.62, distance 1000, floor 0.93 (7% at
most). Tsukuba: distance 120, floor 0.6. Grand Valley East: 0.35 0.37
0.40, 100, 0.75. Nurburgring: 0.24 0.25 0.23, 100, 0.3 (70% far away).

**Sheen.** `func_003411D0` is the scene's slot +0x378, right after the
world (+0x370) and before the cars:
1. `func_00342370(0)`: the frame's alpha cleared to 0.
2. The mask set (+0x34; alpha-only textures, FBMSK 0x00FFFFFF, no depth
   write) drawn with the world's depth: the frame's alpha is the sheen's
   strength there (texel alpha 12..114 of 128 times the vertex alpha).
3. A picture of Env0 +0xC24 x +0xC28 texels (64 or 128): the reflection's
   sky (+0xC8, drawn as the sky) and the reflection set (+0x24: a dome of
   white with an alpha pattern on the courses seen), from the same eye,
   with the fog of Env0 +0xC40.
4. +0xC2C times, the picture copied half a texel across (a 2x2 mean).
5. A rectangle over the frame with the picture, colour +0xC10 times
   +0xC1C (0.5, Grand Valley East 0.4) and GS ALPHA +0xC20 = 0x58:
   Cs * Ad + Cd.
After the scene `func_0033E828` multiplies the whole frame by Env0 +0x38
(2.0) through a FIX of 255: 255/128. That is where the course "comes out
doubled", and the cars with it.

Not done: the extra passes of Env0 +0x10 bits 0x10000 / 0x20000 (slots
+0x388 / +0x390) and a picture that is not square. Four courses were
looked at (Laguna Seca, Tsukuba, Grand Valley East, Nurburgring). The renderer still doubles
by 2 rather than 255/128. Away from the road and the sky the world is
still some 10 to 15 below the PS2 in the same captures; that difference
was there before and is not explained.

## Car shadows in the race — 2026-10-02

Read from the game, compared with the PCSX2 screenshots of `s03` (Laguna
Seca) and `s06` (Grand Valley). `OPENGT4_NO_CAST_SHADOW=1` leaves the blot
alone, `OPENGT4_NO_FOG=1` turns the course's fog off.

The scene's car pass (`func_00340950`) sets the shadow's colour before
each car (`func_0038F240`: Env0 +0xA60 divided by +0x38, alpha +0xA6C, and
the alpha's factor `func_003983E0` = (sun * (+0xAB0 - +0xAB8) + +0xAB8) *
level) and calls the car's slot +0xC8 (`func_003907A0` ->
`func_00451D18`). sun and level are the two values of the ground under the
wheels (runway vertex bytes 0xE and 0xF, the ground blocks' +0x14 and
+0x18), the mean of the four (`func_003D7460`).

`func_00451D18` moves to the pose's +0x40 (the drawn reference point,
car +0x894) and turns by its +0x38 (y: heading +0x674, x: the ground's
pitch +0x678, z: minus its roll +0x67C), then `func_00452418`: the
projection's near and far planes 0.008 further (`func_0049AB58`) and
0.02 up (the pose's +0x80). In that frame:
1. `func_004524B8`: model 1 (the car's closed dark volume, vertex colours
   black, alpha 128) through the matrix x' = x + y * Lx, y' = 0,
   z' = z + y * Lz, where L is the sun's way (`func_00398758`: angles
   +0xA70 and +0xA74 in degrees, (sin B cos A, -sin A, cos B cos A) with
   both negated, times +0xA78) taken into the shadow's frame; colour
   offset = the shadow's colour, alpha scale = +0xA6C times the factor.
2. `func_00452660`: model 2 (two triangles, the soft blot texture), alpha
   scale +0xA6C.
3. `func_00452998`: model 3, the lamps' glow, when the pose's +0x78 is
   above 0 (not drawn here yet).
Both shadow models blend (Cs - Cd) * As + Cd and do not write depth. The
VU1 program at 0x2A1 gives each vertex colour * scale + offset, limited
to (255, 255, 255, context +0xB14).

Laguna Seca: alpha 0.8, factor 1.8 in the sun and 0.5 out of it, sun 44
degrees up. Tsukuba: 0.7, 2.0 / 0.5. Grand Valley East: 0.9, 1.5 / 0.5.
Nurburgring: 0.75, 1.5 / 0.6, sun 21 degrees up.

The race's model (`/car/lod`, not `/car/menu`) carries model 1 with a
vertex alpha that falls with height: 77 of 128 at the ground down to 0 at
the roof. Times the scale and limited to 128, that is what makes the
cast shadow fade away from the car. The display list left in the PS2's
memory in `s03` and `s06` has the same numbers as read here (alpha scale
1.44 / 1.35 for model 1 and 0.8 / 0.9 for model 2, blue offset 0.64 /
1.28, ALPHA 0x44, FBMSK 0xFF000000, ZMSK set). In `s06` the shadow comes
out at 0.95 to 0.30 of the road against 0.87 to 0.40 on the PS2 at the
same screen points; the frames are not aligned to the pixel.
Courses with a table of lamps (RNW4 +0x40 above 0: one
flat volume a lamp, `func_0038F168` and `func_00451EE0`) are not done;
none of the four courses looked at has one. The dark volume the game
draws under the body (`func_00452808`, model 1 with the shadow's colour)
was not touched.

## Car lights in the race — 2026-10-02

2026-10-12: cars now also use the RNW4 local-light list and independent
Env0 +EF0 global ambient, via race_lights.c (0038E7F8 / 0038EFF8).
The missing branch made the Bentley in Rdemo0/paris_rally nearly black,
while its environment reflections were present. That course has 215 local
light records. 192 EE-upload comparisons pass within 2e-6; negative fails.
Native surface adapter selects the triangle under the drawn pose (+5 m
ceiling), so full scene binding/raster parity remains A. No physics edits.

The scene's car pass (`func_00340950` -> `func_0038EFF8`) sets the cars'
lights through `func_00398210(Env0, sun, level)`: light i is on with bit
i of Env0 +0x10 and is 0x80 bytes from +0xD0 (+0 the way to the light,
+0x10 ambient, +0x20 diffuse, +0x30 specular). The set at +0xAD0 (bits
11.., `func_00398058`) is another one; the cars were drawn with it before,
which left them dark and purple (Laguna Seca: ambient 0.25 0.24 0.34
against 0.30 0.30 0.32).

Each colour goes through `func_00397FF8`: times
((1 - (1 - sun) * a) * b * level + 1 - b) / Env0 +0x38, with a and b one
less +0xAC8 and +0xACC (ambient), +0xAB4 and +0xABC (diffuse), +0xAC0 and
+0xAC4 (specular); sun and level are the ground's two values under the
car, the same the shadow takes.

The lights reach VU1 as nine vectors at 201..209 (`0049E9B0`): three for
the light matrix (column x the way to light 1, column z its half vector),
the ambient, two diffuse, two specular colours (light times material,
times 128) and the limit (255, 255, 255, 128). Programs 0x23, 0x52 and
0x83 (shine below 6, below 12, or more) give each vertex
ambient + diffuse * max(N.L, 0) + specular * max(N.H, 0)^4 (8, 16),
limited to that; the GS then multiplies by the texel over 128, and the
frame's doubling undoes the division by +0x38.

The block in the PS2's memory in `s03` (Laguna Seca) reads ambient 19.2
19.2 20.48, diffuse 71.68 71.68 71.04 (times the material's 1, 0.9, 0.8,
0.6), specular 64 64 63.36: the numbers of Env0 +0xD0 over 2, times 128.

Not as the game: the half vector is made at each vertex from the camera's
direction; the game sends one per object.

