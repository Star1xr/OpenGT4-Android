# GT Mode bitmap animations

`FlashFace` assets on the GT4 disc are converted STRB animations, even when
their package names end in `.swf`. The native reader in `src/core/strobe.c`
handles version 0 bitmap rectangles with a single sprite timeline. It uses
the file's frame lists, placement parameters, depth order, bitmap matrices,
color/alpha transforms and frame rate. Embedded Tex1 images use the existing
decoder; UVs account for the difference between the stored power-of-two size
and the decoded crop. No generated or replacement pictures are used.

Format research: [Nenkai's Strobe description](https://nenkai.github.io/gt-modding-hub/formats/animation/strb_strobe/)
and [binary layout template](https://github.com/Nenkai/GT-File-Specifications-Documentation/blob/master/Formats/Shared/STRB_Strobe.bt),
cross-checked against the user's local disc. The implementation is original C.

The License Center's `image/gtmode/license/bg1.swf` contains five images,
750 frames at 29.96875 fps, and 1050 placed bitmap rectangles. Overlapping
placements encode the original fades; playback loops after about 25 seconds.
This restores the animated background beneath the labels/cones and shared
header, rather than adding a separate MPEG movie.

The UI caches each decoded clip/textures, uses elapsed time for the timeline,
and preserves parent clipping/opacity. `flash_path`, `setFlashPath`, `reset`
and `doInitialize` support script-selected backgrounds, including volume paths.
Hidden FlashFaces don't advance their own clock. Reentry resumes unless the
script resets it; exact original reentry semantics remain to be confirmed.

Supported subset is deliberately bounded: no vector curves, gradients,
rotated/sheared placements, nested sprites, masks, or ActionScript. These
return an explicit unsupported diagnostic. License test gameplay, progression,
the selected cone's MChaseActor effect and other incomplete GT Mode widgets
are separate work; a rendered background does not implement them.

Validation: `native_media` uses synthetic fixtures for bitmap decoding,
UVs, alpha, frame wrapping, malformed offsets and unsupported masks/curves.
Local LicenseRoot captures cover the first image, a later image and a loop;
the GT Mode navigation smoke covers entering/leaving License Center.

Manufacturer slides now support a strictly validated final-frame action:
GotoFrame(0), Play, End. The loop period excludes that redirect frame, avoiding
an extra frozen frame at the boundary. Other scripts/actions remain rejected.
All 96 files under /piece/tuner_screen in the user's USA v2.00 disc passed
native decoding and clock-wrap checks; two manufacturers were visually
checked in DealerRoot. Synthetic tests cover the action's frame/index, exact
bytecode, bounded offsets and rejection of an unsupported Stop action.
