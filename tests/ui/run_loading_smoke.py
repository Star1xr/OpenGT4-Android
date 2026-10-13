"""Disc-backed loading screens and menu continuity, with disposable profiles."""
import argparse, subprocess, shutil, tempfile, os, re
from pathlib import Path
from PIL import Image, ImageStat
p=argparse.ArgumentParser();p.add_argument("disc",type=Path);p.add_argument("--save-dir",type=Path,required=True);p.add_argument("--exe",type=Path,default=Path("build/career-native/Release/opengt4.exe"));a=p.parse_args()
out=Path("build/ui-audit/loading-routes");out.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(dir=out) as folder:
    saves=Path(folder)/"saves";shutil.copytree(a.save_dir,saves)
    cases=[("gt_enter","arcade",["--input","down,accept"],"gtmode / TopRoot"),
           ("gt_exit","gtmode",["--input","back","--dialog-answer","yes"],"arcade / TopRoot"),
           ("race_load","arcade",["--race-preview","--race-course","tsukuba","--race-car","ka_01"],"race preview:")]
    for name,project,options,expected in cases:
        loader=out/(name+"_loading.png");final=out/(name+".png");reveal=out/(name+"_reveal.png")
        cmd=[str(a.exe.resolve()),str(a.disc.resolve()),project,"TopRoot","--save-dir",str(saves),"--with-music","--loading-logo","1","--loading-shot",str(loader),"--shot-delay","3.0","--shot",str(final),*options]
        r=subprocess.run(cmd,env=dict(os.environ,OPENGT4_LOADING_TRACE="1",SDL_AUDIODRIVER="dummy",OPENGT4_LOADING_REVEAL_SHOT=str(reveal.resolve())),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding="utf-8",errors="replace",timeout=65)
        log=r.stdout;(out/(name+".log")).write_text(log,encoding="utf-8")
        assert r.returncode==0 and expected in log,log
        assert "original assets unavailable" not in log,log
        for phase in ("in","out"):
            levels=[float(x) for x in re.findall(r"loading fade: "+phase+r" ([\d.]+)",log)]
            assert levels and any(0<x<1 for x in levels),(name,phase,log)
            assert all((b>=a if phase=="in" else b<=a) for a,b in zip(levels,levels[1:])),(name,phase,levels)
            if phase=="out":assert levels[-1]<.15,(name,phase,levels)

        if name=="gt_enter":
            gaps=[float(x) for x in re.findall(r"loading work frame: gap=([\d.]+)",log)]
            assert len(gaps)>=5 and max(gaps)<250,("loading must keep presenting during preparation",gaps)
            print(f"GT preparation: {len(gaps)} loading frames, largest gap {max(gaps):.1f} ms",flush=True)
            assert "music: map sequence=" in log,log
            assert log.index("loading fade: complete") < log.index("music: map sequence="),log
        if name=="gt_exit":
            assert "music: map sequence=" not in log,log

        if name=="gt_exit":
            assert reveal.exists(),log
            def destination_light(path):
                image=Image.open(path).convert("RGB");w,h=image.size
                # Away from either loading logo/spinner: only the title can
                # illuminate this region during the reveal.
                region=image.crop((int(w*.20),int(h*.25),int(w*.35),int(h*.45)))
                return sum(ImageStat.Stat(region).mean)/3
            light=destination_light(reveal);full=destination_light(final)
            assert 1 < light < full*.85,("title must already be fading in",light,full)

        assert loader.exists(),"Loading screen missing: "+name
        im=Image.open(loader).convert("RGB");w,h=im.size
        assert im.getpixel((w//4,h//4))==(0,0,0)
        pixels=list(im.get_flattened_data());assert any(b>2*r and b>50 for r,g,b in pixels)
        assert any(r>2*b and r>50 for r,g,b in pixels)
        print(name+": original assets and destination passed",flush=True)

# Inspect both disc images independently; the normal selection remains dynamic.
for index in (0,1):
    shot=out/("logo_"+str(index)+".png")
    r=subprocess.run([str(a.exe.resolve()),str(a.disc.resolve()),"arcade","TopRoot","--loading-preview","--loading-logo",str(index),"--shot",str(shot)],capture_output=True,text=True,timeout=35)
    assert r.returncode==0,r.stderr
    im=Image.open(shot).convert("RGB");w,h=im.size
    center=im.crop((w//3,h//3,2*w//3,3*h//5))
    pixels=list(center.get_flattened_data())
    if index==0:
        assert any(r>100 and abs(r-g)<3 and abs(g-b)<3 for r,g,b in pixels)
        assert not any(r>2*b and r>50 for r,g,b in pixels)
    else:
        assert any(r>2*b and r>50 for r,g,b in pixels)
    print("logo "+str(index)+": passed",flush=True)
