"""Real-disc native car catalog checks; no game assets are bundled."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("disc", type=Path)
parser.add_argument("--exe", type=Path, default=Path("build/Release/opengt4.exe"))
args = parser.parse_args()
out = Path("build/car_smoke")
out.mkdir(parents=True, exist_ok=True)
base = "accept,accept,accept,accept"
cases = [
    ("ford", "down,accept", "CarRoot", "SelectBox", "model_t_tourer_15"),
    ("next", "down,accept,right", "CarRoot", "SelectBox", "ka_01"),
    ("previous", "down,accept,right,left", "CarRoot", "SelectBox", "model_t_tourer_15"),
    ("toyota", "down,down,accept", "CarRoot", "SelectBox", "sports_800_65"),
    ("bmw", "down,down,down,down,right,right,accept", "CarRoot", "SelectBox", "_120i_04"),
    ("return", "down,accept,right,back", "MakerRoot", "ford", "ka_01"),
    ("reopen", "down,accept,right,back,accept", "CarRoot", "SelectBox", "ka_01"),
    ("color_select", "down,accept,right,accept", "CarRoot", "ColItem", "ka_01"),
    ("color_next", "down,accept,right,accept,right", "CarRoot", "ColItem", "ka_01"),
    ("driving_options", "down,accept,right,accept,accept", "SettingPopup", "OptionMenu", "ka_01"),
    ("era", "right,accept", "HistoryRoot", "History", None),
    ("era_car", "right,accept,down,accept", "CarRoot", "SelectBox", "charger_superbee_426_hemi_71"),
    ("favorites_empty", "right,right,accept,back", "CourseRoot", "Item", None),
    ("scroll", "down,right,right,right,right,right,right", "MakerRoot", "buick", None),
]
for name, inputs, page, focus, car in cases:
    run = subprocess.run(
        [str(args.exe.resolve()), str(args.disc.resolve()),
         "arcade", "TopRoot", "--preview-courses", "--input", base + "," + inputs,
         "--dialog-answer", "no", "--shot", str(out / (name + ".png"))],
        capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30)
    log = run.stdout + run.stderr
    (out / (name + ".log")).write_text(log, encoding="utf-8")
    assert run.returncode == 0 and "error:" not in log, log
    states = [line for line in run.stdout.splitlines() if line.startswith("input ")]
    assert states[-1].endswith(f"arcade/{page} focus={focus}"), log
    if car:
        assert f"selected car={car}\n" in run.stdout, log
    if name == "driving_options":
        assert "choice Mission/OptionMenu index=0 item=AUTOMATIC" in log, log
        assert "choice Assist/OptionMenu index=1 item=TCSOnly" in log, log
        assert "choice Open/OptionMenu index=0 item=close" in log, log
    for member in ("CarData", "loadGpb", "setItemTemplate", "setItemCount",
                   "getItemWidget", "insertChild", "GetCarLabel", "getCarLabel", "isCarAvailable"):
        assert f"undefined '{member}'" not in log, log
    assert (out / (name + ".png")).stat().st_size > 1000
    print(name + ": passed")
