"""Real-disc course preview integration; images stay in ignored build/.

Successful capture checks integration, not visual parity or complete terrain.
"""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument("disc", type=Path)
args = p.parse_args()
out = Path("build/race_smoke")
out.mkdir(parents=True, exist_ok=True)
exe = str(Path("build/Release/opengt4.exe").resolve())


def run(name, options, success=True):
    shot = out / f"{name}.png"
    shot.unlink(missing_ok=True)
    r = subprocess.run([exe, str(args.disc.resolve()), *options, "--shot", str(shot)],
                       capture_output=True, encoding="utf-8", errors="replace", timeout=45)
    log = r.stdout + r.stderr
    (out / f"{name}.log").write_text(log, encoding="utf-8")
    assert (r.returncode == 0) == success, log
    if success:
        assert "race preview:" in log and shot.stat().st_size > 1000, log
        assert "timed out" not in log, log
    else:
        assert not shot.exists(), log
    print(name + ": passed", flush=True)
    return log


for course, car in [("tsukuba", "ka_01"), ("suzuka", "focus_rs_02"), ("nurburgring", "ka_01")]:
    log = run(course, ["--race-course", course, "--race-car", car])
    assert f"course={course} car={car}" in log

log = run("menu_selection", ["arcade", "TopRoot", "--preview-courses", "--input",
          "accept,accept,accept,accept,down,accept,right", "--race-preview"])
assert "selected course=nurburgring" in log and "selected car=ka_01" in log
assert "race preview: course=nurburgring car=ka_01" in log
run("missing_course", ["--race-course", "nonexistent_course_test"], success=False)
