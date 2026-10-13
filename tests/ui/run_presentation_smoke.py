"""Real-disc async replacement and original paint variants; local captures only."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser();p.add_argument("disc",type=Path);args=p.parse_args()
out=Path("build/presentation_smoke");out.mkdir(parents=True,exist_ok=True)
base="accept,accept,accept,accept,down,accept"
cases=[
    ("rapid_courses","accept,accept,accept,right,right,left,right,back,down,accept",0,"selected course=capri"),
    ("rapid_cars",base+",right,right,left",0,"selected car=ka_01"),
    ("paint_blue",base+",right",0,"car color=Capri-Blau  (1/11)"),
    ("paint_alternative",base+",right",2,"car color=Lugano-Blau Metallic  (3/11)"),
    ("paint_wrap",base+",right",11,"car color=Capri-Blau  (1/11)"),
]
for name,inputs,color,expected in cases:
    result=subprocess.run([str(Path("build/Release/opengt4.exe").resolve()),str(args.disc.resolve()),"arcade","TopRoot","--preview-courses","--input",inputs,"--input-interval","0.06","--color",str(color),"--shot",str(out/(name+".png"))],capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=30)
    log=result.stdout+result.stderr;(out/(name+".log")).write_text(log,encoding="utf-8")
    assert result.returncode==0 and expected in log and "timed out" not in log,log
    if "paint" in name or name=="rapid_cars":assert "showroom: ka_01 model=fdeu0006" in log,log
    else:assert "movie frames=" in log,log
    assert (out/(name+".png")).stat().st_size>1000
    print(name+": passed")
