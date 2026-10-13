# Vehicle physics: where it is in CORE.GT4

Goal: reproduce GT4's own vehicle dynamics in readable C (same formulas,
parameters and update order), validated against the original code. This
file records what has been located so far; names are provisional until a
function's behaviour is established.

## Located (US v2.00, CORE.GT4 addresses)

- The executable keeps GCC 2.9x RTTI names. Type-info functions (`__tf...`)
  reference the name strings; class vtables hold a pointer to that function
  followed by `{0, method}` pairs.
- `VehicleModel` (vtable 0x67dfa0) is an abstract interface (all entries
  `__pure_virtual`, 0x5bc5c0). `RaceCarModel` (vtable 0x67de00) implements
  it with methods at 0x38f9b8-0x390e78: presentation of the car, not its
  dynamics.
- `DynamicsConductor` (vtable 0x679e98) and its per-mode subclasses
  (`...SinglePlayer`, `...FreeRun`, `...License`, `...MachineTest`,
  `...Battle2P`, ...) have their 49 methods in 0x347000-0x366000, the
  densest floating-point region of the executable (0x340000-0x370000).
- Method 27 of the conductor (0x34d490) reaches 302 functions and ~1700
  FPU instructions through direct calls: the per-frame step candidate.
  Below it, 0x363b10 leads to 0x361908 (1340 instructions, 259 FPU),
  0x3630c0 and 0x46a510.
- **The simulation step is 0x34db30** `(conductor, flag)`, called by
  0x389b08 once per frame with flag 0 (next to a 1/60 constant) and by
  0x3f58b0 with flag 1. (Conductor method 12, 0x34db10, only calls
  0x34daa8, one of the step's own branches.) Every heavy FPU function of the region is
  below it; about 4800 FPU instructions are reachable. Its branches (to be
  identified; `tools/analysis/callgraph.py` rebuilds the graph):

  | Branch | Reachable FPU | Heaviest functions below |
  |--------|--------------:|--------------------------|
  | 0x344530 | 1171 | 0x36b0c8 (104), 0x353ee8 (82), 0x367148 (68), 0x36ba88 (61), 0x36a758 (55), 0x35d350 (54) |
  | 0x349fd0 | 1208 | 0x349418 (145), 0x465078 via 0x3f3400 (272) |
  | 0x3535b0 | 1121 | 0x368628 (177) via 0x36d818 (120), 0x356418 (126), 0x352848/0x352408 (~45) |
  | 0x34daa8 -> 0x45bf20 | 606 | 0x45e028 (34), 0x45e570 (30), 0x45d8e0 |
  | 0x357700 | 301 | 0x3f3090 (35) |
  | 0x3f1368 | 279 | 0x3f05e0 (58), 0x3f0a48 (41) |
  | 0x3f79a0, 0x3f7ea8, 0x3f4638, 0x344458, 0x351778, 0x35f658, 0x366588 | 60-200 each | |
- Car parameters come from the SpecDB tables (names at 0x6a6a38, pointer
  array at 0x6234b0: CHASSIS, SUSPENSION, ENGINE, DRIVETRAIN, GEAR, LSD,
  FRONTTIRE/REARTIRE, TIRESIZE, TIRECOMPOUND, TIREFORCEVOL, WING, ...);
  `func_00443518` opens them.
- Float constants are built with `lui`+`ori` (splat prints the full value,
  e.g. `(0x411CCCCC >> 16)`), so a search for literal words misses them.
  Read that way, the dynamics use SI units: gravity 9.8 (0x411CCCCC), 1/3.6
  and multiples (km/h to m/s), 2*pi/60 (rpm to rad/s), 716.2 (PS from
  kgf*m and rpm), 1/60 in a few filters. `tools/analysis/funcinfo.py`
  lists the constants, loops, callees and field offsets of functions.
- VU0 is little used (about 860 instructions, mostly in the graphics region
  0x49xxxx): the dynamics are ordinary FPU code, translatable to C.

## The step (0x34db30), in order

Argument: the conductor. Car `i` is `conductor + 0x6698 + i * 0x10a0`
(`func_0034c190`); the car count is at `conductor + 0x10000 - 0x760`.
Each subsystem runs in a loop over the cars, one phase after the other
(all cars finish a phase before the next one starts). Roles are
provisional, inferred from constants, fields and callees.

| Order | Function | Role (provisional) | Evidence |
|------:|----------|--------------------|----------|
| 1 | 0x3f1148 | mode-dependent setup | only when `func_003c0e78(mode)` |
| 2 | 0x35be60 | per-car reset when `car+0x545` is set | |
| 3 | 0x343ed0 | copies input/state blocks (`+0x994..0x99c`) | no FPU |
| 4 | 0x355e10, 0x35ca60, 0x35df80 | controller/driver input | flags, almost no FPU |
| 5 | **0x357700** | **engine torque** | torque curve lookup `0x34e9a0` (curve at `spec+0x134`, engine speed `car+0x600`) -> `car+0x5ec`; engine braking `0x34ea08` -> `car+0x5f0`; weight term `9.8 * spec+0x48` |
| 6 | **0x349fd0** | **driving aids or AI steering**, then 0x349418 | 0x465078 (1708 instr.): thresholds in degrees (10-80) and km/h (30, 5, 2); 0x349418: 9.8, pi/2, atan2 |
| 7 | 0x35d1d0, 0x3f7ea8 | ? (0x3f7ea8: pi, 1/2, loop over elements) | |
| 8 | 0x351778, 0x3f79a0 | speed-dependent assist (1/3.6, 1.5, 0.15) | |
| 9 | **0x3535b0** | **transmission and drivetrain** | 0x36d818 (+-98000, 0.99); 0x368628: rpm <-> rad/s, 1000/2000 rpm steps, 50000 rpm cap, gear logic; 0x356418: 716.2, 2*pi |
| 10 | 0x3f38d8, 0x350308 | small updates | |
| 11 | 0x3f12b0, 0x3f1368 | mode-dependent (0x3f05e0: 9.8, 1/60, degrees) | |
| 12 | 0x34daa8 -> 0x45bf20 | walls: every car against the course walls (see the rewrite status) | |
| 13 | 0x3f4638, 0x3f28b8 | 0x3f28b8: 9.8, 10/3.6, 2/3.6 | |
| 14 | 0x344420, 0x344458, 0x369888 | | |
| 15 | **0x344530** | **chassis and wheels** (below) | |
| 16 | 0x352ea0, 0x353218, 0x35a678 | 0x35a678: 1/60, 500 | |
| 17 | car against car | nested loop over pairs, conductor vtable `+0x20` | |
| 18 | 0x366588 ... 0x355868, 0x4146e8 | presentation, sound, controller output | |

`0x344530` calls, in order: 0x36a490, 0x36a758 (per wheel, 5 divisions),
0x36b0c8 (body attitude: angle rates integrated with `dt`, clamped to
+-pi/2 and +-pi/4, `9.8 * sin`), 0x36ba88, 0x36cee8 (vibration motor from
wheel data and surface), 0x353ee8 (steering: 0.0872665 = 5 degrees,
5.72958), 0x367148, 0x350080 (vibration from the engine), 0x366718,
0x345658 (1/60 filter), 0x345a20, 0x36c9f8 (rotation matrix from the three
angles at `car+0x66c..0x674` into `car+0x110..0x130`), 0x35d350, 0x35de20,
0x344040 (wheel rotation angles wrapped to 2*pi), 0x3668f0, 0x34a668, and
finally 0x34a788.

### Structures (provisional)

- Car (0x10a0 bytes): `+0x10` pointer to the car's parameter block (built
  from SpecDB; `+0x30` drive layout, `+0x48` mass term, `+0x134` torque
  curve, `+0x202` an rpm as u16); `+0x104` state block used as the base by
  most functions; `+0x110` rotation matrix (3x3); `+0x164` wheels;
  `+0x54c` time step `dt` (read by the integrators); `+0x600` engine speed
  (rad/s); `+0x66c..0x674` body angles.
- Wheel: `car + 0x164 + i * 0xec`; count from the layout table. Fields seen
  so far: 0x38, 0x48 (compared with 0: contact?), 0x70, 0x74, 0x89 (surface
  kind: 1, 10, 11, 21), 0xa0.
- Drive layout table `D_00620478` (84 bytes per layout, `func_00359510`
  with `spec+0x30`): byte 1 is the wheel count, then per-wheel index bytes
  (0x7f = none) and +-8 side markers.
- Math: 0x57d1f0 sinf, 0x57d2b8 cosf, 0x57d380 tanf (polynomials),
  0x57d118 atan2f (0x57d008 atanf), 0x36cc40 wraps an angle to [-pi, pi].
  Square roots appear as `c1 0x..` words in the disassembly (see below).
- Controller vibration (0x350080, 0x36cee8) writes 0-127 values into a
  per-player block; useful later for the wheel's force feedback.

## Running the original step on a snapshot

PCSX2 savestates (`.p2s`) are zip archives (zstd entries, which Python
3.14's `zipfile` reads) holding `eeMemory.bin` (32 MB) and
`Scratchpad.bin`. Snapshots stay local, in `captures/physics/`.

The conductor is found through its vtable pointer, stored at
`conductor + 0x10140` (the step reads it there to make virtual calls): in
a Time Trial the object is a subclass whose vtable is 0x685890, and the
word holding that value is at 0x1c079a0, so the conductor is at 0x1bf7860
(2 cars).

`physics_run <eeMemory.bin> <Scratchpad.bin> <conductor> <frames> <out>`
(`tests/physics/step_run.c`, same generated code as `main_run`) calls the
recompiled step with flag 0 once per frame and writes the car array after
every frame. From the straight-line snapshot (Ford Ka, Tsukuba, 3rd gear,
full throttle) 60 steps run with no kernel call and no unrecompiled code;
speed goes from 23.18 to 24.12 m/s, engine from 4640 to 4826 rpm.

Fields confirmed by the snapshots (Ford Ka at rest, on the straight and in
a corner; values in the order rest / straight / corner):

| Car offset | Meaning | Values |
|-----------:|---------|--------|
| 0x104 | position (x, y, z) | changes by -sin(yaw) * v * dt per step |
| 0x54c | dt | 1/60 in all three |
| 0x5ec, 0x5f0 | engine torque (N*m), engine braking | 82.7 / 88.6 / 74.0; 5.6 / 32.5 / 40.8 |
| 0x600 | engine speed, rad/s | 83.8 (800 rpm idle) / 485.9 (4640 rpm) / 609.9 (5824 rpm) |
| 0x640 | speed, m/s | -0.06 / 23.18 (HUD 52 mph) / 19.79 (HUD 44 mph) |
| 0x66c..0x674 | pitch?, roll?, yaw | yaw 1.159 / 1.159 / -1.060 |
| 0x6d4 | distance travelled | grows by v * dt |
| wheel + 0x20 | wheel circumferential speed (omega * r) | straight: front 23.83, rear 23.18 (front-wheel drive, 2.8% slip) |
| wheel + 0x28 | ground speed at the wheel | |

The throttle-like value at `car+0x4e8` goes from 0.71 to 1.0 on the first
step: the controller state in the snapshot is held for every step, so the
oracle replays one constant input.

Check against the real game: two savestates on the straight about 5 s
apart, full throttle all the way (D, then E). Running the step from D, frame 293
has the distance of E (884.157 against 884.158 m) and there:

| | Oracle, frame 293 | PCSX2 (E) |
|--|--:|--:|
| speed, m/s | 27.261 | 27.262 |
| engine, rpm | 5429 | 5429 |
| yaw | -1.5675 | -1.5674 |
| position | (-23.177, 168.450, 11.809) | (-23.178, 168.456, 11.809) |

The recompiled step reproduces almost five seconds of the game to the
millimetre with a held input, so it is a valid oracle for the rewrite
(steering and throttle changes still to be checked).

## Rewrite status (src/core/physics)

The rewrite keeps the game's memory layout and 32-bit addresses for now
(`gt4_ptr`), so each function can replace the original inside the running
step. `physics_run` is built from a recompilation with `--hooks` (every
function first asks `ee_ctx.hook` for a replacement); the harness maps
original addresses to the rewritten functions. `tests/physics/run_diff.py`
runs every snapshot with and without the rewrite and requires identical
car arrays and identical RAM (all 32 MB except the harness's stack). Arithmetic is plain float C with the host rounding set toward
zero, as the PS2 FPU rounds.

| Original | Rewrite | Status |
|----------|---------|--------|
| 0x359568 | `gt4_curve_eval` (piecewise linear table) | identical, ~10k calls per 600 frames |
| 0x34e9a0 | `gt4_engine_torque` | identical |
| 0x34ea08 | `gt4_engine_braking` | identical (only the default mode runs: flag at 0x620e14 is 0) |
| 0x350868, 0x350678 | `gt4_engine_wear_factor` | written; not exercised (the Time Trial car has the "no wear" flag) |
| 0x57d008, 0x57d118 | `gt4_atanf`, `gt4_atan2f` (the game's own polynomials) | identical, ~13k calls |
| 0x357700 | `gt4_car_engine_update` | identical; 0x35c0b8 (only when `car+0x5be` is set) still runs the original |
| 0x357418, 0x360f80, 0x3573a8, 0x359830 | `gt4_car_tyre_grip`: per wheel, grip = load * curve(load / axle reference) * axle multiplier * mu(surface) * curve(camber + body roll) | identical; Ka at the corner: loads 3502/1620/2803/1163 N, grip 4070/2072/3263/1421 N. Tyre wear/heat (0x356ef0, session flags 0x68/0x69 off in all snapshots) runs the original |
| 0x467470, 0x463bb8 | `gt4_surface_factor` (driving modes with a surface table, used by the second car) | identical |
| 0x57d1f0, 0x57d2b8, 0x36cb90, 0x36cb20, 0x36cc40 | `gt4_sinf`, `gt4_cosf`, `gt4_angle_wrap`, `gt4_angle_diff` | identical on 4 million inputs each (`OPENGT4_MATHCHECK`) |
| 0x3535b0 | `gt4_car_drivetrain`: the whole transmission step | identical; gearbox kinds 1-3 and 0x351eb0 run the original |
| 0x3511a0, 0x350fb8, 0x351178, 0x350e28, 0x350da0 | clutch state machine, brake and handbrake torque, wheel flags; engine speed implied by the driven wheels (overall ratio per gear: Ka -13.86, 13.52, 7.93, 5.22, 3.76, 2.97) | identical |
| 0x3534a0 | rolling resistance: sum of load * surface coefficient times a speed curve | identical |
| 0x351850, 0x3598f8, 0x3518e8 | tyre coefficient tables, force split per wheel | identical |
| 0x352408, 0x352218 | `gt4_car_tyre_forces`: slip angle, side force curve, friction circle (sqrt), camber thrust | identical |
| 0x351fa8 | `gt4_car_longitudinal`: forces along the body, mass plus free wheel inertia, speed | identical |
| 0x36d230, 0x36e4b8, 0x36d2f8 | wheel torque; differentials open (Ka rear) and kind 2 (Ka front); kinds 1, 3, 4, 5 and assists 6/7 run the original | identical |
| 0x36d818, 0x3554a0, 0x355410, 0x34ea68, 0x354b78, 0x354ce8 | `gt4_car_clutch`: engine output (rev limiter at the redline, back 500 rpm below), fuel use, clutch torque, drive line speeds; front-wheel drive with gearbox 0 (other kinds and turbo engines run the original) | identical |
| 0x3519b8, 0x351dc8, 0x350f48 | `gt4_car_gearbox`, `gt4_car_gear_select`: engagement, idle and redline clamps, rpm; the controls (0x36c8e8) and the shift itself (0x36beb8) run the original | identical |
| 0x352848, 0x3522b0, 0x350d40 | `gt4_car_yaw`: tyre forces in the body frame, yaw moment, yaw rate within +-2 pi, lateral acceleration; creep force at walking pace | identical |
| 0x352c90 | `gt4_car_velocity`: velocity from the accelerations; holds a car at rest with the brake (and the parked second car) | identical |
| 0x350ed0 | tachometer needle | identical |
| 0x344530 | `gt4_car_chassis` | orchestrator; 0x36a490 and 0x36a758 rewritten, the rest still original |
| 0x36a490, 0x36a080, 0x3696c8, 0x369760, 0x36a1d8 | `gt4_car_suspension_reach`: travel each wheel needs to reach the ground | identical |
| 0x36a758, 0x369de8, 0x369b48, 0x35d248, 0x36a690, 0x36a578, 0x36a048 | `gt4_car_suspension`: unsprung mass, spring (Ka front 19404 N/m), bump stop, square-root dampers blended to a square law, contact state, contact lateral position and camber against travel, anti-roll bars, load transfer | identical; linear dampers and the params 0x1076 correction run the original |
| 0x36c9f8 | `gt4_rotation_from_angles` | identical |
| 0x36b0c8, 0x36af30 | `gt4_car_attitude`: pitch and roll rates from the front/rear and left/right loads (clamped to +-pi/2 and +-4 pi, angles to +-pi/4), then the body's vertical motion from each wheel's load along its ground normal, downforce and gravity 9.8 | identical; layout kind 1, CAR_5BE, aero kind 7 and vertical modes other than 2 run the original |
| 0x36ba88 | `gt4_car_ground_fix`: lifts and tilts the body when wheels cannot reach the ground | written, not reached by the snapshots: runs the original unless `gt4_phys_unverified` is set |
| 0x345a20, 0x3457b0, 0x3458d0 | `gt4_car_mounts`: wheel mount positions from the body orientation | identical |
| 0x345658, 0x34a668, 0x34a788, 0x344858.. | `gt4_car_view_follow`, `_sway`, `_bob`: springs of the player's view against yaw, pitch and body motion (per-player block at session + 0xcab8) | identical (whole RAM) |
| 0x366718, 0x366678 | vibration reset | identical |
| 0x344040 | `gt4_car_wheel_rotation`: wheel angles (whole turns kept apart), reference points, steering share, filtered throttle and brake | identical |
| 0x367148, 0x3438c8, 0x34c968 | `gt4_car_surface_feel`: per-wheel vibration/sound terms from tables per surface class, contact countdown | identical on plain surfaces; loose (1, 10, 11, 21) and rough surfaces, which also add tyre wear, run the original |
| 0x35d350 | `gt4_car_ground_frame`: ground plane under the car from the four contact heights (pitch, roll, height, rotation rows at car+0x140) | identical; layout kinds other than 0 run the original |
| 0x35de20, 0x35d920, 0x35d6b0, 0x35dd00, 0x343518, 0x35dc08, 0x35d990 | `gt4_car_ground_speeds`: speeds in the ground frame, each wheel's velocity (yaw and roll terms), odometer, speedometer limited to 400 km/h per second, lateral g filter, free wheels rolling with the ground and the drive line speed from them | identical; engine wear and layout kind 1 run the original |
| 0x344458, 0x3957f0, 0x395760, 0x3956f8 | `gt4_car_ground_query`: ground under each wheel, a vertical ray from 5 m above its ground point; faces steeper than 60 degrees count as flat | identical |
| 0x3e5a30, 0x3e5bc8, 0x3e5fd8, 0x3e5f40, 0x3e5fa8 | `gt4_course_ray_down`: the course's collision mesh as a kd tree (split byte per node: axis and position in 64ths of the box), nearest front-facing triangle, point kept in the triangle's box, per-vertex bytes interpolated, unit normal and plane | identical (also the calls from 0x34daa8) |
| 0x34daa8, 0x45bf20 and 30 more | `gt4_walls`: each car as an outline of eight points on the plane (x, y); the move over the frame is cut at the first wall hit (points crossing a wall segment, edges sweeping over a wall's corner), the hit gets a normal impulse (restitution at 0x846634) and friction (0x84663c, capped), points slide along the wall and round its ends, points still behind a wall are pushed back; a stuck car turns with its steering; hit strength feeds the view shake. Walls come from the course: chains of vertices listed by the ground triangle under the car | identical, also on 900 variants of the snapshots (`fuzz_diff.py`); car against car (0x45e9d8) and an edge hitting a corner (0x45e570) run the original |
| 0x3f7ea8, 0x3f7ca8, 0x36ccd8, 0x40cc58 | `gt4_car_steer_geometry`: each wheel's angle (Ackermann factor per side, rear steer, toe), the steering column's lag toward where the wheel points or rolls (modes and settings at D_00845BF0), compliance steer from the side force (D_00620AF0), the cockpit steering wheel's spring | identical, also with the column modes and load scaling drawn |
| 0x3f79a0, 0x3f71e8, 0x3f9d00, 0x3f9870, 0x3f94c0, 0x3f9340, 0x3f76e8, 0x3f7880, 0x359778, 0x3596c0 | `gt4_car_aids`: understeer and oversteer measures (filtered), traction control on the throttle (above 100 km/h through a spring), ABS with the tyre's slip limit lowered with steering, stability control braking one side, speed limits (race rules, pit lane, empty tank) and the grip-use throttle limit | identical, also with the aid settings (params + 0x1098, switches, global table) drawn |
| 0x3f28b8, 0x3f2798 | `gt4_car_active_aero`: moving wing and air brake by speed, braking and lateral g (params + 0x1077 kinds 1-6) | identical with the kinds drawn |
| 0x351dc8, 0x36c8e8, 0x36c160, 0x36c098, 0x36bf28, 0x36beb8 | the gear the controls ask for: automatic (up at the shift rpm unless the driven wheels spin past the tyre's best slip, down below a per-gear rpm moved by the pedals, overrev guard), sequential button mode, the model's gear for its speed plus a pedal offset | identical; manual gearbox kinds (0x36c690, 0x36c780, 0x36c850) run the original |
| 0x34db30 | `gt4_step`: the step itself: each subsystem over every car in the game's order, the slowed-time gate, the countdowns, the session's virtual calls (walls, distance along the course, race order), car against car by race position (original contact function) | identical; the calls not rewritten go through the bridge |
| 0x351778, 0x3513f8, 0x351358 | `gt4_car_slip`: longitudinal slip of each wheel (plain difference for tyres with params flag 0x1d8, else ratio to the wheel or ground speed with a start-up factor), faded in over the first km/h | identical, also with the start-up table (D_00620CD8 + 0xb6) on |
| 0x352ea0 | `gt4_car_tyre_wear`: tyre temperature and wear when the session turns them on | identical with them on (settings drawn by `fuzz_diff.py`) |
| 0x344420, 0x345a50, 0x3697a8 | `gt4_car_wheel_setup`: mounts, the wheels' speed weight, the point under each wheel to look for the ground | identical |
| 0x369888, 0x3f4e40, 0x3f4ea0, 0x369640 | `gt4_car_ground_info`: surface under each wheel (marked blocks remapped), the surface's bumps cos(kx) + sin(kz), wheels off the road, the course's calls | identical |
| 0x353218, 0x343c20 | `gt4_car_race_end`: laps or time done, fuel for the rest, worn or punctured tyres set the retire flags | identical; the fuel case needs a finish line Time Trial has not |
| 0x343ed0, 0x3f38d8, 0x35a678, 0x579330, 0x578598 | state kept for the view and replay, brake temperature, the wheels' random shake (the game's generator at D_00874D50, its int-to-float done on the bits with PLZCW) | identical |
| 0x365e80, 0x365910, 0x366588 | gaps between cars in laps and metres; at the finish, player cars handed to the driver model | identical |
| 0x469e58, 0x36bf68 | `gt4_ai_gear`: the gear for the model's target speed | identical |
| 0x349fd0, 0x3484d0, 0x350c20, 0x349f80 | `gt4_car_driver`: controller read (through its virtual table), steering input extrapolated per device (D_00622110 + 0x70 rows), display values, the reference point's ground, then the controls by mode | identical, also with made-up commands and settings (`fuzz_diff.py`); 0x469e58 and the replay input 0x3f3400 run the original |
| 0x465078 (player's case), 0x463ea8, 0x464868, 0x46b480, 0x46b310, 0x469f20, 0x464ed8, 0x464e88 | `gt4_ai_player_aim`: the driver model on the course line ("ADLN" lines of 0x40-byte points: straight or arc, distance along the course), the section the car is in, how much of the target speed the next corner allows (tyre grip on the corner's surface, braking distance), and the aim point ahead (distance by speed, a second line blended in, lateral offsets from the line, the wheels off the road and the grip in use) kept at car+0x720 | identical, also with the model's state and settings drawn by `fuzz_diff.py`; the rest of 0x465078 drives AI cars and still runs the original, as do the other line format (0x46b7b8), 0x4679c0 and 0x4684a8 |
| 0x349f48, 0x349418, 0x348850, 0x3485d0, 0x348588, 0x348800, 0x3481d8, 0x343cf8 | `gt4_car_controls`: steering limit by speed (curves at params + 0x58/0x94/0xd0), counter-steer allowance from the slip (settings D_00622110 + 0x22/0x26), GT Force responses, yaw assist, the steering's spring and damper (tables at 0x843598); throttle, brake and handbrake analog (dead zone to 0.1, optional rate limit) or digital (ramps 1/6, 1/3 per step); clutch request and the idle throttle | identical on pad, digital, GT Force (4) and wheel (6) inputs; layout kind 1, steering along the course line (assist 0x84357c) and gearbox kind 3 run the original |

Still original in the chassis step: the steering force feedback
(0x353ee8, output to the wheel controller through its driver's virtual
calls and per-mode tables in D_008436B0), the vibration outputs (0x350080,
0x36cee8) and the tyre smoke effects (0x3668f0). With the natives on,
about 390 original functions (26.8k instructions) still run per step, most
of them in the driver layer 0x349fd0 (controller read, steering smoothing,
aids/AI 0x465078). Course collision (ground under the wheels, walls) is
native, and so are the driver's controls and the step itself. With the
natives on, about 300 original functions (17k instructions, part of them
reached only through rare cases handed to the original) still run: the
sound (0x3f71c8), the force feedback and vibration outputs, 0x3f79a0,
0x3f4638 (cars against each other: slipstream, the model's awareness),
0x359008 (laps and timing), 0x35f658 and the AI driver.
| 0x3f3090, 0x3f2db8 | `gt4_car_aero`: drag `params+0x1068 * v^2` (Ka: 0.363, 195 N at 23 m/s), front/rear downforce lost between 5 and 10 degrees of yaw, slipstream amount rising 2/s and falling 0.5/s (halves the drag) | identical; slipstream and downforce paths not exercised (Ka has no downforce, no car ahead) |
| 0x35acb0, 0x3658a0, 0x354ac0 | countdown, step length (dt = scale/60), masses (params mass + 0.74 * extra) | identical |
| 0x357690, 0x3575e0 | `gt4_car_wheel_directions`: per wheel, direction of the velocity atan2(-lat, long) and a weight from 0 (1 km/h) to 1 (10 km/h) | identical |

Note: splat prints the R5900 `sqrt.s` (encoded with the ft field) as
`c1 0x44`-style words, so a search for `sqrt.s` misses it; the dynamics
do use square roots (e.g. 0x3575e0).

A deliberate error in the rewrite (grip times 1.0001) makes every snapshot
differ within 120 frames, so the comparison does catch small deviations;
each new part gets the same check. The PS2 FPU flushes denormals: the host
needs flush-to-zero and denormals-are-zero as well as rounding toward zero
(`gt4_phys_fpu_enter`), or the tiny terms of the sin/cos polynomials leave
one-ulp differences.

`tests/physics/fuzz_diff.py` runs the same comparison on variants of the
snapshots: the player's car gets another velocity (direction, 8 to 70 m/s)
and yaw rate, which sends it into walls from other angles and into spins.
With `FUZZ_INPUT=1` the controller read returns made-up commands (pad,
digital, GT Force, wheel; analog and digital axes) and with
`FUZZ_SETTINGS=1` the driving settings are drawn too (counter-steer modes,
pedal ramps, wheel response, assists, driver kind, steering extrapolation).
It reaches paths the laps do not (wall ends, corners, pushes) and found two
faults the laps had not: a loop in the original that visits only seven of
the eight outline points (reproduced), and the one below.

MSVC compiles float selects such as `x <= 0 ? 0 : x` to `maxss`/`minss`,
even with `/fp:strict`, and then returns the wrong zero on a tie (-0
against 0), where the PS2 keeps the sign. Clamps against zero use
`gt4_select` (a select on the bits) instead. `opengt4_physics` is built
with `/fp:strict` so the rounding mode is respected.

Branches that no snapshot exercises are not rewritten on trust: the
rewrite tests the condition and hands those cases to the original.

Rewritten code that still needs an original function calls
`gt4_orig_call` (the harness runs the recompiled function on the same
context); it goes away as the callees are rewritten.

The Ford Ka's engine block (params + 0x134): 13 torque points from 1000 to
7000 rpm every 500 rpm (peak 104.9 N*m at 2500 rpm), engine braking 0.0668
N*m per rad/s.

Harness stand-in: `func_004AC400` (VU0 `vrnext` random number, used by an
effect under 0x3668f0) is answered by the harness with the same generator
and a fixed seed.

## Car setup: the parameters from the record

The block the step reads through `car+0x10` is built once per car at the
race start. `func_0034D490` (race init) takes the car's garage entry
(`[[race+0x60]+8][i]`; its SpecDB keys start at +0x20), and:

1. `func_003F3B68` clears a 0x600-byte record and fills it with
   `func_004413C8`: each 64-bit key of the garage entry (high word the
   table, low word the row) is fetched from the in-memory SpecDB
   (`func_00443ED0`, tables at D_006235A8 + 0x134) and copied field by
   field into the record, with the tuning levels of the entry. Table
   indices: 1 BRAKE, 2 BRAKECONTROLLER, 3 SUSPENSION, 4 ASCC, 5 TCSC,
   6 CHASSIS, 7 RACINGMODIFY, 8 LIGHTWEIGHT, 9 STEER, 0xA DRIVETRAIN,
   0xB GEAR, 0xC ENGINE, 0xD NATUNE, 0xE TURBINEKIT, 0xF PORTPOLISH,
   0x10 ENGINEBALANCE, 0x11 DISPLACEMENT, 0x12 COMPUTER, 0x13 INTERCOOLER,
   0x14 MUFFLER, 0x15 CLUTCH, 0x16 FLYWHEEL, 0x17 PROPELLERSHAFT, 0x18 LSD,
   0x19 FRONTTIRE, 0x1A REARTIRE, 0x1B NOS, 0x1C SUPERCHARGER, 0x1D WHEEL,
   0x1E WING, 0x1F TIRESIZE, 0x20 TIRECOMPOUND, 0x21 TIREFORCEVOL (0 is
   GENERIC_CAR, 0x22 COURSE, 0x23 RACE).
2. `func_00363B10` (car init) calls `func_00361908` with the record: the
   parameter block (conductor + 0x98 for car 0).

`tests/physics/setup_run.c` runs both on a snapshot with the arguments of
the race init. On the Ka snapshot the recompiled setup gives the
snapshot's block except for one-ulp differences in 250 floats (PCSX2 rounds
divisions to nearest, the PS2 truncates) and four aid words (+0x10A4..
+0x10C8) that Time Trial clears after the setup.

`gt4_car_params` (`src/core/physics/setup.c`) is `func_00361908` and
everything under it: masses and ballast, geometry and weight split,
inertias, the engine (`func_0034EF30`: torque curve from the record,
tuning factors, turbo stages `func_0034ED48`/`0034EB00`, the race's power
limit), the drivetrain (`func_0034ACA8`: drive kind, differentials, gear
ratios and their automatic setting `func_00354FF8`, wheel sizes
`func_0034BA30`, clutch, shift points), tyres (`func_00361570`: slip and
side-force curves, grip per surface, load sensitivity), springs and
dampers (`func_00360B60`, the damper fit `func_00360980`), travel limits,
steering and camber curves, aerodynamics, brakes, the driving aids, and
the summary (`func_00360780`: engine power table `func_0034F3F8`, top
speed by simulation `func_00360520`, class and rating). Only the hybrid
drive (`func_00355978`, drivetrain 2 with kind 5) calls the original.

The record is native too (`src/core/physics/record.c`: `gt4_car_record`,
`func_003F3B68` with the copiers of `func_004413C8`), and so is a new
car's garage entry (`src/core/physics/garage.c`: `gt4_garage_new`,
`func_00444480`): the GENERIC_CAR row of the car holds the key of its
DEFAULT_PARTS row, whose keys (one per part table, in table order) go to
the entry's slots (`func_00440B50` gives the slot of each table: 0x20
BRAKE, 0x28 BRAKECONTROLLER, 0x30 CHASSIS, 0x38 ENGINE, 0x40 DRIVETRAIN,
0x48 GEAR, 0x50 SUSPENSION, 0x58 LSD, 0x60/0x68 FRONTTIRE/REARTIRE, 0x70
STEER, 0x78 LIGHTWEIGHT, 0x80 RACINGMODIFY, 0x88 PORTPOLISH, 0x90
ENGINEBALANCE, 0x98 DISPLACEMENT, 0xA0 COMPUTER, 0xA8 NATUNE, 0xB0
TURBINEKIT, 0xB8 FLYWHEEL, 0xC0 CLUTCH, 0xC8 PROPELLERSHAFT, 0xD0 MUFFLER,
0xD8 INTERCOOLER, 0xE0 ASCC, 0xE8 TCSC, 0xF0 WHEEL, 0xF8 NOS, 0x100 WING,
0x108 SUPERCHARGER; +0 the car, +0x10 its VARIATION key from
CAR_VARIATION, +0x18 the colour); the settings at +0x110.. get the parts'
defaults (`func_004449C0`) and the gear ratios their automatic setting
when the gearbox has one (`func_00443240`). The rows come from the disc
(`src/core/spectables.c`: `/specdb/GT4_US2560/<TABLE>.dbt`, the
VARIATION and CAR_VARIATION tables in their `american` versions); the
tables the game has in memory are byte for byte the disc's files.

`tests/physics/setup_diff.py <disc image>` builds, for each of the 722
cars of GENERIC_CAR, the garage entry, the record and the parameter block
with the original code (on the snapshot's memory) and with the rewrite
(reading the disc): all 722 are identical in the three. The Ka built this
way gives the snapshot's own parameter block.

Checked with `SETUP_SEED=n`: the record's path-choosing fields (layout,
ballast, weights, aero, suspension, dampers, tyre counts and curves,
engine and turbo, gearbox, ratios, differentials, wheel sizes), the game
options the setup reads (D_00620CD8) and the race's power limit get
made-up values; the whole RAM after the native setup equals the
original's for 200 seeds. Divisions by zero give FMAX on the PS2 and
infinity on the host; the fuzz keeps the divisors that the game's data
never leaves at zero (front brakes, final drive, rim size) non-zero.
`SETUP_GARAGE=n` draws garage entries instead (other rows of each part
table, tuning levels, colour, race mode): record and parameters equal for
300 seeds, with the rows from memory or from the disc.

## The car at the race start

After the record, the race init calls `func_00363B10` for each car:
`gt4_car_init` (`src/core/physics/init.c`). It builds the parameters,
hooks the car to its race entry and course, copies the entry's wear and
damage to the wheels, sets the driving aids (`func_003F1890`: levels from
the game options, D_00622080 for the player, D_00622081 else), clears the
car to rest (`func_00363560`), finds where it starts and places it there
(`func_003630C0`: ground under the body and each wheel, wheel angles,
camber and track, the ground plane, attitude and height), and for a
rolling start sets it moving (`func_00363868`: body and wheel speeds, the
gear for the speed, engine rpm through the converter, turbo boost).

Where it starts is the start kind (t3): 0 the grid slot of its entry
(`func_003952C0`), 1..5 a rolling start `start` metres along the course on
the line picked for it (`func_0034C5E0`/`func_0046ADE8`: the line nearest
the grid, `func_0046B108`, or 6; each line has a fallback list at
D_00624030), 6/7 the start points of line 4 (`func_0046BEC0`), 8 the grid
with a delay. The driver model's start (`func_004639E0`) is native except
`func_00463C00`, which enters the AI's line code. Still the original: the
AI settings and profile (`func_00468EE8`, `func_00466C20`,
`func_00467148`), `func_00345400` and `func_003452F0`; they go with the AI.

Checked by running the race init (`func_0034D490`, up to where it loads
sounds) on a snapshot with the original `func_00363B10` and with the
rewrite (`setup_run` with `SETUP_REINIT=<course>`; `SETUP_KIND` forces the
start kind, `SETUP_POKE` sets race modes and aids): the whole RAM is equal
for every start kind, five courses, the race modes, and all 722 cars with
a rolling start (CVT and automatic gearboxes included). The exception is a
rolling start with no ground under the line (Suzuka, Nurburgring): the
original then takes the car's height from a stale stack word, the rewrite
from -FLT_MAX; both leave the car far below the course.

### The race init itself

`gt4_race_init` (`func_0034D490`) is native too. It sets the race mode,
then the course's settings from its COURSE row (`func_0034C7E0`: the key at
race+0x78; race mode, rule bytes, and the course code, a UnistrDB.sdb
string whose first characters become digits at conductor+0xCBE6), the
course AI settings (`func_00466B90`: the .ad file's block at course+0xA0
or D_00623DC0), the rule bits from the runway's flags (`func_00360348`),
the lines, and the race's shared tables (`func_003F0100`). Then, per car:
the car cleared and hooked to its race and session (`func_00343AB8`), its
record, its grid slot and start from the session (virtual calls +0xE8 and
+0xF0), `gt4_car_init`, the lowering some cars start with
(`func_003F2560`), four bytes of the GEAR row the car keeps at +0x1080
(`func_00385410`), and the first wheel rotation. What follows the cars
(sounds, replays: `func_003F1E60`, then `func_003F71A0` per car) still runs
the original.

The rows come from `gt4_specdb_src`, which now also gives UnistrDB strings.
The original's in-memory SpecDB keeps a cache of decompressed rows in each
table object (from +0x24): the rewrite reads the tables directly, so those
caches (GEAR and COURSE here) are left out of the comparison. Checked: the
start kinds, courses and race modes of the suite, 180 cars, and each of the
110 COURSE rows put at race+0x78.

### The session

The conductor is the race session: its virtual table (conductor+0x10140)
says how the race runs, one class per kind of race. The physics and the
race init reach it only through `gt4_session` (`session.c`), which runs
the Time Trial's methods (table 0x00685890) in C and the others through
the table: +0x30/+0x170/+0x180/+0x1A0 (1) and +0x128/+0x160/+0x178 (0),
+0x108 a car's distance along the course (`func_00358558`), +0x118/+0x120
a start the session gives (conductor +0xF86C/+0xF866), +0x130/+0x138 two
values of the course's checkpoint set and +0x150/+0x158 its entries
(`func_00395420`..: the runway's own set or a variant at course+0xB0),
+0x188 a car off the course (`func_0035FA98`), +0xE8 how car i is held at
the start (`func_0034CF30`), +0xF0/+0xF8 where it starts: a rolling start
from the course's AI settings, the session's own, or 5 m before the first
tight corner back from the lap's end on line 6 (`func_0046AB80`), +0x68
the walls (`gt4_walls`), +0x140 0xFF (no car against car in a Time
Trial), +0x08/+0xC0 nothing. Still the original: +0xC8 (the pad's
vibration), +0xD0/+0xD8 (the start and end of the race's other systems),
+0x20 (car against car). `gt4_session_orig` sends everything to the
original.

The race against other cars (the Arcade's table, 0x00679CE8) shares
many of these functions and is rewritten too (`race` in `session.c`).
Its setup: +0xD0 is `func_0034CA20` itself (the Time Trial's adds the
split times); +0xD8 (`func_003477C8`) is the common after-cars part
(`func_0034CD60`), the race flagged as a rolling start (cond+0xCBE4)
when the course's settings give one (cond+0xF884 metres back, +0xF886
speed, +0xF887 metres between cars) and the lap limits; +0xE8
(`func_0034CE48`) holds a player's car 0 (2 in race mode 4, 1 when the
world's +0xCC8 is 1 or 3) and the others 2 (4 in a replay, the world's
+0x298); +0xF0 (`func_00347618`) starts a car where the session says
(+0x118/+0x120), else, with the rolling start, `back + (cars - slot - 1)
* gap` metres from the line (kind 1 ahead, 2 behind) at the given speed;
+0x58 sets the catch-up bytes cond+0xF844..0xF84C by the race's level
(race+8, `func_00354F30`: 1 and 2 set them, else cleared); +0x60 and
+0x1A0 return 1. Checked by running the race init again on the six
Arcade captures and on copies with the level at 1, 2 and 3: identical.
The race worlds' methods that only return 0 (`func_00389DF0`,
`00389DF8`, `0038A3B0`) are answered without the original.

Also in C: the race clock (+0x78, `func_00357E80`: conductor +0xF898 and
+0xF89C, 50 a frame), the race order and positions (+0x18,
`func_00365BC0` with the comparison `func_003659E8`: laps done, distance,
finish times), the lap-time offset per car (+0x80) and the small ones
(+0x38, +0x40, +0x48, +0x90, +0xB0). The lap events (+0x98 sector, +0xA0
start, +0xA8 lap, +0xB8 finish, +0x70) mostly feed the presentation: the
race's event queue (race+0xF4, read by another thread: HUD, sound,
replay), the ghost (switched, re-stepped and recorded on each lap) and the
records. They stay the original, except with `gt4_phys_headless`, where
only the race's part runs in C: the lap start time (car+0x718), the best
lap (car+0x71C, `func_0035E3C8`), the car's flags, leaving the pit lane,
the finish. Checked: identical to the original for car 0 over 16 lap
variants; the one difference seen is the wheels' random shake
(`func_0035A678`), whose shared generator the skipped ghost step also
draws from. Ghost, records and the event queue are to be rewritten with
the game mode.

### What the step does besides the physics

Stubbing each original function the step still calls, one at a time, and
comparing 600 frames on the five snapshots sorts them out. Presentation,
which the physics never reads back: replay recording (`func_003F1148`,
`003F12B0`, `003F1368`), pad vibration (`func_0035BE60`), what goes to the
pad (`func_00354A78`, force feedback), the engine sound (`func_003F71C8`:
its state is the object at car+0x9A0, two components of 0x370 bytes), the
car the view follows (`func_0035F658`). With `gt4_phys_headless` set
(PHYS_HEADLESS=1 in the harnesses) the step leaves them out; the cars are
bit for bit the same outside car+0x9A0..0x1090. Physics or race state, to
rewrite: `func_003F4638` (the driver model's memory of the others),
`func_0034C0E0`. Rewritten on the way: `func_00355E10`,
`003F3368`, `00350308`, `003658D8`, and the message state of cars of
kind 1 (params+0x32; `func_0035CA60`, `0035D1D0` and what they call: the
state at car+0x788 with the message, its duration and pause from the
tables D_00620880/90/A0, the rival from the relation table at
conductor+0xCD7C+0x4C and the lap gaps `func_00366018`; checked with 80
variants with the car made kind 1 and the state drawn), and the relation
of each pair of cars (`func_004146E8`: a counter per pair and a state
0..3 moved on by what the race saw between them; checked with 80 variants
with states, counters and contact data drawn).

### Laps

`gt4_car_laps` (`laps.c`, `func_00359008`): each frame the car's distance
along the course (car+0x5A8, kept at +0x99C as the previous one) is
checked against the start line, the next sector line (session +0x158),
the course's timed points (`func_003C0098`), the timed start (+0x130) and
the finish (+0x138) (`func_00358608`). A crossing's time is the race clock
(conductor+0xF898) plus 50 times the fraction of the frame at which the
line was crossed plus the session's +0x80, in thirds of a frame; it goes to
the session (+0x90 sector, +0x98, +0xA0 start, +0xA8 lap, +0xB0, +0xB8
finish) and to the replay's lap records (`func_00359C10`, `00359B60`).
Going back over the line counts laps down (+0x4B0). Then the car's signal
lights (`func_0035ACD8`) and the pit lane (`func_0035B060`). Checked with
run_diff/fuzz_diff and 60 variants that put the start line, a sector line
or a going-back inside the next frame's move (car+0x5A8 moved, lap and
sector counters drawn).

The pit lane's marks are the points of line 4: the lane's entry and exit
at its first and last points, each car's box at point k (distance and
heading). The car's block +0x467 says where it is in the lane (2 going
in, 3 at its box, 4 going out), +0x4B8 what the stop will do
(`func_0035AF18`: tyres, fuel, set going as bits 0x40/0x80/0x100), +0x4BA
the stop's countdown (0x2D1 at the stop; at 0x2D0 the car is turned to
the box's heading; refuelling `func_00354D60` while it runs), +0x62E a
stop the race asks for. Stops are counted per car with a history of the
laps they were made on (`func_0035AD10`). A car stops only under 100 km/h
at 25 m before its box, else it goes through. Still the original: the
driver model's change of line (`func_00463C28`), the race events
(`func_0035E000`), and `func_00343D78`. Checked with 80 variants with the
previous distance drawn around the marks and the lane state, countdown,
plan and requests drawn: identical, the lane state changing in a third of
them.

Checked with run_diff.py and fuzz_diff.py (the step, identical) and the
race init suite with the session's own starts on six courses.

## Course data from the disc

What the step reads of the course is the runway and the driving lines.

- The runway is the RNW4 block of `/crs/<course>` (road and wall triangles,
  the kd tree, the course's segments, the grid, the length at +0x14),
  copied to memory and relocated (`func_003E5818`: the header's offsets,
  the entries of the tables at +0x58, +0x7C (the leaves) and +0x9C); the
  step reaches it at `[[course+4]+0xDC]`. `gt4_runway_relocate`
  (`src/core/physics/runway.c`). The Tsukuba runway loaded this way is the
  snapshot's word for word.
- The lines are the ADLN block of `/crs/<course>.ad` (offset at +4 of the
  file; the file's other four blocks follow it), at `course+0x9C`. At the
  race start `func_0046B0A8` makes it ready (`func_0046B248`: offsets to
  addresses, lines without points dropped, x and z negated, headings turned
  half round; then per line, in the order 6, 0, 1, 2, 5, 3, 4, 7: each
  point's ground height by rays, slope along and across, surface, and
  distance along the course (`func_003955E8`: the point in the quads of
  the course segments of its kd leaf, `func_003E6858`/`func_003E6610`),
  line 4 snapped to the grid slots near the start, arc lengths and grades;
  lines 1, 2 and 7 dropped when their distances wrap) and `func_0046B9F0`
  clears line 5's +0x38. `gt4_lines_setup`, `gt4_lines_clear` and
  `gt4_course_distance` (`src/core/physics/lines.c`); the step itself
  calls `func_003955E8` once per car and frame. The old line format (two
  gymkhana courses) still goes to the original.

`setup_run` with `SETUP_COURSES=1 SETUP_DISC=<disc image>` loads, for every
course on the disc with a .ad file (110), its runway and lines into the
snapshot's memory and runs the relocation and the line setup with the
original code and with the rewrite: identical for all 110. Two things the
comparison caught: the game's "largest float" (0x7FFFFFFF, D_006A39F8) is
a NaN on the host, and a Sarthe line point has a heading of 0xFFC00000,
which the PS2 reads as -FLT_MAX (its FPU has no NaN or infinity).

## Method

1. Resolve indirect calls (vtables without RTTI, function pointer tables)
   from the conductor's step down to the per-car update.
2. Split into subsystems (engine/drivetrain, tyres, suspension, body/aero,
   integration, collision with the runway road surface) and document each
   function's inputs, outputs and formulas here before rewriting it.
3. Rewrite each subsystem in C under `src/core/physics/`, with the same
   floating-point order where it matters (PS2 FPU rounding differs from
   IEEE: `src/ee/fpu.c` models it for comparisons).
4. Validate with the static recompiler as an oracle: run the original
   function (recompiled, `tools/recomp`) and the C rewrite on the same
   inputs and compare, then compare whole laps against PCSX2 recordings.

## Driving a snapshot (opengt4_drive, transitional)

`opengt4_drive <disc.iso> <eeMemory.bin> [Scratchpad.bin]` runs the step
at 60 Hz on the RAM of a savestate and draws the car on the course with
the native renderer (`src/platform/sdl/race_view.c`, apart from the menus'
showroom). The wheels are drawn as their own models: each gets the steer
(WHEEL_STEER, about the body's up axis), the rotation (WHEEL_ROTATION,
about the axle) and the suspension travel against the start's
(WHEEL_TRAVEL) of the physics wheel whose mount (WHEEL_MOUNT) is on that
corner of the model. The course collision is in that RAM, so the step
keeps the car on the road over the whole lap. The controller read
(func_00426D50, a virtual call of the driver's pad object) is replaced by
SDL: it returns the game's 32-byte driver command (flags: bit 0 steering
analog, 1 throttle, 2 brake, 4 handbrake; steering at +4 as -4096..4096,
throttle +8, brake +0xa, handbrake +0xe as 0..4096). The game's own
steering smoothing, aids and automatic gearbox (func_0036C160) run on it.
Position and rotation map from the physics frame (x, y, z up) to the
render frame as (x, z, -y); the model origin is put the car's initial ride
height below its centre along the body's up axis. The PS2 FPU mode is set
only while the step runs. `--auto N --shot out.png` holds the throttle for
N steps and saves a frame (from the snapshot at rest: 70 km/h after 10 s,
first to second gear at ~6300 rpm).

`--car <code> --course <name>` (GENERIC_CAR code, /crs name) now change the
physics as well as the drawing: `tests/physics/race_setup.c` loads the
course's runway and lines from the disc into the snapshot's memory (the
runway at its own offset in the course file, where the game has it, the
.ad after it; the course object's +0xDC and +0x9C..+0xAC point to them;
put where the file's models would be, the race init's allocations wrote
over the runway), builds car 0's garage entry with `gt4_garage_new`, and runs the
game's race init (func_0034D490; the record, parameters and lines are
the rewrite) to its end. Stopping at func_003F1E60, as the physics
checks do, would keep the snapshot course's wall outlines: on another
course the car then drives through the walls and out of the world. The time trial's rolling start is 10 x byte 0x19 of the
course's .ad settings metres before the line, at 5 x byte 0x1A (func_0034D288),
on line 6 (the run-up road). It matches a PCSX2 capture of the Nurburgring
start. On 94 of the 110 course files the car starts so; the others (Photo
Travel places, gymkhanas, a test course) have no Time Trial and no ground
there: the demo then places the car on its grid slot (func_003630C0), or
on the nearest driving-line point with ground, a standing start. Launchers such as
`build/Dirigir R34 Grand Valley.cmd` pick a car and a course. Still
from the snapshot: the session (Time Trial), the second car, the
controller and camera objects.

## Out of the snapshot

`opengt4_drive <disc.iso> - --car <code> --course <name> [--elf core.elf]`
(and `physics_run - - ...` with `PHYS_BUILD=<core.elf>`) run without a
savestate. `tests/physics/race_build.c` loads the executable's image (its
loadable segments, bss zero) and makes the few objects the setup and the
step read before building the rest themselves: the conductor (the Time
Trial session: its virtual table, per-car bytes, race clocks, the race
order), the race (COURSE key, entry list, course, "world" object, no
records), the two garage entries and their driver profiles, the course
object (where its runway and lines go), and car 0's header. Two globals
too: the display units (0x6244D8..0x6244E8) and the random generator's
state (0x874D50). The objects sit at the addresses they have in the Time
Trial snapshots, so both runs can be compared word for word.

Which words those are was found by delta debugging: RAMs made of the
image plus a part of a snapshot, set up and stepped, compared with the
same on the whole snapshot; the part was cut down until nothing more could
go (first by regions, then by words of the heap).

A built race needs the whole race init, not the shortcut the snapshot
runs take (stopping at func_003F1E60): what follows the cars
(func_0034CD60, from session +0xD8) makes the walls' outlines, which hold
each car's pointer (conductor+0xDA78+i*0x4F0+0x450), the relations of the
cars (func_00414660), the positions, and the pit tyres. The pit tyres
(func_0035C4D8) read the car's parts file from the disc through the IO
thread; `gt4_pit_tyres` does the same from `/specdb/.../CARS/<car>`
(`car_parts` of `gt4_specdb_src`): the front and rear tyre kinds of the
garage entry, their range of compounds (0x620860/0x620870) and the parts
of those compounds. The SpecDB rows come from the disc as well. The step
then runs headless (no sound, replay or vibration objects exist).

Checked: the built race against the snapshot one, both with the whole init
and headless, bit for bit on car 0 over 1200 steps with made-up input, on
Grand Valley, Laguna Seca, Tsukuba, Suzuka, Nurburgring and Deep Forest
(since the runway moved to its file offset, all with the rolling start). The drive demo gives the
same numbers either way. The snapshot shortcut differs from the whole init
once the car reaches a wall: the shortcut keeps the snapshot course's
wall outlines.

Still made by hand rather than by the game: the race's objects above (the
game mode, ported later, will build them), the second entry (a ghost slot
with an empty garage), and the Time Trial session itself.

A race against the AI (the Arcade's session) is built the same way
(`phys_race_build_arcade`; `PHYS_ARCADE=<cars>` with `PHYS_BUILD`,
`opengt4_drive --arcade [--ai code,...]`), at the addresses of the
Arcade captures. The words it needs were found on `ar_02` by splitting
top-down by address (`build/physics_cmp/delta.py`, local: 118 tests at
16 KB, then to single words): 40 of them. The display units, the
random generator's state and a controller object's table; the race's
entry list, COURSE key, course and world (whose table, 0x0067BAD8, makes
it a RaceArcadeSingle); per entry its driver profile, grid slot and
+0x347C; the profiles' table (the AI's shared profile 0x00622B60 is in
the executable's data, only its table set at run time) and the player's
+0x1CC; the conductor's race, car count, session table and +0xF864. The
player's car is entry 0 at the back of the grid, the AI's in slots 0..n-2.
The course object's data pointer (+4) was not needed on the capture only
because the course is loaded again; the build sets it. `race_setup`
takes the other entries' cars too (`phys_race_other_cars`,
`PHYS_CARS=id,...`): garages made from the disc give the same race as the
capture's. Checked: the built race against `ar_02` with its init run
again, all six cars bit for bit over 1200 steps.
