"""Exercise authored event dialogs and course navigation with a local profile.
Requires the user's disc and a native save; copies saves into build/ first.
Screenshots are evidence for visual review, not pixel-equivalence assertions.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("disc", type=Path)
    parser.add_argument("--save-dir", type=Path, required=True)
    parser.add_argument("--exe", type=Path, default=Path("build/career-native/Release/opengt4.exe"))
    args = parser.parse_args()
    output = Path("build/ui-audit/event-routes")
    output.mkdir(parents=True, exist_ok=True)
    assert list(args.save_dir.glob("slot*.ogt4")), "A profile with a car is required"
    base = "down,right,accept,accept"
    cases = [
        ("regulations_cancel", base + ",back", "EventHallRoot"),
        ("courses_confirm", base + ",accept", "EventCourseRoot"),
        ("courses_next", base + ",accept,right,right", "EventCourseRoot"),
        ("courses_return", base + ",accept,back", "EventHallRoot"),
    ]
    with tempfile.TemporaryDirectory(dir=output) as folder:
        saves = Path(folder) / "saves"
        shutil.copytree(args.save_dir, saves)
        before = {p.name: p.read_bytes() for p in args.save_dir.glob("*.ogt4")}
        for name, inputs, root in cases:
            screenshot = output / (name + ".png")
            command = [str(args.exe.resolve()), str(args.disc.resolve()), "gtmode", "TopRoot",
                       "--save-dir", str(saves.resolve()), "--input", inputs,
                       "--input-interval", "0.2", "--dialog-answer", "no",
                       "--shot-delay", "0.5", "--shot", str(screenshot)]
            if name == "regulations_cancel":
                command += ["--dialog-shot", str(output / "regulations.png")]
            result = subprocess.run(command, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=35)
            log = result.stdout + result.stderr
            (output / (name + ".log")).write_text(log, encoding="utf-8")
            assert result.returncode == 0, log
            assert "gtmode / " + root + ": drawn" in log, log
            for missing in ("undefined 'getUpdateContext'", "enterEventLoop' of nil",
                            "undefined 'course_code'", "undefined 'build'",
                            "undefined 'syncLoading'", "CarFace model loading failed", "'warp' of nil"):
                assert missing not in log, log
            assert screenshot.stat().st_size > 1000
            # Authored Auto Save may update the isolated copy; the input profile must remain intact.
            assert {p.name: p.read_bytes() for p in args.save_dir.glob("*.ogt4")} == before
            print(name + ": passed", flush=True)
    print("Authored modal cancellation, confirmation, course navigation and return: passed")


if __name__ == "__main__":
    main()
