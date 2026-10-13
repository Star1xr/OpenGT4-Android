# Native menu text

`src/core/font.c` reads FT01, `rtext.c` reads RT03, and `ui.c` draws the
glyphs as ordinary platform texture quads. All font images, metrics and
translations come from the user's volume. No font or menu string is bundled.

## FT01

Little endian, 48-byte header. At 0x10 is the u16 face count; at 0x14 and
0x18 are the offsets of the face and Tex1 offset arrays. Both arrays have
one entry per face in the US v2.00 system/menu/race font files. The u16
field at 0x12 is not used as the number of entries in those arrays.

A 32-byte face header holds the name offset, glyph table offset, inclusive
u16 character range, and byte metrics (height at +13, baseline at +14).
Glyphs are 20 bytes: signed byte x/y bearing, byte bitmap width/height,
32-bit advance, u16 atlas x/y, and left/right contour offsets (+12/+16).
The contours are signed bytes, face height +15; pair gap is face +21.
The ff/ff bitmap size means no ink (space), retaining its advance.
The associated Tex1 contains an outlined atlas (0) and ink-only atlas (1).
The renderer uses atlas 1 for foreground and atlas 0 for offset shadows.
Using atlas 0 tinted black had merged the outlines into thick unreadable text.

Lookup tries system.fnt, menu.fnt, race.fnt, caching each requested face.
Missing faces are logged, not silently replaced by host fonts. Characters
below 0x300 outside the face's range use `?`.

## KNJI (.kf)

Codes from 0x300 go to the KNJI font, as 00492EF0 -> 00492748 does. The
US disc loads `/font/jp_menu.kf` (0048F9B8 with 00108D80(1): regions "jp",
"uk" and "us"). Header: "KNJI", u16 subfont count at +0x10, u16 palette
count at +0x12, u32 subfont list at +0x14, u32 palettes at +0x18 (16 RGBA
colours each, PS2 alpha 0x80 opaque; the first outlined, the second ink).
A subfont: u16 first/last code, u32 glyph data, u32 bytes per glyph, u16
cell width/height, u32 code table (u16 glyph index per code). 00491CB8: a
code outside the range uses glyph 0. Glyphs are 4-bit, left pixel in the
low nibble. jp_menu.kf has "medium" (16x16) and "bold" (22x22).

The FT01 face picks the subfont (+0x16) and places the cell: width +0x10,
height +0x11, signed x/y offsets +0x12/+0x13 from the pen and baseline,
advance +0x14; one more pixel on offset and advance after a glyph (context
+0x78). The whole cell is drawn into that rectangle (00492198). A glyph
right after a KNJI cell with no glyph before gets no bearing (00492978
+0x38C). Checked against the user's PCSX2 capture of the Tune Shop's
"●Price": size, baseline and left edge within one pixel.

Format research: [Nenkai's FT01 template](https://github.com/Nenkai/GT-File-Specifications-Documentation/blob/master/Formats/PS2/GT4/Fonts/FT01_FontSet.bt).
The reader is original C, with offset/string/table bounds validation, and
was checked against the user's files. Bitmap dimensions/baseline placement
were visually checked in the native output; an exact PCSX2 comparison is
still pending.

## RT03

The files are named rtext.rt2 but their magic is RT03. A u32 page count is
at +12, followed by 16-byte records containing absolute name offset, pair
count, pair table offset and padding. Each pair is two u32 offsets to
NUL-terminated UTF-8 key/value strings. Pages scope the keys; identical keys
in separate pages must not be merged.

TextFace `key` resolves under `<ProjectName>::<RootName>` in that project's
US/rtext.rt2. `text` overrides the translated key. Assigning `key` clears
that override. Script calls `context.translate(root, key)` and
`context.translate(root, page, key)` use the same table. Missing TextFace keys
yield an empty string. Script translation preserves unmatched input strings:
OptionRoot translates its exit message twice. The US directory is still
the existing supported-disc assumption, not a language selector.

Format cross-check: [PDTools RT03Page](https://github.com/Nenkai/PDTools/blob/master/PDTools.RText/RT03Page.cs).
No code copied; offsets and strings were independently checked in the file.

## Rendering and limits

TextFace supports runtime text/key and scale_x/scale_y, text_color, opacity,
CR/LF, per-line alignment, baseline-relative horizontal skew, and a simple offset shadow. `adjustScale()` only
shrinks horizontally when a line exceeds the widget's width. Alignment is
provisionally a 3x3 grid: 0/1/2 top left/center/right, 3/4/5 middle,
6/7/8 bottom; shadow and layout are not
yet pixel-matched to the original. Automatic wrapping, control-symbol
substitution, alternative glyph palettes, and CJK are pending. The quick
project asks for `sw_20`, absent from the three font sets currently searched.

Navigation is now handled by the menu runtime: arrows select, Enter
activates and Escape cancels through scripts. F1/F2 and PageUp/PageDown
remain development shortcuts. See [MENU_NAVIGATION.md](MENU_NAVIGATION.md).

## Validation

`ui_text_tests` uses synthetic data only: page-scoped lookups, missing keys,
UTF-8 including malformed/truncated sequences, signed glyph bearings,
atlas pixels, spaces, out-of-range UVs, bad offsets, and every truncated
length of the synthetic FT01 fixture. Run with CTest in Release too
(checks do not depend on assert/NDEBUG).

Local captures are under build/ and remain excluded from version control.
The main screen and its next selection were inspected visually. First
screens of arcade, gtmode, boot, option, setting, event and quick were run;
this does not imply all their widgets, scripts or modes work.

### Spacing adjustment

Rendering and measurement now share the same inter-character gap. The port
adds one unscaled pixel between characters by default (`letter_spacing`, also
available to scripts); there is no trailing gap. Alignment and `adjustScale`
include it. This is a readability adjustment requested after inspecting the
menu, **not a recovered FT01 kerning algorithm**. Original glyph advances,
space widths and bearings remain intact. The legacy layout outside `quick-*` still omits contour kerning.
The quick menu uses the recovered placement described below.
TextFace `skew.x` is provisionally interpreted in degrees around the baseline;
the Arcade label's -16 value produces the missing rightward slant.

Left-aligned labels (align 3) shrink horizontally to their declared width when
needed. This is a native layout fallback, not recovered original kerning.

The 3x3 alignment interpretation fixes the course heading vertical placement
and the right-aligned Best A-Spec label overlapping its separate score widget.
This is inferred from widget geometry and captures, not yet pixel-matched
against PS2 output.

### Original quick-menu layout and abbreviation

The quick-menu area now uses default `hn_14` (00246F58 +0x38) and zero
extra letter gap (+0xD4), keeping the readability setting elsewhere.
Default RGB 212, alignment 4 and opaque black shadow also come from that
constructor and are checked by the EE oracle at the string allocator boundary.
`font_layout.c` ports 00492978: contour sums use signed saturation, digits
skip pair kerning, and spaces clear the preceding glyph. Width uses the last
bitmap width (+0x478) and subtracts half the integer space advance
(00491418 +0x1A0..+0x1CC). No automatic horizontal compression is applied.

`abbreviate()` invokes the 0048F790 algorithm separately from `adjustScale()`:
truncate the character-count estimate, append `...`, then shorten until the
candidate is strictly narrower than the widget. The unusual UTF-8 prefix
condition from 0048F708 +0x58..+0x70 is retained. The original script calls
this on each pre-race car label. TextFace uses its authored scissor
(00247448 +0xD8..+0x114). Layout supports plain text in the tested faces;
control symbols and CJK remain pending.

`pregrid_text_oracle` executes original EE instructions against disc FT01:
32,841 placement steps (tolerance .00001), 32 string widths (tolerance .00002),
and 20,480 abbreviation cases at the shared measurement boundary. A +1 pixel
negative fails. Rasterization, shadow and vertical layout remain A; this
proof does not establish pixel equality of the screen. The production abbreviation
sets EE rounding for its computation and restores the caller mode, also tested.
