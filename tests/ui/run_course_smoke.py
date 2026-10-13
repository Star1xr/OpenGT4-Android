"""Optional US v2.00 disc checks: course catalog, selection and direction.

Assets and captures stay local in ignored build/course_smoke/.
"""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("disc", type=Path)
parser.add_argument("--exe", type=Path, default=Path("build/Release/opengt4.exe"))
args = parser.parse_args()
out = Path("build/course_smoke")
out.mkdir(parents=True, exist_ok=True)
cases = [
    ("locked_without_save", "accept,accept,accept", False, "CategoryRoot", "Real", None),
    ("first", "accept,accept,accept", True, "CourseRoot", "Item", "nurburgring"),
    ("next", "accept,accept,accept,right", True, "CourseRoot", "Item", "laguna"),
    ("previous", "accept,accept,accept,right,left", True, "CourseRoot", "Item", "nurburgring"),
    ("back", "accept,accept,accept,back", True, "CategoryRoot", "Real", None),
    ("direction", "accept,accept,down,accept,accept,right", True, "CourseRoot", "Reverse", "capri"),
    ("cancel_direction", "accept,accept,down,accept,accept,back", True, "CourseRoot", "Item", "capri"),
    ("confirm", "accept,accept,accept,accept", True, "MakerRoot", "Maker", "nurburgring"),
    ("confirm_reverse", "accept,accept,down,accept,accept,right,accept", True, "MakerRoot", "Maker", "r_capri"),
]
for name, inputs, preview, page, focus, course in cases:
    shot = out / (name + ".png")
    run = subprocess.run(
        [str(args.exe.resolve()), str(args.disc.resolve()),
         "arcade", "TopRoot", *(["--preview-courses"] if preview else []),
         "--input", inputs, "--shot", str(shot)],
        capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30)
    log = run.stdout + run.stderr
    (out / (name + ".log")).write_text(log, encoding="utf-8")
    assert run.returncode == 0 and "error:" not in log, log
    states = [line for line in run.stdout.splitlines() if line.startswith("input ")]
    assert states[-1].endswith(f"arcade/{page} focus={focus}"), log
    if course:
        assert f"selected course={course}\n" in run.stdout, log
    # Other unimplemented engine services remain logged; these are the bridge
    # functions this change promises, and may not silently regress to nil.
    for member in ("GetMaxCarNum", "isCourseAvailable", "format", "callFocus",
                   "outFocus", "getFocusedWidget", "GetCourseLengthString", "GetCourseHeightString"):
        assert f"undefined '{member}'" not in log, log
    assert shot.stat().st_size > 1000
    print(name + ": passed")
