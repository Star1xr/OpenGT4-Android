"""Download existing public-domain Tango icons, without accessing a game disc.

Default: beside the launcher's per-user launcher.ini. Override --destination
with <directory of custom launcher.ini>/launcher_icons. PNGs stay outside Git;
only this reproducible downloader is tracked. The launcher never downloads.
"""
import argparse
import hashlib
import os
from pathlib import Path
import urllib.request

REVISION = "0a1564d0861bcca68ced31a5798f8d07c39a19ed"
SOURCE = f"https://raw.githubusercontent.com/nigeltao/tango-icon-library-pngs/{REVISION}"
# Tango Icon Library 0.8.90, public domain; unmodified 512px PNG editions.
# https://github.com/nigeltao/tango-icon-library-pngs/blob/master/COPYING
FILES = {
    "actions-media-playback-start.png": "33027b706d19356ed0522229b8428f6946ca16886d9d9335371a6bcdeb300397",
    "devices-media-optical.png": "4afe56d867f4ea372618c71738031e323e8546bd98e0750a03790c05b33154f4",
    "devices-video-display.png": "fd634c0edc3441cf7427c761566b2e1a86dfefbed657d48c8e4a553ae6e6ef8e",
    "status-audio-volume-high.png": "47705e52817fe8434f1532cbafdbd27d5513434d8df912d44f7ce9d807c29983",
    "devices-input-gaming.png": "efb43c82da5d55391860db9e7b2aadf6c4032ff82f2e3eb875613447397b85ed",
    "places-folder.png": "a74b7ca8256f639acc1b3a861bb90d6caccc0f51199b550ab9fccb8769b5ba97",
    "actions-system-log-out.png": "8fb112f71ebd94b8e69085bc2362e0953a1631a084cf9a0e403fcc0d144dad48",
}
LICENSE = b"The icons in this repository are herefore released into the Public Domain.\n"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    default = Path(os.environ.get("APPDATA", str(Path.home() / ".config"))) / "OpenGT4" / "OpenGT4" / "launcher_icons"
    parser.add_argument("--destination", type=Path, default=default)
    args = parser.parse_args()
    # Validate all responses before replacing any known-good cache file.
    with urllib.request.urlopen(SOURCE + "/COPYING", timeout=30) as response:
        license_text = response.read(4096)
    if license_text != LICENSE:
        raise ValueError("Upstream public-domain declaration changed")
    icons = {}
    for name, expected in FILES.items():
        with urllib.request.urlopen(SOURCE + "/png-512/" + name, timeout=30) as response:
            data = response.read(2 * 1024 * 1024 + 1)
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError(f"Checksum mismatch: {name}")
        icons[name] = data
    args.destination.mkdir(parents=True, exist_ok=True)
    icons["COPYING"] = license_text
    icons["SOURCE.txt"] = ("Tango Icon Library 0.8.90, public domain.\nUnmodified rasterizations by Nigel Tao.\n"
                           + SOURCE + "\n").encode()
    for name, data in icons.items():
        temporary = args.destination / (name + ".tmp")
        temporary.write_bytes(data)
        temporary.replace(args.destination / name)
    print(f"Installed {len(FILES)} verified Tango icons in {args.destination}")

if __name__ == "__main__":
    main()
