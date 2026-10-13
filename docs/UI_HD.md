# HD menu images (local cache)

The menu images on the disc (logos, buttons, car thumbnails, backgrounds)
are bitmaps for a 640x480 screen. OpenGT4 can use upscaled copies made on
the user's own machine from their own disc. Nothing is shipped: no image
and no upscaler is in the repository.

## Pieces

- Upscaler: Real-ESRGAN, portable ncnn/Vulkan build (BSD-3-Clause),
  official release `realesrgan-ncnn-vulkan-20220424-windows.zip` from
  github.com/xinntao/Real-ESRGAN (v0.2.5.0, about 45 MB), unpacked by the
  user to `%LOCALAPPDATA%\OpenGT4\tools\realesrgan`. It runs on the GPU. A
  future launcher can download it on first use (its license allows
  redistribution with its notice).
- `tools/c/gt4hdui.c`: walks every `.gpb` package and every `/piece` image
  on the disc, decodes each image (8..1024 texels a side, skipping fully
  transparent ones), spreads opaque colours into transparent texels (no
  dark fringes from the upscaler), and runs Real-ESRGAN
  (`realesr-animevideov3`, 4x, tiles of 128) in batches of 32. A batch the
  GPU drops (device lost, e.g. while a game runs) gives blank output, is
  not cached and is retried (three passes). Results: 4x for images up to
  256 texels (icons, buttons, logos), 2x for larger ones, as `<hash>.hdi` in the cache folder.
- Checks: the upscaler's alpha is unreliable (faint noise where the
  original is clear, near zero on some logos), so each result keeps the
  upscaler's colours and takes the original's alpha, resampled
  bilinearly. A GPU fault can also leave wrong colours that are not blank:
  a result is only cached if, reduced back to the original size, its
  colours match the original's where it is fairly opaque (mean error under
  24 of 255). Texels below alpha 16 get their neighbours' colours before
  upscaling (they often hold stray colours). `--verify` drops cached images
  that fail this check; `--fix-alpha` redoes the alpha of cached ones.
  First full run on the USA disc: 6363 images cached, about 390 kept as
  originals after three failed tries.
- `src/core/ui_hd.c`: the hash (FNV-1a 64 over the size and the decoded
  texels) and the file reader. File: `"HDI2"`, u32 width, u32 height, then
  the RGBA texels as raw deflate.
- `ui.c` (`image()`): after decoding an image it looks up its hash in the
  cache and, when found, uploads the upscaled texels; the widget keeps the
  original size, only the texture has more detail.
- `main.c`: cache folder `%LOCALAPPDATA%\OpenGT4\ui_hd` by default;
  `OPENGT4_UI_HD=0` turns it off, `OPENGT4_UI_HD=<folder>` uses another.

## Use

    gt4hdui <disc.iso> <cache dir> <realesrgan dir> [--model name] [--limit n]

Local shortcut: `build\Gerar UI HD.cmd`. About 20 minutes and under 1 GB for
the whole disc on a GTX 1650 (measured on 300 images: 25 s, 12 MB). A
second run only processes images not cached yet.

## Limits

Upscaled art is a reconstruction: small text inside images and fine
details can differ from the originals. Classic keeps the disc's images
when the cache is absent or turned off.
