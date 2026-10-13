"""Real application launch from a copied owned-car profile; never alters the source."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("disc", type=Path)
    parser.add_argument("--save-dir", required=True, type=Path)
    parser.add_argument("--exe", type=Path, default=Path("build/Release/opengt4.exe"))
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--start", action="store_true", help="Activate A-spec in the production executable")
    args = parser.parse_args()
    output = Path("build/ui-audit/sunday-entry")
    output.mkdir(parents=True, exist_ok=True)
    original = {p.name: p.read_bytes() for p in args.save_dir.glob("*.ogt4")}
    assert "slot01.ogt4" in original
    with tempfile.TemporaryDirectory(dir=output) as directory:
        saves = Path(directory) / "saves"
        shutil.copytree(args.save_dir, saves)
        command = [str(args.exe.resolve()), str(args.disc.resolve()), "gtmode", "TopRoot",
             "--save-dir", str(saves.resolve()), "--input",
             "down,right,accept,accept,accept,accept,accept", "--input-interval", "0.2",
             "--shot-delay", "4" if args.start else "1", "--shot",
             str((output / ("start.png" if args.start else "pregrid.png")).resolve())]
        if args.start:
            command += ["--quick-input", "accept"]
        try:
            result = subprocess.run(command,
                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)
        except subprocess.TimeoutExpired as error:
            log = "".join(value.decode("utf-8", errors="replace") if isinstance(value, bytes) else (value or "")
                          for value in (error.stdout, error.stderr))
            (output / "entry.log").write_text(log, encoding="utf-8", newline="")
            raise
        log = result.stdout + result.stderr
        (output / "entry.log").write_text(log, encoding="utf-8", newline="")
        assert result.returncode == 0, log
        expected = "quick-tt / TimeTrialRoot" if args.negative else "quick-event / RaceEventRoot"
        assert expected + ": drawn" in log, log
        if args.start:
            assert "quick menu: COMMAND_START" in log, log
            assert expected + ": drawn 0, not yet drawn 0" in log, log
        for fault in ("undefined 'IsTestCar'", "undefined 'getNeedCountry'",
                      "undefined 'getAllowedCarList'", "career event could not",
                      "original function", "undefined", "error:", "instruction budget"):
            assert fault not in log, log
        assert (output / ("start.png" if args.start else "pregrid.png")).stat().st_size > 1000
        assert {p.name: p.read_bytes() for p in args.save_dir.glob("*.ogt4")} == original
    print("PASS actual GT Mode -> Sunday Cup regulations -> owned-car race " +
          ("A-spec start" if args.start else "pregrid") + "; source save unchanged")


if __name__ == "__main__":
    main()
