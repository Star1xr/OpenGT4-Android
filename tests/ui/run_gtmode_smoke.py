"""GT Mode presentation checks with a user-owned disc; no save/race changes."""
import argparse
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("disc", type=Path)
    parser.add_argument("--exe", type=Path, default=Path("build/Release/opengt4.exe"))
    args = parser.parse_args()
    output = Path("build/gtmode_smoke")
    output.mkdir(parents=True, exist_ok=True)
    cases = [
        ("mission_icon", "TopRoot", "down,down", "TopRoot focus=Mission"),
        ("home", "TopRoot", "accept", "HomeRoot focus=Garage"),
        ("home_return", "TopRoot", "accept,back,accept,back", "TopRoot focus=Home"),
        ("license", "TopRoot", "down,accept,right,back,accept", "LicenseRoot focus=l0b"),
        ("license_details", "LicenseRoot", "accept,right", "LicenseDetailRoot focus=l0002"),
        ("license_details_return", "LicenseRoot", "accept,right,back", "LicenseRoot focus=l0b"),
        ("italian_dealer", "TopRoot", "up,accept,accept", "DealerRoot focus=New"),
        ("italian_buy", "TopRoot", "up,accept,accept,accept", "BuyCarRoot focus=CarItem"),
        ("italian_buy_next_return", "TopRoot", "up,accept,accept,accept,right,left", "BuyCarRoot focus=CarItem"),
        ("dealer_return", "TopRoot", "up,accept,accept,back,back", "TopRoot focus=DealerItaly"),
        ("fiat_dealer", "TopRoot", "up,accept,right,right,accept", "DealerRoot focus=New"),
        ("direct_license", "LicenseRoot", "right", "LicenseRoot focus=lib"),
        ("home_notice", "TopRoot", "up,down", "TopRoot focus=Home"),
        ("license_notice", "TopRoot", "left,right,down,up,down", "TopRoot focus=License"),
        ("notice_hidden", "TopRoot", "up", "TopRoot focus=DealerItaly"),
    ]
    for name, root, inputs, expected in cases:
        shot = output / f"{name}.png"
        run = subprocess.run(
            [str(args.exe.resolve()), str(args.disc.resolve()), "gtmode", root,
             "--input", inputs, "--input-interval", "0.2", "--shot-delay", "0.3",
             "--shot", str(shot)],
            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=45)
        log = run.stdout + run.stderr
        (output / f"{name}.log").write_text(log, encoding="utf-8")
        assert run.returncode == 0, log
        states = [line for line in run.stdout.splitlines() if line.startswith("input ")]
        assert states and states[-1].endswith("gtmode/" + expected), log
        for missing in ("createRenderContext", "getRenderContext", "removeChild",
                        "MProgress", "MColorTransition", "MCrossTransition", "MBlockTransition"):
            assert f"undefined '{missing}'" not in log, log
        assert "'transition' of nil" not in log, log
        assert "instruction budget" not in log, log
        for missing in ("menuPush", "menuRef", "menuPop", "substr", "toString", "toInt", "MColorObject", "LicenseCarData"):
            assert f"undefined '{missing}'" not in log, log
        for missing in ("hasRidingCar", "getElapsedDate", "getElapsedWeek", "isLicensePassed", "getByRace", "getByTime"):
            assert f"undefined '{missing}'" not in log, log
        if name in ("italian_dealer", "fiat_dealer"):
            maker="alfaromeo" if name=="italian_dealer" else "fiat"
            assert f"flash: piece/tuner_screen/{maker}.strb" in log, log
            assert f"unsupported or missing FlashFace piece/tuner_screen/{maker}.strb" not in log, log
            assert re.search(r"gtmode / DealerRoot: drawn \d+, not yet drawn 0", log), log
        if name.startswith("italian_buy"):
            assert "GetMoneyString requires" not in log and "onInitialize stopped" not in log, log
            assert "showroom: _147_20_twinspark_02" in log, log
            assert "gtmode / BuyCarRoot" in log, log
        assert shot.exists() and shot.stat().st_size > 1000, log
        if name in ("license", "direct_license"):
            assert "undefined 'MChaseActor'" not in log, log
            assert "'doStart' of nil" not in log and "'doStop' of nil" not in log, log
            assert "flash: image/gtmode/license/bg1.swf, 5 images, 750 frames" in log, log
            assert "unsupported or missing FlashFace" not in log, log
        if name in ("home_notice", "license_notice"):
            assert "notice NoticeBase visible=1 opacity=1.000" in log, log
            message = "If you have GT3 game save" if name == "home_notice" else "License tests are currently"
            assert "text=" + message in log, log
            packed = re.search(r"notice bg .*rect=0.0,0.0,([\d.]+),([\d.]+)", log)
            assert packed and 180 < float(packed[1]) < 260 and 60 < float(packed[2]) <= 100, log
        if name == "notice_hidden":
            assert "notice NoticeBase visible=0" in log, log
        if name == "license_notice":
            route = [line.rsplit("focus=", 1)[-1] for line in states]
            assert route == ["GTAuto", "Home", "License", "Home", "License"], log
        print(f"{name}: passed ({expected})")


if __name__ == "__main__":
    main()
