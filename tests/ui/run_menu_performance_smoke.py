"""Driving Missions draw submission and frontend pacing regression on the real disc."""
import argparse, os, re, subprocess
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("disc",type=Path)
p.add_argument("--save-dir",type=Path,required=True)
p.add_argument("--exe",type=Path,default=Path("build/Release/opengt4.exe"))
p.add_argument("--baseline",type=Path)
a=p.parse_args()
out=Path("build/ui-audit/menu-performance");out.mkdir(parents=True,exist_ok=True)
# Profiling synchronizes only diagnostic runs; frame work excludes pacing/swap.
env=dict(os.environ,OPENGT4_VIDEO_PROFILE="1")
shot=out/"missions.png"
result=subprocess.run([str(a.exe.resolve()),str(a.disc.resolve()),"gtmode","MissionRoot",
    "--save-dir",str(a.save_dir.resolve()),"--no-config","--vsync","off",
    "--ui-filter","sharp","--window","1280x720","--shot-delay","3","--shot",str(shot.resolve())],
    env=env,capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=45)
log=result.stdout+result.stderr
(out/"missions.log").write_text(log,encoding="utf-8")
assert result.returncode==0 and "gtmode / MissionRoot" in log,log
samples=re.findall(r"video profile: ([\d.]+) ms/frame, ([\d.]+) draws/frame, ([\d.]+) quads/frame, ([\d.]+) fps",log)
assert len(samples)>=2,log
# No hardware-dependent frame-time target. Reject thousands of invisible draws
# and uncapped frontend rendering after the initial warm-up sample.
for ms,draws,quads,fps in samples[1:]:
    assert float(draws)<2000,(draws,log)
    assert 0<float(fps)<=65,(fps,log)
if a.baseline:
    from PIL import Image, ImageChops
    before=Image.open(a.baseline).convert("RGBA")
    after=Image.open(shot).convert("RGBA")
    assert before.size==after.size
    assert ImageChops.difference(before,after).convert("RGB").getbbox() is None
print("Driving Missions: visible draw submission and 60 Hz pacing passed")
if a.baseline:print("Pixels identical to baseline")
