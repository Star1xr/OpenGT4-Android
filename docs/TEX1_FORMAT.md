# Tex1 images

Decoder: `src/core/tex1.c`. Tool: `gt4img <disc.iso> <path.img> <out>` (or a
`.gpb` and an asset name).

A `Tex1` file (every `.img`, and the images inside `.gpb` packages) is a
snapshot of PS2 video memory: data blocks uploaded to the GS, and texture
descriptors saying how to read them back. The port converts it to plain RGBA
once, when the file is loaded.

Header (GT Modding Hub description, checked on this disc):

| Offset | Field |
|--------|-------|
| `0x00` | `"Tex1"` |
| `0x0c` | file size |
| `0x14` | texture count (u16) |
| `0x16` | transfer count (u16) |
| `0x18` | textures offset: 0x28 bytes each, GS registers `TEX0`, `TEX1`, `MIPTBP1`, `MIPTBP2`, `CLAMP` |
| `0x1c` | transfers offset: 12 bytes each, `u32` data offset, `u16` TBP, `u8` buffer width, `u8` pixel format, `u16` width, `u16` height (the Hub lists width/height as bytes; they are 16-bit) |

Conversion:

1. Each transfer is written into a 4 MB video memory buffer using the layout
   of its pixel format (PSMCT32, PSMCT16, PSMCT16S, PSMT8 or PSMT4).
2. Each texture is read back with its own format (`TEX0.PSM`: PSMT8, PSMT4,
   PSMCT16, PSMCT16S, PSMCT32 or PSMCT24), buffer width and base, through
   that format's layout.
3. Palettes (`TEX0.CBP`, 32-bit, storage mode CSM1): index bits 3 and 4 are
   swapped; PSMT4 uses the 16 entries at `CSA * 16`.
4. PS2 alpha (`0x80` = opaque) is doubled and clamped to 0 to 255.
5. When `CLAMP` is in region mode, `MAXU+1` × `MAXV+1` is the image's own size
   inside the power-of-two texture; the output is cropped to it.
6. Addresses wrap at 4 MB, as on the GS.

The page/block/column tables are the GS memory layout. Checked visually on
the game's images: car photos, maker and course logos, the Polyphony logo,
the legal screen text, menu backgrounds and buttons all decode cleanly.
PSMT4 backgrounds with few colours show ordered dithering, which is in the
data.

### Small frame palettes

The native decoder also accepts the 8x2 RGB5A1 palette transfer used by
4-bit frame fill images such as 4R_A. It expands the standalone 16 entries
directly (CSM1, CSA zero); such a transfer is read from the file and is not
written into video memory, where it would cover the image it belongs to.
CLUT format reference:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/GSClut.cpp
No reference implementation code was copied.

### PSMCT16 and PSMCT16S (2026-10-18)

16-bit images (RGB5A1: five bits per channel, doubled, top bit = alpha `0x80`)
store 16x8 pixels to a 256-byte block, two rows to a 32-byte line. Each 32-bit
unit holds the pixels x and x+8 of one of the block's columns, so the 32-bit
column table gives the unit and its lowest bit the half. PSMCT16 uses the same
block order as PSMT4; PSMCT16S has its own order (third block of a page is 16,
where PSMCT16 has 8). Layout reference:
https://gist.github.com/TellowKrinkle/c05b2a50c4f272da0a03538950d86a1c
No reference implementation code was copied.
tex1_formats_test checks the 16-bit layout through a 32-bit read of the same
memory, the PSMCT16S block order, 16-bit mip levels, and the isolation below.

### Per-image isolation (2026-10-18)

A texture in a format the decoder does not take (`PSMT8H`, `PSMT4HL`,
`PSMT4HH`, an unknown value), an image whose palette transfer is missing, or a
transfer in a format it cannot write (PSMCT24) now leaves that image blank and
goes on with the next one, where a single such image used to drop the whole
file. Each format is named once in the log with the size it was used at.
Truncated files, bad offsets and descriptors past the 1024 size limit are
still refused outright.


The decoded image also retains CLAMP U/V modes and 10-bit MIN/MAX values for
3D sampling. The showroom applies address modes separately to every bilinear
tap. Treating REGION_CLAMP as repeat duplicates lamp textures outside their UV
edges (observed on BMW M Coupe). The UI RGBA buffer/size convention is unchanged.

### Authored mip levels (2026-10-12)

Descriptor +08 TEX1.MXL selects up to six stored levels. +10 MIPTBP1 and
+18 MIPTBP2 each pack three TBP14/TBW6 pairs. tex1_decode now retains their
native RGBA pixels (alpha stays 0..255/128), using TEX0's format/palette.
UI cropped level-zero buffers remain unchanged. GPU storage is power-of-two
and region addressing is applied separately at each mip level.
tex1_mips_test checks three distinguishable authored levels and truncation;
its intentional altered texel fails. Tsukuba retains 32 textures/96 levels.
GPU selection currently uses host derivatives, not original GS Q/L/K:
rendered image A. render3d_mips_test verifies upload/sampling over eight
camera positions, RGB tolerance1, with a failing MXL-disabled negative.
