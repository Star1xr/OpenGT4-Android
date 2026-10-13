#!/usr/bin/env python3
"""Test for the first recompiled chunk: crt0 + _InitSys.

Usage: python tests/initsys/run_test.py [path/to/initsys_run.exe]

Build first (needs extracted/core.bin and the splat output):
  cmake -S . -B build -DOPENGT4_RECOMP_TESTS=ON
  cmake --build build --config Release --target initsys_run

What this checks, and what it does not:

1. Reproducible: two runs give identical trace, registers and memory.
2. Internal checks: facts that follow from the game's own code, not from a
   reference: the run ends at the stop address; gp and sp are what crt0 set
   up; program text is unchanged; the code libkernl copies into kernel memory
   arrived intact (it is copied by the game's own Copy handler).
3. Lists everything a reference comparison must treat specially: stubbed
   services, values that are ours by construction (IDs, HLE addresses),
   and what threads and interrupts leave out.

It does NOT show the behaviour matches a real PS2. That needs the PCSX2
reference capture (docs/analysis/REFERENCE_CAPTURE.md), which is still pending.
"""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "extracted" / "core.bin"
OUT = ROOT / "build" / "initsys_test"
BASE = 0x00100000
TEXT_END = 0x00616F24
STOP_PC = 0x001001F0
# Code libkernl copies into kernel memory: (source in core.bin, destination, size)
KERNEL_COPIES = [(0x00658B48, 0x80076000, 0x740), (0x00659318, 0x80075000, 0x330),
                 (0x00659288, 0x00082000, 0x28)]

failures = []


def check(ok, what):
    print(("  ok    " if ok else "  FAIL  ") + what)
    if not ok:
        failures.append(what)


def run(exe, out):
    out.mkdir(parents=True, exist_ok=True)
    p = subprocess.run([str(exe), str(CORE), str(out)], capture_output=True, text=True)
    return p.returncode, p.stdout.strip()


def digest(d: Path):
    return {f.name: hashlib.sha1(f.read_bytes()).hexdigest() for f in sorted(d.iterdir())}


def main() -> int:
    exe = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "Release" / "initsys_run.exe"
    if not exe.exists() or not CORE.exists():
        print(f"missing {exe if not exe.exists() else CORE}; see the build steps in this file")
        return 2

    print("1. reproducibility")
    rc_a, msg_a = run(exe, OUT / "a")
    rc_b, _ = run(exe, OUT / "b")
    print(f"  run: {msg_a}")
    check(rc_a == 0 and rc_b == 0, "both runs end at the stop address (exit 0)")
    da, db = digest(OUT / "a"), digest(OUT / "b")
    check(da == db, "trace, registers and memory identical across runs")

    run_dir = OUT / "a"
    trace = [json.loads(l) for l in (run_dir / "trace.jsonl").read_text().splitlines()]
    regs = json.loads((run_dir / "regs.json").read_text())
    core = CORE.read_bytes()
    ram_lo = (run_dir / "ram_00000000_00100000.bin").read_bytes()
    ram = (run_dir / "ram_00100000_008a215c.bin").read_bytes()
    reg = lambda n: int(regs["gpr"][n], 16) & 0xFFFFFFFF

    print("2. internal checks (from the game's own code)")
    end = next(e for e in reversed(trace) if e["ev"] == "end")
    check(end["ev"] == "end" and int(end["pc"], 16) == STOP_PC, f"ended at {STOP_PC:#010x}")
    check(reg("gp") == 0x006DDDF0, "gp = 0x006dddf0 (crt0, header _gp)")
    setup = next(e for e in trace if e.get("name") == "SetupThread")
    check(reg("sp") == (setup["ret"] & 0xFFFFFFFF), "sp = value returned by SetupThread")
    check(ram[:TEXT_END - BASE] == core[:TEXT_END - BASE], "program text unchanged")
    for src, dst, size in KERNEL_COPIES:
        phys = dst & 0x1FFFFFFF
        got = ram_lo[phys:phys + size]
        check(got == core[src - BASE:src - BASE + size],
              f"{size:#x} bytes from {src:#010x} copied to {dst:#010x}")
    blob_calls = [e for e in trace if e["ev"] == "syscall" and "handler" in e
                  and int(e["handler"], 16) >= 0x80000000]
    checks = {e["seq"] + 1 for e in trace if e["ev"] == "code_check"}  # check comes right before the call
    check(bool(blob_calls) and all(e["seq"] in checks for e in blob_calls),
          f"copied code checked against recompiled bytes before each of {len(blob_calls)} calls into it")
    # Thread behaviour seen on PCSX2 (docs/analysis/THREAD_REFERENCE.md): the thread
    # created by _InitSys starts, runs until it blocks on a semaphore, and the
    # main thread resumes. IDs are compared by role, not by value.
    seq = [(e["ev"], e.get("name"), e.get("from"), e.get("to"), e.get("reason")) for e in trace
           if e["ev"] in ("thread_switch", "thread_wait") or e.get("name") in ("StartThread", "ChangeThreadPriority")]
    start = next((e for e in trace if e.get("name") == "StartThread"), None)
    new_id = start["ret"] if start else None
    switches = [e for e in trace if e["ev"] == "thread_switch"]
    waits = [e for e in trace if e["ev"] == "thread_wait"]
    check(start is not None and len(switches) == 2
          and switches[0]["from"] == 1 and switches[0]["to"] == new_id
          and switches[0]["reason"] == "ChangeThreadPriority"
          and len(waits) == 1 and waits[0]["id"] == new_id
          and switches[1]["from"] == new_id and switches[1]["to"] == 1 and switches[1]["reason"] == "WaitSema",
          "created thread starts, runs until WaitSema blocks it, main resumes (as on PCSX2)")
    bss = ram[0x006D5E00 - BASE:0x008A215C - BASE]
    nonzero = sum(1 for x in bss if x)
    print(f"  info  bss has {nonzero} non-zero bytes after _InitSys (written by the run)")

    summary = json.loads((run_dir / "summary.json").read_text())
    print("3. test inputs (chosen for this test, NOT PS2 values)")
    for k, v in summary["test_inputs"].items():
        if not k.startswith("_"):
            print(f"  {k}: {v}")

    print("4. limitations recorded in this run")
    unhandled = [e for e in trace if e.get("status") == "unhandled"]
    for e in unhandled:
        print(f"  {e['name']} at {e['pc']}: {e['note']}")
    for lim in summary["limitations"]:
        print(f"  {lim['what']}: num {lim['num']}, handler {lim['handler']}")
    if not unhandled and not summary["limitations"]:
        print("  none")

    print("5. what a reference comparison must handle")
    stubbed = sorted({e["name"] for e in trace if e["ev"] == "syscall" and e.get("status") == "stubbed"})
    implemented = sorted({e["name"] for e in trace if e["ev"] == "syscall" and e.get("status") == "implemented"})
    print(f"  implemented kernel calls: {', '.join(implemented)}")
    print(f"  stubbed kernel calls:     {', '.join(stubbed)}")
    print("  stubbed hardware:         timers (COUNT +1 per read, no interrupts)")
    print("  variables (ours by construction, compare by role, not value):")
    print("    thread, semaphore and handler IDs; HLE syscall table 0x80070000")
    print("    (the game stores its base at 0x00658360); timer COUNT values")
    handlers = [e for e in trace if e.get("name") == "AddIntcHandler2"]
    print(f"  not run: {len(handlers)} interrupt handler(s) (timer interrupts are never raised);")
    print("    their memory or timer effects on the real system are missing here")
    print("  reference comparison: see docs/analysis/THREADS.md (kernel context save, IDs, boot args differ by design)")

    json.dump({"implemented": implemented, "stubbed": stubbed, "failures": failures,
               "test_inputs": summary["test_inputs"], "limitations": summary["limitations"],
               "digests": da}, open(OUT / "report.json", "w"), indent=2)
    print("FAILED" if failures else "PASSED (reproducibility and internal checks only)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
