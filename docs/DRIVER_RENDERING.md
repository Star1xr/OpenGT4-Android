# Native driver rendering: evidence and current status

2026-10-02, NTSC-U SCUS-97328 v2.00. **Race seating and steering animation are
integrated for ordinary and classic pilots, including reduced builtin heads.
Ordinary pilots also change gear, operate the pedals and close/release their
fingers using authored hand morphs. The complete original controller, special
pilots and replay posture remain pending.** The race
adapter is a partial source-based port; its complete output has not yet been
compared with the original wrapper. The separate character preview uses the
authored bind pose and does not establish PCSX2 visual parity.

## Implemented and verified

- Bounded CARINFO driver type, crew and geometry parsing; builtin MDLS offset
  validation. The geometry supplies the steering wheel, feet, seat, chair
  angle, gear lever and handbrake, rather than model-name-specific positions.
- Native selection from `003B5438`, including builtin precedence, the three
  normal driver variants, classic drivers, Nike and Motor Triathlon crew.
  The caller still owns the original global variant cycle.
- Ordinary A-spec roof selection (`003882D4..003882F4`): roof type 1 opens
  with fewer than three entries; roof type >=2 always opens. The race loader
  reads `/car/open` or `/car/lod` accordingly. This does not implement the
  separate B-spec/headless conditions. An S2000 with six entries therefore
  remains closed, as the original branch requires.
- MDLS published bone names, dense palette slots, inverse bind matrices,
  rigid model ranges, `SKINNING`, `CRANIUM2`, body dimensions and LOD extension.
  Evidence: `003B0C80`, `003B0D18`; detail class: `00459178`.
  Reduced builtin pilots may have rigid bone symbols and dimensions with
  zero bind slots and no `SKINNING`; these are now accepted and rendered.
- Compressed rigid positions use the scale at each shape's +4, matching
  `0049D328` -> `0049E548`. The original scales the matrix's three columns
  and preserves translation. A driver2 helmet uses 5/32767; applying the
  former universal 1/4096 made it about 60% too large. Compressed S2000
  cabin shapes also use their authored scale. Float-position and zero-scale
  legacy streams retain their existing decoding paths.
- Authored smooth normals are preserved from compressed signed V3-8 streams:
  `8080` for lit shapes, `8040` for reflection shapes. The former decoder only
  accepted float normals and discarded these car streams. Its face-normal
  fallback exposed every triangle on the S2000 body and environment pass.
  The corrected capture uses the same triangles, draw order and vertex layout;
  decoding happens once, with no additional per-frame smoothing or polygons.
- Shape kind 4 (`0049D1A0` -> `0049EB90`) is a specular-only material pass.
  Both GPU and CPU showroom rendering omit ambient/diffuse from that pass,
  and GPU grouping distinguishes it from ordinary lit shapes. Re-lighting
  the S2000's texture-9 additive mask had whitened its dark texture-8 seats.
  Texture content and UV coordinates were retained.
- Skin coordinates and four influences after the VIF packet, which the static
  decoder previously omitted. Evidence: `0049DA60`, `0049D970`, `0049DCB0`,
  `0049DB08`, `0049D728` and `0049D580`.
- Quantized weights multiply by `0x3C010204` (1/127), as VU0 microprogram
  `0x2E8`/`0x460`; float path `0x308` preserves authored weights. The VU0
  microcode was inspected locally in an existing PCSX2 savestate. Weights are
  **not renormalized**: real assets include sums different from one.
- `003B1248` palette composition: pose times inverse bind, mapped from the
  published skeleton order into MDLS slot order.
- Native dimension-based neutral skeleton (`00419F80` -> `00419D80`) and
  quaternion pose matrices (`00418A58` -> `004875B0`), expressed as authored C.
  The 2% scapula plus 48% clavicle construction preserves the original
  operations rather than folding to 50%.
- Native `003B1DB0` control update for steering, lever rotation and hand grasp. This
  consumes caller-supplied control channels and does not read physics memory.
  DriverPosture control type is distinct from CARINFO's character type.
  `003B21D8` lever displacement now follows the recorded gate rotations for
  ordinary and classic control types except type five. Foot compression uses
  separate throttle, brake and clutch channels.
- Native ordinary hand controller (`0040FC78`), including regrip states,
  inter-hand coordination, lift and the eleven-sample velocity envelope
  (`004145C8`, table `00622A20`). Original gates (`00411EB0`), lever movement
  (`00411628`) and ordinary gear states (`00412018`) drive reaching for the
  gear lever, shifting, waiting and returning to the wheel. Rates, springs and
  phase advance use the simulation timestep rather than the render clock.
- Classic steering controllers (`0040F850` Patent, `0040F888` Carriage),
  including their distinct steering bounds and spring. Patent's throttle/brake
  lever (`00413FC8` / `00413F68`) and the type-five pedal lever (`00414130`,
  input provider `003446A8` at car+598) are ported. Carriage's separate gear
  controller and the type-five arbitrary-axis lever target are still pending.
- Native two-bone arm/leg IK (`00420510`), three-segment spine IK
  (`00420AA8` / `00420190`), lower-body posture (`0041E678`) and torso
  posture (`0041CB20`), including reach flags, locked torso twist and optional
  look target. Upper-body posture (`0041D140`) covers automatic clavicle
  placement, elbow guides (`0041C6F8`), wrist aiming (`0041C458`), endpoint
  correction and the separate simple-hand-twist path. SDK trigonometry is
  ported locally with the original constants and operation order, including
  the integer square root (`0059F050`) inside arcsine. Head/neck (`0041EFE0`)
  reproduces the look cone, separate yaw/pitch limits, roll relative to up and
  neck-yaw lock. Full posture dispatch (`0041F560`) uses original defaults
  (`0041C1C8` / `00418F88`) and restores the caller's rounding mode on success
  and failure. Native vehicle target construction (`004173B0`) now positions
  the body and feet from the descriptor, adjusts shoulder reach and wrist
  orientation, and applies the local-to-world transform. Virtual hand targets
  (`00415D38`, `00415510`, `004154A0`, `00415648`) reproduce wheel grips,
  gear-lever travel, rim clearance and resting/free hands. These APIs consume
  authored geometry and controller channels. A partial `003B2708` adapter
  and ordinary/classic race records are integrated. Complete special control
  types, secondary phase offsets, vehicle inertia and a full independent
  wrapper comparison remain pending.
- Optional GPU skinning, with a resident VBO/textures and a 20-matrix palette.
  Static geometry keeps its 19-float vertex layout; animated geometry adds
  eight floats. Animated batch bounds cover affine bone transformations and
  the actual weight-sum range, including rigid vertices in a mixed batch.
- Race shared characters are decoded once per file on the worker, uploaded
  once on the GL thread and reused by separate vehicle skeletons. Rigid parts
  share textures and follow their bone pose. Car factory lift/pitch and world
  pose apply to body and driver together. `race_view_set_driver_frame` accepts
  original 64-channel human frames and updates IK/palette without rebuilding
  vertex buffers. `race_game` now supplies live, read-only steering, gear,
  throttle, brake, transmission and shift-anticipation inputs for every entry.
  Each actor retains its controller between simulation steps. IK and palette
  construction run once per rendered frame for drawable actors; hidden cars
  retain the newest pending channels. This does not change car geometry or LOD.
  The original clutch-blocking predicate needs its primary-vehicle/local-speed
  provider; the current adapter supplies false instead of inventing that input.
- Steering wheels now move with that same retained channel +34. CAR4 callback
  8 (`00453698`, jump table `006AA8E0`, branch `00453758`) applies local
  `RotateZ` through `004A79D8`, converting radians with `0x42652EE1`.
  The decoder retains each authored callback matrix; race loading splits its
  geometry into local coordinates once. The GPU shares the car textures and
  changes only the steering matrix, composed with the same factory body
  pitch/lift as the pilot. Panel/body triangles are excluded from this turn.
  This also works when a normal car has no drawable pilot. Static showroom
  callers keep their complete, neutral car geometry.

The decoder's ordinary human mode uses VM branch zero/base morph for bind
diagnostics. Race hands recognize the asset's bounded grasp expression and
retain its two endpoints; other material VM expressions and blinking remain pending.
Quantized normal scaling is immaterial to subsequent normalization, but this
shader does not establish bit-for-bit PS2 VU arithmetic or rasterization parity.

Validation: all 20 CTests passed; the real GL probe verified the specular-only
mask over an ambient-lit surface, plus four distinct
influences including slot 19, moving off-screen geometry into view, animated
culling and a following static draw. CPU tests cover 100,000 transformed box
cases and 10,000 blends with non-unit weight sums. Real-disc decoding verified
all eight shared rigs, skin bodies and rigid parts, plus every builtin pilot
MDLS in the 722 catalog menu/LOD entries: zero conversion failures. All eight
shared models were captured through the GPU bind diagnostic. A separate
original-code oracle compared 20,340 skeleton bones and 20,340 quaternion
matrices (absolute tolerance 0.000002), plus 51,456 control updates with all
output fields bit identical. Oracle code is generated only into ignored local
build outputs and is never linked to the application. Further comparisons
verified 10,000 two-bone IK cases, 10,000 three-segment spine cases, 1,000
complete lower-body postures, 5,000 torso postures, 10,000 elbow guides and
5,000 complete arm/wrist postures, 5,000 head/neck postures and 1,000 full
posture dispatches, including all bone transforms, mutable controls and
relevant reach flags. The full dispatch comparison covers all 16 enable
masks and alternates the caller's rounding mode between nearest and upward;
the native scope restores each mode. After porting the SDK integer
square root, the maximum absolute difference in each of these runs is zero.
Another 10,000 comparisons cover all virtual hand target types, both sides,
normal modes, gear-phase travel and output flags. Another 2,000 comparisons
cover the complete vehicle target builder, all enable masks, body/foot/hand
targets and mutable wrist/clavicle controls. Both have maximum error zero.
The vehicle descriptor's wheel basis is nine packed floats (12-byte column
stride), despite the surrounding 16-byte vector fields.
Animation comparisons added another 66,000 original-code cases: 12,000 modern
hand updates, 12,000 classic hand updates, 12,000 lever updates, 10,000 ordinary
gear updates, 10,000 Patent lever updates and 10,000 type-five pedal-lever
updates. These cover all respective states, both hands and the transmission
gate kinds. The gear oracle supplies the three vehicle anticipation predicates
and wheel/lever positions through small fixture callbacks; it does not verify
the whole physics-input adapter or original race wrapper. The Patent comparison
covers live mode, not the recorded-frame branch. Native integration also tests
1,200 successive steering/shift/pedal steps, finite solved bone transforms,
invalid-input rollback and restoration of the caller's floating-point mode.
The diagnostic's `ee_init` sets floating-point rounding toward zero, shared
by the original functions and the native calls during comparison. The
complete native posture solve now scopes that mode independently. These oracle runs
replace only the four decoded VU0 cross, quaternion-product, affine and add
helpers because the diagnostic EE runtime has no VU registers; scalar
solver code and SDK trigonometry execute from the original disc. They do
not establish bit-for-bit VU hardware parity.

The authored-scale correction also decoded all 807 `/car/menu` CAR4 files
without failures. The catalog audit now reads builtin rig metadata, including
zero-slot rigid heads: all 722 entries and eight shared rigs passed. Native
grid captures show the S2000 '99 open with a seated driver and corrected head
scale/dark seats, and Patent/Carriage with their classic pilot. A Ka capture
verified that its reduced builtin pilot loads (it is mostly occluded by the
closed car). These are integration checks, not an original-wrapper oracle or
a complete PCSX2 motion comparison.
The animated path was captured over 360 successive controller/render steps
for S2000 '99, Patent and Carriage, plus an actual autopilot Caterham race.
Arms and the ordinary gear hand move. The subsequent finger-grasp captures
also show the fingers closing on the wheel and releasing during movement.
Those diagnostic sequences include PNG readbacks and are not game-performance
benchmarks. A synchronized comparison against original PCSX2 motion is pending.
Steering extraction was audited independently across 807 menu CAR4 files,
77 open CAR4 files and 807 LOD CAR4 files: zero decoder/extraction failures.
They contain 27, 75 and 13 callback-8 frames respectively, all with rigid
authored bases. Neutral reconstruction differed by at most 0.00000011921 metres.
The steering regression checks an oblique pivot, both turn directions, callback
branch selection, distinct frames, unchanged body vertices and invalid angles.
The Release application and all 21 CTests passed; additional S2000, Patent and
Carriage sequences show the steering parts turning with the driver controls.

Current probe measurements at 640x448 (300 frames, ten warm-up frames,
VSync off, `glFinish` included) were 10.208 ms/frame for the S2000 '99
Tsukuba grid with its shared pilot (934 draws, 45,516 submitted triangles),
and 11.137 ms/frame for Focus RS studio (162 draws, 12,659 submitted
triangles). These include present and were not a controlled before/after
comparison; do not infer a speedup or regression against earlier probes.

The earlier S2000 '04 grid benchmark (before race driver integration) measured
2.012 ms/frame over 300 frames after 10 warm-up frames, VSync disabled and
`glFinish` included: 981 draws, 51,643 submitted triangles, 2,267 culled groups.
This is one scene measurement, not an animated-driver performance claim or
a controlled comparison with the older Ka benchmark.

## Finger closure (2026-10-02)

The original host methods `003B09C0` / `003B09D0` read HumanModel +690/+694,
published from recorded frame +80/+84 by `003B3FB4` (`00385918` / `00385928`).
These are distinct from +C8/+D8, which control lever/wrist posture. Native
`004104B8` and its table `006A4630` release or tighten the grasp using hand
state, gear phase, and the rate 5 from `0044F5A0` / `0044FAE0`. `004113B8`
selects the gear-side hand; type five follows `00411570`, keeping the right
hand closed on the lever. The integrated controller covers the existing
ordinary/classic states; Carriage's separate gear phases remain pending.

Each hand MDLS callback computes `x = leftGrasp/rightGrasp * scale`,
`branch = (int)x`, `tween = x % 1`. Its authored scale is 0.999899983; a
bounded recognizer reads and validates that expression and the registers of
commands 76/77. With grasp 0..1, the selected branch is always zero. Shapes
with +8 = 0x41 carry two position streams at VU 00/20, two normal streams at
80/A0 and two UV streams at 40/60. The decoder formerly rendered endpoint
zero only. It now validates both streams, preserves their matrix/winding,
and uploads both endpoints once. The GPU interpolates positions, normals
and UVs using the individual actor's retained grasp; bounds contain both
endpoints. Textures remain shared, with no per-frame mesh conversion,
allocation or vertex upload. Each 97-triangle hand adds 9,312 GPU bytes;
static car/course vertex formats retain their existing size.

Verification:

- All twelve hands in driver/driver2/driver3/driver4/classic1/classic2 decode
  with finite moving endpoints, 97 triangles per hand. Nike has static rigid
  hands and no grasp program; Motor Triathlon has no separate rigid hands.
  Their existing asset paths are preserved, not assigned invented poses.
- `driver_model_tests` checks an independent synthetic tween with mirrored
  winding, translation, both normals/UVs, malformed VM/offset/host inputs
  and every truncation. Supply `extracted/driver` to also audit shared assets.
- `driver_pose_oracle`: **17,920 grasp updates bit identical** to original
  `004104B8`, exercising hand states 0..13 and gear states 0..39, zero/0.95/
  random phases and 60/120 Hz steps. **E** applies to this isolated scalar
  control, not the full wrapper or the final rendered image.
- `driver_skin_probe`: real GL pixel checks for endpoints, intermediate
  ratio, conservative bounds, shared mesh/actor ratios and static isolation.
  Negative controls `--finger-negative` on the scalar oracle and environment
  `OPENGT4_PROBE_MORPH_NEGATIVE=1` on the GPU probe both fail as intended.
- Release build and 21 CTests pass. Native 360-step S2000/Patent sequences
  are under `extracted/driver/grasp-final-*` and `grasp-patent-*`; the S2000
  sequence is `s2000-finger-grasp.gif`. Rendering remains **A**: OpenGL
  normal interpolation/rasterization is not a PS2 VU/GS equivalence proof,
  and a synchronized PCSX2 image comparison is still pending.

## Catalog audit

`gt4driver_survey` resolves every GENERIC_CAR label through SpecDB, then reads
menu, open and LOD CAR4 files independently. Availability of an open asset
does **not** prove the original race will choose it in every configuration.
The native path rule in `004541E0` chooses menu/open/LOD; world creation around
`00388244` supplies the roof/detail conditions; ordinary A-spec roof selection
is now integrated as described above.

There are 57 catalog cars with open assets. 56 select a shared pilot in that
asset; the Chaparral 2J open CARINFO has type zero. Its menu/LOD files do carry
a builtin pilot, so type zero is not proof that no driver appears in game.

| Open driver type | Cars | Shared model |
| --- | ---: | --- |
| 1 | 40 | driver2 / driver / driver3, by entry and variant |
| 5 | 2 | classic1: Patent Motor Wagen and Motor Carriage |
| 6 | 11 | classic1 |
| 7 | 1 | classic2: Model T Tourer |
| 10 | 1 | driver_nike |
| 15 | 1 | driver_mtr or driver_nike, by crew/entry |
| 0 | 1 | none selected from open CARINFO |

All 57 open labels, in catalog order:

```text
ssr_03 beat_91 s500_63 s600_64 s800_66 cobra_67 sl500_02 sl600_04
tf160_03 beat_f_92 beat_z_93 tamora_02 z4_30i_03 elan_s1_62 mgf_vvc_97
mtr_gmv_04 prowler_02 s2000_a_99 miata_rs_00 miata_us_89 nike_one_04
s2000_us_01 s2000_us_04 sl55_amg_02 sl65_amg_04 tank_car_03 _206cc_20_01
ac_427_sc_66 miata_18rs_98 miata_j_lim_91 miata_sspl1_95 miata_vspl2_93
s2000_typev_03 slk230_komp_98 barchetta_gd_00 griffith_500_94 miata_j_lim2_93
miata_sr_lim_97 miata_vr_lim_95 caterham_7_fb_02 fairlady_2000_68
miata_1800_rs_04 mr2_spyder_us_99 spider_duetto_66 _350z_roadster_03
lexus_sc430_us_01 miata_1600_nra_04 model_t_tourer_15 s2000_typev_us_00
s2000_typev_us_01 copen_activetop_02 motor_carriage_886 mr2_spyder_v_us_02
alfa_spider_30v6_01 chaparral_2j_race_70 patent_motor_wagen_886
corvette_convertible_54
```

LOD metadata: 716 builtin pilots, two shared (Motor Triathlon/Caterham) and
four absent (Prowler, Tank Car, Patent and Carriage). The last four acquire
their shared pilot through the native open-file selection.

## Required next work

1. Complete head/LOD/crew selection and open-file paint handling.
   Open CAR4 main paint-patch offsets are zero; blindly using the menu loader
   on open files does not reproduce the original material overrides.
2. Finish the race adapter as authored native C. Skeleton
   construction from 16 MDLS dimensions is implemented and verified, but its
   neutral pose is not a race pose. Torso, legs, arms/wrists, head and the full
   dispatch and target construction are ported and independently compared.
   `003B2708` builds normal race posture (ordinary and classic animated channels
   are now integrated, with the partial adapter described above),
   `003B4040` the recorded posture. `00416FD8` calls `004173B0` and `0041C3F8`.
   `0041F560` dispatches torso (`0041CB20`), arms (`0041D140`), legs
   (`0041E678`) and head (`0041EFE0`). Do not link the analysis EE runtime or
   generated recompilation into the game, or invent intermediate poses.
3. Complete the original controller beyond the integrated ordinary steering,
   gear and pedal states: Carriage gear/handbrake phases (`00412C78`), other
   handbrake movement, head/body inertia, primary-vehicle clutch blocking and
   other hand-grasp providers. The type-five lever controller is verified, but its separate
   axis rotation (`003B1FA8`) and secondary hand target remain pending. Physics
   has a separate owner: keep presentation integration read-only.
4. Preserve native head/LOD selection, other VM expressions/blinking and crew rendering
   in the cached race character path. Nike and Motor Triathlon seating remain
   unsupported by the race adapter; their original assets decode correctly.
5. Compare stopped, turning, shifting and replay frames against PCSX2 for
   S2000, Patent, Carriage, an ordinary builtin driver and special drivers.

## Reproduction

```powershell
cmake --build build --config Release --target opengt4 gt4driver_survey driver_skin_probe
ctest --test-dir build -C Release --output-on-failure
build/Release/driver_skin_probe.exe
build/Release/gt4driver_survey.exe "<user disc.iso>" > "<local audit.csv>"
build/Release/driver_skin_probe.exe "<user disc.iso>" /character/classic1.mdl "<local bind.png>"
```

The standalone original-code comparison also builds when the repository's
existing `OPENGT4_RECOMP_TESTS` option is enabled:

```powershell
cmake --build build --config Release --target driver_pose_oracle
build/Release/driver_pose_oracle.exe extracted/core.bin
```

Research, original assets, CSV and screenshots stay ignored under
`extracted/driver/`; only authored readers, shaders, tools and tests are tracked.
