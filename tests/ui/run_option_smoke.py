"""Real-disc dropdown application/cancellation checks; assets stay in build/."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("disc", type=Path)
args = parser.parse_args()
exe = Path("build/Release/opengt4.exe").resolve()
out = Path("build/option_smoke")
out.mkdir(exist_ok=True)
cases = [
    ("change_fields", "right,down,right,down,right,down,right",
     {"Transmission": 1, "DisplayType": 1, "Map": 1, "Steering": 0}),
    ("apply_reopen", "right,down,down,down,down,accept,accept",
     {"Transmission": 1, "DisplayType": 0, "Map": 0, "Steering": 1}),
    ("cancel_reopen", "right,back,accept",
     {"Transmission": 0, "DisplayType": 0, "Map": 0, "Steering": 1}),
]
for name, inputs, expected in cases:
    result = subprocess.run([str(exe), str(args.disc.resolve()), "option", "RaceRoot",
                             "--input", inputs, "--shot", str(out / (name + ".png"))],
                            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30)
    log = result.stdout + result.stderr
    (out / (name + ".log")).write_text(log, encoding="utf-8")
    assert result.returncode == 0 and "error:" not in log, log
    for field, value in expected.items():
        assert f"choice {field}/OptionMenu index={value} " in log, log
    print(name + ": passed")
