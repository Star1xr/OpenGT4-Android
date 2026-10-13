"""Optional integration checks using the user's disc, never bundled assets.

Run: python tests/ui/run_menu_smoke.py <disc.iso>
Writes screenshots/logs only to build/menu_smoke/. Does not modify the disc.
"""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("disc", type=Path)
    parser.add_argument("--exe", type=Path, default=Path("build/Release/opengt4.exe"))
    args = parser.parse_args()
    out = Path("build/menu_smoke")
    out.mkdir(parents=True, exist_ok=True)
    cases = [
        ("arcade", "accept", "yes", "arcade/GameRoot focus=SingleRace", False),
        ("directional", "accept,right,down", "yes", "arcade/GameRoot focus=LANBattle", False),
        ("return", "accept,down,back,accept,back", "yes", "arcade/TopRoot focus=Arcade", False),
        ("options", "down,down,down,accept", "yes", "option/OptionRoot focus=Item", False),
        ("decline_exit", "down,down,down,accept,back", "no", "option/OptionRoot focus=Item", False),
        ("accept_exit", "down,down,down,accept,back", "yes", "arcade/TopRoot focus=Option", False),
        ("unsupported_save_recovery", "down,accept,up,accept,back", "yes", "arcade/TopRoot focus=Arcade", True),
    ]
    for name, inputs, answer, expected, allow_save_error in cases:
        shot = out / f"{name}.png"
        run = subprocess.run(
            [str(args.exe.resolve()), str(args.disc.resolve()), "arcade", "TopRoot",
             "--input", inputs, "--dialog-answer", answer, "--shot", str(shot)],
            capture_output=True, text=True, timeout=30, encoding="utf-8", errors="replace")
        log = run.stdout + run.stderr
        (out / f"{name}.log").write_text(log, encoding="utf-8")
        if run.returncode:
            raise AssertionError(f"{name}: exit {run.returncode}\n{log}")
        states = [line for line in run.stdout.splitlines() if line.startswith("input ")]
        if not states or not states[-1].endswith(expected):
            raise AssertionError(f"{name}: expected {expected}\n{log}")
        errors = [line for line in log.splitlines() if "error:" in line]
        if allow_save_error:
            if not errors or any("Save data is not implemented" not in e for e in errors):
                raise AssertionError(f"{name}: unexpected errors {errors}")
        elif errors:
            raise AssertionError(f"{name}: unexpected errors {errors}")
        if not shot.exists() or shot.stat().st_size < 1000:
            raise AssertionError(f"{name}: screenshot missing")
        print(f"{name}: passed ({expected})")


if __name__ == "__main__":
    main()
