"""Require native victories AND authored prize delivery for every GT series.

Owned cars/upgrades and driver pit decisions are test input fixtures, not
B-Spec or a change to the solver. Failed attempts remain failed in logs.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("iso")
    parser.add_argument("--bin", type=Path, default=Path("build/Release"))
    parser.add_argument("--exe", default="gtmode_series_playable.exe", help="Explicit test-driver executable; recorded in the identity")
    parser.add_argument("--controller-kind", type=int, choices=(0, 1, 2), default=0,
                        help="Original human/takeover, B-Spec or model driver; test input only")
    parser.add_argument("--pace", type=int, choices=range(5), default=4)
    parser.add_argument("--round-controllers", help="Comma-separated original controller selections, one per physical round")
    parser.add_argument("--keep-part-types", help="Keep only these owned part types from each fixture; actual admission is rechecked by the native harness")
    parser.add_argument("--round-fixtures", type=Path, help="Explicit owned car/parts per independent event round")
    parser.add_argument("--victory-inputs", type=Path, help="Reviewed input recipe for the one selected series")
    parser.add_argument("--overtake", action="store_true")
    parser.add_argument("--aids", action="store_true", help="Permit the equipped original aid levels in the test entry")
    parser.add_argument("--pit-band", type=int, choices=(1, 2, 3), default=2)
    parser.add_argument("--max-attempts", type=int, default=24, help="Diagnostic input-selection budget, never a race rule")
    parser.add_argument("--fuel-only-pits", action="store_true", help="Do not request early tyre stops; native tyre wear still runs")
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("build/career-series/victory-suite"))
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--series", action="append", help="Run only explicit series; default is all 144.")
    parser.add_argument("--exclude-series", action="append", default=[], help="Leave explicit series for separate controller input recipes")
    parser.add_argument("--reuse-passed", action="store_true")
    args = parser.parse_args()
    recipe = None
    if args.victory_inputs:
        assert args.series and len(args.series) == 1 and args.exe == "gtmode_series_pace.exe"
        recipe = json.loads(args.victory_inputs.read_text())["series"][args.series[0]]
        for name in ("controller_kind", "pace", "overtake", "aids", "pit_band", "round_controllers"):
            if name in recipe:
                setattr(args, name, recipe[name])
        if "rounds" in recipe:
            args.out.mkdir(parents=True, exist_ok=True)
            args.round_fixtures = args.out / "round-inputs.json"
            args.round_fixtures.write_text(json.dumps(recipe["rounds"], indent=2)+"\n", encoding="utf-8", newline="")
    text = args.fixtures.read_text(encoding="utf-8")
    expected = re.search(r"PASS (\d+) series / (\d+) stages", text)
    assert expected and tuple(map(int, expected.groups())) == (144, 527)
    catalog = {}
    for series, stages, choice, car, parts in re.findall(
            r"^fixture (\w+) stages=(\d+) choice=(\d+) car=(\w+) parts=([^\r\n]+)$", text, re.MULTILINE):
        catalog.setdefault(series, []).append(dict(car=car, parts=parts, stages=int(stages), choice=int(choice)))
    if os.getenv("GT4_SERIES_SUITE_NEGATIVE"):
        catalog.pop(next(iter(catalog)))
    assert len(catalog) == 144 and sum(v[0]["stages"] for v in catalog.values()) == 527, "incomplete series catalog"
    if args.series:
        assert set(args.series) <= catalog.keys()
        catalog = {key: value for key, value in catalog.items() if key in args.series}
    assert set(args.exclude_series) <= catalog.keys()
    catalog = {key: value for key, value in catalog.items() if key not in args.exclude_series}
    assert catalog
    if recipe and "car" in recipe:
        for code in catalog:
            catalog[code] = [f for f in catalog[code] if f["car"] == recipe["car"] and
                            f["parts"].replace("-", "") == recipe["parts"]]
            assert catalog[code], "recipe setup is absent from the audited legal fixture manifest"
    args.out.mkdir(parents=True, exist_ok=True)
    exe = args.bin / args.exe
    identity = hashlib.sha256(exe.read_bytes() + args.fixtures.read_bytes()).hexdigest()
    iso = Path(args.iso)
    identity += f":{iso.resolve()}:{iso.stat().st_size}:{iso.stat().st_mtime_ns}"
    strategy = None
    round_plan = None
    kept = set()
    if args.exe == "gtmode_series_pace.exe":
        strategy = dict(controller_kind=args.controller_kind, pace=args.pace, overtake=args.overtake,
                        aids=args.aids, pit_band=args.pit_band, fuel_only_pits=args.fuel_only_pits)
        identity += f":controller={args.controller_kind}:pace={args.pace}:overtake={int(args.overtake)}:aids={int(args.aids)}:pit_band={args.pit_band}"
        identity += f":fuel_only_pits={int(args.fuel_only_pits)}"
        if args.round_controllers:
            assert all(part in ("0", "1", "2") for part in args.round_controllers.split(","))
            strategy["round_controllers"] = args.round_controllers
            identity += f":round_controllers={args.round_controllers}"
        if args.keep_part_types:
            kept = {int(value) for value in args.keep_part_types.split(",")}
            assert all(0 < value < 32 for value in kept)
            strategy["keep_part_types"] = args.keep_part_types
            identity += f":keep_part_types={args.keep_part_types}"
        if args.round_fixtures:
            assert args.series and len(args.series) == 1
            plan_bytes = args.round_fixtures.read_bytes()
            round_plan = json.loads(plan_bytes)
            assert len(round_plan) == catalog[args.series[0]][0]["stages"]
            assert all(re.fullmatch(r"\w+", item["car"]) and re.fullmatch(r"(?:\d+=\d+(?:,\d+=\d+)*)?", item["parts"]) for item in round_plan)
            strategy["round_fixtures"] = str(args.round_fixtures)
            strategy["round_fixtures_sha256"] = hashlib.sha256(plan_bytes).hexdigest()
            identity += f":round_fixtures={strategy['round_fixtures_sha256']}"
    else:
        assert args.controller_kind == 0 and not args.overtake and not args.aids and args.pit_band == 2 and not args.fuel_only_pits
        assert not args.round_controllers and not args.round_fixtures and not args.keep_part_types
    prior_path = args.out / "results.json"
    prior = json.loads(prior_path.read_text(encoding="utf-8")) if args.reuse_passed and prior_path.exists() else {}
    if prior.get("identity") != identity:
        prior = {}
    records = {item["series"]: item for item in prior.get("records", [])
               if item["passed"] and item["series"] in catalog}

    def drive(item):
        series, fixtures = item
        if series in records:
            return records[series]
        # The two road tyre fixtures have equal power/weight. For endurance
        # try the harder authored compound first; this is test policy only.
        if series.startswith("en_") or series == "eur1000mile_0400":
            fixtures = sorted(fixtures, key=lambda f: (f["car"] != "formula_gran_turismo_04", "25=7,26=7" not in f["parts"]))
        attempts = []
        if round_plan:
            fixtures = [dict(round_plan[0], choice=0, stages=len(round_plan))]
        for fixture in fixtures[:args.max_attempts]:
            fixture = dict(fixture)
            if args.keep_part_types and fixture["parts"] != "-":
                fixture["parts"] = ",".join(part for part in fixture["parts"].split(",")
                                           if int(part.split("=")[0]) in kept) or "-"
            number = fixture["choice"]
            path = args.out / f"{series}.{number}.log"
            env = {key: value for key, value in os.environ.items() if not key.startswith("GT4_SERIES_")}
            if not args.fuel_only_pits:
                env["GT4_SERIES_PIT_WEAR"] = "1"
            env["GT4_SERIES_STALL_FRAMES"] = "18000"  # diagnostic budget, never a race rule
            if strategy:
                env["GT4_SERIES_CONTROLLER_KIND"] = str(args.controller_kind)
                env["GT4_SERIES_PACE"] = str(args.pace)
                env["GT4_SERIES_PIT_BAND"] = str(args.pit_band)
                if args.round_controllers:
                    assert len(args.round_controllers.split(",")) == fixture["stages"]
                    env["GT4_SERIES_ROUND_CONTROLLERS"] = args.round_controllers
                if round_plan:
                    env["GT4_SERIES_ROUND_CARS"] = ",".join(item["car"] for item in round_plan)
                    env["GT4_SERIES_ROUND_PARTS"] = "|".join(item["parts"] for item in round_plan)
                if args.aids:
                    env["GT4_SERIES_AIDS"] = "1"
                if args.overtake:
                    env["GT4_SERIES_OVERTAKE"] = "1"
            if fixture["parts"] not in ("-", ""):
                env["GT4_SERIES_PARTS"] = fixture["parts"]
            with path.open("wb") as output:
                try:
                    run = subprocess.run([str(exe), args.iso, "--physical", series, fixture["car"], "1"],
                                         env=env, stdout=output, stderr=subprocess.STDOUT, timeout=3600)
                    status = run.returncode
                except subprocess.TimeoutExpired:
                    status = -1
            log = path.read_text(encoding="utf-8", errors="replace")
            passed = status == 0 and "PASS complete physical series and authored car-prize delivery/save reload" in log
            results = [dict(race=race, frames=int(frames), rank=int(rank), award=int(award), grand=int(grand), pits=int(pits))
                       for race, frames, rank, award, grand, pits in re.findall(
                           r"physical (\w+) car=\w+ frames=(\d+) rank=(\d+) award=(\d+) grand=(\d+) pit_stops=(\d+)", log)]
            # Success also requires every stage to have physically finished.
            passed = passed and len(results) == fixture["stages"]
            attempts.append(dict(**fixture, status=status, log=str(path), results=results))
            print("PASS" if passed else "FAIL", series, fixture["car"], "choice", number,
                  "ranks", [result["rank"] for result in results], flush=True)
            if passed:
                return dict(series=series, passed=True, attempts=attempts)
        return dict(series=series, passed=False, attempts=attempts)

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        # Start time-limited events first so the 24-hour fixtures do not
        # serialize at the end of the catalog. Scheduling is test policy.
        scheduled = sorted(catalog.items(), key=lambda item: not item[0].startswith("en_"))
        futures = [pool.submit(drive, item) for item in scheduled]
        for future in concurrent.futures.as_completed(futures):
            record = future.result()
            records[record["series"]] = record
            prior_path.write_text(json.dumps(dict(identity=identity, strategy=strategy, records=list(records.values())), indent=2) + "\n",
                                  encoding="utf-8", newline="")
    passed = sum(item["passed"] for item in records.values())
    print(f"Native victories and prize delivery: {passed}/{len(catalog)} series.")
    assert passed == len(catalog), "some physical victories / car prizes remain unproved; inspect failed logs"


if __name__ == "__main__":
    main()
