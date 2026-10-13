"""Original Options/Screen integration and native UI/FSR isolation checks.
Requires a user-owned disc; screenshots/preferences remain under build/.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("disc", type=Path)
parser.add_argument("--exe", type=Path, default=Path("build/Release/opengt4.exe"))
parser.add_argument("--full-opening", action="store_true", help="Wait for the complete opening, menu and idle return")
args = parser.parse_args()
exe = args.exe.resolve()
out = Path("build/ui_quality")
out.mkdir(parents=True, exist_ok=True)

def run(name, project="option", root="ScreenRoot", inputs=None, extra=()):
    command = [str(exe), str(args.disc.resolve()), project, root]
    if inputs:
        command += ["--input", inputs]
    command += list(extra) + ["--shot", str(out / (name + ".png"))]
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=240 if args.full_opening else 45)
    log = result.stdout + result.stderr
    (out / (name + ".log")).write_text(log, encoding="utf-8")
    assert result.returncode == 0, log
    assert "shader compile" not in log.lower() and "incomplete frame target" not in log, log
    assert (out / (name + ".png")).stat().st_size > 1000, log
    return log

log = run("original_route", root="OptionRoot", inputs="right,right,accept")
assert "input accept -> option/ScreenRoot" in log, log
for row in ("AspectRatio", "WindowMode", "Resolution", "UIFilter", "TextSharpness", "VSync", "Upscaling"):
    assert "choice " + row + "/OptionMenu" in log, log
log = run("resolution_popup", inputs="down,down,accept", extra=("--shot-delay", "0.3"))
assert "choice Resolution/OptionMenu index=1" in log, log
log = run("popup_cancel", inputs="down,down,accept,down,back")
assert "option / ScreenRoot" in log and "choice Resolution/OptionMenu index=1" in log, log
log = run("page_cancel", inputs="down,down,down,right,back")
assert "option / OptionRoot" in log and "video settings: filter=0" in log, log

with tempfile.TemporaryDirectory(dir=out) as temporary:
    config = Path(temporary) / "video.ini"
    log = run("save_video", inputs="down,down,down,right,right,down,down,down,down,accept", extra=("--config-file", str(config)))
    assert config.exists() and "ui_filter=2" in config.read_text(), log
    assert "option / OptionRoot" in log and "video settings: filter=2" in log, log
    log = run("reload_video", extra=("--config-file", str(config)))
    assert "choice UIFilter/OptionMenu index=2" in log, log
    before = config.read_bytes()
    run("cancel_saved", inputs="down,down,down,left,back", extra=("--config-file", str(config)))
    assert config.read_bytes() == before

log = run("custom_resolution", extra=("--window", "1234x800"))
assert "choice Resolution/OptionMenu index=6 item=Custom" in log, log

run("ui_native", root="RaceRoot", extra=("--upscale", "off"))
run("ui_fsr", root="RaceRoot", extra=("--upscale", "quality"))
assert (out / "ui_native.png").read_bytes() == (out / "ui_fsr.png").read_bytes(), "FSR modified a UI-only frame"

route = "accept,accept,accept,accept,down,accept,right"
for mode, size in (("off", "1280x960"), ("ultra-quality", "985x738"), ("quality", "853x640"), ("balanced", "753x565"), ("performance", "640x480")):
    log = run("scene_" + mode, "arcade", "TopRoot", route,
              ("--preview-courses", "--dialog-answer", "no", "--upscale", mode))
    assert "scene: " + size + " -> 1280x960, MSAA" in log, log
    assert "FSR unavailable" not in log, log
    assert "arcade / CarRoot" in log and "selected car=" in log, log
# Verify the ratio persists on OK and remains unchanged on Cancel.
with tempfile.TemporaryDirectory(dir=out) as temporary:
    config = Path(temporary) / "video.ini"
    log = run("save_aspect", inputs="right,down,down,down,down,down,down,down,accept", extra=("--config-file", str(config)))
    assert "aspect_ratio=1" in config.read_text(), log
    log = run("wide_settings", extra=("--config-file", str(config)))
    assert "choice AspectRatio/OptionMenu index=1" in log and "aspect=1" in log, log
    before = config.read_bytes()
    log = run("cancel_aspect", inputs="left,back", extra=("--config-file", str(config)))
    assert "aspect=1" in log and config.read_bytes() == before, log

print("Original settings route, dropdown cancellation, save/reload, custom resolution, UI isolation and five scene modes: passed")

log = run("aspect_wide", inputs="right,down,down,down,down,down,down,down,accept")
assert "aspect=1" in log, log

for answer in ("no", "yes"):
    log=run("exit_"+answer,root="OptionRoot",inputs="back",extra=("--dialog-answer",answer,"--dialog-shot",str(out/("dialog_"+answer+".png"))))
    assert "in-game dialog: Info / Exit Options?" in log,log
    assert (out/("dialog_"+answer+".png")).stat().st_size>1000
print("In-game original confirmation with Cancel and OK: passed")

# A cached caller must become visible again after confirming Options exit.
for attempt in (1, 2):
    log=run("roundtrip_"+str(attempt),project="arcade",root="TopRoot",
            inputs="down,down,down,accept,back"+(",accept,back" if attempt==2 else ""),
            extra=("--dialog-answer","yes","--shot-delay","1"))
    assert "input back -> arcade/TopRoot focus=Option" in log,log
    from PIL import Image, ImageStat
    assert sum(ImageStat.Stat(Image.open(out/("roundtrip_"+str(attempt)+".png")).convert("RGB")).mean)>30, "Return stayed black"
run("picture_controls",root="ColorRoot",inputs="right,right,right,accept")
print("Picture Quality sliders and repeated Options exit: passed")

with tempfile.TemporaryDirectory(dir=out) as temporary:
    config=Path(temporary)/"video.ini"
    log=run("save_picture",root="ColorRoot",inputs="right,right,right,"+",".join(["down"]*9)+",accept",
            extra=("--config-file",str(config)))
    game=Path(str(config)+".game_options")
    assert game.exists() and "race_color_correction 2 3,0,0,0" in game.read_text(),log
    before=game.read_bytes()
    run("cancel_picture",root="ColorRoot",inputs="left,back",extra=("--config-file",str(config)))
    assert game.read_bytes()==before
print("Original game option commit and cancellation: passed")

with tempfile.TemporaryDirectory(dir=out) as temporary:
    config=Path(temporary)/"misc.ini"
    game=Path(str(config)+".game_options")
    log=run("misc_commit",root="MiscRoot",inputs="right,down,right,down,right,down,right,down,accept",
            extra=("--config-file",str(config)))
    assert "undefined" not in log and "call of nil" not in log,log
    values=game.read_text()
    for field in ("opening_after_autoload 0 0","timeout_count_to_autodemo 0 40","demo_movie_interval 0 2","auto_save 0 0"):
        assert field in values,(field,values)
    log=run("misc_reload",root="MiscRoot",extra=("--config-file",str(config)))
    assert "choice OpeningMovie/OptionMenu index=1" in log and "choice AutoSave/OptionMenu index=1" in log,log
    before=game.read_bytes()
    run("misc_cancel",root="MiscRoot",inputs="left,down,left,down,left,down,left,back",extra=("--config-file",str(config)))
    assert game.read_bytes()==before
    # Shorten only this fixture's timer: no change to the UI's 10-second steps.
    game.write_text("timeout_count_to_autodemo 0 1\ndemo_movie_interval 0 1\n")
    for project,root,inputs,name in (("arcade","TopRoot",None,"idle_opening"),("boot","MovieRoot","accept","opening_return")):
        log=run(name,project=project,root=root,inputs=inputs,
                extra=("--config-file",str(config),"--shot-delay","2"))
        assert "boot / MovieRoot" in log and "movie frames=" in log and "error:" not in log,log
    if args.full_opening:
        log=run("opening_full_cycle",project="boot",root="MovieRoot",
                extra=("--config-file",str(config),"--shot-delay","205"))
        assert "movie: opening_us finished after" in log and "boot / MovieRoot" in log,log
        import re
        frames=re.search(r"movie frames=(\d+)",log)
        assert frames and 0<int(frames.group(1))<1000,log
        print("Complete opening, menu, inactivity and second opening: passed")
print("Miscellaneous commit/reload/cancel and opening return after inactivity: passed")
