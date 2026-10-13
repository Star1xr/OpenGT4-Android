"""Native onboarding routes, with disposable profiles and original disc UI."""
import argparse
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser()
p.add_argument("disc",type=Path)
p.add_argument("--exe",type=Path,default=Path("build/career-native/Release/opengt4.exe"))
a=p.parse_args()
out=Path("build/ui-audit/first-run-tests");out.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(dir=out) as temporary:
    base=Path(temporary)
    def run(name,inputs=None,folder=None,first=True):
        saves=folder or base/name
        command=[str(a.exe.resolve()),str(a.disc.resolve()),"save" if first else "boot",
                 "--save-dir",str(saves.resolve()),"--shot-delay","0.5","--shot",str(out/(name+".png"))]
        if first: command += ["--first-run"]
        else: command += ["--intro"]
        if inputs: command += ["--input",inputs]
        result=subprocess.run(command,capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=45)
        log=result.stdout+result.stderr;(out/(name+".log")).write_text(log,encoding="utf-8")
        assert result.returncode==0 and "error:" not in log,log
        assert (out/(name+".png")).stat().st_size>1000
        return log,saves
    log,saves=run("welcome");assert "save / LoadRoot" in log
    assert not list(saves.glob("slot*.ogt4"))
    log,saves=run("cancel","accept,back");assert "save/LoadRoot focus=Begin" in log
    assert not list(saves.glob("slot*.ogt4"))
    log,saves=run("guest","right,down,accept");assert "boot/MovieRoot" in log
    assert not list(saves.glob("slot*.ogt4"))
    log,saves=run("profiles","down,accept");assert "focus=SaveCard1" in log
    log,saves=run("create","accept,accept,"+"right,"*10+"down,accept")
    assert "boot/MovieRoot" in log and (saves/"slot01.ogt4").exists(),log
    # Serialized username: string tag + little-endian length + capitalized Q.
    assert b"username\x04\x01\x00\x00\x00Q" in (saves/"slot01.ogt4").read_bytes()
    before={file.name:file.read_bytes() for file in saves.glob("*.ogt4")}
    log,_=run("returning",folder=saves,first=False)
    assert "boot / MovieRoot" in log and "Welcome" not in log
    # Normal Auto Save may persist presentation/options changes on exit.
    # It must retain the existing slots rather than create another profile.
    assert set(before)=={file.name for file in saves.glob("*.ogt4")}
    assert (saves/"slot01.ogt4").stat().st_size>0
print("Welcome, cancellation, guest, profile selection, creation and returning-player routes: passed")
