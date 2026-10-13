"""Inventory real-disc UI roots and diagnostics; screenshots remain local.

Direct root opening does not supply every authored caller's arguments. This
is a discovery report, not a claim that every listed error is a production
navigation failure or that a screenshot proves original visual fidelity.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess


def main():
    p = argparse.ArgumentParser()
    p.add_argument("disc", type=Path)
    p.add_argument("--exe", type=Path, default=Path("build/career-native/Release/opengt4.exe"))
    p.add_argument("--vol-exe", type=Path, default=Path("build/Release/gt4vol.exe"))
    p.add_argument("--projects", nargs="+", default=["gtmode", "option", "arcade"])
    p.add_argument("--output", type=Path, default=Path("build/ui-audit"))
    p.add_argument("--all-projects", action="store_true", help="Discover project folders from the disc")
    p.add_argument("--inventory-only", action="store_true", help="List authored roots without opening them")
    args = p.parse_args()
    if args.all_projects:
        listing = subprocess.run([str(args.vol_exe.resolve()), str(args.disc.resolve()), "ls", "/projects"],
                                 capture_output=True, text=True, timeout=30, check=True)
        args.projects = re.findall(r"^\s+([\w-]+)/\s*$", listing.stdout, re.MULTILINE)
    args.output.mkdir(parents=True, exist_ok=True)
    records = []
    for project in args.projects:
        folder = args.output / project
        folder.mkdir(exist_ok=True)
        description = folder / "project.mproject"
        extracted = subprocess.run([str(args.vol_exe.resolve()), str(args.disc.resolve()),
                                    "cat", f"/projects/{project}/{project}.mproject", str(description)],
                                   capture_output=True, text=True, timeout=30)
        if extracted.returncode:
            records.append({"project": project, "extraction_error": extracted.stderr})
            continue
        roots = re.findall(r'RootWindow\s*\{\s*name\s+string\s*\{\s*"([^"]+)"',
                           description.read_text(encoding="utf-8", errors="replace"))
        if args.inventory_only:
            records.append({"project": project, "roots": roots,
                            "widget_counts": {name: len(re.findall(r"\b" + name + r"\s*\{", description.read_text(encoding="utf-8", errors="replace")))
                                              for name in sorted(set(re.findall(r"\b(\w+(?:Face|Box|Window))\s*\{", description.read_text(encoding="utf-8", errors="replace"))))},
                            "boundary": "authored inventory; not rendered or validated"})
            (args.output / "inventory.json").write_text(json.dumps(records, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            print(f"{project}: {len(roots)} authored roots", flush=True)
            continue
        for root in roots:
            shot = folder / (root + ".png")
            command = [str(args.exe.resolve()), str(args.disc.resolve()), project, root,
                       "--save-dir", str((folder / "isolated-saves").resolve()),
                       "--dialog-answer", "no", "--shot-delay", "0.2", "--shot", str(shot)]
            try:
                result = subprocess.run(command, capture_output=True, text=True,
                                        encoding="utf-8", errors="replace", timeout=45)
                log = result.stdout + result.stderr
                code = result.returncode
            except subprocess.TimeoutExpired as error:
                def decode(value):
                    return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else value or ""
                log = decode(error.stdout) + decode(error.stderr) + "\naudit timeout\n"
                code = None
            (folder / (root + ".log")).write_text(log, encoding="utf-8")
            diagnostics = [line for line in log.splitlines() if
                           re.search(r"script:|unsupported|not implemented|error:|audit timeout", line)]
            records.append({"project": project, "root": root, "exit_code": code,
                            "screenshot": str(shot) if shot.exists() else None,
                            "diagnostics": diagnostics,
                            "boundary": "direct root; caller-specific arguments not supplied"})
            (args.output / "report.json").write_text(json.dumps(records, indent=2, ensure_ascii=False) + "\n",
                                                     encoding="utf-8")
            print(f"{project}/{root}: exit={code}, shot={shot.exists()}, diagnostics={len(diagnostics)}", flush=True)
    report = args.output / ("inventory.json" if args.inventory_only else "report.json")
    print(f"Discovery report: {report}", flush=True)


if __name__ == "__main__":
    main()
