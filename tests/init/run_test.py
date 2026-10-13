#!/usr/bin/env python3
"""Exercise crt0 + _InitSys + _init against synthetic SIF/ROMVER fixtures.

Not a PS2/PCSX2 reference comparison. Requires the user's extracted core.bin.
Build: cmake --build build --config Release --target init_run
"""
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x100000
TEXT_END = 0x616F24
OUT = ROOT / "build" / "init_test"


def check(ok, why):
    if not ok:
        raise AssertionError(why)


def digest(folder):
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in folder.iterdir() if p.is_file()}


def run(exe, fresh, kind, delay, repetition):
    folder = OUT / f"{fresh}_{kind}_{delay}" / repetition
    folder.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(exe), str(ROOT / "extracted/core.bin"), str(folder),
                             str(fresh), kind, str(delay)], capture_output=True,
                            text=True, timeout=30)
    check(result.returncode == 0, result.stdout + result.stderr)
    return folder


def main():
    exe = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build/Release/init_run.exe"
    core = (ROOT / "extracted/core.bin").read_bytes()
    report = {"reference_validation": "pending", "cases": []}
    osd_calls = {}
    for fresh in (0, 1):
        for kind in ("retail", "tool", "missing"):
            for delay in (0, 7):
                a = run(exe, fresh, kind, delay, "a")
                b = run(exe, fresh, kind, delay, "b")
                check(digest(a) == digest(b), "non-reproducible output")
                trace = [json.loads(line) for line in (a / "trace.jsonl").read_text().splitlines()]
                summary = json.loads((a / "summary.json").read_text())
                regs = json.loads((a / "regs.json").read_text())["gpr"]
                ram = (a / "ram_00100000_008a215c.bin").read_bytes()
                kernel = (a / "ram_00000000_00100000.bin").read_bytes()
                at = lambda addr, n: ram[addr - BASE:addr - BASE + n]
                u32 = lambda addr: struct.unpack("<I", at(addr, 4))[0]
                check(summary["end_reason"] == 1 and summary["end_pc"] == "0x00100204", "stop point")
                check(summary["reference_validation"] == "pending", "must not claim reference validation")
                check(ram[:TEXT_END - BASE] == core[:TEXT_END - BASE], "text modified")
                check(int(regs["gp"], 16) & 0xFFFFFFFF == 0x6DDDF0, "gp not restored")
                setup = next(e for e in trace if e.get("name") == "SetupThread")
                check(int(regs["sp"], 16) & 0xFFFFFFFF == setup["ret"], "stack not balanced")
                for src, dst, size in ((0x658B48, 0x76000, 0x740),
                                       (0x659318, 0x75000, 0x330), (0x659288, 0x82000, 0x28)):
                    check(kernel[dst:dst+size] == core[src-BASE:src-BASE+size], "kernel copy changed")
                wanted = b"\0" * 14 if kind == "missing" else (
                    b"0000T000000000" if kind == "tool" else b"0000A000000000")
                check(at(0x657AD0, 14) == wanted, "ROMVER not copied by guest handler")
                check(at(0x8899E8, 4) == b"3000" and u32(0x658340) == 1, "FILEIO handshake")
                check(any(at(0x8744D0, 0x40)), "allocator not initialized")
                calls = [e["fno"] for e in trace if e["ev"] == "fileio"]
                check(calls == ([255, 0, 0] if kind == "missing" else [255, 0, 2, 1]), "RPC sequence")
                sent = [bytes.fromhex(e["bytes"]) for e in trace if e["ev"] == "sif_send"]
                ids = [struct.unpack_from("<I", p, 8)[0] for p in sent]
                check(ids[0] == (0x80000002 if fresh else 0x80000000), "SIF init path")
                check((0x80000002 in ids[1:]) == bool(fresh), "RPC init path")
                recv = [bytes.fromhex(e["bytes"]) for e in trace if e["ev"] == "sif_receive"]
                done = [p for p in recv if struct.unpack_from("<I", p, 8)[0] == 0x80000011]
                check([struct.unpack_from("<I", p, 12)[0] for p in done] ==
                      ([0, 1] if kind == "missing" else [0, 1, 0]), "completion slot alternation")
                check([struct.unpack_from("<I", p)[0] >> 8 for p in done] ==
                      ([0x20, 0x20] if kind == "missing" else [0x20, 0xA4, 0x20]), "completion record sizes")
                check(summary["sif_result"]["pending"] == 0 and
                      summary["sif_result"]["file_open"] == 0, "unfinished IOP operation")
                check(sum(e["ev"] == "sif_irq" for e in trace) == len(recv), "reply bypassed IRQ handler")
                check(not any(e.get("name") == "DelayThread" for e in trace), "unexpected retry")
                check([e["num"] for e in summary["limitations"]] == ["0x12c"], "known limitation missing")
                osd_calls[fresh, kind, delay] = sum(e.get("name") == "GetOsdConfigParam" for e in trace)
                report["cases"].append({"fresh": fresh, "kind": kind, "delay": delay,
                                        "passed": True, "digests": digest(a)})
                print(f"PASS fresh={fresh} ROMVER={kind} delay={delay}: stop, memory, RPC, IRQs, reproducibility")
    for fresh in (0, 1):
        for delay in (0, 7):
            check(osd_calls[fresh, "retail", delay] > osd_calls[fresh, "tool", delay],
                  "ROMVER development marker did not change OSD path")
            check(osd_calls[fresh, "missing", delay] == osd_calls[fresh, "retail", delay],
                  "failed open did not use OSD fallback")
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("PASSED: 12 scenarios, each run twice. PS2/PCSX2 equivalence is NOT established.")


if __name__ == "__main__":
    main()
