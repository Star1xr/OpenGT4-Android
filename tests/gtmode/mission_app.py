"""C: pad-driven mission result/return in the native application.

The fixture supplies controller bytes only. No finish, rank, reward or physical
state is injected. Screenshots and isolated saves stay outside the repository.
This test does not certify PS2 pixels/audio or completion of all 34 missions.
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
p.add_argument("--case", choices=["win", "fail"], default="win")
p.add_argument("--verify-log", type=Path, help="Check a previously captured run")
p.add_argument("--negative", action="store_true")
p.add_argument("--negative-step", action="store_true", help="Deliberately add one reported physics step")
a = p.parse_args()
mission = 9 if a.case == "win" else 10
presentation, page, position, prize = (
    ("board", "PrizeRoot", 0, 1000) if mission == 9 else ("return", "AfterRoot", 3, 0)
)
if a.verify_log:
    log_path = a.verify_log
else:
    root = Path("build/ui-audit/mission-app") / a.case
    root.mkdir(parents=True, exist_ok=True)
    root = root.resolve()
    tape = root / "controller.pad"
    env = os.environ.copy()
    for key in ["OPENGT4_RACE_AUTOPILOT", "OPENGT4_RACE_SNAPSHOT", "GT4_MISSION_LANE"]:
        env.pop(key, None)
    env.update(GT4_MISSION_ONE=str(mission), GT4_MISSION_RUN="1",
               GT4_MISSION_STEER="1", GT4_MISSION_PAD_OUTPUT=str(tape))
    with (root / "driver.log").open("w", encoding="utf-8", newline="") as log:
        subprocess.run([str(a.exe.resolve().with_name("gtmode_mission_disc.exe")), str(a.disc.resolve())],
                       env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60)
    env.update(OPENGT4_MISSION=f"l0m{mission:04d}", OPENGT4_MISSION_TEST_PAD=str(tape),
               OPENGT4_RACE_SPEED="4", OPENGT4_RACE_RESULT_CONFIRM="1")
    log_path = root / "app.log"
    shot = root / "frame.png"
    args = [str(a.exe.resolve()), str(a.disc.resolve()), "gtmode", "MissionRoot",
            "--save-dir", tempfile.mkdtemp(prefix="saves-", dir=root),
            "--no-config", "--skip-intro", "--quick-input", "accept",
            "--shot-delay", "24" if mission == 9 else "55", "--shot", str(shot)]
    with log_path.open("w", encoding="utf-8", newline="") as log:
        result = subprocess.run(args, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120)
    assert result.returncode == 0 and shot.is_file(), result.returncode
text = log_path.read_text(encoding="utf-8")
assert not re.search(r"error:|call of nil|could not be recorded|race: the step failed", text), text[-3000:]
assert f"mission presentation: {presentation}" in text, text[-3000:]
assert re.search(rf"mission l0m{mission:04d}: through 1 position {position} time \d+ prize {prize}", text), text[-3000:]
expected = "ImpossibleRoot" if a.negative else page
assert re.search(rf"quick-mission / {expected}: drawn [1-9]", text), text[-3000:]
assert "replay: starting" not in text
if mission == 10:
    assert "mission presentation: board" not in text
    final_frame = re.search(r"mission presentation: return frame (\d+)", text)
    stop_frame = re.search(r"mission stop: frame (\d+)", text)
    assert final_frame and stop_frame, "Missing mission shutdown trace"
    assert int(final_frame[1]) == int(stop_frame[1]) + int(a.negative_step), "Extra physics step after mission exit request"
print(f"PASS native mission {mission}: {presentation}, position {position}, prize {prize}, {page}.")
