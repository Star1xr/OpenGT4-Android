"""Real native app, disposable medal fixture, CLI only (no desktop control)."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
exe, fixture, disc, destination = sys.argv[1:5]
negative = "--negative" in sys.argv[5:]
out = Path(destination).resolve()
out.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="profile-", dir=out) as saves:
    seed = subprocess.run([fixture, disc, "--seed", saves], capture_output=True, text=True, timeout=30)
    assert seed.returncode == 0, seed.stdout + seed.stderr
    env = dict(os.environ, SDL_AUDIODRIVER="dummy", OPENGT4_PRESENT_TRACE="1",
               OPENGT4_PRESENT_CAPTURE=str(out / "phase"))
    run = subprocess.run([exe, disc, "gtmode", "PresentRoot", "--save-dir", saves,
                          "--input", "accept", "--with-music", "--window", "960x720",
                          "--shot-delay", "1", "--shot", str(out / "final.png")],
                         env=env, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
    log = run.stdout + run.stderr
    (out / "app.log").write_text(log, encoding="utf-8", newline="")
    assert run.returncode == 0, log
    for number, duration in enumerate((3, 5, 4, 1, 3), 1):
        assert f"award Sync {number} begin {duration:.3f}" in log, log
        assert f"award Sync {number} end" in log, log
        assert (out / f"phase-{number}.png").stat().st_size > 1000
    starts = [i for i, line in enumerate(run.stdout.splitlines()) if line.startswith("music: PCM/get_car")]
    lines = run.stdout.splitlines()
    assert len(starts) == 2, log
    for number, start in zip((1, 3), starts):
        begin = next(i for i, line in enumerate(lines) if line.startswith(f"award Sync {number} begin"))
        end = lines.index(f"award Sync {number} end")
        if negative:
            assert start > end, "Deliberate negative: late audio ordering rejected."
        else:
            assert begin < start < end, log
    assert "car pose lupo_14_02 motion=1 explicit=1" in log, log
    assert (out / "phase-2.png").read_bytes() != (out / "phase-2-late.png").read_bytes()
    assert (out / "phase-5-late.png").read_bytes() != (out / "final.png").read_bytes()
    print("PASS native certificate/prize presentation: original Sync sequence, PCM inside first wait, moving captures and OK.")
