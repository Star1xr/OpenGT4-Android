"""GT Mode native profile entry; writes only to a temporary save directory."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("disc", type=Path)
    parser.add_argument("--exe", type=Path, default=Path("build/Release/opengt4.exe"))
    args = parser.parse_args()
    output = Path("build/gtmode_entry_smoke")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="opengt4-entry-") as saves:
        def run(name, inputs, expected):
            result = subprocess.run(
                [str(args.exe.resolve()), str(args.disc.resolve()), "arcade",
                 "--save-dir", saves, "--input", inputs, "--shot-delay", "0.2",
                 "--shot", str(output / (name + ".png"))],
                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=45)
            log = result.stdout + result.stderr
            (output / (name + ".log")).write_text(log, encoding="utf-8")
            assert result.returncode == 0, log
            states = [line for line in result.stdout.splitlines() if line.startswith("input ")]
            assert states and states[-1].endswith(expected), log
            assert "No memory card (PS2)" not in log, log
            assert "instruction budget" not in log, log
            print(name + ": passed")

        run("empty", "down,accept", "save/LoadRoot focus=SaveCard1")
        run("cancel", "down,accept,back", "arcade/TopRoot focus=GTmode")
        # Type q on the original name keyboard, then select its Enter key.
        route = "down,accept,accept,accept," + "right," * 10 + "down,accept"
        run("create", route, "gtmode/TopRoot focus=Home")
        assert (Path(saves) / "slot01.ogt4").is_file()
        run("active", "down,accept", "gtmode/TopRoot focus=Home")
        # Leave the profile available but clear the active choice for a load test.
        for index in Path(saves).glob("active.ogt4*"):
            index.unlink()
        run("load", "down,accept,accept", "gtmode/TopRoot focus=Home")


if __name__ == "__main__":
    main()
