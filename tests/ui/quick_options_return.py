"""Exercise the real app's quick menu -> Options -> native caller (0010FE40).

Uses the app's CLI input injector; no desktop automation or user save.
"""
import os
from pathlib import Path
import subprocess
import sys

exe, disc, output = sys.argv[1:4]
negative = "--negative" in sys.argv[4:]
folder = Path(output).resolve()
folder.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, SDL_AUDIODRIVER="dummy", OPENGT4_MENU_TRACE="1")
result = subprocess.run(
    [exe, disc, "arcade", "--pre-race", "--race-course", "testcourse",
     "--race-car", "ka_01", "--quick-input", "right,right,accept,back",
     "--dialog-answer", "yes", "--with-music", "--window", "640x480",
     "--shot-delay", "10", "--shot", str(folder / "returned.png")],
    env=env, capture_output=True, text=True, encoding="utf-8", errors="replace",
    timeout=90,
)
log = result.stdout + result.stderr
with (folder / "app.log").open("w", encoding="utf-8", newline="") as stream:
    stream.write(log)
assert result.returncode == 0, log
routes = [line for line in log.splitlines() if line.startswith("project route:")]
destination = "arcade" if negative else "quick-arcade"
assert routes == ["project route: arcade -> quick-arcade",
                  "project route: quick-arcade -> option",
                  f"project route: option -> {destination}"], routes
assert log.count("music: PCM/default_quick") == 2, log
assert "quick-arcade / ArcadeRoot: drawn" in log, log
assert (folder / "returned.png").stat().st_size > 1000
print("PASS real app: Options returns directly to pre-race and restarts authored PCM.")
