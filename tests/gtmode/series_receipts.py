"""Require complete physical victories/prizes, preserving each test identity.

Combine explicitly supplied controller/owned-car fixtures. No result is
manufactured: every receipt is checked against its executable, disc-backed
catalog and the actual native race/prize log.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("iso", type=Path)
    parser.add_argument("--run", nargs=3, action="append", required=True,
                        metavar=("RESULTS", "EXECUTABLE", "FIXTURES"))
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--expected-series", type=int, default=144,
                        help="Explicit partial proof scope; default requires the entire catalog")
    args = parser.parse_args()
    accepted, catalog, identities = {}, None, []
    for result_path, executable, fixtures in args.run:
        fixture_bytes = Path(fixtures).read_bytes()
        text = fixture_bytes.decode("utf-8")
        expected = {code: int(stages) for code, stages in re.findall(
            r"^fixture (\w+) stages=(\d+)", text, re.MULTILINE)}
        assert len(expected) == 144 and sum(expected.values()) == 527
        if catalog is None:
            catalog = expected
        assert expected == catalog
        identity = hashlib.sha256(Path(executable).read_bytes() + fixture_bytes).hexdigest()
        identity += f":{args.iso.resolve()}:{args.iso.stat().st_size}:{args.iso.stat().st_mtime_ns}"
        data = json.loads(Path(result_path).read_text(encoding="utf-8"))
        strategy = data.get("strategy")
        if strategy:
            assert strategy["controller_kind"] in (0, 1, 2) and 0 <= strategy["pace"] <= 4
            identity += (f":controller={strategy['controller_kind']}:pace={strategy['pace']}"
                         f":overtake={int(strategy['overtake'])}")
            if "aids" in strategy:
                identity += f":aids={int(strategy['aids'])}:pit_band={strategy['pit_band']}"
            if "fuel_only_pits" in strategy:
                identity += f":fuel_only_pits={int(strategy['fuel_only_pits'])}"
            if "round_controllers" in strategy:
                identity += f":round_controllers={strategy['round_controllers']}"
            if "keep_part_types" in strategy:
                identity += f":keep_part_types={strategy['keep_part_types']}"
            if "round_fixtures" in strategy:
                digest = hashlib.sha256(Path(strategy["round_fixtures"]).read_bytes()).hexdigest()
                assert digest == strategy["round_fixtures_sha256"]
                identity += f":round_fixtures={digest}"
        assert data["identity"] == identity, "executable/disc/catalog changed"
        identities.append(dict(results=result_path, executable=executable, fixtures=fixtures, identity=identity, strategy=strategy))
        for record in data["records"]:
            if not record["passed"]:
                continue
            code = record["series"]
            assert code in catalog
            attempt = record["attempts"][-1]
            assert attempt["status"] == 0
            raw = Path(attempt["log"]).read_bytes()
            log = raw.decode("utf-8", errors="replace")
            assert "PASS complete physical series and authored car-prize delivery/save reload" in log
            results = [dict(race=race, frames=int(frames), rank=int(rank), award=int(award), grand=int(grand), pits=int(pits))
                       for race, frames, rank, award, grand, pits in re.findall(
                           r"physical (\w+) car=\w+ frames=(\d+) rank=(\d+) award=(\d+) grand=(\d+) pit_stops=(\d+)", log)]
            if os.getenv("GT4_SERIES_RECEIPTS_NEGATIVE") and not accepted:
                results[0]["rank"] = 0
            assert results == attempt["results"], "receipt disagrees with the native race log"
            assert len(results) == catalog[code] == attempt["stages"]
            assert len({r["race"] for r in results}) == len(results)
            assert all(r["frames"] > 0 and 1 <= r["rank"] <= 6 for r in results)
            if int(code[-4:-2]) == 0:
                assert all(r["rank"] == 1 for r in results), "independent races need every victory"
            cars = re.findall(r"physical \w+ car=(\w+) frames=", log)
            planned = json.loads(Path(strategy["round_fixtures"]).read_text()) if strategy and "round_fixtures" in strategy else None
            assert cars == ([r["car"] for r in planned] if planned else [attempt["car"]]*len(results))
            accepted.setdefault(code, dict(series=code, identity=identity,
                                            log_sha256=hashlib.sha256(raw).hexdigest(), **attempt))
    assert 1 <= args.expected_series <= 144
    assert len(accepted) == args.expected_series, f"missing victories/prizes: {sorted(catalog.keys()-accepted.keys())}"
    stages = sum(len(r["results"]) for r in accepted.values())
    endurance = [r for code, r in accepted.items() if code.startswith("en_")]
    if args.expected_series == 144:
        assert accepted.keys() == catalog.keys() and stages == 527 and len(endurance) == 16
    frames = sum(s["frames"] for r in accepted.values() for s in r["results"])
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(dict(runs=identities, series=len(accepted), stages=stages,
                                       missing=sorted(catalog.keys()-accepted.keys()),
                                       frames=frames, records=list(accepted.values())), indent=2)+"\n",
                        encoding="utf-8", newline="")
    print(f"PASS {len(accepted)} series / {stages} physical stages / {len(endurance)} endurance prizes / {frames} frames; "
          "native victory and authored prize delivery/save reload, no injected race results.")


if __name__ == "__main__":
    main()
