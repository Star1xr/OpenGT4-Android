# Native menu movies (Windows)

MovieFace now calls a platform video renderer. Static movie_name fields and
script assignments both work; hidden MovieFaces do not play. Movie backgrounds
render before the original masks, labels and map widgets, using authored tree
order. CategoryRoot selects category.pss; CourseRoot selects the course code.

The supported US v2.00 ISO has a second ISO9660 volume. The first PVD's volume
sector count locates the second PVD; the second layer base is that count minus
16 sectors. GT4L1.VOL is located in its root directory, then parsed using the
same bounded RoFS table reader. Actual videos are /mpeg/main/*.pss. The ISO is
read-only. gt4vol accepts a trailing --movies to inspect this volume.

The Windows SDL backend uses the installed FFmpeg executable as an MPEG-2
decoder, launched directly with CREATE_NO_WINDOW (no shell). Configure its
real executable path, not a launcher shim, through OPENGT4_FFMPEG in CMake.
FFmpeg is not bundled. Non-Windows video playback is not implemented yet.
Missing data/decoder is reported instead of supplying unrelated imagery.

A temporary PSS file feeds FFmpeg; a worker reads RGBA into a bounded three-frame
queue. Rendering consumes at 30000/1001 fps using elapsed time, independent of
menu ticks. The process, pipe, worker, texture and temporary file are released
after replacement and at shutdown. The last decoder is retained (its bounded
queue stops producing when full) while another page is visible. Looping menu
videos are silent; see "Boot opening" for movies with sound. Movie decoding
never runs on the audio callback.

Videos are 640x448. For MovieFace interlace=1 (default), each MPEG frame
stacks two 224-line fields captured 1/59.94 s apart, top first (measured on
opening_us: top/bottom differ less than top/next top). FFmpeg's `il`
filter interleaves them back into lines and `yadif` (send_field, top field
first) outputs one picture per field at 59.94 fps, as the PS2 shows fields.
Weaving both into one 30 Hz picture made motion look slow; a plain bob
(line-doubled fields) flickered on static detail. The decode queue holds
6 frames. CategoryRoot explicitly uses interlace=0 (30 fps, no filter).

Movies with sound follow `plat_audio_music_position`, which advances
through the device's current 1024-frame block by elapsed time. Counting
only whole blocks (21 ms steps) made two frames due at once: a trace showed
13 skipped frames per second at 60 fps; with interpolation, about 1 in 2 s.
A MovieFace without geometry covers the whole 640x480 logical screen.

Validation: real category/course screenshots, category playback frame count
(61 in two seconds), loop check beyond its 24.024-second duration, course
navigation smoke checks and CTest. --shot-delay N waits after first-frame
availability for reproducible playback captures; --shot alone waits up to five
seconds for the decoder's first frame and fails if it times out.

## Nonblocking transitions

A separate loader now owns the movie volume/cursor, performs ISO reads, writes
its temporary PSS and starts/stops decoders. Draw only submits a generation-
numbered request and polls completed work. Obsolete requests are discarded.
The previous video stays visible until the replacement has a frame; an 180 ms
blend follows. Texture uploads reuse the texture instead of reallocating each
frame. SDL texture destruction stays on the render thread; process/thread
cleanup stays on the loader. Shutdown joins all workers before closing volumes.
`--input-interval 0.06` presents between scripted inputs to exercise rapid
replacement and cancellation; screenshots wait for the requested generation.

## Boot opening (2026-09-29)

Running `opengt4.exe <disc>` with no other arguments (or `--intro`) starts the
`boot` project's MovieRoot; `--skip-intro` disables it. The original script
picks `opening_us` by region and sets `Mpeg.audio_track` (1 stereo, 2 Pro
Logic II). Enter/Esc reach MovieRoot's onActivate/onCancel even without a
focused widget; its `skip`, and `Mpeg.onMovieFinish` at the end, run
`endBoot`, which opens Arcade (or GT Mode). The raw `onKeyPress` path
(keysym 65421) is not dispatched; Enter already covers it.

`opening_us.pss` (196 s of video) interleaves two private-stream audio tracks,
`ff a1 00 <track>`, each an SShd/SSbd stream: 48 kHz stereo SPU ADPCM,
1024-byte channel interleave, about 200 s. `src/core/pss_audio.c` decodes
the selected track to PCM when the movie opens (non-looping movies only).

The sound plays as the platform's **music source** (`plat_audio_set_music`),
through the same single SDL device and producer ring as the Arcade track and
effects; there is no second device. `plat_audio_music_position` reports the
frames the device has consumed, and the pictures follow that clock. Leaving
the page releases the source only if the movie still owns it
(`plat_audio_release_music`), so it cannot stop the Arcade music.
An earlier version opened its own SDL device per movie; the user heard no
music with it. `--shot` runs are muted and use the wall clock. A traced run
showed frame 600 at 19.99 s of wall time and of audio.

## Boot notices (2026-09-29)

Before MovieRoot the PS2 shows two still screens (`src/core/boot_notice.c`),
faded from and to black and centered in the 640x448 frame. They take no input.

1. Legal notice: `notice2005.img`, a gzip member inside the boot executable
   `SCUS_973.28` (read from the ISO root by `gt4_disc_read_root`; decoded at
   0x01000940). Its loop at 0x01000BA0 draws the list at 0x0102DCA8, `{0}`,
   waiting two vblanks per step: steps 0-14 fade in, 15-134 hold, 135-150
   fade out, about 5 s. The last image also holds while CORE loads.
2. `/advertise/us/main.img` image 2, "Sony Computer Entertainment America
   Presents", drawn by CORE's `GT4FIRSTADVERTISE::DisplayAdvertiseImage`
   (0x00108F78, 0x00109080, 0x001091B8) from the list `D_00618700 = {2}`:
   15 steps in, 3.0 * 30 held, 15 out, about 4 s, also held while loading.
   The 30-steps-per-second rate is inferred from the `* 30` hold; the update
   rate of that CORE task was not traced.

main.img's images 0 (a copy of the legal text), 1 (a PD logo) and 3 (a
demo warning) are not listed on this disc. The Polyphony Digital logo is
the first seconds of `opening_us` itself. Neither loading hold is
reproduced: only the authored minimum times. `gt4img --file <img> <prefix>`
decodes a Tex1 file extracted locally.
