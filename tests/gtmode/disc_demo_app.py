"""Pad-driven native demo UI checks; no claim of PCSX2 pixel fidelity."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("exe", type=Path)
p.add_argument("disc", type=Path)
p.add_argument("--case", choices=["license", "license-return", "theater", "theater-return", "license-menu", "license-menu-loop", "theater-icons"], default="license-return")
p.add_argument("--negative", action="store_true")
a = p.parse_args()
root = Path("build/disc-demo/app") / a.case
root.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
for k in list(env):
    if k.startswith(("OPENGT4_REPLAY_", "OPENGT4_LICENSE_TEST_", "OPENGT4_LICENSE_RESULT_", "OPENGT4_RACE_")):
        env.pop(k)
env["OPENGT4_RACE_SPEED"] = "4"
theater = a.case.startswith("theater")
if theater:
    env["OPENGT4_LABO_ARG"] = "ReplayTheaterRoot"
    inputs = "up,accept,right,accept" if a.case == "theater-icons" else "up,accept,right,accept,right,accept"
    args = ["labomode", "--input", inputs, "--input-interval", "0.5"]
    delay = 53 if a.case.endswith("return") else 6
else:
    args = ["gtmode", "LicenseRoot", "--input", "accept,accept", "--input-interval", "0.4"]
    if not a.case.startswith("license-menu"):
        args += ["--quick-input", "right,accept"]
    delay = 18 if a.case.endswith("return") else 6
if a.case == "license-menu":
    delay = 18
if a.case == "license-menu-loop":
    delay = 58
args += ["--save-dir", tempfile.mkdtemp(prefix="save-", dir=root), "--shot-delay", str(delay), "--shot", str(root / "frame.png")]
with (root / "app.log").open("w", encoding="utf-8", newline="") as log:
    run = subprocess.run([str(a.exe.resolve()), str(a.disc.resolve()), *args], env=env, stdout=log, stderr=subprocess.STDOUT, timeout=100)
text = (root / "app.log").read_text(encoding="utf-8")
assert run.returncode == 0, text[-3000:]
assert (root / "frame.png").exists()
assert "race: the step failed" not in text and "could not be played" not in text
if theater:
    assert "set count = " + ("14" if a.negative else "13") in text
    if a.case == "theater-icons":
        assert "focus=List" in text
    else:
        assert "focus=Play" in text and "exp_speed8_03" in text
    drawn = "labomode / ReplayTheaterRoot: drawn "
elif a.case.startswith("license-menu"):
    assert "quick menu: COMMAND_START" not in text and "COMMAND_DEMONSTRATION" not in text
    assert "license menu demo: l0b0001, " + ("1472" if a.negative else "1471") + " original commands" in text
    assert "licence attempt l0b0001:" not in text
    if a.case == "license-menu-loop":
        assert "license menu cycle: l0b0001 restarted " + ("3" if a.negative else "2") in text
    drawn = "quick-license / LicenseRoot: drawn "
else:
    assert "quick menu: COMMAND_DEMONSTRATION" in text
    assert "demonstration: l0b0001, " + ("1472" if a.negative else "1471") + " original commands" in text
    assert "licence attempt l0b0001:" not in text and "race music: PCM/license_silver" not in text
    drawn = "quick-license / LicenseRoot: drawn "
    if a.case.endswith("return"):
        assert "demonstration: returned to examination menu" in text
if a.case.endswith("return") or a.case.startswith("license-menu") or a.case == "theater-icons":
    assert drawn in text and drawn + "0," not in text, text[-3000:]
else:
    assert drawn + "0," in text, text[-3000:]
print(f"PASS native {a.case}: authored menu input, on-disc playback, isolated save; {root / 'frame.png'}")
