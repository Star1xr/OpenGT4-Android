# Showroom studio scene (research notes, 2026-09-29)

The PS2 car showroom (CarRoot's CarFace) draws a studio scene from
`/menu/select/`, animated by two motions. The port currently draws a flat
approximated backdrop and a constant turn; this file records what the CORE
code shows before implementing it. Addresses are CORE (splat) addresses.

## Files

| File | Size | Loaded by |
|---|---|---|
| `bg.mdl` | 1076014 | 0x001D5078, scene +0x0 (model; its header u16 at +0x16 is the number of lights) |
| `env.mdl` | 83505 | 0x001D5078, scene +0x4 |
| `tire.mdl` | 6687 | 0x001D5078, scene +0x8 |
| `camera.mot` | 256 | 0x001D5078, scene +0xC, played by scene +0x20 |
| `light.mot` | 4672 | 0x001D5078, scene +0x10, played by scene +0x2C |

Scene object: constructor 0x001D4FC8, load 0x001D5078, reset 0x001D51A0
(binds both motions to their players, clears the times +0x14/+0x18/+0x1A8,
light count +0x1C from bg.mdl +0x16), release 0x001D5140. Its owner calls
constructor, load and reset in sequence at 0x00154DE8..0x00154E44
(not yet identified as CarFace).

## Light update, 0x001D5258

For i < light count: the light motion's channel `Alpha<i>` (names table
D_006955C8, Alpha0..Alpha7) is evaluated at time +0x1A8 through
func_00429360; the result, clamped to >= 0, is light i's weight at +0x5C+4i.
Channel `BlackLevel` -> +0x1A0 and `WhiteLevel` -> +0x1A4 (clamped at 0).
`light.mot` also has `ColorScale`, `shadowDarkness0/1`, `shadowBlur0/1`
and `bottomShadow` (their readers are not traced yet).

So the periodic darkening is not one light: the studio's lit layers are
cross-weighted over time, plus black/white levels.

## Mot1 format (from the loader 0x00427698)

- `"Mot1"` magic; +0x10 offset of table A, +0x14 u16 count A;
  +0x16 u16 count B, +0x1C offset of table B. The loader rebases both
  tables' pointers (A entries via func_0042B098, B via func_00427680).
- Table B, 16 bytes per channel: +0 name offset, +4 s16 index into A,
  +6 s16 index of a packed curve, +8 s16 index of a keyframe curve
  (-1 = none). Lookup by name: func_00427778.
- Evaluation, func_00427930: packed curves (32 bytes each, array at A
  entry +4) through func_0042D430; keyframe curves (32 bytes each, array
  at A entry +8) through func_0042A4A8.
- camera.mot: one A entry, two channels (`TEST_CAMERA` and an unnamed one),
  no curves: its parameter list holds the camera (see below).
  light.mot: one A entry; keyframes look like groups of four floats
  (value, two tangents, time).

## Findings, second pass

- light.mot's single channel `bottomShadow` has a parameter list at its end:
  `10 <flag> name`, flag 1 = animated (u16 curve index follows), flag 0 =
  constant (float follows). Animated: Alpha0 (21), Alpha1, Alpha2,
  Alpha3 (24), ColorScale (25), shadowDarkness0 (26), shadowBlur0 (27).
  Constant: BlackLevel 0, WhiteLevel 1, shadowDarkness1 0, shadowBlur1 0,
  bottomShadow 0.7. So BlackLevel/WhiteLevel do not change on this disc.
- Records at 0x1060 describe 4 lights: type 0x0F position, 0x0C/0x0D/0x0E
  colours, then light index, a component mask (7 = x,y,z animated) and per
  component a u16 curve index or a float constant (colours end with a
  constant 1.0). Lights 0-1 animated (curves 0-20); 2-3 constant.
- Timeline element (32 bytes, 0x1040): end 24.15 s, loop start 1.0667 s,
  name offset, 28 curves, curve data offset. func_0042D430 maps time:
  t < 0 -> 0; t < end -> t; else loop_start + (t - loop_start) mod
  (end - loop_start). The light cycle repeats every 23.08 s.
- camera.mot has no curves: the studio camera is one constant pose,
  applied in the scene draw 0x001D5C00 via func_00427FE8 (near 0.1,
  far 100, aspect from the frame).
- Scene update 0x001D5B88: light time +0x1A8 += dt, then loops as above.
- Scene draw 0x001D5C00: light weights normalised to sum 1; each bg.mdl
  light layer i drawn with its share (func_001D5588); car drawn when the
  owner set +0x38 (model) and pose +0x40..+0x58.
- Owner per frame 0x00157378 (object at +0x8AC holds the scene): dt fixed
  1/60 s; when the car is loaded (func_00156660) it passes the model
  (+0x2A4, +0x864), position from +0x2C..+0x34 (z negated), rotation
  +0x38/+0x44(+180 deg)/+0x48, and the opacity +0x4C (see "Car layer").
  Owner constructor 0x00154CF8 starts a loader task func_00155080.

## Which CarFace uses the studio

MCarFace (class 0x00137F40..0x0013BD38) script API: getImagePath,
setImagePath, setColorIndex, syncLoading, loadModelSet, loadModelMotion,
loadModelOptionMotion; properties car_code, wheel, wing, car_color,
image_path, perfect_dark, quality_level, mirror, action_type ("normal",
"advanced"), car_model, car_data, car_garage, stream, debug_display; event
onStreamFinish.

`action_type` stores its index at +0x2A0 (0x0013AC48); the constructor
default is 1 = "advanced". The CarFace update 0x0013B018 sets the owner's
+0x94 = (action_type == 1), and the owner draws the studio scene
(0x00157378) only then. CarRoot never sets action_type, so the single-car
showroom is the studio; the 2-player page sets "normal" (plain view).

Without studio mode, the owner (0x00156F18) can play a per-car motion
given by loadModelMotion (+0x88; setter 0x00154290, called from
0x0013A678) with channels `fade`, `lift` (+0x8A8), `shadowOpacity` and a
camera. Only GT Mode scripts call loadModelMotion/loadModelOptionMotion.

## Parameter records: one bytecode for camera and lights

The parameter list at an A entry's +0xC..+0x8 is the byte stream run by the
motion interpreter func_0042B440 (reached from func_0042C300). Each opcode
reads its operand, evaluates its components (func_0042D688: a set mask bit
means a u16 curve index, a clear one an inline float) and calls a method of
the receiver object passed in; opcode 1 ends the list, and the loop also
stops once the receiver's +0 is set. Jump tables jtbl_006A4D50 (opcodes
0-4) and jtbl_006A4D70 (5-19). Opcodes seen on this disc:

| op | operand | components | receiver method | meaning |
|---|---|---|---|---|
| 0x0A | u8 mask | 1 | +0x50 | field of view, radians |
| 0x0B | u16 mask | 9 | +0x58 | look-at: eye xyz, target xyz, up xyz |
| 0x0C | light, u8 mask | 4 | +0x60 | light diffuse (rgba) |
| 0x0D | light, u8 mask | 4 | +0x68 | light ambient |
| 0x0E | light, u8 mask | 4 | +0x70 | light specular |
| 0x0F | light, u8 mask | 3 | +0x78 | light position |
| 0x10 | u8 flag, name | 1 | | named scalar (Alpha0, ColorScale, ...) |

The light colour meanings are our reading, chosen by the result: read
this way the fill lights 1-2 have diffuse and specular but no ambient and
the total ambient is 0.4; read as ambient/diffuse/specular (ModelFace's
`l0_amb`/`l0_dif`/`l0_spe` order) the ambient summed to 0.9 and lit the
cars flat, wheel barrels included.

## Studio camera

camera.mot's stream (at 0x70): `0B 0000` + nine constants, `0A 00` + one
constant, `01`. Eye (0, 1.280, -9.137), target (0, 0.426, 0.004), up
(0, 0.996, 0.093) (perpendicular to the view direction: a 5.3 degree
downward look), fov 0.4495 rad.

Scene draw calls func_00427FE8(camera player +0x20, channel +0x14,
time +0x1A8, aspect, near 0.1, far 100) -> func_00427E90 with a receiver
of type D_00687678: its +0x50 method (func_0042D858) stores the fov in
degrees (x 180/pi) at +0xC and sets +0/+8; its +0x58 is empty, so this
call only takes the fov. func_0049AC00 builds the frustum: tan(fov/2 in
degrees x pi/360) x near is the half height, times aspect the half width.
The aspect is frame width / height (0x00105818 returns the height, doubled
in field mode): 640/448, so one focal length, 224 / tan(fov/2) = 980 px,
serves both axes of the 640x448 frame. The look-at is applied by
func_00427A70 later in the same draw. The camera is constant; the
timeline words 0.15 / 0.1667 do not matter for it.

## Car layer, shadow and reflections

- Appearing: the car pose (owner +0x2C..) keeps an opacity at +0x4C that
  func_00153B50 (placing a car) sets to 0 with target +0x50 = 1, rate
  +0x60 = 0 and step +0x64 = 1/120; func_00153BB8 adds the step each
  frame, clamped to 0..1. The owner passes it to scene +0x58, and
  0x001D6378 draws the car into an offscreen buffer and composes it over
  the studio with that opacity (func_004A2808(0x64, +0x58)); below 0.5 and
  0.25 it also halves the shadow's pass count. So the car fades in
  linearly over 2 s. No scale or camera change was found: if PCSX2 shows
  a zoom, its source is still open.
- Soft shadow, 0x001D6968: the car (with tire.mdl for the wheels,
  func_00452008 gets scene +0x8) rendered at half resolution from six
  light positions jittered around light 0 (offsets D_006956B8), blurred,
  darkness from shadowDarkness0/1 (+0x180) and blur from shadowBlur0/1.
- Levels, 0x001D6220: subtract BlackLevel, then scale by 1 / (WhiteLevel -
  BlackLevel); both constant (0 and 1) on this disc, so a no-op.
- Reflections: 0x001D5C00 renders env.mdl (scene +0x4) around the car's
  position into a 128x256 map (func_0044EB10 with callback 0x001D5AE8 ->
  0x001D5B08), each panel model i with its raw Alpha_i weight. env.mdl's
  four models are light panels (grid softbox, round lamps) around and
  above the car; the rest of the map is black, so the reflections add
  only the panels.
- Post: func_001D56B0(0.6, 4, 2.5) runs four framebuffer passes (a glow);
  not reproduced.

## light.mot curves

The timeline element's +0x18 (0xFD0) is a table of 28 u32 offsets to
16-byte curve headers (type 1, data offset at +0xC). Curve data: u32 n,
then n+1 floats: one leading word (0), then keys of four floats (time,
value, control 1, control 2) - a cubic Bezier from each key's value
through its two controls to the next key's value; the last key has only
time and value. Evaluated this way every curve is a smooth set of
plateaus, and all end at 24.17 s, matching the 24.15 s timeline.

The cycle (weights normalised to sum 1 at draw time):

| time (s) | lit layer | notes |
|---|---|---|
| 0 - 1.07 | none -> Alpha3 | intro, outside the loop: from dark; ColorScale 1.0 -> 1.4 |
| 1.07 - 3 | Alpha3 | ColorScale back to 1.0 by 3.72 |
| 4.7 - 7.6 | Alpha0 | shadowBlur0 3, shadowDarkness0 0.7 |
| 11 - 14.8 | Alpha1 | |
| 16 - 21.4 | Alpha2 | shadowDarkness0 0.75 |
| 22.5 - 24.15 | Alpha3 | then loops to 1.07 |

Lights 0/1 (car lighting): light 0's position moves (x -0.96 -> -3.26 ->
7.17 ...), colours of lights 0/1 change between 0.4 and 0.55 per channel
(ambient-like 0 -> 0.3). Lights 2/3 constant (grey 0.2).

If the scene time is reset when a car appears, the intro (dark -> bright
overshoot -> settle) is the subtle "loading" seen in PCSX2; not yet
confirmed where the reset happens per car.

## Port implementation

- `src/core/mot1.c`: Mot1 reader (timeline, wrap, named scalars, light
  records, camera look-at and fov). `gt4model_survey <disc> --mot <path>`
  samples it; `--mdl <path> [png prefix]` decodes an MDLS and dumps its
  textures; `--raw <path> <file>` copies a volume file for local
  inspection.
- bg.mdl: 4 models of 198 triangles sharing 4 textures, one prelit room
  per layer (layer 3 bright open grey, layer 0 dark with a floor spot).
- showroom.c: the worker loads bg.mdl, env.mdl, light.mot and camera.mot.
  Room and car use camera.mot's camera (the car keeps its turntable pose:
  its yaw is offset by the camera's so it starts facing the same way as
  under the arcade CarFace camera). Each room layer is rendered once,
  unlit; per frame the layers are averaged by Alpha0..3, dimmed while the
  weights sum below 1, and multiplied by ColorScale (our reading).
- Car lighting: light.mot's four lights (diffuse, ambient, specular,
  position used as a direction) at the light time, in both renderers
  (`r3d_draw_lit` in render3d.c); specular scaled by 0.18 as the arcade
  CarFace path is (a calibration, not traced).
- Soft shadow: one projection of the car from light 0 onto the floor at
  half resolution, two box blurs of radius shadowBlur0 / 4, darkness
  shadowDarkness0 (the PS2's six jittered passes are approximated; a
  wider blur washed the core out and the car looked afloat).
- Contact shadow (the car's shadow model): opacity bottomShadow (0.7), our
  reading of the name; the arcade path keeps 0.55.
- Reflections: each env.mdl panel model is rendered once into a cube from
  (0, 0.8, 0), turned 180 degrees about y as the PS2's reflection camera
  (0x0044EEA8: height 0.8, tilt 15 degrees; 0x0044EF18: rotate y 180),
  and resampled into a 128x128 sphere map in the studio camera's view (the
  lookup the renderers already use); per frame the four maps are summed
  with the raw Alpha weights. The PS2 renders three 120-degree views and
  remaps them into its 128x256 map (0x0044EB10); the 15-degree tilt and
  that mapping are not reproduced.
- Each new car plays move_car's entrance (`CarFace::move_car(0, 1)` from
  set_car in arcade.adc): it starts at tz 8.5 facing the camera and eases
  to tz 0 by posInterpolateSpeed 0.025 per frame while the turntable
  restarts. A colour change of the same car fades only. The fade is 0.4 s
  eased, as PCSX2 shows (the code read above suggested 2 s).
- Wheels: rim and tire share an axial frame in tire widths (tire 0..1,
  rim about -0.1..0.3), so the rim maps by the tire width about the
  tire's nominal centre 0.5, both parts (car_model.c); stretched by their
  own extents the rim sank (the GT40's tire bulges to -0.076). The light time restarts when a car
  showroom is entered from a maker, not per car (as PCSX2 shows;
  the PS2's reset 0x001D51A0 was only seen in the owner constructor).
- tire.mdl is not needed: our car models include their wheels.

## Next

1. Compare with PCSX2 at several light times: room framing, car
   brightness (light colour meanings), shadow softness, reflections.
2. If PCSX2 shows a zoom when a car appears, find its source (not in the
   pose or the camera as traced).
3. The glow pass func_001D56B0 and the shadow's second light
   (shadowDarkness1/shadowBlur1, 0 on this disc).

## Turntable speed

Pose mode 1 (0x00153BB8) spins the car with a damped integrator on a
{force, velocity, angle} triple, once per 1/60 s frame: 0x0057AF30 adds a
force of 80, 0x0057AFF0 (damping 4) does v += f dt, v -= 4 v dt,
angle += v dt, force = 0; 0x0057AED8 wraps the angle to 0..360 degrees.
The car settles at 20 degrees per second, starting from rest (time
constant 0.25 s).

## GT Mode CarFaces

Fifteen GT Mode pages have a CarFace (dealers' BuyCarRoot, CarViewRoot,
HomeRoot, CarWashRoot, OilChangeRoot, the wheel and wing shops...). The
scripts set `car_code` (or `car_garage`, a garage entry: not supported
until the save exists) and often `action_type = "normal"`. ui.c draws a
CarFace in place through a platform hook (`ui_set_car_renderer`, set for
the gtmode project), after the widgets before it (the dealer's
`new_<nation>.png` background). Normal: `showroom_draw_face` draws just
the car and its contact shadow over the page, with the widget's
camera_trans / camera_rotate / view_angle and its l0..l2 lights plus
world_ambient (specular x0.18 as the arcade CarFace path), straight into
the frame (the 3D scene target is cleared opaque and would hide the
page). Otherwise the arcade studio. Not yet: the widget's own
environment image (the arcade env0 is used), garage cars, move_car
positions in GT Mode.

## Car hull (model 1)

Each /car/menu file's model 1 is a black, untextured low-poly volume of
the whole car (Audi quattro '82: 650 triangles, x +-0.92, y 0.10..1.18).
Drawn before the car with depth (showroom, dealers and their floor
reflection), it blacks out what lies behind the body: the far wheels,
which have no inner texture, and the underbody, as PCSX2 shows. Loaded by
car_model.c with the body's suspension pose.

## Dealers (normal CarFace)

PCSX2 shows the dealers moving the car exactly as the showroom (move_car's
zoom out, the turntable) with static lighting; the studio camera
(camera.mot) frames it. The car is composed over the page with the fade
by grabbing the page first (video_grab) and drawing it back over the car
at 1 - opacity. The floor reflection is the car (and hull) mirrored in
y = 0, then the page drawn back from the floor line down, 35% through at
the line and fully opaque 40% of the view lower (our tuning from PCSX2
captures, not traced).
