#!/bin/sh
# The physics' everyday checks against the original (recompiled) code.
# Run from the repository root in Git Bash; outputs go to $OUT.
#
#   sh tests/physics/compare.sh build
#       build opengt4_physics, opengt4_recomp_physics and physics_run
#       straight with MSBuild (no CMake regeneration)
#   sh tests/physics/compare.sh old
#       build $OUT/pr_old.exe from HEAD's physics sources (the working
#       tree's changes put aside and back), then the working tree again
#   sh tests/physics/compare.sh arcade [frames] [VAR=value ...]
#       the six Arcade captures (captures/physics/ar_*), original only
#       against the rewrite: every car every step, and the whole RAM at the
#       end (the original's stack excluded). Env settings (PHYS_POKE_CARS=...,
#       PHYS_HEADLESS=1...) go to both runs.
#   sh tests/physics/compare.sh built <car hex> <course> [frames]
#       a race built from the disc ($ISO), pr_old.exe against the current
#       build: the RAM after setup and steps, and the cars. Three words of
#       the engine sound object (left out headless) and the type_info
#       objects the original's dynamic_cast builds on demand are ignored.
#
# Other checks: PHYS_AB=<case>:<n> (physics_run, made-up states, see
# ab_test in step_run.c), PHYS_BRIDGE_LOG=<file> (each call the rewrite
# still makes to the original), OPENGT4_COVERAGE=<file>.
set -u
ISO=${ISO:?set ISO to your own disc image}
OUT=${OUT:-build/physics_cmp}
PY=${PY:-.venv/Scripts/python}
MSB=${MSB:-"/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe"}
mkdir -p "$OUT"

build() {
    for p in opengt4_physics opengt4_recomp_physics physics_run; do
        "$MSB" build/$p.vcxproj -p:Configuration=Release -p:BuildProjectReferences=false -v:minimal -nologo 2>&1 | grep -E " error|warning C"
    done
    return 0
}

case "${1:-}" in
build)
    build
    ;;
old)
    files=$(git diff --name-only HEAD -- src/core/physics tests/physics)
    mkdir -p "$OUT/cur"
    for f in $files; do mkdir -p "$OUT/cur/$(dirname "$f")"; cp "$f" "$OUT/cur/$f"; git show "HEAD:$f" > "$f"; done
    build
    cp build/Release/physics_run.exe "$OUT/pr_old.exe"
    for f in $files; do cp "$OUT/cur/$f" "$f"; done
    build
    echo "pr_old.exe built from HEAD; put back: $files"
    ;;
arcade)
    F=${2:-300}
    shift; [ $# -gt 0 ] && shift
    for t in ar_02 ar_03 ar_04 ar_05 ar_04b ar_05b; do
        d="$OUT/$t"; mkdir -p "$d/o" "$d/n"
        env "$@" timeout 900 build/Release/physics_run.exe captures/physics/$t.eeMemory.bin captures/physics/$t.Scratchpad.bin 0x1cf8ed0 "$F" "$d/o" > /dev/null 2>&1 || echo "$t original failed"
        env "$@" timeout 900 build/Release/physics_run.exe captures/physics/$t.eeMemory.bin captures/physics/$t.Scratchpad.bin 0x1cf8ed0 "$F" "$d/n" all > /dev/null 2>&1 || echo "$t native failed"
        "$PY" - "$d" <<'EOF'
import os, sys
d = sys.argv[1]; CS = 0x10a0; N = 6
a = open(d + '/o/cars.bin', 'rb').read(); b = open(d + '/n/cars.bin', 'rb').read()
bad = []
for car in range(N):
    for f in range(min(len(a), len(b)) // (CS * N)):
        o = (f * N + car) * CS
        if a[o:o + CS] != b[o:o + CS]:
            bad.append('car %d step %d %s' % (car, f, [hex(i) for i in range(0, CS, 4) if a[o + i:o + i + 4] != b[o + i:o + i + 4]][:6]))
            break
name = os.path.basename(d)
if os.path.exists(d + '/o/ram.bin') and os.path.exists(d + '/n/ram.bin'):
    ra = open(d + '/o/ram.bin', 'rb').read(); rb = open(d + '/n/ram.bin', 'rb').read()
    diff = [i for i in range(0, len(ra), 4) if ra[i:i + 4] != rb[i:i + 4] and not (0x1ee0000 <= i < 0x1ef0000)]
    print(name, 'cars', 'SAME' if not bad else bad, 'ram words differ', len(diff), [hex(x) for x in diff[:8]])
else:
    # the original stops where it meets an unrecompiled VU0 effect
    print(name, 'stopped: steps', len(a) // (CS * N), len(b) // (CS * N), 'cars', 'SAME' if not bad else bad)
EOF
        rm -f "$d"/o/*.bin "$d"/n/*.bin "$d"/o/trace.jsonl "$d"/n/trace.jsonl
    done
    ;;
built)
    car=$2; crs=$3; F=${4:-300}
    d="$OUT/$car$crs"; mkdir -p "$d/o" "$d/n"
    [ -f "$OUT/pr_old.exe" ] || { echo "no $OUT/pr_old.exe: run '$0 old' first"; exit 2; }
    for w in o n; do
        exe=build/Release/physics_run.exe; [ $w = o ] && exe="$OUT/pr_old.exe"
        PHYS_HEADLESS=1 PHYS_FULLINIT=1 PHYS_BUILD="$ISO" PHYS_DISC="$ISO" PHYS_COURSE=$crs PHYS_CAR=0x$car PHYS_INPUT_SEED=7 \
            timeout 600 "$exe" - - 0x1bf7860 "$F" "$d/$w" all > "$d/$w.log" 2>&1 || echo "$car $crs $w failed"
    done
    "$PY" - "$d" <<'EOF'
import os, sys
d = sys.argv[1]; CS = 0x10a0
a = open(d + '/o/ram.bin', 'rb').read(); b = open(d + '/n/ram.bin', 'rb').read()
SOUND = {0xd0c, 0x1070, 0x107c}
car0 = 0x1bf7860 + 0x6698
def sound(i): r = i - car0; return 0 <= r < 6 * CS and r % CS in SOUND
def rtti(i): return 0x88d000 <= i < 0x88fa00 or 0x6d5e08 <= i < 0x6d5e10
diff = [i for i in range(0, len(a), 4) if a[i:i + 4] != b[i:i + 4] and not (0x1ee0000 <= i < 0x1ef0000) and not sound(i) and not rtti(i)]
ca = open(d + '/o/cars.bin', 'rb').read(); cb = open(d + '/n/cars.bin', 'rb').read()
cd = [i for i in range(0, min(len(ca), len(cb)), 4) if ca[i:i + 4] != cb[i:i + 4] and (i % CS) not in SOUND]
print(os.path.basename(d), 'cars', 'SAME' if not cd and len(ca) == len(cb) else 'DIFFER %s' % [hex(i % CS) for i in cd[:6]],
      'ram words differ', len(diff), [hex(x) for x in diff[:10]])
EOF
    rm -f "$d"/o/*.bin "$d"/n/*.bin
    ;;
*)
    sed -n '2,30p' "$0"
    exit 2
    ;;
esac
