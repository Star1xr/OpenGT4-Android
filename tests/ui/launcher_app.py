"""Exercise the standalone launcher through its real window messages.

No disc/game assets are needed. Files and profiles stay in the test folder.
An optional ISO checks Play's arguments in the harness, without booting a race.
"""
import argparse
from pathlib import Path
import subprocess


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--iso", type=Path)
    args = parser.parse_args()
    root = args.folder.resolve()
    root.mkdir(parents=True, exist_ok=True)
    saves = root / "saves"
    saves.mkdir(exist_ok=True)
    prefs = root / "launcher.ini"
    with prefs.open("w", encoding="utf-8", newline="") as f:
        f.write(f"disc=\nsaves={saves.as_posix()}\nshow=1\n")
    video = Path(str(prefs) + ".video.ini")
    options = Path(str(video) + ".game_options")
    with options.open("w", encoding="utf-8", newline="") as f:
        f.write("future_option 0 123\n")

    def run(name, inputs, shot=True):
        command = [str(args.executable.resolve()), "--launcher", "--launcher-config", str(prefs)]
        if inputs:
            command += ["--input", inputs]
        if shot:
            command += ["--shot", str(root / (name + ".png"))]
        completed = subprocess.run(command, capture_output=True, text=True, timeout=30)
        check(completed.returncode == 0, completed.stdout + completed.stderr)
        return completed.stdout

    run("home", "")
    check("LAUNCHER FOCUS 1" in run("navigation-right", "focus:0,right"), "Right from Play reaches Game disc")
    check("LAUNCHER FOCUS 0" in run("navigation-wrap", "focus:5,right"), "a row wraps to its first tile")
    check("LAUNCHER FOCUS 7" in run("navigation-exit", "focus:4,down"), "Down from Controls reaches Exit")
    check("LAUNCHER FOCUS 6" in run("navigation-startup", "focus:0,up"), "Up from Play reaches the startup switch")
    check("LAUNCHER FOCUS 0" in run("navigation-down", "focus:6,down"), "Down from the switch returns to Play")
    check("LAUNCHER FOCUS 4" in run("hover", "hover:4"), "the mouse over Controls shows its details")
    run("apply", "video,set:2:3,apply,audio,set:0:40,apply,controls,set:1:16,set:2:15,apply,startup")
    values = dict(line.split("=", 1) for line in video.read_text().splitlines())
    check(values["width"] == "1920" and values["height"] == "1080", "video Apply")
    text = options.read_text()
    check("menu_bgm_volume_raw 0 40\n" in text and "future_option 0 123\n" in text, "audio Apply/preserved options")
    lines = dict((line.split()[0], line.split(maxsplit=2)[2]) for line in text.splitlines())
    pad = [int(n) for n in lines["pad_buttons_1p"].split(",")]
    check(pad[14] == 5 and pad[15] == 4, "1P remap")
    check("show=0\n" in prefs.read_text(), "startup switch")
    run("conflict", "controls,set:1:15,set:2:15,apply")
    check(options.read_text() == text, "duplicate assignment must not persist")
    run("cancel", "audio,set:0:10,back,controls,default,back")
    check(options.read_text() == text, "Cancel/Default without OK must not persist")
    run("player2", "controls,2p,set:1:16,set:2:15,apply")
    lines = dict((line.split()[0], line.split(maxsplit=2)[2]) for line in options.read_text().splitlines())
    pad2 = [int(n) for n in lines["pad_buttons_2p"].split(",")]
    check(pad2[14] == 5 and pad2[15] == 4, "2P remap")
    check(lines["pad_buttons_1p"] == ",".join(map(str, pad)), "1P unchanged by 2P")
    run("video", "video")
    run("audio", "audio")
    run("controls", "controls")
    if args.iso:
        with prefs.open("w", encoding="utf-8", newline="") as f:
            f.write(f"disc={args.iso.resolve().as_posix()}\nsaves={saves.as_posix()}\nshow=1\n")
        boot = run("play", "play", shot=False)
        check("BOOT " in boot and str(saves).replace("\\", "/") in boot.replace("\\", "/"), "Play forwards ISO/save/config")
    # Deliberate wrong expectation: the comparator must detect a saved error.
    check(int(values["width"]) == (1921 if args.negative else 1920), "deliberate width mismatch")
    print("PASS launcher window: Apply, Cancel, conflicts, defaults, both players, persistence and startup switch")


if __name__ == "__main__":
    main()
