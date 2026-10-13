# Native selection showroom

2026-10-02: race pilots now close/release their fingers from the original
recorded grasp channels and authored MDLS hand tween endpoints. Both endpoints
stay resident on the GPU; each actor supplies its own grasp value while sharing
meshes/textures. The scalar grasp controller is verified bit for bit in 17,920
original-code cases; rendered-image parity remains pending. See
`docs/DRIVER_RENDERING.md` for evidence, coverage and reproduction.

2026-10-02: CAR4 callback 8's authored pivot/tilt is preserved for steering
animation in races (`00453758` -> local `RotateZ`). The car stays complete
in static showroom callers; race loading splits the steering part once and
shares its textures with the body. Wheel rotation uses the pilot's retained
steering channel, including Patent/Carriage's classic controllers. All 807
menu, 77 open and 807 LOD CAR4 conversions passed, with no steering-part
extraction failures; see `docs/DRIVER_RENDERING.md` for coverage and limits.

2026-10-01 follow-up: native whole-inch/tenths diameter decoding restores
the Patent Motor Wagen and Motor Carriage wheels. Auxiliary model 1 is no
longer composited as opaque bodywork over open S2000 cabins. The shared
showroom/race loader was audited across all 722 catalog labels; every car
loaded with wheel geometry and finite positions. See `docs/RENDERER.md`.

2026-10-01: wheel-frame callbacks now restore authored brakes, including the
Focus; calipers steer with suspension without spinning with tires. Tire
normals follow radial deformation. The shared GPU renderer uses filtered
textures and a GPU studio shadow. See `docs/RENDERER.md` for evidence,
measurements and remaining approximations; the bring-up history below records
earlier limitations.

The frontend converts CAR4/MDLS data from the user's disc to ordinary triangles
and RGBA textures. It does not execute EE/VU code. Original implementation,
with layouts cross-checked against Nenkai's MIT PDTools and file templates:
- https://github.com/Nenkai/PDTools/tree/master/PDTools.Files/Models/PS2
- https://github.com/Nenkai/GT-File-Specifications-Documentation/blob/master/Formats/PS2/GT4/GT4_CAR4_CarInfo.bt
Research checkouts and extracted data remain in ignored build/reference and
build; no game assets are distributed.

## Data

GENERIC_CAR label -> CAR_VARIATION_american ID -> VARIATIONamerican rows gives
the model filename, paint order/name and colour chips. Variation rows can share
an ID: gt4_specdb_row_at preserves every entry rather than repeating the first.
Rows for the same model are sorted by VarOrder. Model data comes from
/car/menu/<ModelCode>. CAR4's main model is at header offset 0x18; Pat0 at 0x1c
contains colour count, patch count and a colour-major offset table. A patch is
destination offset, byte count, replacement bytes relative to the main MDLS.
Bounds are validated before applying any patch to a private copy. VarOrder is
one-based. The patch changes original materials/palettes, not a uniform tint.

MDLS shapes contain bounded vertex/UV streams and strip restart descriptors.
The converter accepts float and signed fixed-point positions/UVs, materials,
vertex colours and texture-table selection. Static default model branches are
followed with a command budget; unknown commands are counted. Environment shapes retain the external texture sentinel (-2); mask-only
shape kinds are retained. Main car presentation uses model 0; shadow
models are not drawn as opaque geometry. Texture sets use the existing Tex1
reader. Shape indices are never assumed to be brake discs: the MP5 uses indices
0..3 for bodywork. Dynamic brake callback transforms remain incomplete.

Wheel and tire MDLS parts are loaded from CAR4 offsets 0x20/0x2c/0x30. Sizing
uses DEFAULT_PARTS -> CHASSIS/RACINGMODIFY and FRONTTIRE/REARTIRE -> TIRESIZE.
The current placement approximates axle positions as +/- half the wheelbase,
with authored tread and physical rim/tire dimensions. Original suspension and
wheel callback transforms remain to be reproduced; this is not exact alignment
for every car. The visible face is oriented outward for both wheel/tire sides.

The preview currently uses a neutral studio backdrop and native three-quarter
camera. `/menu/select/bg.mdl` was inspected but is not a verified match for this
Arcade screen and is not currently drawn. Original camera/light motion files
are not yet interpreted. Body rotation is elapsed-time based.

## Presentation and ownership

A persistent worker owns a separate ISO cursor, reads tables/assets and decodes
CPU geometry/textures. New requests replace obsolete pending requests. The
render thread owns uploaded textures; old CPU assets are disposed on the worker.
Only active/pending/retired assets are kept, not an unbounded catalog cache.
The last view stays visible while loading and crossfades after replacement.

The current backend is a 640x448 native software rasterizer: near-plane clipping,
perspective-correct UVs, bilinear texture sampling, depth buffer, authored
vertex normals and material lighting. SDL uploads reuse a texture. GPU 3D
rendering, full original blend/mask state and exact colour-management remain pending; lighting/transparent glass
and some dithered textures visibly differ from PS2 output. This is an initial
3D presentation implementation, not complete graphical parity.

Enter toggles colour browsing; Left/Right choose paint; Enter/Escape exits that
mode. C/V also cycle colours. Race entry remains unavailable. --color N selects
a zero-based preview variant for screenshots. --input-interval N renders between
scripted navigation commands, including during script sync waits.

Validation includes synthetic malformed geometry/strip/patch bounds, duplicate
SpecDB IDs, eased strips, audio attach/detach, real-disc Ford/Toyota/BMW menus,
colour captures and rapid course/car changes. The renderer prints measured CPU
mean/max at shutdown; those figures exclude vsync and are not total-frame FPS.


## Reference showroom follow-up (2026-09-27)

The camera now starts at a front three-quarter view with a larger car framing.
The room MDLS's currently unsupported material/blend passes produced opaque
obstructions and a dark floor. The preview therefore uses an explicitly
provisional neutral studio backdrop and approximate contact shadow, and no
longer loads the unused room geometry. This is not the original 3D environment.
Original room passes, reflection mapping (including the CarFace env0 texture),
vertex-normal lighting and exact wheel attachment transforms remain unfinished.
The original car geometry, paint variants and manufacturer logo remain in use.


## Native material/reflection work (2026-09-27)

- Decode bounded float3 normals from C040/C080 streams. Reject nonfinite normals.
- Preserve material ambient, specular and shininess, alongside diffuse colour.
  Normal transforms for wheel instances use the inverse transpose of scale.
  Diffuse alpha is deliberately not treated as opacity without enabled blend
  state: doing so incorrectly removed tire geometry during investigation.
- Apply the three CarFace light directions/intensities and ambient value.
  Vertex lighting interpolates perspective-correctly across triangles.
- Render type-2 external-texture geometry as a separate environment pass, using
  the original `image/arcade/car/env0.png` from CarRoot.gpb and spherical mapping.
  This is actual disc geometry/texture, not an AI-generated reflection image.
- Decode individual model indices. Car model 2 supplies the original ground
  shadow geometry/texture; render it before the body. Wheel/tire loading selects
  model 0 instead of accumulating every model variant.
- Environment/specular strengths and shadow opacity are provisional native
  parameters (0.24/0.18/0.55). Full destination-alpha masks, ordered GS blend
  equations, camera matching, exact wheel callbacks and studio parity remain
  incomplete. These changes improve the preview but do not establish PS2 parity.

Tests cover normals/material preservation, invalid model indices, NaN rejection,
mirrored nonuniform attachment transforms, existing bounds/patch cases, and
real-disc navigation/captures for Ford, Toyota and BMW. Screenshots under build
are local verification artifacts and contain original assets from the user disc.


## Ordered native draw states (2026-09-27)

Conversion now emits each shape at the point it is requested, preserving repeat
submissions and the order of base, alpha-mask and environment passes. It records
colour channel masks, blend selectors/fixed alpha, depth test/write enable,
source-alpha test and destination-alpha test per draw. Attachments preserve state.
The software rasterizer applies the authored blend expression `(A-B)*C+D`, keeps
an independent destination-alpha buffer, and supports mask-only geometry instead
of dropping kind 0. The provisional 0.24 reflection multiplier is removed.

Remaining limitations after the fixes below: authored depth bias, exact
suspension/wheel transforms, original camera motion and full scene reconstruction.
The preview still uses the neutral studio background and provisional specular
strength/shadow opacity. It is not claimed to match every PS2 render state.


## Opaque bodywork and stable reflections (2026-09-27)

Fixed the missing Midget MP5 body: an earlier blanket exclusion of shape indices
0..3 was invalid. The converter now retains those draws for every CAR4.

The VIF packet's GIF primitive ABE bit now controls whether blending happens.
The blend equation alone does not enable it. Opaque paint can write its RGB
while storing a lower alpha as the strength of a following reflection pass;
previously it was incorrectly blended with the background. Tex1 now retains the
original coefficient bytes (128 = unity, values above 128 remain intact) beside
its unchanged UI RGBA image, plus TCC/TFX metadata. Alpha is quantized before
destination tests so interpolated 127.99999 does not randomly lose bit 7. This
removed the speckled windshield mask observed on the Toyota Sports 800.

Authored culling, depth comparisons and alpha-fail actions are retained. Mirrored
wheel instances reverse triangle winding, and a zero-tread axle gets one tire
shell rather than two coincident mirrored shells (MP5). Both outward rim faces
are kept. Half-open raster edges avoid blending shared strip edges twice.
Read-only coplanar passes tolerate 1 mm of depth difference between separately
stored base/reflection geometry; this is a native raster tolerance, not an exact
reproduction of the original depth quantization/bias.

Environment mapping interpolates and normalizes view-space normals per pixel,
uses the perspective viewing direction and bilinear filtering of the original
environment map. The camera now uses Arcade CarFace's 25-degree vertical FOV,
translation (0, .98, 9) and -3-degree pitch. Its starting model angle and automatic
orbit remain a native approximation. Lighting now rotates the car under the
fixed scene lights rather than rotating the camera/light relationship, and adds
ambient material colour separately from diffuse. Specular strength (.18), shadow
opacity (.55), studio backdrop and suspension attachment remain provisional.
This improves the reported defects but is not a claim of pixel-exact PS2 parity.

Validation: 14 CTest tests and nine real-disc navigation cases pass. New tests
cover primitive ABE, opaque low-alpha paint, destination-alpha additive reflection,
alpha rounding, raw Tex1 alpha above 128, mirrored winding, and low-index body
shapes with a separate wheel model. Visual captures cover Ka (two colours and
angles), BMW 120i, Toyota Sports 800, Model T, MP5, Midget II and Audi R8.
`showroom_probe disc car_label output.png [colour_index] [seconds]` captures the
actual asynchronous presentation path without menu navigation; it embeds no assets.


## Factory suspension height (2026-09-27 follow-up)

The earlier attachment placed tire centres at their ground-contact radius but
left the body at its zero-height chassis reference plane. This embedded wheels
in the fenders even with correct tire diameter and axle spacing. Before attaching
wheels, lift/pitch the body using DEFAULT_PARTS.Suspension (offset 16), then the
SUSPENSION row's factory front/rear ride heights (signed millimetres at offsets
4 and 10). The Focus RS uses 155 mm, Ka 145 mm, and Audi R8 80 mm. Rotate normals
with the same pitch. Tire centres and ground shadow stay on the ground; do not
apply a universal wheel offset or resize tires to hide the intersection.

The native pose test covers equal and unequal axle heights. All 14 CTest checks
pass. Visual checks include front and rear-side Focus RS views, Ka and Audi R8,
and an executable menu capture of Focus ST170. Suspension animation/camber and
exact callback attachment transforms remain separate from this static factory
ride-height correction.


## Conditional lamp geometry (2026-09-27)

CAR4 model callback 7 (GetTimeZone) now selects branch 0 for the daytime
showroom, and callback 0 (IsTailLampActive) selects branch 0 for released brakes.
Previously both followed the out-of-range fall-through block, which can be empty:
Mustang SVT Cobra R then omitted its headlamp/taillamp inner geometry. The fix
retains nested branch execution, state changes and authored exit jumps; it adds
194 triangles to the Cobra's presentation. No model-name override is used.
Unknown callbacks and generic MDLS callback semantics retain their prior path.
Branch offsets are validated against the table and buffer boundaries.

Synthetic tests cover nested time-of-day/brake callbacks, their distinction from
the default path, generic MDLS behaviour and malformed branch offsets. All 14
CTest tests and nine car-navigation smoke cases passed. Visual checks include
Cobra front/rear, Mustang GT, Focus RS and BMW 120i. This restores static daytime
lamp components; switching headlights/brakes at runtime is not implemented.


## Lamp texture addressing (2026-09-27)

Fixed the repeated red strip above the BMW M Coupe rear lamp. The showroom
sampled every texture with modulo repetition, ignoring the authored CLAMP
register. Lens UVs beyond a REGION_CLAMP edge therefore repeated the coloured
lens instead of holding the edge texel. Tex1 now preserves U/V address modes and
MIN/MAX values. The showroom addresses each of the four bilinear taps separately,
including native-alpha taps, for REPEAT, CLAMP, REGION_CLAMP and REGION_REPEAT.
The change is not restricted to BMW or to lamps. Existing UI RGBA output remains
unchanged. Region-repeat uses the authored bit mask and fixed bits.

Tests cover negative/repeated coordinates, both clamped edges, nonzero region
bounds, region-repeat masks, and metadata decoding. All 14 CTest tests and nine
car navigation checks pass. Same-angle M Coupe captures before/after confirm the
upper duplicate strip is removed; Cobra and the navigation sample cars were also
checked. The updated opengt4 executable includes this correction.


## Converter survey, matrix commands and VM command sizes (2026-09-27)

`gt4model_survey <disc> [--each [prefix]]` converts every `/car/menu` model
(807 on US v2.00) with the showroom converter and lists failures, commands,
callbacks and the first transform arguments; `--labels LABEL...` maps
showroom labels to model files through the same SpecDB tables.

- **Command sizes.** VM_pglVariableColorScale/Offset (84/85) carry four u16
  registers (8 bytes), VM_pglRotate (72) one u16 (2 bytes), per PDTools. The
  converter used 2 and 8: the stream went out of step in 8 cars. Three then
  failed outright and showed no car: `fpv_0001` (fpv_f6_typhoon_04),
  `fpv_0002` (fpv_gt_04) and `benz0001` (patent_motor_wagen_886), which
  appeared to use opcodes 2 and 116. Now 807/807 convert (+13,026 triangles).
- **Matrix commands** (12 push, 13 pop, 15 load, 16 mult, 17 translate,
  18 scale, 19 rotate, 20-22 rotate X/Y/Z) follow OpenGL: column-major,
  multiplied on the right, degrees. Evidence: multMatrix translations sit in
  elements 12-14; `spider_duetto_66` places a part with translate(-0.30,
  0.63, -0.21) + rotateX(-20.5), the pose of a left-hand steering wheel.
  Positions use the matrix, normals its inverse transpose (renormalized),
  mirrors reverse the winding; an unbalanced pop keeps the base matrix.
  Used in the disc's cars: translate 147, scale 76, mult 44, rotateX 20,
  rotateY 3. RenderModel (2/3) is implemented per PDTools but not met.
- VM-driven transforms (68-76) are still skipped: they need the model VM.

Regression set: M Coupe, Focus, Ka, BMW 120i, Model T captures are
byte-identical to the previous ones; Cobra, Audi R8, Midget MP5 and Sports
800 differ only by sub-pixel edge changes (small authored translations now
applied). Tests: synthetic push/translate/mirror-scale/pop, rotateZ(90)
positions and normals, and command 84's size. Known new gap:
`patent_motor_wagen_886` now draws but without wheels, floating; its wheel
attachment is not investigated yet.

The vintage missing-wheel issue above was resolved by the native tire-size
decoding follow-up documented in `docs/RENDERER.md` (2026-10-01).
