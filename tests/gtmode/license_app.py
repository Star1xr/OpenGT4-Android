"""C integration: enter and finish B-1 using pad input in the native app.

Uses an isolated save folder and generated screenshots under build only.
This does not compare pixels/audio against PCSX2 or prove the other exams.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("exe", type=Path)
p.add_argument("disc", type=Path)
p.add_argument("--case", choices=["pregrid", "race", "result", "fail", "exit", "retry", "next", "leave"], default="exit")
p.add_argument("--negative", action="store_true")
p.add_argument("--concourse", action="store_true", help="Require resident staff and the authored result camera transition")
p.add_argument("--concourse-negative", action="store_true", help="Deliberately require the wrong bind count")
a = p.parse_args()
root = Path("build/ui-audit/license-app") / a.case
root.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
for key in ["OPENGT4_RACE_AUTOPILOT", "OPENGT4_RACE_SNAPSHOT", "OPENGT4_RACE_RESULT_CONFIRM", "OPENGT4_LICENSE_RESULT_KEYS", "OPENGT4_LICENSE_TEST_WAIT"]:
    env.pop(key, None)
env["OPENGT4_LICENSE_TEST_BRAKE_AT"] = "1420" if a.case == "fail" else "980"
if a.case == "fail":
    env["OPENGT4_LICENSE_TEST_WAIT"] = "900"
env["OPENGT4_RACE_SPEED"] = "4"
delay = {"pregrid": 2, "race": 4, "result": 13, "fail": 18, "exit": 23, "retry": 24, "next": 32, "leave": 33}[a.case]
args = [str(a.exe.resolve()), str(a.disc.resolve()), "gtmode", "LicenseRoot",
        "--save-dir", tempfile.mkdtemp(prefix="saves-", dir=root), "--input", "accept,accept",
        "--input-interval", "0.4", "--shot-delay", str(delay), "--shot", str(root / "frame.png")]
if a.case != "pregrid":
    quick = "accept"
    if a.case in ["next", "leave"]:
        quick += "," + ",".join(["right"] * (7 if a.case == "next" else 8)) + ",accept"
    args += ["--quick-input", quick]
if a.case in ["exit", "next", "leave"]:
    env["OPENGT4_LICENSE_RESULT_KEYS"] = "8:40,9:13"
elif a.case == "retry":
    env["OPENGT4_LICENSE_RESULT_KEYS"] = "8:38,9:13"
with (root / "app.log").open("w", encoding="utf-8", newline="") as log:
    result = subprocess.run(args, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=100)
text = (root / "app.log").read_text(encoding="utf-8")
assert result.returncode == 0, (result.returncode, text[-3000:])
assert (root / "frame.png").is_file()
assert not re.search(r"error:|call of nil|undefined|could not be recorded", text), text[-3000:]
if a.concourse or a.concourse_negative:
    expected_slots = 19 if a.concourse_negative else 20
    assert f"licence scene: resident staff skin/head/pen/board, {expected_slots} bind slots" in text, text[-3000:]
    if a.case in ["result", "fail", "exit", "retry", "next", "leave"]:
        assert "licence scene: phase 2, license_cam01_seq, authored place" in text, text[-3000:]
if a.case != "leave":
    assert "quick-license / LicenseRoot:" in text
if a.case in ["pregrid", "exit"]:
    assert re.search(r"quick-license / LicenseRoot: drawn [1-9]", text), text[-2000:]
else:
    assert "quick menu: COMMAND_START" in text
if a.case in ["result", "exit", "retry", "next", "leave"]:
    assert "race music: PCM/license_silver" in text
    assert "licence trophy: model=2 triangles=2388 unsupported=0" in text
if a.case == "fail":
    assert "race music: PCM/license_failed" in text, text[-3000:]
    assert "licence trophy:" not in text
if a.case in ["exit", "retry", "next", "leave"]:
    expected = "state 4" if a.negative else "state 3 time 10248 best 10248"
    assert "licence attempt l0b0001: " + expected in text, text[-3000:]
if a.case == "retry":
    assert text.count("race music: PCM/license_start") >= 2, text[-3000:]
if a.case == "next":
    assert "quick menu: COMMAND_NEXT_RACE" in text
    assert "quick-license / LicenseRoot: drawn" in text
if a.case == "leave":
    assert "quick input accept -> Exit" in text
    assert "gtmode / LicenseDetailRoot:" in text
if a.negative and a.case not in ["exit", "retry", "next", "leave"]:
    assert "quick-license / ImpossibleRoot" in text
print(f"PASS native B-1 {a.case}; pad-driven examination, original assets, isolated save. Screenshot: {root / 'frame.png'}")
