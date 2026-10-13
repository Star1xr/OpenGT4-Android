#!/usr/bin/env python3
"""Build the Windows release folder and zip from a release build.

Usage:
  cmake -S . -B build-release -G "Visual Studio 17 2022" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DSDL_FORCE_STATIC_VCRT=ON
  cmake --build build-release --config Release --target opengt4
  python tools/package_release.py VERSION --icons PATH [--ffmpeg PATH/ffmpeg.exe]

The release holds no game data: the player supplies their own disc image.
Output: dist/OpenGT4-VERSION-win64/ and dist/OpenGT4-VERSION-win64.zip
"""
import argparse
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

README = """OpenGT4 {version}
==================

An unofficial native PC port of Gran Turismo 4 (work in progress).
It is not an emulator and contains no game data. You need your own copy of
Gran Turismo 4, NTSC-U (SCUS-97328), as a disc image (.iso).

Not affiliated with or endorsed by Sony Interactive Entertainment or
Polyphony Digital. Gran Turismo is a trademark of Sony Interactive
Entertainment.

Getting started
---------------
1. Run opengt4.exe.
2. Choose "Game disc" and select your Gran Turismo 4 disc image.
3. Adjust Video, Audio and Controls if you like, then choose "Play".

Your disc image stays where it is. Profiles, replays and photographs are
kept in the "Player data" folder shown in the launcher.

Controls
--------
A controller is recommended (Xbox and PlayStation pads work through SDL).
Buttons can be remapped in the launcher or in the game's own Options.
Keyboard: arrows, Enter and Esc in menus.

Checking your disc
------------------
The supported executable on the disc, CORE.GT4, has the SHA-1
30031137ce494323816bb707b90d3ec0d07937d2.

Movies
------
The game's movies are decoded with FFmpeg. {ffmpeg_note}

License
-------
OpenGT4 is free software under the GNU General Public License, version 3
(LICENSE.txt), with additional terms in NOTICE.txt: works based on OpenGT4
must say "Based on OpenGT4", and modified versions must be marked as
different from the original. There is no warranty.

Source code, issues and updates: {repo}
"""

FFMPEG_BUNDLED = "ffmpeg.exe is included in this folder (see THIRD_PARTY_NOTICES.txt)."
FFMPEG_MISSING = ("Place ffmpeg.exe next to opengt4.exe (or have it on your PATH) to see\n"
                  "them; the game runs without it, only the movies are skipped.")

NOTICES = """Third-party components
======================

OpenGT4 itself is released under the GNU General Public License, version 3
(LICENSE.txt), with the additional terms in NOTICE.txt. Its source code is
published at the project page given in README.txt.

SDL2 - zlib License - https://www.libsdl.org/
miniz - MIT License - https://github.com/richgel999/miniz
stb_image - MIT License - third_party/stb in the source code (STB_LICENSE.txt)
PDTools (Nenkai) - MIT License - third_party/PDTools in the source code
AMD FidelityFX Super Resolution 1 - MIT License - third_party/fsr1 in the source code
ps2float (PCSX2) - GNU LGPL 3.0 or later - third_party/ps2float in the source code.
  It is linked into opengt4.exe; the complete source code of OpenGT4 is
  published, so the program can be rebuilt with a modified ps2float.
Tango Icon Library - public domain - launcher_icons/SOURCE.txt
{ffmpeg}"""

FFMPEG_NOTICE = """FFmpeg - ffmpeg.exe is a separate program (an LGPL build), run as its own
  process to decode the game's movies. Its license is in FFMPEG_LICENSE.txt
  (see also https://ffmpeg.org/legal.html); its source code is available at
  https://ffmpeg.org/download.html and https://github.com/BtbN/FFmpeg-Builds.
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("version")
    parser.add_argument("--build", type=Path, default=ROOT / "build-release" / "Release")
    parser.add_argument("--icons", type=Path, required=True, help="the Tango launcher_icons folder")
    parser.add_argument("--ffmpeg", type=Path, help="an ffmpeg.exe to bundle")
    parser.add_argument("--ffmpeg-license", type=Path, help="that FFmpeg build's LICENSE.txt")
    parser.add_argument("--repo", required=True, help="the project's public page")
    args = parser.parse_args()

    exe = args.build / "opengt4.exe"
    if not exe.is_file():
        sys.exit(f"missing {exe}: build the release first")
    if not (args.icons / "SOURCE.txt").is_file():
        sys.exit(f"missing {args.icons / 'SOURCE.txt'}")

    name = f"OpenGT4-{args.version}-win64"
    out = ROOT / "dist" / name
    # Validate the absolute destination before any recursive removal.
    release_root = (ROOT / "dist").resolve()
    if out.resolve().parent != release_root or out.name != name:
        sys.exit(f"invalid release destination: {out}")
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    shutil.copy2(exe, out / "opengt4.exe")
    shutil.copytree(args.icons, out / "launcher_icons")
    if args.ffmpeg:
        if not args.ffmpeg_license:
            sys.exit("--ffmpeg needs --ffmpeg-license, the build's own license text")
        shutil.copy2(args.ffmpeg, out / "ffmpeg.exe")
        shutil.copy2(args.ffmpeg_license, out / "FFMPEG_LICENSE.txt")
    shutil.copy2(ROOT / "LICENSE", out / "LICENSE.txt")
    shutil.copy2(ROOT / "NOTICE", out / "NOTICE.txt")
    shutil.copy2(ROOT / "third_party" / "stb" / "LICENSE", out / "STB_LICENSE.txt")
    readme = README.format(version=args.version, repo=args.repo,
                           ffmpeg_note=FFMPEG_BUNDLED if args.ffmpeg else FFMPEG_MISSING)
    (out / "README.txt").write_text(readme.replace("\n", "\r\n"), encoding="utf-8", newline="")
    notices = NOTICES.format(ffmpeg=FFMPEG_NOTICE if args.ffmpeg else "")
    (out / "THIRD_PARTY_NOTICES.txt").write_text(notices.replace("\n", "\r\n"), encoding="utf-8", newline="")

    # nothing of the game may ride along
    for path in out.rglob("*"):
        if path.suffix.lower() in (".iso", ".bin", ".img", ".elf", ".irx", ".gt4", ".vol"):
            sys.exit(f"refusing to package game data: {path}")

    archive = ROOT / "dist" / (name + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for path in sorted(out.rglob("*")):
            if path.is_file():
                z.write(path, Path(name) / path.relative_to(out))
    print(f"{archive} ({archive.stat().st_size // 1024} KiB)")


if __name__ == "__main__":
    main()
