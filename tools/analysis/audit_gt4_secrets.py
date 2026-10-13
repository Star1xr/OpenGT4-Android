"""Read-only inventory of authored GT4 tables and hidden input sequences.

Run from the repository root; inputs are local extracts of the user's disc.
Produces metadata, not redistributed compiled scripts or assets.
"""
import json
import sys
import subprocess
import re
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from adhoc_dis import load, Frame

KEYS = {65379: "Select", 65361: "Left", 65362: "Up", 65363: "Right",
        65364: "Down", 65480: "L1", 65481: "L2", 65490: "R1",
        65491: "R2", 65421: "Start"}

def inventory(path):
    frame = load(path.read_bytes())
    tables, commands, functions = [], [], []
    module = ""
    ins = frame.ins
    for i, (line, op, args) in enumerate(ins):
        if op == "MODULE_DEFINE":
            module = "::".join(args[0])
        if op == "FUNCTION_DEFINE":
            sub = next((a for a in args if isinstance(a, Frame)), None)
            if sub and args[0] in ("craft", "doCraft"):
                functions.append({"module": module, "name": args[0], "line": line,
                                  "operations": [{"line": ln, "op": code,
                                                  "args": aa} for ln, code, aa in sub.ins]})
        if op != "STATIC_DEFINE":
            continue
        name = args[0]
        block = []
        for entry in ins[i+1:]:
            if entry[1] in ("STATIC_DEFINE", "FUNCTION_DEFINE", "MODULE_DEFINE"):
                break
            block.append(entry)
        constants = [a[0] for _, code, a in block
                     if code in ("STRING_CONST", "INT_CONST", "FLOAT_CONST")]
        arrays = [a[0] for _, code, a in block if code == "ARRAY_CONST_OLD"]
        if name in ("craft_command", "sSecretCommand"):
            commands.append({"module": module, "line": line, "keys": constants,
                             "buttons": [KEYS.get(k, str(k)) for k in constants]})
        elif name.endswith("Events") and name != "AllEvents":
            tables.append({"module": module, "name": name, "line": line,
                           "strings": [c for c in constants if isinstance(c, str)],
                           "array_sizes": arrays})
        elif "carlist" in name or "carnum" in name or name == "getcourse_list":
            tables.append({"module": module, "name": name, "line": line,
                           "constants": constants, "array_sizes": arrays})
    return {"input": str(path), "source": frame.source, "tables": tables,
            "commands": commands, "hidden_handlers": functions}

if __name__ == "__main__":
    arguments = sys.argv[1:]
    if arguments and arguments[0] == "--used-disc":
        disc = arguments[1]
        folder = Path("build/secret-audit/usedcar")
        folder.mkdir(parents=True, exist_ok=True)
        exe = str(Path("build/Release/gt4vol.exe").resolve())
        listing = subprocess.run([exe, disc, "ls", "/usedcar"], capture_output=True,
                                 text=True, check=True).stdout
        names = re.findall(r"\b(\w+_used\d{4}\.adc)\b", listing)
        paths = []
        for name in names:
            path = folder / name
            if not path.exists():
                subprocess.run([exe, disc, "cat", "/usedcar/"+name, str(path)],
                               capture_output=True, check=True)
            paths.append(path)
        items = [inventory(path) for path in paths]
        black = []
        codes = {"_787b_91", "r92cp_92", "r390_race_98", "gt_one_race_exxon_99", "gt_one_race_99"}
        for item in items:
            columns = {t["name"]: t.get("constants", []) for t in item["tables"]}
            for name, values in columns.items():
                if not name.startswith("usedcar_") or not name.endswith("_carlist"):
                    continue
                group = name.split("_")[1]
                for i, code in enumerate(values):
                    if code in codes:
                        black.append({"script": Path(item["input"]).name, "group": group,
                                      "index": i, "code": code,
                                      "color": columns.get("color_"+group+"_carlist", [])[i]})
        target = folder.parent / "usedcar-inventory.json"
        target.write_text(json.dumps({"scripts": items, "black_car_occurrences": black},
                                    ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
        print("weekly scripts", len(items), "black occurrences", len(black))
        print(json.dumps(black, indent=2))
        sys.exit(0)
    paths = [Path(p) for p in arguments]
    target = Path("build/secret-audit/inventory.json")
    target.parent.mkdir(parents=True, exist_ok=True)
    result = [inventory(p) for p in paths]
    target.write_text(json.dumps(result, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    event_tables = [t for item in result for t in item["tables"]
                    if t["name"].endswith("Events") and t["name"] not in ("CourseEvents", "PowerAndSpeedEvents")]
    if event_tables:
        lines = ["# Inventário de eventos do GT4 — disco local", "",
                 "Extraído por `tools/analysis/audit_gt4_secrets.py`. IDs de famílias de eventos;",
                 "não são contagens de corridas individuais nem confirmação de gameplay implementado.", "",
                 "Os nomes e regulamentos localizados devem ser resolvidos pela RACE/SpecDB do disco.", ""]
        for table in event_tables:
            ids = table["strings"][1:]
            lines += ["## "+table["name"]+" ("+str(len(ids))+")", "",
                      "Origem: "+table["module"]+", linha Adhoc "+str(table["line"])+".", ""]
            lines += ["- `"+code+"`" for code in ids]
            lines += [""]
        Path("docs/GT4_EVENT_INVENTORY.md").write_text("\n".join(lines)+"\n", encoding="utf-8")
    rewards = {}
    for item in result:
        for module in ("PresentData", "PresentTimeData"):
            cols = {t["name"]: t.get("constants", []) for t in item["tables"] if t["module"] == module}
            names = ["condition", "detail", "type", "content", "content2"]
            if all(n+"_carlist" in cols for n in names):
                rewards[module] = [dict(zip(names, values)) for values in
                                   zip(*(cols[n+"_carlist"] for n in names))]
    if rewards:
        Path("docs/GT4_REWARD_INVENTORY.json").write_text(
            json.dumps({"source": "Local NTSC-U /projects/gtmode/US_carlist.adc",
                        "warning": "Authored conditions, not proof of implemented rewards", **rewards},
                       ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    for item in result:
        events = [t for t in item["tables"] if t["name"].endswith("Events")]
        print(item["input"], "event groups", len(events), "commands", len(item["commands"]))
        for table in events:
            print(table["name"], ", ".join(table["strings"]))
        for command in item["commands"]:
            print(command["module"], " -> ".join(command["buttons"]))
