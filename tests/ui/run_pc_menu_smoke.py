"""PC menu additions using original widgets and disposable native profiles."""
import argparse, shutil, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("disc",type=Path)
p.add_argument("--save-dir",type=Path,required=True)
p.add_argument("--exe",type=Path,default=Path("build/career-native/Release/opengt4.exe"))
a=p.parse_args();out=Path("build/ui-audit/pc-menu");out.mkdir(parents=True,exist_ok=True)
source={f.name:f.read_bytes() for f in a.save_dir.glob("*.ogt4")}
with tempfile.TemporaryDirectory(dir=out) as temporary:
    saves=Path(temporary)/"saves";shutil.copytree(a.save_dir,saves)
    def run(name,project,inputs,expected,answer="no"):
        command=[str(a.exe.resolve()),str(a.disc.resolve()),project,"TopRoot" if project=="arcade" else "OptionRoot",
                 "--save-dir",str(saves.resolve()),"--input",inputs,"--dialog-answer",answer,
                 "--shot-delay","0.6","--shot",str(out/(name+".png"))]
        result=subprocess.run(command,capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=40)
        log=result.stdout+result.stderr;(out/(name+".log")).write_text(log,encoding="utf-8")
        assert result.returncode==0 and expected in log,log
        assert "stopped with an error" not in log and "error:" not in log,log
        print(name+": passed",flush=True)
        return log
    # Race -> Arcade row -> Photo Lab -> profile management below it.
    run("settings_profiles","option","down,down,down,accept","save / LoadRoot")
    run("new_career_entry","arcade","down,"*3+"accept","save / EntryRoot")
    existing={f.name:f.read_bytes() for f in saves.glob("slot*.ogt4")}
    run("new_career_created","arcade","down,"*3+"accept,accept,"+"right,"*10+"down,accept","gtmode / TopRoot")
    created=set(f.name for f in saves.glob("slot*.ogt4"))-set(existing)
    assert len(created)==1,created
    blob=(saves/created.pop()).read_bytes()
    assert b"username\x04\x01\x00\x00\x00Q" in blob
    # Older profile payloads may receive normal Auto Save timestamp/header updates.
    for name,before in existing.items():
        assert (saves/name).read_bytes()[32:]==before[32:],name
    run("quit_cancel","arcade","down,"*7+"accept","arcade / TopRoot")
    run("quit_confirm","arcade","down,"*7+"accept","result=1","yes")
assert {f.name:f.read_bytes() for f in a.save_dir.glob("*.ogt4")}==source
print("Profile management, new career, retained prior profile, and quit routes: passed")
