# GT Mode: original logic validation

Rules are complete only after original-code comparison and the same action
with the same PS2 save. Script contracts/UI smoke tests alone are not proof.
This document supersedes any claim that inferred GT Mode rules are original.

## Current delivery status

| Requested rule | Original source and C | Verified scope | Still required |
|---|---|---|---|
| Credits | 0043AFB0 / 00163990, career_original.c | 1154 full-memory arithmetic cases; 4352 identical signed affordability spans | Adhoc semantics and same actual transaction |
| Used/sale prices | 00441128 / 004411F0, career_prices.c | 134292 equal returned prices | Other key namespaces, memory effects, weekly inventory and actual transactions; getUsedPrice/getUsedSellPrice adapters now bound |
| Garage capacity | 00435348 / 00435920 / 00435860, career_original.c | 9040 count/free cases; 4000 delete cases + 2 imported handler cases; 1000 slots; native adapters integrated | Full insertion/selection and import mapping |
| Packed results | 00430958 / 00430A10 / 00430A20 / 00430A58 / 00430B58 / 00430BC0 / 00430C50, career_original.c | 65536 full-memory cases | Native record binding, result producers and actual event/license action |
| Grade predicate and thresholds | 00430C28 / 00447670 / 004476B0 / 004476F0, career_original.c | 262144 grade returns; 5790 timing returns/input checks; exact disc-label imported-record adapter integrated | Result producers and real actions; aggregate/fallback return validation below now passes |
| Oil state update | 00343928 and callees, career_oil.c | 3843 primitive cases + 2 full-memory imported service cases | Complete UI action, non-selected/new-car wear, power response |
| Date conversion and elapsed days/weeks | 005797A8 / 00579898 / 0042F4F0 / 0042F530 / 0042F0E0, career_calendar.c | 560000 date returns; 4096 full-memory inverse cases; 6144 elapsed cases; 8192 append cases; disc-backed date adapters integrated | Full event generation and same-save transactions |
| Acquisition day | 00435148 / 0042F4C8, career_calendar.c | 2048 full-memory cases | Original full insertion and actual purchase |
| Per-position cash prize | 004475A8, career_original.c | 2048 full-memory/return cases | Award integration, prize cars and championships; runtime race data now integrated |
| Odometer assignment | 003434A8, career_original.c | 2048 full-memory cases | Full original car initialization |
| Entry rules, opponents, A-Spec calculation and tuning effects | Entry/part callbacks located below; complete rule ports still pending | Zero equivalence cases for complete rules | Full ports, runtime disc tables, oracle and same-save actions |

These numbers count independent input cases across the tested captures, not
complete playthroughs. Credit arithmetic, garage count/free-slot selection,
both date conversions and elapsed days/weeks are integrated into the native handler.
Used/sale prices are now bound to resolved GENERIC_CAR garage objects. Oil replacement is bound for imported retail selected cars with preserved original wear. Other ports remain callable C primitives pending their runtime adapters. Existing UI contract tests do not
validate the unported rules. Menu navigation and GT Mode contract regression
tests pass, and the application builds separately from all EE oracle targets.

## Reference states

The user's PCSX2 slots 1 and 2 from 30 September 2026 were copied locally to
`captures/gtmode/20260930/slot01` and `slot02`. Their embedded screenshots show
the GT Mode map and Home respectively, both with Cr. 10,000 on 2 April.
Slots 3 and 4 were subsequently collected from Home and GT Auto, with a Honda CR-X SiR '90, Cr. 4,586 and 3 April (two game days). The calendar object is profile+0x220; its current day at +0xb6c0 is 2453463, which the original decodes as 2005-04-03. This is one less than conventional Julian numbering. PS2 import now uses the disc-backed original decoder when available and compensates this convention in its fallback.
The source files are preserved. Captures, screenshots, extracted memory and
generated C remain ignored. `source.json` records each source path and SHA256.

## Calendar conversions and elapsed time

`career_calendar.c` ports func_005797A8 from assembly, including signed
month normalization, truncating divisions and wrapping 32-bit operations.
The 26 month offsets come directly from CORE.GT4 at 0x006CC728, with
instruction/layout checks before accepting the data. There is no embedded
copy of the extracted table. It also implements func_0042F4F0 (current day
minus the original conversion of 2005-04-02) and func_0042F530 (signed
division by seven). The live native handler derives elapsed time from `today`
instead of trusting an independently incremented `elapsed_days` counter.

`gtmode_calendar_oracle` runs on both captures: 140000 date cases each,
covering years 1..10000, all months, days including overflow, and 20000
random signed year/month/day tuples. All returned 32-bit results match.
The date routine is a leaf with no memory writes; full RAM/SPR remain
identical after every 512 calls and at the end. Each capture additionally
passes 1536 elapsed-day/week cases, including signed boundary patterns:
full RAM/SPR compare per case with an adapter for original dead stack stores.
All invoked function ranges are checked against the captured instructions.
The current generated closure has eight functions and five unreachable division
break sites; no syscall or hardware replacement is used.

The harness also checks Adhoc month normalization and verifies that elapsed
queries use `today` when a stale PC `elapsed_days` field disagrees.

Run `build/Release/gtmode_calendar_oracle.exe RAM SPR ISO` for each capture.
Slots 3 and 4 additionally pass 2048 inverse-date cases each, comparing full RAM/SPR with only dead ABI stack stores adapted. func_00579898 and its binary search func_00579708 use the year/month offset tables read directly from the disc. Normal dates around the captured day and random signed 32-bit day patterns are covered. The harness checks Adhoc day/date conversion and the captured date explicitly.
This does not validate event payloads, used inventory turnover,
or a transaction performed in both games. Those remain pending.

Further original calendar path located: putBuyNewCarEvent callback
0x00134C60 calls 0x0042F1B8; event insertion 0x0042F0E0 appends a 16-byte
record to a 2922-entry ring and increments the current day. Its ring helpers
are 0x006009F0 / 0x00600A18. The append primitive is ported and passes 2048 full-memory cases per capture: empty/one/nearly-full/full rings, wrap positions, random payloads and day overflow. It is not yet integrated into the PC event producers; event construction and car/event key resolution remain uncertified.

## Credit increment: primitive verified, action comparison pending

Registration: `incMoney`, string 0x006905f0, registration site 0x001697bc,
callback `func_001638D8`, implementation `func_0043AFB0`.
The callback converts its argument through the Adhoc value's virtual method;
the implementation gets credits via `func_0043AF68` / `func_004359E0`, adds
with `daddu` at 0x0043afd0, and stores via `func_004359B0`.
It has no saturation or overdraft rejection. The garage is profile+0xb8e8;
its encoded balance is at +0x81e8, XOR key 0x7a726f706c6c525e.

C: `src/core/career_original.c`, functions money_get, money_set, credit_add,
money_inc. The native script handler uses this same credit_add operation.
PC save serialization preserves the 64-bit pattern, including bit 63; it
does not reject a balance just because its signed container appears negative.
Argument conversion, script return convention and save bookkeeping are not
part of the primitive validation.

Oracle: `tests/gtmode/oracle_run.c`, separate `gtmode_oracle` executable under
OPENGT4_RECOMP_TESTS. Only that target links generated original code and EE.
`opengt4` does not link the oracle. Generated code is local build/gen/gtmode.c.

577 independent cases per snapshot, 1,154 total: unmodified snapshot balance,
8x8 boundary matrix and 512 deterministic random balance/delta bit patterns.
All 32 MiB of RAM and all 16 KiB of scratchpad matched byte for byte per case.
The adapter reproduces the original ABI's dead stack stores so full RAM can
be compared; those stack stores are not in the production C implementation.
No kernel or hardware stubs were needed; the generated closure has four
functions and zero unsupported sites. Profile address was 0x00a0cf58 in both
snapshots, identified from the unique encoded balance and profile name.
Before calling the original, the harness verifies captured instruction bytes
for all four functions against extracted/core.bin; mismatches abort the test.

Run after building gtmode_oracle:

```
build/Release/gtmode_oracle.exe captures/gtmode/20260930/slot01/eeMemory.bin captures/gtmode/20260930/slot01/Scratchpad.bin 0xa0cf58
build/Release/gtmode_oracle.exe captures/gtmode/20260930/slot02/eeMemory.bin captures/gtmode/20260930/slot02/Scratchpad.bin 0xa0cf58
```

Remaining for credits: compare an actual purchase/payout in both applications
with the same PS2 save; verify full Adhoc argument and return semantics.

## Packed race/license records: primitive checks, integration pending

C: `src/core/career_original.c`. The packed record is a 32-bit word;
type is its low nibble, best result bits 4..7, latest result bits 8..11,
present result bits 12..15 and score the byte at offset 2. Original type
names at D_00622D80 are `none`, `license`, `event`, `mission`. The generic
predicates below retain address-based C names until their callers and UI
semantics are validated; their numeric results must not be interpreted as
medal names merely from apparent behavior.

| Original | Corresponding C | Cases per snapshot |
|---|---|---:|
| func_00430C28 | record_meets_grade | 131,072 |
| func_00430958 | record_init | 4,096 |
| func_00430A10 | record_present | 4,096 |
| func_00430A20 | record_score | 4,096 |
| func_00430A58 | record_update | 8,192 |
| func_00430BC0 | record_predicate_00430bc0 | 4,096 |
| func_00430C50 | record_predicate_00430c50 | 4,096 |
| func_00430B58 | record_predicate_00430b58 | 4,096 |

`gtmode_grade_oracle` exhausts all 256 record bytes against all 256 limits,
then repeats with nonzero argument bits above the low byte. Every return
matches in both snapshots (262,144 total); full RAM and SPR are checked
after each 512-call group. The original leaf has no store instructions.

`gtmode_record_oracle` verifies return values where defined and compares
all 32 MiB RAM and all SPR after every case. Its only ABI adapter replicates
the two dead stack words written by func_00430A58. Inputs include arbitrary
upper record bytes, all low-byte type/grade combinations, signed score
boundaries and random words. Original instruction bytes are verified against
each snapshot before running. All 32,768 cases on each snapshot passed
(65,536 total). No game-logic stubs are used;
the eight record functions have zero unsupported sites (the shared generated
file also includes the two garage leaf functions).

Important behavior: a matching type updates the latest result even when the
best result does not improve. Type zero initializes both results. A mismatched
nonzero type leaves the record untouched. Scores clamp above 255 using a
signed comparison, then compare the low byte. Negative values therefore do
not behave like an added lower clamp. Initialization clears the type and
score while preserving the top byte. These ports have not replaced the
provisional string-valued runtime records yet.

Build and run independently of the application:

```
cmake --build build --config Release --target gtmode_grade_oracle gtmode_record_oracle
build/Release/gtmode_grade_oracle.exe captures/gtmode/20260930/slot01/eeMemory.bin captures/gtmode/20260930/slot01/Scratchpad.bin
build/Release/gtmode_record_oracle.exe captures/gtmode/20260930/slot01/eeMemory.bin captures/gtmode/20260930/slot01/Scratchpad.bin
```

Still pending: Adhoc record getter/setter conversions, race-name to SpecDB
record index, whole-category license queries, thresholds from RACE data,
producer of results, native save integration and same-action PCSX2 comparison.
These primitive results do not validate an entire license or event rule.

## Located registrations; full rule equivalence pending

| Method | Callback | Status |
|---|---|---|
| getElapsedDate | 0x00133288 | Calendar rule not validated |
| putBuyNewCarEvent | 0x00134c60 | Day advancement not validated |
| getUsedPrice | 0x0013e9e0 | Price rule not validated |
| getUsedSellPrice | 0x0013ea80 | Sale rule not validated |
| replaceOil | 0x0014eb38 | Oil effect not validated |
| canTryLastLicense | 0x001a64e0 | License gate not validated |

Following the callbacks resolves used price to `func_00441128`, used sale
price to `func_004411F0`, oil replacement to `func_00343928`, and the last
license gate to `func_004312D8` / `func_004311A0`. Price and oil validation are
detailed below; whole-category license queries remain identified paths only.

`tools/find_gtmode_natives.py` locates string references and subsequent
registration callbacks in the user's disassembly. Addresses found by this
tool are candidates; inspect the callback and follow its C++ callee before
porting. The Sony-library naming tool remains unchanged.

## Used and sale price: matching values, full action/memory comparison pending

C: `src/core/career_prices.c`. Used price follows `func_00441128`'s unsigned
odometer division, signed threshold comparisons and negative sentinel. The
base price is the signed word at GENERIC_CAR row offset 8, reached originally
through `func_00441280`, `func_00445350`, `func_004487D8` and `func_00448868`.
Sale price follows `func_004411F0`: use that used price, convert to float,
multiply by 0.25 and convert back to integer. Bit arithmetic preserves each
individual PS2 toward-zero rounding; no host rounding-mode change is needed.

D_006A6988's 80 bytes are read from CORE.GT4 on the user's ISO at runtime,
after decompression and load-record validation. Instruction/field/table
addresses are checked for the supported executable layout. Car rows are read
through the existing runtime SpecDB reader. No mileage-factor or car-price
table is embedded in C. The current price API supports GENERIC_CAR keys;
other namespaces and the original label fallback path are still pending.

Oracle: `tests/gtmode/prices_run.c`, standalone `gtmode_price_oracle` target.
It executes the full original price/SpecDB path without replacing helpers.
The mileage table loaded from the ISO must match the capture byte-for-byte;
original price entry instructions must match extracted/core.bin. It reads
all 722 captured GENERIC_CAR keys and tests 31 odometers (zero, maximum and
both sides of every reachable mileage boundary) in both used/sale modes.
Both snapshots passed 44,764/44,764 identical return values: 89,528 total.
The synthetic car's 0x4b8 bytes and all SPR remain unchanged on every call.
Original SpecDB cache and transient stack writes are not yet normalized or
compared against native memory; this is not full-RAM equivalence. The generated
closure contains 36 unsupported sites, but none is reached by these cases.

The port has not been wired into the provisional MCarGarage/menu purchase
flow. Still pending: other key namespaces, full memory effects, weekly used
inventory, actual buy/sell actions with the same save and native save result.

```
cmake --build build --config Release --target gtmode_price_oracle
build/Release/gtmode_price_oracle.exe captures/gtmode/20260930/slot01/eeMemory.bin captures/gtmode/20260930/slot01/Scratchpad.bin "path/to/user.iso"
```

## Garage capacity and slot lookup: primitive verified

`func_00435348` finds the first free slot; `func_00435920` counts occupied
slots. Both inspect bit zero at garage+0x10+index*0x20 and scan exactly
1000 entries. C: garage_first_free and garage_count in career_original.c.
This confirms the 1000-car capacity directly from original code.

`tests/gtmode/garage_run.c` / `gtmode_garage_oracle` compares all RAM and SPR
after every call, with no stack adapter or game-logic stub. Both snapshots
passed 1130 cases per function (4520 total). Cases cover the original empty
garage, a full garage, a hole at each of the 1000 slots, arbitrary other flag
bits and random occupancy. Full insertion/deletion/selected-car behavior and
Adhoc return semantics remain unvalidated.

```
build/Release/gtmode_garage_oracle.exe captures/gtmode/20260930/slot01/eeMemory.bin captures/gtmode/20260930/slot01/Scratchpad.bin 0xa18840
```

## Oil replacement: state update primitive verified

Registration: `replaceOil`, string 0x0068faf8, callback `func_0014EB38`;
the callback gets MCarGarage through func_00147D80 and passes car+0x4a0
to `func_00343928`. C: `src/core/career_oil.c`, replace_oil. Its callees are
`func_003507D0`, `func_00343698`, `func_00350808` and `func_00350678`.

If the wear block byte at +0x14 is nonzero, it makes no state change.
Otherwise, it initializes the oil parameter block at D_00620320 if needed,
from bytes of D_00620CD8, and computes the replacement value with the original
separate float operations. It writes the result at wear+8, clamping with the
original comparison against the parameter at +0x1c. Native code receives
the live settings/parameter bytes; no extracted settings table is embedded.
The shared career_float.h implements the same conversions, multiplication
and comparisons without changing the host floating-point environment.

Oracle: `tests/gtmode/oil_run.c` / `gtmode_oil_oracle`. All five functions'
instruction bytes must match the snapshot. Slots 1, 2 and 4 passed 1281/1281
cases (3843 total), comparing the whole RAM and SPR byte-for-byte after
every call, including lazy parameter initialization and its flag. Tests cover
the captured state, all 256 inhibit-byte values, initialized/uninitialized
parameters, varied settings bytes and arbitrary input float bit patterns.
The adapter only reproduces original dead stack saves; no original output
is copied into native expected memory. Five generated functions, zero
unsupported sites, no game-logic or hardware stubs.

Live disc parameter provisioning and the imported selected-car native oil
method are now integrated (see the later service validation section).
Still pending: other car wear initialization, full maintenance/day flow,
power response over mileage and the same actual oil-service action in
PCSX2/OpenGT4. No physics file was edited
for this port; other worktree changes are preserved.

```
cmake --build build --config Release --target gtmode_oil_oracle
build/Release/gtmode_oil_oracle.exe captures/gtmode/20260930/slot01/eeMemory.bin captures/gtmode/20260930/slot01/Scratchpad.bin 0xa209e8
```

## Unvalidated rules

- Purchase/sale prices; used lists, weekly rotation and mileage pricing.
- Per-event credit/car prizes and choice of prize car.
- Entry requirements: license, drivetrain, power, country and year.
- License medal thresholds and result recording.
- Opponent selection and A-Spec points.
- Calendar advancement, effects of days and mission-group queries.
- Tuning part prices/effects and oil replacement.
- Garage insertion/selection/removal, championship and career progression.
- Imported PS2 record/car-ID conversion to native runtime data.

No new gameplay option or Classic default was introduced. No files in the
physics directories, race_view or physics documentation were changed.

## Additional owned-car snapshot verification

Slot 4 independently passed the used/sale price oracle: 44,764 returned
prices identical for all 722 GENERIC_CAR labels, 31 mileage inputs and both
modes. Combined with slots 1/2 this is 134,292 cases. Car memory and SPR
are checked; this remains a return-value validation, not a whole-RAM or
actual purchase/sale validation. Slot 4 also passed all 1,281 oil cases.

Next original license path located: isLicensePassed registration at
0x001A6C1C, callback func_001A63A0 calls func_004312B0, which delegates to
func_004310F0 with the disc format string at D_006A5518 and range 1..16.
func_004312D8 delegates to func_004311A0 with limit 11 and range 1..15;
func_00431300 uses the caller's low-byte grade limit and range 1..16.
These are located assembly paths only, with zero oracle cases so far;
the existing PC medal-string queries are still provisional.

## Native imported oil service: integrated oracle path

Production files: career_oil_disc.c loads D_00620320 (36 bytes) and
D_00620CD8 (0xd9 bytes) directly from the user's CORE.GT4, checks the
func_00343928 signature and supplies the native lazy cache. No parameter
table or generated EE code is embedded. career_prices_disc.c now keeps
price-table loading separate from pure price arithmetic. getUsedPrice and
getUsedSellPrice in menu.c resolve GENERIC_CAR via the actual .idi/.dbt
files and invoke the previously verified used/sale float sequence.
Imported numeric car IDs are resolved without replacing the original ID.
The menu bridge has a synthetic routing test, not an additional original
price validation or certification of complete purchase/sale behavior.

The import originally confused serialized and live car offsets. The
original funcs 0043DEF0/0043E098 copy serialized car+0x498 to live car+0x4a0;
the serialized object omits the live eight-byte class header. All 32 wear
bytes are preserved in oil_state for the selected retail imported car.
Online and non-selected car wear remain pending; no wear is fabricated.

The copied memory card is captures/gtmode/20260930/Mcd001.ps2, SHA256
DF7AABF46DD7C0F51C5AAA03027BA033BCD186D450778463AC0A8AA2F5373D3E.
The user's source card was read only. Its imported money, car key, mileage
and all 32 wear bytes agree with slots 3/4. tests/gtmode/service_run.c uses
the same imported game through the production Adhoc bindings. The authored
OilChangeRoot at script lines 12135/12137 subtracts 50 credits then calls
riding_car.replaceOil. The harness performs those native calls and the
original funcs 0043AFB0/00343928 on the captured state, comparing all 32 MiB
RAM plus SPR. Only original dead ABI stack stores are adapted, and lazy
global state is derived independently from the captured input settings.
Both captures pass (2 cases total), and post-service money and all wear
bytes survive native save pack/replace and handler rebinding.

Generated service closure: 11 functions, zero unsupported sites, no hardware
or game logic stubs. All eleven function ranges are checked against captured
instructions before execution. This validates the imported state plus the
native service calls; it does not claim that a complete GT Auto UI action
was performed in PCSX2, nor validate power response or all garage records.
The raw ps2_import blobs remain the source snapshot; mutable oil_state is
the native authoritative wear representation. PS2 export is not implemented.

```
cmake --build build --config Release --target gtmode_service_oracle
after build: gtmode_service_oracle.exe RAM SPR ISO copied-memory-card.ps2
```

Still required for the requested transaction stage: original add
and selected-car state ports, affordability callback semantics, imported
non-selected cars and garage-file decoding, full purchase/sale UI actions
and save comparison, original event payload generation and day advancement.

## Car deletion: original memory behavior and native adapter

Registration: delCar string 0x00690a70, registration 0x0016F694,
callback func_0016DEA8 -> func_00435860 -> func_00434E08.
C: gt4_career_garage_delete in career_original.c. It clears only the
occupied bit in the 64-bit flags at garage+0x10+index*32. When index equals
the selected index at garage+0x81c8 it writes -1 there and returns 1;
otherwise it returns 0. Empty-slot calls do not produce a separate error.
The original keeps all other slot bytes, including other flags.

The garage oracle now passes 2000 deletion cases per owned-car capture,
covering every index 0..999 selected/unselected with random full slot data
and occupied/empty flags. Full RAM/SPR compare per call, with three dead
stack saves adapted. This is 4000 cases across slots 3/4. Existing free/count
cases also pass both owned captures, bringing their total across four states
to 9040. The current generated garage closure is 12 functions with no
unsupported sites or helper stubs.

The native delCar handler uses this port and keeps the deleted object's
metadata under garage.vacant_slots until reuse. Repeating deletion of a
valid empty slot returns false, matching the original primitive; the PC
bounds check still rejects invalid indices. Slot selection, flag updates,
count and selected-car cancellation are bound; whole original Adhoc argument
conversion and new-car initialization remain pending.

The service oracle additionally calls production native delCar on the
imported selected CR-X after oil service and invokes func_00435860 on the
same original state. All RAM/SPR and the returned selected status match on
slots 3/4 (2 additional integrated cases). Original current-car bytes remain
unchanged in that comparison; the PC sparse occupied array becomes empty
while metadata is retained. This is not certification of a complete sale
UI transaction, sale credits, event payload or calendar advancement.


### Acquisition date: original port validated, transaction integration pending

- Original `func_00435148` calls `func_0042F4C8` (2005-04-02 via
  `func_005797A8`) and writes the elapsed date into the low 24 bits of the
  second 64-bit slot flags word, at slot + 0x10. Its sign-extended
  `0xff000000` mask preserves the upper 32 bits as well as bits 24..31.
- C: `gt4_career_car_acquisition_day`, in `src/core/career_calendar.c`.
  The date tables come from the user's disc at runtime.
- Oracle: 1,024 cases each on captures slot03 and slot04, **2,048/2,048
  identical full-RAM and Scratchpad comparisons**. Cases include 512
  successive career days per capture and 512 arbitrary signed 32-bit dates,
  with randomized slot bytes. The adapter accounts only for original ABI
  stack saves; car flags and all other memory compare without normalization.
- Existing calendar tests were repeated on both captures and passed:
  280,000 date returns, 4,096 inverse date cases, 3,072 elapsed day/week
  cases and 4,096 ring append cases. These are repeat coverage, not added
  independent rules.
- Application build and `gtmode_game` / `menu_navigation` passed.

Purchase initialization traced, still unvalidated: callback `func_0016DBB0`
accepts at least two arguments (label, variation), and an optional third
mileage argument defaults to zero. It forwards the current calendar day
from the original game global to `func_00435800`, which calls
`func_00435780` (first free slot / `func_00434F78`) and then
`func_00435148`. `func_00434F78` resolves the disc key through
`func_00447BB8`, then `func_00434E38` initializes equipment and wear through
`func_0043DC20`, `func_00440EC8`, `func_00441298`, `func_003434A8`,
`func_00440F78`, and `func_00434FD0`. Those dependencies must be ported and
validated before certifying the whole purchase. The current PC insertion
still rejects the optional mileage argument and lacks full original car
initialization. The validated acquisition-date primitive is not yet wired
into that provisional insertion, to avoid treating an incomplete slot as
an original initialized vehicle.


### Combined career work: prizes, mileage, license queries and remaining roots

This work is one career delivery. A passing primitive is not certification
of its entire event, transaction or native callback.

| Rule | Original / C | New validation | Integration status |
|---|---|---|---|
| Race cash prize per zero-based position | `004475A8` / `gt4_career_race_prize` | 2,048/2,048 identical return + full RAM/Scratchpad, slot03/04 | Primitive ready; runtime race-data loader and award action still pending |
| Odometer assignment | `003434A8` / `gt4_career_odometer_set` | 2,048/2,048 identical full RAM/Scratchpad, slot03/04 | Primitive ready; original vehicle initialization still pending |
| License passed versus all-bronze | `004312B0 -> 004310F0 -> 00430C50`; bronze `00431300 -> 004311A0 -> 00430C28` | Existing predicate oracles plus PC adapter regression tests for red, absent grade and original record override | Adapter now uses original predicates; full original lookup/aggregate oracle and PS2 record-ID binding still pending |

`tests/gtmode/rules_run.c` verifies both function instruction ranges and
original prize branch table against the capture. It randomizes the input
structures and includes rank boundaries, negative-as-unsigned arguments,
2^32, the mileage cap and adjacent values, and 64-bit extremes. There are
no hardware stubs or generated routines in the application. Prize fields
are read from the caller's original runtime structure, never copied into
a hand-authored prize table. These randomized prize structures do not
validate event-data loading or actual event completion.

The license adapter in `gtmode_game.c` supports a supplied
`original_record` word. Legacy medal strings are mapped at its boundary;
its queries no longer compare presentation medal ranks. The original
passed predicate accepts type-1 grade 12, whereas all-bronze rejects it.
Import currently exposes the 768 indexed records separately; mapping
those records to event labels through the original SpecDB path is still
required before declaring imported license queries complete.

Additional original entry points located (zero equivalence cases until
explicitly recorded otherwise):

| Area | Native callback | Underlying path inspected / next dependency |
|---|---|---|
| Event license requirement | `001A16C0` | unsigned byte at race structure +0x55 |
| Drivetrain requirement | `001A1758` | original race structure adapter |
| Power requirement | `001A19B8` | original race structure adapter |
| Country requirement | `001A2638` | original country identifier / original race structure |
| Year restriction | `001A26D0` | signed halfwords at race structure +0x38/+0x3a |
| Championship prize | `001A2910` | signed halfword at +0x3c multiplied by 100 |
| Additional driver points | `001A29C0` | `00447598`, signed byte table at original `006A6EF0`; this is not the complete A-Spec algorithm |
| Part price | `001402A8` | `0044A2E0 -> 0044A048 / 00443ED0`, original SpecDB row/part type dispatch |
| Equipped parts | `00140530` | equipment object and SpecDB mapping |
| Current power | `001412A8` | full original equipment/car parameter dependencies |
| Used-car insertion | `0016DD68` | `00435800`, original variant and odometer arguments |
| Total prize money | `00163BB8` | 64-bit wrapping addition in original game state |
| A-Spec distance | `001640D0` | original raw state + unit conversion; separate from points |

Still required across the delivery: full buy/sell and used rotation,
actual prize cars/championship outcomes, requirement enforcement,
license timing/results and record lookup, opponent selection and A-Spec
points, complete day/event effects, tuning prices/effects and wear
initialization, and real before/after PCSX2/OpenGT4 actions with the same
save. Registration candidates alone are never marked implemented.


### Disc-backed imported records and license thresholds

- `gt4_specdb_row_index` in `src/core/specdb.c` follows
  `func_00449190`: original sorted GTDB identifiers and the unsigned
  64-bit comparison after a signed `lw`. `gtmode_rules_oracle` compared
  **131,080/131,080 indices** across slot03/slot04: all 65,536 low-word
  identifiers plus four 64-bit boundary arguments per capture. Full RAM
  and Scratchpad were checked unchanged every 512 calls and at the end.
  The RACE directory was read from the ISO and checked identical to the
  captured directory before comparison.
- The menu reads RACE.idi/RACE.dbt from the ISO at runtime and binds
  `_getRecordIndex` on `race_record`. Its key path follows
  `00447C50 -> 004483B8 -> 00449190` (namespace 0x23, low 16 ID bits,
  original table ordinal). No labels, identifiers or data tables are
  compiled into the application. The fallback name transformations in
  `00430E58` are still pending; exact disc labels are supported.
- The importer retains each complete `original_record` word, including
  the fourth byte. With the copied PS2 card, **768/768 records matched
  the live captured state on each capture** (1,536 comparisons). Native
  save round-trip retention is tested separately.
- Queries resolve the current indexed record instead of caching another
  object under `units`. This prevents old cached objects from masking
  imported state after native save restoration. A native menu regression
  test checks resolution even with a preexisting empty cached unit.
- Fixed native insertion callback status handling: `ah_call` returns zero
  for successful native callbacks. The metadata binder had incorrectly
  treated that as failure. This fix removes the false insertion error;
  it does not certify complete original vehicle initialization.

`gtmode_race_data_oracle` calls original `00443ED0` with namespace 0x23
and every original RACE identifier, then compares the C SpecDB decoder
against the original output: **709/709 rows per capture, 1,418 total,
151 bytes each, all identical**. This proves the decoded row bytes,
not the original loader's complete memory side effects or event outcomes.

`gt4_career_license_time` ports `00447670` (gold), `004476B0` (silver)
and `004476F0` (bronze). It preserves minute/second byte arithmetic,
the unsigned little-endian millisecond residual and the zero-time
sentinel `0x157529ff`. Across both captures, all three timers were tested
on all 709 decoded rows and 256 additional byte-pattern structures:
**5,790/5,790 identical return values and unchanged input structures**.
Full RAM equality is not claimed by this timer harness. Threshold loading
is now ported/validated; applying time results and awarding licenses in
the actual career flow remains pending.

The SpecDB decoder oracle currently reuses a test-only generated closure
with 170 functions and 36 unsupported sites; none was reached by the
709-row tests. Generated C stays under ignored build/gen and is never
linked into OpenGT4. The compact index/prize/mileage oracle has three
functions and zero unsupported sites.

Still outstanding: boxed native callback/full-memory validation, actual result updates, complete purchase/sale and used stock
rotation, event/championship award logic and prize cars, requirements,
opponents, complete A-Spec, tuning effects, remaining calendar event
semantics and real before/after actions in both games with the same save.

## MRaceData runtime integration and scalar spans (2026-09-30)

`main::menu::MRaceData` is now a callable native class. It lazily reads
`RACE.idi` and `RACE.dbt` from the user's disc, decodes the selected original
151-byte row and binds instance getters. `available` checks exact label
existence (001A2A90 -> 00447C50). No event tables or prize values are copied
into source. `getPrize` calls the already validated 004475A8 port.

The scalar C port is `gt4_career_race_field` in
`src/core/career_original.c`; `src/core/menu.c` bridges its values into Adhoc.
The boxed MRaceData pointer and the row pointer differ by 0x10. In particular,
001A26D0 first sets s5=boxed+0x10, then reads s5+0x38/0x3a: these are row
0x38/0x3a, not row 0x28/0x2a. The latter offsets are prize fields.

| Getter | Original callback | Row field and operation | Cases identical |
|---|---|---|---:|
| getNeedLicense | 001A16C0 | unsigned byte +0x45 | 3,466 |
| getNeedDrivetrain | 001A1758 | unsigned byte +0x46 | 3,466 |
| getNeedAspiration | 001A17F0 | unsigned byte +0x48 | 3,466 |
| getNeedCarType | 001A1888 | unsigned byte +0x49 | 3,466 |
| getNeedTire | 001A1920 | unsigned byte +0x4a | 3,466 |
| getNeedPs | 001A19B8 | byte +0x92, shift/add multiply 10 | 3,466 |
| getNeedPrice | 001A1A60 | byte +0x93, multiply 10000 | 3,466 |
| getLimitedWeight | 001A1B08 | byte +0x94, shift/add multiply 10 | 3,466 |
| getLimitedLength | 001A1BB0 | byte +0x95, shift/add multiply 100 | 3,466 |
| getNeedCountryID | 001A2638 | unsigned byte +0x96 | 3,466 |
| getNumberOfLaps | 001A2E30 | unsigned byte +0x43 | 3,466 |
| getLimitTime | 001A2D98 | unsigned halfword +0x18 | 3,466 |
| getPrizeGC | 001A2910 | signed halfword +0x2c, multiply 100 | 3,466 |
| getNeedYearRange[0] | 001A26D0 | signed halfword +0x38 | 3,466 |
| getNeedYearRange[1] | 001A26D0 | signed halfword +0x3a | 3,466 |

`tests/gtmode/fields_run.c` (`gtmode_field_oracle`) executes the original
instruction spans for field loading and arithmetic. It verifies the original
span bytes against each captured EE RAM before executing them. All 709 disc
rows plus 1,024 deterministic random rows, on both slot03 and slot04, yielded
**51,990/51,990 identical scalar values and unchanged 151-byte input rows**.
The spans stop at their explicit end or at the original call to VM integer
boxing after its delay slot. Only those exact expected stop PCs are accepted.
VM allocation/boxing, complete callback execution and full RAM equivalence
are **outside this test's coverage**. This is field arithmetic validation,
not complete requirements enforcement or original event outcome validation.

Native bridge regression tests additionally cover constructor invocation,
instance-bound getters, row lifetime after GC, signed GC prize and years,
unsigned invalid prize position, and the distinction between row/boxed offsets.
Release application build and ctest `specdb`, `menu_navigation`, `gtmode_game`
passed. Test-only generated spans remain ignored under build/gen and are not
linked into the application. No new Classic gameplay option was introduced.

### Not validated / still missing

- MRaceData's no-argument constructor. Formatted fallback row resolution is
  now integrated and validated within the scope below. Missing rows raise
  a script error.
- Other MRaceData methods, including course/car labels, allowed-car lists,
  opponent selection and time-string presentation adapters.
- Complete original entry checks: reading a requirement value is not enforcing
  it against the original equipped-car state.
- Event/championship result processing, award transactions and prize cars.
- Full car acquisition initialization, purchase/sale transactions and used
  stock selection/weekly rotation; optional acquisition mileage is unported.
- A-Spec calculation and aggregation, license result/award flow, tuning prices
  and effects, and remaining calendar transaction semantics.
- Real before/after action comparisons in PCSX2 and OpenGT4 with the same save.

Next step: finish MRaceData's original label resolution and car-state checks,
then connect the exact result/award transactions to the imported race records.
The remaining career domains above remain part of the requested delivery.

## Lookup, aggregate queries and remaining race metadata (2026-09-30)

This section supersedes the earlier pending license-query/fallback status.
C ports are `career_race_lookup.c`, `career_original.c` and the native
bridges in `gtmode_game.c` / `menu.c`. Runtime data come from
`career_race_lookup_disc.c`: the two prefix strings at 0068BB20/00692800,
the 16 original result enum entries at 006A55C8 and six championship point
bytes at 006A6EF0 are read from the user's CORE.GT4. RACE labels and rows
continue to come from the user's SpecDB. No extracted table is embedded.

The original record lookup 00430E58 retries exact, US-prefixed, then
JP-prefixed labels. It only retries a missing label, whereas constructor
001A0E48 retries each failed row load through 00447188. Static `available`
001A2A90 -> 00447C50 still checks only the exact label. Original 005A6900
copies up to 63 source bytes *after* the two-byte prefix, rather than reducing
the limit by the prefix length. The C port reproduces that candidate string
using a sufficiently sized buffer, without reproducing the stack overrun.

`gtmode_lookup_oracle` checks original entry bytes against each capture.
It invokes complete original lookup/aggregate/load helpers and compares the
real bound PC query handlers. The table below counts slot03, slot04, slot05
and slot06, with identical tests applied to each:

| Rule | Original function(s) | C / bridge | Identical cases |
|---|---|---|---:|
| Record label/index resolution | 00430E58, 004483B8, 00449190, 00430F70 | gt4_career_race_record_index | 6,076 |
| Constructor fallback candidates / loaded row | 001A0E48 flow, 00447188 / 004470E8 | gt4_career_race_row, nat_race_init | 5,672 |
| Perfect/passed/final-test/bronze/silver license queries | 00431260 / 004312B0 / 004312D8 / 00431300 and callees | record native handler, packed predicates | 27,020 |
| Mission range queries | 001A6620 flow, 00431328 / 004311A0 | record native handler | 33,968 |
| Result name enum | 00431C60 / 0057B1E8 / 0057B210 | gt4_career_result_grade | 68 |
| Range integer conversion cases | 005A2F88 | adapter strtoll then signed low32 | 15,440 |
| Championship driver points | 00447598 (001A29C0 bridge) | gt4_career_driver_points, getAdditionalDriverPoints | 6,168 |
| Participant count scalar | 001A1370 span 001A139C..001A13A4 | GT4_RACE_ENTRY_NUM, entry_num | 6,932 |
| All 16 scalar race fields including participant count | spans listed above plus 001A1370 | gt4_career_race_field | 110,912 |
| Signed affordability comparison | 00163990 span 001639F8..00163A08 | gt4_career_money_enough, isEnoughMoney | 4,352 |

Aggregate states include the captured state, all 64 type/grade combinations
and 128 deterministic random states, across seven category strings. Mission
ranges include default 1..40, individual tests, empty/reversed ranges,
negative and signed-wrap values, whitespace/trailing text, and three result
names plus the omitted optional result argument. Previously the native
`isMissionClear` wrongly compared a single event code; it now checks the
original inclusive range and its optional grade limit (default zero).
The range conversion adapter is checked on the stated inputs only; this
is not a complete libc conversion/locale/overflow port.

Lookup and aggregate comparisons require all 3,076 bytes of the original
record object to remain unchanged. Row-load comparisons check loaded status
and 151 decoded bytes, not all loader side effects. Driver-point comparisons
cover supported positions 0..5 with the disc table and every signed-byte
pattern. They are championship standings points, not A-Spec points. Reads
outside that six-position table are not certified; the native bridge raises
an error rather than exposing unrelated adjacent original memory.

Scalar field and affordability comparisons stop at explicitly checked
instruction-span ends or VM boxing calls after their delay slots. The
16 fields cover 709 disc rows plus 1,024 random rows per capture; the
signed affordability span covers 64 edge pairs and 1,024 random pairs.
Boxed original Adhoc callbacks, VM allocation and full RAM equivalence are
excluded. Native regression tests cover constructor aliases versus exact
`available`, instance/GC lifetime, participant attributes, signed driver
points, and integer/field bridges. Release application and both oracle
executables build; ctest specdb/menu_navigation/gtmode_game passes 3/3.
Generated original code is test-only, never linked into the application.

Slots05/06 were collected without modifying PCSX2's originals. Their embedded
screenshots show an ongoing Grand Valley race (start and second lap), not a
post-event career award. They have Cr. 4,586 and the same occupied garage
count/date as the earlier car states. They broaden oracle coverage but do
not prove any actual purchase, sale, license award or event transaction.

### Remaining requested work, not certified as complete

- Complete original car initialization and garage selection, purchase/sale
  transactions, optional acquisition mileage, used inventory and weekly
  rotation, including original mileage-related prices in the real flow.
- Full event/championship result processing, monetary awards, prize cars,
  license time-result classification and license/mission awards.
- Original equipped-car entry checks for license, traction, power, country,
  year and other conditions; reading their scalar requirements is validated,
  enforcing them in the whole original flow is not.
- Event car/course labels, allowed-car lists, opponents and their selection;
  remaining MRaceData attributes, formatted times and no-argument creation.
- A-Spec calculation/aggregation; championship driver-point table validation
  does not certify this distinct calculation.
- Complete original calendar event generation and the transactions that
  advance days/used stock.
- Complete tuning purchase/equipped-state/effects and oil/power response.
  Part selection and price returns now have ports and oracle coverage below;
  that does not certify the transaction or effect on the car.
- Original result producers and full award transactions. Record attributes,
  packed primitive updates and native save round trips are now covered below.
- Actual same-save before/after actions in PCSX2 and OpenGT4. The captured
  race states are not replacement evidence for these actions.

Next implementation step: original record attribute/result producers and
complete car/part state, followed by event transactions and requirements.
All remaining domains above stay in scope; no per-domain permission is needed.


## Native records, part quotes and car metadata (2026-09-30)

This section supersedes the earlier pending record-attribute and part-quote
status. All totals combine the same suite against slot03, slot04, slot05
and slot06. Captures and generated original C stay ignored and test-only.
The application links the handwritten C ports, not the EE oracle.

| Rule | Original function / instruction span | C / native bridge | Identical cases |
|---|---|---|---:|
| Direct packed type/result/latest/present writes | 004309E8..00430A04, 001A4D88..001A4DA4, 001A4FA0..001A4FBC, 001A51B8..001A51D8 | gt4_career_record_field_set / record-unit host setter | 65,536 |
| Record type and three result-name getters | 004309B0, 00431C38 / 0057B1A8 | runtime enums / record-unit host getter | 65,536 |
| Improvement-aware record update | 00430A58 | gt4_career_record_update / update | 16,384 |
| Best score with signed 64-bit input | 00430A20 | gt4_career_record_score / updateBestScore | 16,384 |
| Refresh present result | 00430A10 | gt4_career_record_present / refreshResult | 16,384 |
| Car-part type/stage selection | 0044A048 / 00449F58 / 00449F68 / 00449F80 | gt4_career_part_key | 223,984 |
| Car-part price return | 0044A2E0 | gt4_career_part_row_price / nat_owned_parts_price | 223,984 |
| Signed model year | 004488A0 (0012FFB8 callback) | gt4_career_car_year / GetModelYear | 265,032 |
| Open-model byte | 00448968 (0012E5A8 callback) | gt4_career_car_open_type / GetOpenModelType | 265,032 |
| Category enum key | 00448908 | gt4_career_car_category | 265,032 |
| Category enum name or null | 00448928 / 004488E8 / 0057B1A8 | gt4_career_car_category_name / GetCategory | 265,032 |

`gtmode_unit_oracle` uses 4,096 randomized packed words per capture. Direct
setters preserve every bit outside their nibble and do not perform best-result
logic. The native names are counterintuitive: result reads the best-result
nibble, present_result reads the latest-result nibble, spot_result reads the
present/prize nibble. best_score is read-only as in the registration; setting
it through a generic attribute does nothing. updateBestScore preserves the
original signed 64-bit comparison before clamping to a byte, including values
outside the signed 32-bit range. Type-name comparisons cover valid original
types 0..3; invalid nibble types are not certified original pointer reads.

New native careers initialize 768 raw words to 0xFFF0. Imported raw words win
against derived fields. Legacy PC records without a raw word are migrated;
that compatibility adapter is not an original PS2 rule. Native binding always
uses the indexed canonical record, returns nil for a missing original label,
and restores host hooks/methods after load. Four GC/pack/merge/rebind checks
preserve all 768 records and the edited word. Dirty callbacks fire only on
actual changes. The save encoder persists only record-owned members, never
host pointers or generated functions. These tests compare the mutated word
and getter values, not the entire original boxed VM or all RAM.

`gtmode_parts_oracle` reads all 722 GENERIC_CAR rows and their counted CARS
arrays from the user's disc; no missing car lists occurred. It compares each
actual part entry plus signed/full-width edge inputs with the original
selector and price function and checks that the entire counted array remains
unchanged. The CARS raw array is supplied at the original file-loader output
boundary: original filesystem/heap side effects are not covered. Native
getPartsPrice(type,stage) is bound to garage cars and reads namespace rows
from SpecDB at runtime. Its fixture regression checks stage separation,
missing-row sentinel -1 and an out-of-byte-range 64-bit stage. Complete
MCarData construction, equipped parts, buying/installing parts and effects
on power/weight/handling remain pending.

Part namespace names (006234B0) and pricing branch targets (006A7B60) are
loaded from CORE.GT4. Category names are loaded from the original initializer
pairs at 006A6FE0, and the callback fallback string from 0068E838. The original
initializer 00448D20 -> 0057B168 / 0057B130 scans to a null name pointer; in
these captures it counts six pairs, including two unrelated adjacent pairs.
Their keys cannot arise from the unsigned-byte remainder modulo 100. The C
loader preserves first-match semantics for reachable keys 0..99 and does not
dereference unreachable pointer values. Every possible category byte is
covered by the scalar/name oracle suite. Category strings are no longer a
handwritten source table.

Metadata checks use each of the 722 disc rows and all 65,536 model-year bit
patterns per capture; open-type and category bytes cover their entire domain.
Each of the four getters checks its return and the unchanged 24-byte row.
The original model-year load is signed lh, correcting the former unsigned
native read (0xFFFF means -1, not 65535). Native regressions exercise signed
sentinels, ordinary years, unsigned open type, runtime category-name lookup
and callback fallback. Full original boxed callbacks remain outside scope.

The runtime SpecDB prefix remains the project's US2560 dataset adapter;
other regions are not certified. Release opengt4 builds with these ports.
The current regression commands and oracle scopes are reproducible using
`gtmode_unit_oracle` and `gtmode_parts_oracle` with RAM, scratchpad and ISO
arguments. Successful function comparisons do not substitute for actual
same-save before/after purchases, events or tuning in PCSX2 and OpenGT4.

Next implementation work remains complete original car construction and
transaction state, event awards and entry enforcement, opponents, A-Spec,
calendar producers and weekly used inventory, plus tuning effects. None of
those larger flows is marked complete by the component checks above.

Final verification for this revision: Release opengt4, menu_tests and
gtmode_game_tests build; ctest specdb/menu_navigation/gtmode_game passes
3/3. Both updated unit/parts oracle suites pass on all four captures.
All files edited in this revision retain LF endings. No commit was made.


## Complete selected-car sale action (2026-09-30)

The native sale sequence is now checked together against one actual PCSX2
before/after action. This closes the selected GENERIC_CAR sale case described
here; it does not certify every sale namespace, the entire UI handler, or a
purchase transaction. Purchase initialization remains pending.

The user created new states in PCSX2 slots 01/02/03. They were read without
modifying the source files and extracted under ignored sale-slot01/02/03
capture directories, with source SHA-256 metadata. Screenshot slot02 shows
the confirmation for the Honda CR-X SiR '90, Flint Black, value Cr. 1,353.
Slot03 is after the sale. Both states have the same original profile layout.
The matching PS2 card imported by the harness has the before-sale car and
money; its calendar history is checked against the before-sale RAM too.

The original CarViewRoot sell script in GTmodeProject orders the logic as:
getUsedSellPrice, getGarageIndex, delCar, incMoney(price), then
putSellCarEvent(car_code,getColorIndex(),price), with UI/sound between some
calls. The C bridges used by that script now supply the missing color getter
and an original binary sale event instead of a simplified textual event.

| Step | Original function(s) | Native C | Validation |
|---|---|---|---|
| Quote | 004411F0 / 00441128 | car_price / career_prices.c, base row resolved from SpecDB | Cr. 1,353 equals original and actual UI quote |
| Current color | 0013C9D8 -> 00441248 / 004458C8 | car_color; selected setup byte imported | Color 0 equals original; all 256 signed-byte patterns also match |
| Remove selected car | 00435860 / 00434E08 | garage delCar / gt4_career_garage_delete | Occupied bit cleared, metadata retained, selected index -1 |
| Credit money | 0043AFB0 | incMoney / gt4_career_credit_add | Cr. 4,586 -> Cr. 5,939 |
| Sale record and day | 00134FE0 -> 0042F218 / 0042F0E0 | calendar_car_event / gt4_career_calendar_car_event | Original sale record, 3 April -> 4 April |
| Persist and restore | PC format is free | gt4_state_pack, gt4_save_write/read, gt4_state_replace, gt4_game_bind | File round trip preserves balance, selection, count, retained car fields, date, elapsed day and all history bytes |

`gtmode_sale_oracle` runs the bound native callbacks in that order, then
calls the original functions with the same before-state and inputs. It
compares all 0x81D0 garage bytes and all 0xB6C8 calendar bytes using an overlay
of native mutations on the unchanged original baseline. It also compares the
logical credit balance and verifies occupied array/count consistency. These
same blocks and balance are then compared directly to the user's actual
slot03 after-action RAM: all match. Original boxed Adhoc callback execution,
heap/VM allocation, UI rendering and arbitrary RAM outside those state blocks
are not part of this equivalence claim.

Original car-event keys use 00447BB8: exact GENERIC_CAR lookup first, then
exact TUNED_CAR namespace 0x26; no result is -1. The menu resolver reads those
runtime label files. The sale case covers GENERIC_CAR. A tuned-car sale needs
its own owned-car binding/state validation before being marked complete.

Event producers 0042F1B8 and 0042F218 write only defined fields in their
16-byte local records; unused bytes retain previous stack contents. The C
producer preserves a caller-provided seed, and 2,048 additional whole-calendar
comparisons use varied padding, signed 64-bit color/detail and sale clamp
boundaries. Native-created events use zero reserved bytes. In the actual user
sale those reserved bytes are also zero, so the complete calendar block
matches without masking. This does not certify arbitrary original stack
padding for every action. Native event return is void as in the callbacks.

PS2 import preserves chronological original event records and the selected
setup's signed color byte, alongside existing wear state. Native car prices
use a base price read from the owning SpecDB row by the catalog binder; they
remain bound after native load and require original mileage data. No extracted
price/category/event table was embedded, and no original generated function
is linked into opengt4.

Reproduce with gtmode_sale_oracle RAM SPR ISO PS2-CARD NATIVE-SAVE
[ACTUAL-AFTER-RAM]. The output save belongs under ignored build/gtmode-temp.
The actual before/after comparison currently covers one sale, not multiple
independent cars/prices/saves. General price arithmetic has its separate
large oracle suite; that is not counted as additional actual sales.

Final verification: the combined sale harness passes on the four preserved
GT Mode captures and the new actual-before state (five executions). Only
sale-slot02 -> sale-slot03 is an independent actual before/after sale; the
other executions replay the same car/save scenario. Additional producer
checks total 10,240 and color checks 1,280 across those executions, with
repeated datasets explicitly counted as executions rather than distinct
input cases. The actual sale comparison and the native disk save round trip
pass. Release opengt4 builds, ctest specdb/menu_navigation/gtmode_game is
3/3, diff whitespace checks pass, and edited sources remain LF.


## Purchase construction and actual used purchase (2026-09-30)

Test-only `gtmode_purchase_oracle` uses the complete original `00435800`
closure with the existing EE scheduler. Its fiber entry explicitly sets PS2
rounding toward zero: setting the host thread's rounding alone is insufficient
on Windows fibers. It verifies each explicitly invoked original entry against
`extracted/core.bin`. Generated code stays in ignored `build/gen` and is not
linked to `opengt4`. No physics sources/tests/docs were edited.

| Rule / original function | C implementation | Validation |
| --- | --- | --- |
| Standard new-car setup, `00444480` | Existing `gt4_garage_new`, read/reused without edits | 722/722 runtime GENERIC_CAR rows; return value and complete 0x178-byte setup identical. Same seeded destination bytes; automatic gearbox temporary frame explicitly zero-seeded on both sides. |
| Explicit mileage wear construction, `003433E8`, `00343420`, `00440FB8` and its wear setters | `gt4_career_car_wear_init` in `career_oil.c` | 28/28 combinations: 14 mileage boundaries x 2 exemption flags. Complete 32-byte wear block, 36-byte lazy oil params and settings identical. Random/negative mileage and random/negative color paths are outside this port. |
| Maximum wear distance, `003660D8 -> 00366100` | Disc loader reads signed `D_00620AF0+6`, converts, multiplies by 1000 separately | Used purchase's complete wear block matches independent actual post-purchase capture. The initialized global is BSS on disc, so copying its zero bytes would be incorrect. |
| Buy calendar subtype, `00134C60` / `00134D40 -> 0042F1B8` | `calendar_car_event` in `gtmode_game.c` | Corrected new purchase subtype from 0 to 1; added original binary used purchase subtype 2. 16/16 native method cases (two methods x eight signed/color-byte boundaries) match original records and dates after native save serialization/replacement. |
| Calendar pending-change consumption, `00600B88` | `gt4_career_calendar_consume_dirty` | Exact dirty-word return and clear used in the actual purchase replay. No bytes are excluded from the calendar comparison. |

The user's three new PCSX2 savestates are preserved under ignored
`captures/gtmode/20260930/purchase-slot04`, `purchase-slot05`, `purchase-slot06`,
with source paths and SHA-256 hashes. They show:

- Slot 4: empty garage, Cr. 5,939, April 4.
- Slot 5: Honda BEAT Version F '92 inserted; calendar advanced; money still
  Cr. 5,939. This is an intermediate state, not a second independent purchase.
- Slot 6: purchase complete, selected BEAT, Cr. 1,082, April 5.

For this one actual action, original `00435800` with the same label, color,
85,574,576 mileage units and pre-action day reproduces the intermediate
capture's entire 0x81d0-byte garage block. Runtime SpecDB pricing is Cr. 4,857.
C credit subtraction and C calendar append match both the original calls and
the completed capture. The original UI consumes the calendar pending-change
word between appending and capturing; executing `00600B88` and its C port on
both sides produces identical complete 0xb6c8-byte calendar blocks. The C wear
constructor also matches the actual selected BEAT's entire 32-byte wear block.

Reproduce:

```powershell
build/Release/gtmode_purchase_oracle.exe `
  captures/gtmode/20260930/purchase-slot04/eeMemory.bin `
  captures/gtmode/20260930/purchase-slot04/Scratchpad.bin `
  'path/to/your/GT4.iso' `
  captures/gtmode/20260930/purchase-slot05/eeMemory.bin `
  captures/gtmode/20260930/purchase-slot06/eeMemory.bin
```

### Still unvalidated / not integrated

Historical checkpoint: the setup-only result below was subsequently extended
by **Complete standard-car purchase integration**, at the end of this document.
Full stock-car construction, seeded used-car construction and the native
buy/select/save action are now validated there. Tuned-car construction and
used-stock weekly rotation remain outside that equivalence result.

Verification after this change: Release application build passed;
`specdb`, `menu_navigation`, `gtmode_game` passed (3/3); the actual selected-car
sale replay still matched all garage/calendar bytes and native file reload;
the oil-service oracle passed 1,281/1,281 full RAM/SPR cases on purchase slot 6.
Changed sources retain LF. No commits were made.


## Riding-car replacement and history integration (2026-09-30)

| Rule | Original | C / native adapter | Validation |
|---|---|---|---|
| Copy the selected live car while retaining the destination class word | `00435388` | `gt4_career_garage_set_riding`, `career_original.c` | Combined selection suite: 256/256 complete `0x81f8`-byte garage objects identical |
| Conditionally clear the previous selected bit, then consume the change flag | `00435418` | `gt4_career_garage_change_riding` | Same suite varies both `garage+0x81f4` states and both entry points |
| Promote selected history position, including vacant slots | `00435690` | `gt4_career_garage_promote`; `garage_select_metadata` | Same suite preserves high history bits; eight production native calls match occupied flags and all 1,000 history words after native serialization/reload |
| Trade/present-car calendar records | `00134E20` / `00134F00` -> `0042F1B8` | `calendar_car_event`, subtypes 3/4 | Four acquisition methods, eight signed-byte boundary colors plus omitted/default color: 36/36 records and dates identical after native serialization/reload |

The raw selection comparison supplies the same complete live car to original
and port. It covers both entry points, 64 selected slots (including 0 and
999), initialized and uninitialized change flags, mixed occupied/vacant slots,
nonzero upper history bits, and destination header preservation. History
promotion examines every slot, not only occupied cars. `setRidingCar` leaves
previous selection bits intact; `changeRidingCar` clears the previous bit
only when the original pending-change word is nonzero. The native adapter
persists this condition as `original_pending_change`. Its two-argument form
accepts the supplied car object and returns `AV_VOID`; the prior PC-only
one-argument convenience form remains supported.

The actual selection stage between purchase captures 05 and 06 also matches
all `0x81d0` garage bytes, using the captured completed BEAT full car as the
input to both implementations. Native flags and all history words match that
stage after save/reload. **This does not prove native construction of that
full car:** the full-car input is deliberately captured, not synthesized by
our purchase implementation. The larger `0x81f8` synthetic suite checks
selection state beyond the actual-action comparison boundary.

PS2 import now preserves all 1,000 history words, acquisition words, and the
original 13-bit weight field rather than truncating weight to 11 bits. The
captured selected car's serialized body is retained on its owning car object
for subsequent selection. Immutable imported body data is still a snapshot;
mutable wear continues to use `oil_state`. Other cars' complete garage-file
records and synchronization of tuning/wear into a full body remain pending.
Reusing a deleted PC slot initializes its history sentinel from `00434D80`,
preserving the previous high word. Full new-car initialization is not implied.

Validation command is the purchase-oracle invocation in the preceding section.
The suite now reports 722 standard setups, 256 full selection cases, 28 wear
cases, 36 acquisition-calendar native cases, and the isolated actual selection
stage. Release application build passed; `specdb`, `menu_navigation`, and
`gtmode_game` passed (3/3). The actual sale replay still matches garage and
calendar bytes and native file save/reload. Modified source files retain LF;
no physics sources, physics tests, race_view, or physics docs were edited by
this change. The recompiled original remains confined to oracle executables.

### Outstanding career work

Still required before claiming the whole GT Mode complete:

- Tuned-car construction (`00444690`, TUNED_CAR namespace). Standard catalog
  construction, compact ratings and the captured used buy/select/save action
  are completed in the section below.
- Full per-car PS2 garage record import and mutable full-body synchronization.
- Award actions for races/championships and prize cars; validating their
  complete record, money, garage, and calendar effects against actual actions.
- Complete entry enforcement, opponent selection, A-Spec, used-stock rotation,
  and tuning parameter effects; their existing isolated ports do not certify
  the complete career flow.
- Race/license/wheel/wing calendar producer integration and complete original
  boxed-callback/UI side effects; trade/present calendar checks do not certify
  those award or trade transactions themselves.

Next implementation step: integrate race/championship awards and progression,
then compare complete award actions with matching before/after PS2 states.


## Complete standard-car purchase integration (2026-09-30)

**Completed scope:** construct a standard catalog car from runtime disc data,
insert it into the garage, deduct its used purchase price, append the calendar
record, select it, perform native Auto Save and load the saved profile again.
The actual action is the user's BEAT purchase in slots 04 -> 05 -> 06. No
completed-car payload from slots 05/06 is supplied to the native constructor.
Those snapshots are comparison outputs only.

| Rule | Original functions | Production C | Validation |
|---|---|---|---|
| Standard setup | `00444480`, `00443240` | `gt4_garage_new`, reused through `career_vehicle.c` | 722/722 setup blocks (`0x178` bytes) identical |
| Complete stock body, owned parts and compact ratings | `0043DC20`, `00440EC8`, `00441298`, `0043FA10`, `0043F270`, `00364818`, `00364868`, `0034F3F8`, `00434D80`, `00434E38` | `gt4_career_vehicle_build`, `owned_parts`, `compact_ratings`, `peak_power` in `career_vehicle.c` | 722/722 compact units (32 bytes) AND full serialized bodies (`0x4b8` bytes) identical; catalog-wide new/used mileage cases with nonzero reserved slot bytes |
| Seeded used paint/mileage and wear | `00440FB8`, `00579330`, `00578598`, `005792F0` | `gt4_career_vehicle_used_choices`, `used_random`, existing career oil/wear port | 722/722 complete bodies identical; random paint in one third of cases; 28/28 explicit wear/global cases identical |
| Append car, acquisition date and money | `00435800`, `00435148`, `0043AFB0` | factory-backed `addNewCar`/`addUsedCar`, `gtmode_game.c` | Actual BEAT reconstructed natively: code 596, color 0, mileage 85,574,576; price 4,857; money 5,939 -> 1,082 |
| Select car and retain history | `00435388`, `00435418`, `00435690` | garage selection/history ports in `career_original.c`, native adapter in `gtmode_game.c` | 256/256 complete `0x81f8` garage objects; eight native history/flag serialization cases |
| Purchase/trade/present calendar producers | `0042F1B8`, `00134E20`, `00134F00`, `00600B88` | `career_calendar.c`, calendar natives in `gtmode_game.c` | 36/36 acquisition cases; captured purchase complete `0xb6c8` calendar identical after normalizing the transient callback word on both sides |
| Complete captured used purchase | Composition above | production factory, garage, money, calendar, native save APIs | One actual transaction: complete `0x81d0` garage, complete `0xb6c8` calendar and money identical before AND after file save/new-VM reload |
| Original authored acquisition script + PC storage | disc `GetCarRoot.open` / `GetCarRoot.start_phase1`, original `incMoney` / `changeRidingCar` calls | `menu.c` native memory-card manager/storage adapter; opt-in output references in `adhoc_vm.c` | One actual transaction: selected BEAT unit/body/money identical after original scripts, Auto Save and native reload; zero script errors |

The storage bridge is a PC adaptation, not a port of PS2 device I/O. It returns
the authored script's expected selection/attachment success results and sets
the Auto Save output reference so the script does not undo successful car
selection. Ordinary native calls retain their previous argument behavior.
`getGarageIndex`, `getColorName`, `copyFrom`, `makeUsed` and standard constructor
bindings now support this acquisition path. Header fitting and sound-bank load
bindings allow its original UI scripts to execute. The menu test stubs drawing
and audio; it certifies script/state/save behavior, not visual or audible output.

SpecDB rows, variation counts, mileage ranges, constants and original branch
tables are read from the user's disc at runtime. The application factory runs
C only. Generated EE code and the EE runtime are linked into the oracle target,
not into `opengt4.exe` or the authored-menu test. No physics sources, physics
tests, race_view or physics documentation were edited for this integration.
Source changes retain LF. No commits were made for this task.

### Reproduce the completed purchase checks

```powershell
$iso = 'path/to/your/GT4.iso'
build/career-native/Release/gtmode_purchase_oracle.exe `
  captures/gtmode/20260930/purchase-slot04/eeMemory.bin `
  captures/gtmode/20260930/purchase-slot04/Scratchpad.bin $iso `
  captures/gtmode/20260930/purchase-slot05/eeMemory.bin `
  captures/gtmode/20260930/purchase-slot06/eeMemory.bin
build/career-native/Release/gtmode_purchase_menu.exe $iso `
  captures/gtmode/20260930/purchase-slot04/eeMemory.bin `
  captures/gtmode/20260930/purchase-slot06/eeMemory.bin
ctest --test-dir build/career-native -C Release `
  -R '^(specdb|menu_navigation|gtmode_game)$' --output-on-failure
```

Final verification: Release application and both purchase targets built;
all purchase comparisons above passed; regression tests passed 3/3. The
actual sale replay also remained identical across money, garage and calendar,
including file reload; its calendar producer suite passed 2,048/2,048 and
color getter suite 256/256.

### Not yet validated by this completion

- TUNED_CAR setup/construction (`00444690`) and modified-car `makeUsed` /
  cached-status rebuilding. Current preview construction covers stock cars.
- Weekly used-stock generation/rotation, rare low-mileage selection and the
  complete purchased-stock availability flags. Matching a chosen purchase
  does not establish matching inventories or every byte of the PS2 profile.
- All unselected full PS2 garage-file records and synchronization of tuning
  changes into those bodies; the captured selected car and newly constructed
  stock cars retain their full bodies and mutable wear.
- Owned-part getter bounds and all miscellaneous preview queries have not
  received an independent comprehensive oracle suite. The acquisition script
  is checked as a whole, with its actual inputs.
- Full dealer interaction by rendered mouse/controller input. The test invokes
  original acquisition script functions and events, not graphical hit tests.
- Race/championship awards, prize cars, full entry enforcement, opponents,
  A-Spec, tuning effects and complete progression remain separate career work.

**Next step:** complete race/championship award and progression transactions;
this purchase flow no longer remains the next implementation step.


## Race/license calendar, present flags and prize statistics (2026-09-30)

These bindings now use the original formats rather than provisional event
strings and independent prize maps. Their state survives native save reload.
This is **not** certification of the complete race/championship award pipeline.

| Rule | Original | Production C | Validation |
|---|---|---|---|
| Race/calendar event | native `00135348` -> `0042F270` | `gt4_career_calendar_race_event`, `calendar_race_event` | 709/709 runtime RACE labels, complete `0xb6c8` calendar blocks identical |
| License/calendar event | native `00135590` -> `0042F2E0` | same event port, type 4 | 709/709 runtime labels, complete calendar blocks identical |
| Driver/method enum | `00431CE8`, `00431CB8`, `0057B1E8` | runtime table `006A56A8`, `gt4_career_race_method` | 17/17 original enum returns, including unknown/empty names |
| Award received flags | natives `0019FB90`, `0019FC58`, `0019FCD0`, `0019FD98`; `00439D70`, `00439D98`, `00439DE0`, `00439E00`, `00439E18` | `gt4_career_present_get/set`, native `present` binding | 2,784/2,784 getter/setter/getter cases: all 928 valid serialized bits, complete 116-byte flags identical |
| Total prize money increment | `00163BB8`, instruction span `00163BFC..00163C14` | existing wrapping u64 `gt4_career_credit_add`, `prize_statistics` | 2,048/2,048 original load/add/store span results, including wrap/high-bit/zero boundaries |
| Total prize car increment | `00163E80`, instruction span `00163EC4..00163EDC` | `gt4_career_prize_cars_add`, `prize_statistics` | 2,048/2,048 original 32-bit load/add/store span results |
| Native storage | same bindings | `gtmode_game.c`, base64 serialized original flags | Race event, present flags and counters match after pack/replace/new VM; earlier PC prize maps migrate when original flags are absent |
| Actual Sunday Cup reward-result consumption | original disc `PresentRoot.makeColumn` / `PresentRoot.win`; original record host field writes | unchanged authored scripts + existing native record bindings; scalar string conversion fix in `adhoc_vm.c` | Slots 07 -> 08: all 768 complete record words identical, including the one changed Sunday Cup record; native file save/load and zero script errors |

Calendar comparisons use original RACE keys (namespace `0x23`) resolved from
the disc. They cover full rings and wrapped write pointers, varied results,
methods and arbitrary u32 prize bits. The event's undefined caller-stack byte
3 is seeded identically (`0xa7`) for raw oracle comparisons; native-created
records initialize that reserved byte to zero. The other fields and the full
calendar state are compared without tolerance. Tests pass on both captured
slot 07 and slot 08 states. No tables or captures are embedded in production.

The statistics oracle stops at the explicitly excluded Adhoc cleanup call
**after** the original store in its jal delay slot. It validates the arithmetic
and resulting counter bytes, not boxed callback allocation/cleanup or the
entire PS2 RAM image. Native getters expose the money counter as decimal
presentation and the car counter with the original signed `lw` result.
Spendable money and prize statistics remain separate values.

The original `PresentRoot.win` calls `get_result_index("NONE")`. The VM used
to reject it as an invalid numeric string, aborting award eligibility. String
`toInt` now follows the previously validated `005A2F88` conversion adapter:
zero for nonnumeric input, numeric prefix parsing and signed low 32 bits.
The actual disc script now completes. Overflow/locale behavior outside that
existing conversion comparison scope remains unvalidated.

### What the new user captures establish

The locally preserved PCSX2 slots are:

- `captures/gtmode/20260930/award-slot07`: Sunday Cup first-place trophy screen.
- `captures/gtmode/20260930/award-slot08`: return to the Sunday Cup event menu.

Both snapshots already contain Cr. 5,186 and calendar day 2,453,464. The
compared garage and calendar blocks and prize statistics do not change
between them. Exactly one race-record byte changes: record ordinal 16
(`am_sunday_0001`) changes from `0x0012f002` to `0x00120f02`. Executing the
original eligibility/consumption script against the same initial 768 records
reproduces the complete resulting record array. Its prize condition row is
loaded through the authored `PresentData` and `makeColumn`, not a C table.
The test stubs drawing/audio; rendered trophy interaction is not certified.

These snapshots **cannot establish the earlier cash award** because it was
already applied before slot 07. A before-finish state was requested; retain
07 and 08 for the reward-consumption comparison.

### Reproduce

```powershell
$iso = 'path/to/your/GT4.iso'
build/career-native/Release/gtmode_award_oracle.exe `
  captures/gtmode/20260930/award-slot07/eeMemory.bin `
  captures/gtmode/20260930/award-slot07/Scratchpad.bin $iso
build/career-native/Release/gtmode_award_menu.exe $iso `
  captures/gtmode/20260930/award-slot07/eeMemory.bin `
  captures/gtmode/20260930/award-slot08/eeMemory.bin
```

Final checks: Release application built; `specdb`, `menu_navigation` and
`gtmode_game` passed 3/3; original authored purchase/Auto Save/reload and
actual sale regressions passed. All edited files remain LF. No changes were
made by this work to physics, race_view or their tests/docs. Concurrent edits
in those areas belong to the other session. Nothing was committed.

### Still required for complete awards/progression

- Actual before-finish -> cash-award comparison, including money, statistics,
  race record/A-Spec effects, day/calendar and save.
- Full prize-car presentation/delivery and availability flags. An attempted
  authored `CarPane.get_car` integration exposed missing `sound.startStreamEx`
  and the derived `option.menu_bgm_volume`; the complete delivery was not
  certified or replaced with success stubs. These bindings still need support.
- Championship standings/context, cash and prize-car award composition and
  repeat-event eligibility; exact original point/cash getters alone do not
  complete championship progression.
- Complete A-Spec calculation/aggregation, entry enforcement and opponents.

Next step: use the before-finish state to replay the actual race reward, and
finish the original prize-car delivery dependencies and championship flow.

## Career action continuation (2026-10-03)

Rebuilt gtmode_purchase_menu and gtmode_sale_oracle in build/Release.
Replayed the captured used BEAT purchase (04 -> 06), original GetCarRoot
and Auto Save/reload, and CR-X sale (02 -> 03), full garage/calendar plus
file reload: both passed. Both harnesses now support the deliberate control
GT4_CAREER_TEST_NEGATIVE=1, which changes the native balance by one credit
before comparison; both returned 1. User profiles/cards were read only;
outputs stayed in local build directories.

The aggregate F row was split to record these bounded E proofs separately
from full event/championship progression, which remains F. The current
menu finish boundary accepts only single_race/time_trial; native race_desc
only describes the Arcade/Time Trial grid. Completing event sessions needs
changes under src/core/physics, reserved by docs/PHYSICS_STATUS.md. The user authorized the physics ownership exception on 2026-10-03.
Do not substitute an Arcade race or award a win when leaving a menu.

### Owned race entry foundation (2026-10-03)

User authorized the physics ownership exception. Original event selection
uses `solo_event` / `championship` (gtmode.ad L10926/10928), and native
callers 001141C8 / 00119890 load quick-event / quick-championship.
RaceGTmode constructor 003EDE50 installs world table 00684678, conductor
table 00679CE8, mode 13 at world+0xd68; this shares the race conductor
with Arcade but does not share its world/session assembly.

`gt4_career_active_setup` selects serialized body+0x178*sheet, with sheet
at body+0x488 (00441248 -> 00441228; full car header is 8 bytes).
004452F8 copies 0x178 bytes. New `gtmode_active_setup_oracle` passes all
768 seeded pointer/copy comparisons, whole RAM/SPR, including all three
sheets; GT4_CAREER_TEST_NEGATIVE=1 changes one gear-setting byte and
returns 1. Empty keys and invalid host sheet indices reject without writes.

`gt4_race_desc.owned_car[6]` permits these owned bodies in native race
assembly, preserves their fitted tyres instead of applying Arcade tyres,
validates the car key, and imports body+0x498 wear to entry+0x3468
(the original init consumes that address). A native BEAT body from
purchase-slot06, extracted at game+0x13938, passed 120 frames in Tsukuba
with a second BEAT and a conflicting RACE_TIRE=10. Entry setup and wear
remain unchanged; deliberate gear-setting corruption rejects. This is
an integration smoke proof (A), not GT session equivalence. The isolated
active setup selection/copy is E.

Full event launch, original enemy selection, event settings, result award
transaction and championship progression still need integration. No
win, credits or prize cars are manufactured by the new entry path.

### Event session rules and wear (2026-10-03)

`gt4_career_session_rules` ports 0035E108 into the 0x80-byte rule block.
4096 forced rows match the original with all RAM/SPR checked outside the
call stack. GT4_CAREER_RULES_NEGATIVE flips a rule byte and fails.
`gt4_tyre_wear_global` / `gt4_tyre_wear_rules` port 00367AF8 / 00367918;
4096 pairs match complete RAM/SPR, including global on/off and each of
the six validation exits forced independently. GT4_CAREER_WEAR_NEGATIVE
flips a byte and returns 1. Both isolated boundaries are E.

`race_desc.event_row` installs the decoded RACE settings and rules.
003EDE50 installs GT world 00684678 and mode 13; 003EF498 +0x50/+0x164
installs championship world 00684BF0 and mode 14. Their RTTI functions
005FEA58/005FEB48 derive from RaceWorld, not RaceArcadeSingle.
`RACE_EVENT=am_sunday_0001` with two BEAT cars runs 120 frames in each
world; GT4_CAREER_EVENT_NEGATIVE corrupts a wear rule and fails. This
integration is A: the harness supplies Tsukuba and the two-car grid,
and does not prove the event's original opponents/course/session state.
Menu launch, original enemy construction, awards and progression remain F.

### Tuned opponent setup construction (2026-10-03)

`gt4_garage_tuned` ports 00444690 and the SETTING application in 00444860.
Namespace 0x26 is named ENEMY_CARS in the disc SpecDB; TUNED_CAR is the
original constructor's type, not a separate .dbt filename. It installs
the supplied DEFAULT_PARTS and SETTING rows, then original default color
and automatic gearbox ratios. Production does not execute EE code.

`gtmode_tuned_oracle` compares original returns and all 0x178 setup bytes
for the disc's 2558 rows. 2556 match; original execution fails on keys
2600000F52 and 260000116B at 00442218 -> 00443ED0 (ra=004422B8), with
out-of-range access 0xB46236DC in both purchase-slot06 and award-slot07.
The suite deliberately returns 1 while either original case is unavailable;
it does not silently count exclusions as success. GT4_CAREER_TUNED_NEGATIVE
changes one fitted suspension byte and fails at the first row. Grade A,
pending those two references and full enemy selection/grid integration.

`GT4_TUNED_RUNTIME_DB=1` is an additional diagnostic using the disc row
reader at 00443ED0/00443F40 instead of the capture's caches. It resolves
the two Toyota 7 (GENERIC_CAR 740, toyota7_70) failures, but later exposes
a failed-construction write mismatch at 260000174E (return 0 in both,
gear-default bytes differ). This diagnostic does not pass and adds no E
claim. The normal captured-cache comparison remains 2556/2558, grade A.

### Opponent index selection (2026-10-03)

`gt4_career_enemy_indices` ports 003BB5A8's random index and exhaustion
logic, with RNG conversion 00579330/00578598/005792F0. The test supplies
the candidate list and country-block flags at explicit data boundaries,
and records calls to 004474B0 instead of constructing cars; 0040AA48
paint assignment is excluded. 8192 forced cases match indices and final
RNG state, including all blocked, none blocked, mixed and exhausted pools.
GT4_CAREER_ENEMIES_NEGATIVE changes one selected index and returns 1.
This isolated selection boundary is E; loading RACES files, country
resolution, paint selection and menu grid integration remain pending.

### Disc-driven event assembly smoke (2026-10-03)

The race descriptor accepts `tuned_car[6]` and constructs each selected
prepared entry with 00444690's port. Owned GT player entries receive
setup+0x178 flag 0x100 from 003EDCB0 +0x3c. `race_run_test` with
RACE_EVENT=am_sunday_0001, RACE_EVENT_GRID=1, and the captured owned BEAT
body runs 600 frames on the row's r_mini course, six entries, 25 actual
RACES candidates. Its explicit seed 17 and all-zero country block fixture
are integration inputs, not a proof of the frontend's selection state.
List paint fields are not yet applied. Grade A.

Award-slot07 retains a RaceGTmode world at 01B797F0, conductor 01B8BDB0,
race 01C10690, six entries and course key 128. A direct physics_run
original/native headless comparison with PHYS_FRAME=1 for 120 frames
does NOT match: first AI-state differences at car+0x9bc and 851 RAM
words outside the test stack. Clearing race+0xdc in both inputs does
not resolve this. No equivalence claim is made for that GT capture;
the comparison outputs remain local at build/career-gt-compare. This
must be resolved before certifying a complete GT session or its results.

### Corrections to the tuned and GT comparison diagnostics (2026-10-03)

The runtime-disc SpecDB boundary now writes the complete 64-bit v0,
matching the original Boolean return; writing only its low word retained
stale upper bits, making a missing row appear present. With this fix,
GT4_TUNED_RUNTIME_DB=1 matches all 2558 ENEMY_CARS rows (return and
complete 0x178-byte setups) in purchase-slot06 and award-slot07.
GT4_CAREER_TUNED_NEGATIVE changes +0x50 and fails. This isolated
constructor is E at the explicit disc-row boundary; the unhooked captured
cache remains unsuitable for the two Toyota 7 rows. Production code
required no guessed correction for the rejected construction.

The car+0x9bc mismatch above was wrongly described as AI state.
003F71C8 passes car+0x9a0 to 00386BF8: it is engine-sound state. With
the native headless exclusions applied to both original/native runs via
PHYS_STUB=0x3f71c8,0x35f658,0x354a78,0x35be60,0x3f1148,0x3f12b0,0x3f1368,
all six complete car objects match for 120 steps. Full RAM still has
534 different words outside the test stack; no complete-session E claim
is made. These excluded presentation paths are not proven by this check.

00448C10 maps region 2 to membership selector 2 and region 3 to 1,
but membership function 0044A5A8 +0x04 always returns zero in the
NTSC-U CORE. gt4_career_enemy_blocked preserves this. 4096 forced
original executions cover regions -2..5 and generic/prepared keys,
with only prepared-row reads substituted. The negative country return
is detected. This is E for opponent exclusion, not player entry rules.

### Event paint selection and disc palettes (2026-10-03)

0040A428 forms RGB direction and magnitude; 0040A338 combines brightness
distance and direction similarity (identity special case included).
0040A6F0 starts with geometric 0x3F7C0000 weights, penalizes other choices,
raises fixed-color similarity to the 64th power then multiplies by 10000,
stably sorts complete 0x18-byte records ascending, and normalizes.
0040AA48 rebuilds all weights after each assignment and takes a fresh
00578598 RNG draw per candidate except the last; it is not a cumulative
probability draw. Native arithmetic is compiled with strict FP.

gtmode_paint_oracle compares full 0x304-byte groups, selected indices and
final RNG for 768 forced grids: 1..6 cars, fixed/unselected mixes, up to
32 paints, black/white/duplicate colors. No unsupported instructions in
its generated oracle. GT4_CAREER_PAINT_NEGATIVE flips a weight bit and
fails. Grade E for selection at the explicit palette-data boundary.

gt4_garage_paint_palette follows 00445808/00445828/00445AB8 and
004458D0's order lookup. GENERIC_CAR +0x12 bit 1 omits the last palette
entry only when selecting a random paint; 15 of 722 disc cars set it.
A missing order still leaves the last VARIATION row in the original
output buffer, and 00445AB8 ignores the failed lookup return. Disc
GENERIC_CAR 619, VARIATION 2A00000210 exercises that gap. The native
loader preserves this behavior without a per-car exception. All 722
cars and both forced bit-1 branches match (2166 count/RGB cases),
including fixed paints retaining the hidden last entry.
GT4_CAREER_PALETTE_NEGATIVE alters RGB and fails. Grade E for palettes.

### Event grid paint integration and RACES conversion (2026-10-03)

The native race descriptor now accepts the RACES raw paint qword and the
caller's RNG state after opponent selection. Prepared grids without that
state fail explicitly. No production seed was invented. Paint selection
runs after all setups exist and before physics initialization, preserving
the player's fixed paint and advancing the caller's RNG as 0040AA48 does.

gtmode_paint_oracle additionally compares 4096 conversions against
004474B0/00445880: zero, UINT64_MAX, sign/truncation and rejected high
indices. GT4_CAREER_LIST_PAINT_NEGATIVE changes an index and returns 1.

gtmode_grid_paint_oracle runs the actual Sunday Cup candidate list through
the complete original 003BB5A8/004474B0/0040AA48 sequence for six explicit
input seeds. All six 0x178-byte setups and the final RNG match native
race assembly. Disc access and prepared construction are explicit
boundaries, relying on their separate proofs. This certifies paint/grid
composition at those boundaries, not full GT world/session initialization.
GT4_CAREER_GRID_PAINT_NEGATIVE changes entry 1's paint and returns 1.

The generated closure now includes the original division-by-zero trap
at 003BB6B4; it is never reached for valid nonempty candidate lists.
The earlier statement of zero unsupported instructions applied to the
smaller, paint-only closure.

The owned BEAT fixture (GENERIC_CAR 596) completes 600 native frames on
r_mini with six prepared entries, preserving its active setup and wear.
The frontend still does not launch solo_event/championship, and complete
result/prize/progression/save flow remains unimplemented.

### GT event dirt/snow tyre enforcement (2026-10-03)

003EE3C8 obtains the type through 00447550: settings +0xa0 resolves the
COURSE row and byte +0x10 supplies its type. It is not a RACE-row field.
Types 2/3 require levels 11/12. If both fitted axles already match, the
original skips the parts list. Otherwise it fits available front/rear
keys independently and retains any axle whose required key is absent.

gt4_garage_event_tyres implements this branch sequence and is called
before gt4_race_init for GT event entries. gtmode_event_tyre_oracle runs
the original 003EE3C8 and its COURSE getter with explicit data/getter/
setter boundaries, using all 722 actual stock setups and CARS lists.
18050 type/axle/missing-list cases compare all 0x178 setup bytes.
GT4_CAREER_EVENT_TYRE_NEGATIVE alters a tyre key and fails (exit 1).
Grade E for this bounded rule; full event session remains A/F.

### Event result processing and authored payment (2026-10-03)

gt4_career_process_event_result ports 00114498 at explicit result-query,
calendar and presentation boundaries. gtmode_event_result_oracle compares
8192 cases covering abandonment, ranks 0..6, driver modes 0..3, record
updates, previous best, arbitrary statistics, signed prize accumulation
and wrap. It compares the complete statistics block, record, previous
best, work prize and calendar arguments. The calendar time is best lap,
not prize. Abandonment preserves the existing work prize; clearing it
belongs to 00114250 initialization. GT4_CAREER_EVENT_RESULT_NEGATIVE
alters a statistics counter and exits 1. Grade E for this bounded rule;
the helper is not yet connected to the frontend race result.

menu_save_refresh now installs the existing native game methods and dirty
save callback in RaceEventProject and ChampionshipProject. Their original
/share/prize_action.ad onPrizeCheck and makePrizeFast scripts can call
incMoney after restoring the career profile. gtmode_event_payment_oracle
compares 126 balance/prize/speed cases and 20370 callback increments per
project against the complete original 0043AFB0 closure. Six initial
balances and twelve prizes cover decimal decline, fast payment and
64-bit wrap. Native file store/load and menu profile restoration retain
the final balance. Tests use a unique build-local save directory.
GT4_CAREER_EVENT_PAYMENT_NEGATIVE alters the observed balance and exits 1.
Grade E for payment/state persistence at these boundaries. UI completion,
prize-car delivery and complete event/championship flow remain outside
this proof.

The grid comparison also runs six seeds each through
rh_paris_rally_e_0001, rh_nursnow_e_0001, rh_icerace_e_0001 and
rh_grandcanyon_e_0001. Original 003EE3C8 runs after paint selection,
with the actual COURSE getter; complete setups and final RNG match.
This integration test caught and fixed the COURSE table ID (0x22)
versus decoded row width (22 decimal) distinction.

After these changes, the Release opengt4 executable builds successfully,
menu_tests and gtmode_game_tests rebuild, and all 22 CTest cases pass.
Both authored payment oracles and the 8192-case result oracle also pass
against the rebuilt native core. This build/regression check does not
raise the full career event/championship flow above F.

### Disc-backed event descriptor and quick-work state (2026-10-03)

career_event.c prepares the actual race descriptor from RACE/COURSE/RACES,
using the supplied equipped-car body and explicit caller RNG. The event
owns its row/body buffers; its descriptor must not outlive/move away from
those buffers. The original list order and NTSC-U country predicate feed
the independently proven selection rule. Prepared ENEMY_CARS rows are
at least 0x18 bytes, not necessarily the 0x20-byte host buffer size.
No clock/Arcade RNG seed is substituted. The grid oracle now invokes this
production builder, preserving the whole body and comparing resulting
setups/final RNG with original selection and event tyre enforcement for
five events and six seeds per event. GT4_CAREER_EVENT_GRID_NEGATIVE
corrupts equipped setup +0x19; comparison reports the byte difference (exit 1). Grade E only at
the documented data/prepared-construction boundaries. Frontend launch
and the global RNG lifecycle still require integration/proof.

menu_quick_race now carries raceLabel, IsSessionFinished, prize,
GrandPrize and IsBSpec. MQuickWork refreshes them on every call, preventing
stale cached-VM results from selecting the following session's post-race
page. pregrid_menu supplies these through the production setter, executes
ten original event/mission/championship getStartPage branches, then resets
each same VM to pre-race and verifies replay is absent. The existing
--selector-negative returns 1. Grade C for this state bridge/page selection;
this is not a drawing-list/pixel comparison or a complete career race.

Release opengt4, pregrid_menu, menu_tests and gtmode_game_tests rebuilt
successfully after the descriptor/quick-work changes. All 22 CTest cases
pass. No native solo_event/championship frontend launch was enabled yet.
The next integration must retain the original RNG lifecycle, use this
owned descriptor in race_game and apply real results/progression to the
profile before claiming the whole event flow is complete.

### Prepared GT grid in the playable bridge (2026-10-03)

race_game_start_prepared consumes the native event descriptor without
reconstructing stock Arcade cars. Model labels resolve the descriptor's
GENERIC_CAR IDs; visual paint indices come from the final entry garages
at entry +0x38 after original-derived selection (003BB5A8/0040AA48).
The bridge owns copies of all supplied owned bodies and the event row.
It retains finalized paint indices and the resulting caller RNG for
restart/replay, so caller/menu storage may be released immediately.
COMMAND_START restart in main.c now uses race_game_restart; normal Arcade
start still uses its existing path and driving options.

pregrid_probe --career reads the local equipped BEAT fixture, prepares
Sunday Cup with explicit test seed 17, opens physics/model presentation,
overwrites all caller buffers, freezes all 32 MB through 1201 pregrid
presentation ticks, advances one native simulation step, opens replay
and restarts. All six complete 0x178 setups and RNG remain unchanged
across reopening. --career-negative corrupts the reference setup byte
+0x19 and returns 1. The normal Arcade pregrid/restart probe also passes.
Local picture: build/career-pregrid.png. This is a runtime/integration
regression, not original full-session/pixel parity: grade A. GT countdown
initialization differs from the Arcade probe and needs its original
constructor/start transition proof; elapsed simulation is tested without
claiming the GT countdown is complete. Entry from career menus, real
result/profile updates and progression are still pending.

### Original countdown initialization proof (2026-10-04)

gtmode_start_oracle executes 003C0190 and 003C0208, including original
queue reset/push, records reset and time-limit initialization. Only
003973D0's presentation of course lights is excluded; native headless
mode omits the same presentation call. All 0x1000 bytes of session state
match after initialization and each of the first three ticks for 2053
cases: five actual event RACE rows and 2048 forced type/seconds/time-limit
combinations. GT4_CAREER_START_NEGATIVE changes session +0xb8 and returns
1. Grade E for this bounded countdown rule, not full GT initialization.
The generated function range explicitly retains all local switch targets;
one generated fall-off sentinel is not reached by these cases.

Sunday Cup, Paris, snow, ice and Grand Canyon RACE rows use start kind 2;
003C0208 maps that to 600 frames, with 597 remaining after three ticks.
The native equipped Sunday Cup presentation probe on Autumn Ring Mini
reports rolling=1, effective kind=-1 and initial frames=0. Its zero count
therefore comes from the rolling override in race_run.c, not from the
kind-2 countdown rule. Original selection/integration of the rolling GT
route remains unproved; no countdown values were adjusted to hide it.
The equipped pregrid/replay/restart regression continues to pass.

### Original common start selection (2026-10-04)

The writer previously missing for Time Trial was found in the common world
initializer 00387FB0 at 003883EC..0038842C, after virtual method +0x50.
Session kind 0/3 sets race+B4=-1; kind 2 sets it to 3; kind 1 and other
kinds preserve the decoded RACE start kind. The race getter 003475B8
uses conductor+F866, signed grid index +F884 and course flag +F886;
it does not read +CBE4. Time Trial getter 003F5528 returns 2.

gtmode_start_select_oracle compares 2048 forced selection states, complete
0x1000-byte race block, and 131072 flag/grid-sign getter combinations.
No callbacks are substituted. Original selection stops at 00388438, before
presentation continues. Two generated unreachable fall-off sites are not
executed. GT4_START_SELECT_NEGATIVE changes B4 and fails;
GT4_START_GETTER_NEGATIVE changes the getter result and fails. Grade E
for these bounded rules. race_run now invokes the common selection after
race_begin, replacing the prior course-CBE4 shortcut and estimated TT
assignment. The prepared Sunday Cup bridge still selects rolling on r_mini;
its full constructor/session integration remains A. Arcade and equipped
pregrid/replay/restart regressions pass. Full menu/result/progression
integration remains F.

### Authored event request to equipped-car snapshot (2026-10-04)

menu_get_career_request reads next_sequence, race_code and
garage.riding_car, preserving all 0x4b8 bytes from gt4_game_car_body,
including current oil_state at +0x498. _tmp_gtmode.ad L10918..10928
clears Arcade course overrides, supplies race_code and chooses
solo_event/championship; exit_menu L891..894 supplies next_sequence.
The snapshot owns its data after the source VM closes. Invalid modes,
labels or missing full garage body clear the output and return an error.

gtmode_event_request executes original exit_menu and its queued original
_start_page callback, with finish and visual transition as explicit host
boundaries. Unrelated initial welcome events/watchers are outside this
test. Routes, label and complete body are checked with zero tolerance;
current wear replaces stale blob wear. Invalid Arcade request and missing
car reject; snapshot survives VM destruction. The Sunday Cup snapshot
feeds career_event_prepare with explicit test RNG 17 and resolves six
entries/r_mini. GT4_CAREER_REQUEST_NEGATIVE changes body+0x19 and exits 1.
Grade C for this request adapter, not admission requirements, page drawing
or full session. Production nat_finish/main launch remains unconnected;
no runtime random seed or championship sequence is invented. The CORE
value at D_00621AE8 was observed to be zero and no direct call/pointer to
003BB598 was found, but this is not yet proof of the full RNG lifecycle.
Build opengt4 and all 22 CTest regressions pass.

## Integração de prova avulsa (04/10/2026)

O aplicativo agora liga solo_event ao descritor de carreira e ao quick-event,
aplica o resultado antes de iniciar replay e executa o seletor original da
página de prêmio/pós-corrida. O melhor resultado inicia em 15 (00113CB4) e
persiste nas tentativas; o RNG inicial vem do construtor original de 00621AE8 (correção documentada abaixo).
Campeonato recebe aviso de sequência ainda ausente.

```powershell
cmake --build build --config Release --target opengt4 gtmode_event_cycle_oracle
build/Release/gtmode_event_cycle_oracle.exe '<ISO>' extracted/core.bin
$env:GT4_EVENT_CYCLE_NEGATIVE='1'
build/Release/gtmode_event_cycle_oracle.exe '<ISO>' extracted/core.bin
Remove-Item Env:GT4_EVENT_CYCLE_NEGATIVE
```

56 casos passaram; negativo saiu com código 1. Compara resultado aplicado ao
perfil com 00114498 e executa pagamento autoral seguido de save/load.
AfterRoot/PrizeRoot avançam 60 ticks sem erro de VM. O teste injeta os dados
da corrida; não compara uma sessão completa nem o desenho. Créditos e
prêmio acumulado conferidos após reload. Desgaste/distância, carro-prêmio,
progressão completa de série e ciclo do RNG continuam pendentes.
setModelSetPath preserva caminho/flags solicitados por quick-event.ad:893;
o modelo 3D do troféu permanece ausente. Notas C na fronteira de perfil,
A na integração de prova avulsa, F no campeonato.

### Desgaste e distância após corrida (04/10/2026)

0010FA58 copia os 32 bytes em car+0x6dc para riding body+0x498 a cada
tentativa; integrado antes do replay. 0035DDB0 separa distância A/B e
0010FB3c..0010FBB4 converte para uint32 pela cvt.w.s do PS2. O original
mantém os dois resultados da última tentativa e só soma em statistics
+0x70/+0x78 ao deixar o laço (00114044..0011407c). O aplicativo segue essa
ordem; não soma novamente ao replay ou depois de pagar o prêmio.

Oráculos gtmode_event_wear_oracle e gtmode_event_distance_oracle: 4096
casos cada, com negativos GT4_EVENT_WEAR_NEGATIVE e
GT4_EVENT_DISTANCE_NEGATIVE saindo com código 1. event_cycle agora verifica
os bytes de desgaste após 56 save/load e a persistência dos totais de
distância. Corpo do carro nesse teste é um fixture, não carro simulado.

E nas rotinas de gravação/conversão/soma; integração do evento permanece A:
falta comparação completa da sequência, atualização dos metadados compactos
da garagem no fechamento, RNG, carro-prêmio e progressão da série.

### Retorno à carreira e entrega da Sunday Cup (04/10/2026)

O retorno quick-event -> GT Mode agora executa a verificação autoral
makeEntitledPresentListByProject (gtmode.ad:141..154) e entra em PresentRoot
quando a lista exige. Corrigida perda da atualização dos registros quando
menu_enter restaurava o perfil compartilhado: capturar o resultado dessa
verificação mesmo quando a lista está vazia.

A ponte startStreamEx passa a aceitar o grupo get_car solicitado por
gtmode.ad:14542, encaminhando ao backend ADS existente; antes só admitia
default_quick e interrompia a entrega. Não prova a paridade do som.

```powershell
cmake --build build --config Release --target gtmode_award_delivery opengt4
build/Release/gtmode_award_delivery.exe '<ISO>'
$env:GT4_AWARD_DELIVERY_NEGATIVE='1'
build/Release/gtmode_award_delivery.exe '<ISO>'
Remove-Item Env:GT4_AWARD_DELIVERY_NEGATIVE
```

O teste parte de perfil novo, aplica resultados injetados nas provas existentes
no catálogo (não presume quantidade), exercita retorno parcial e final e
get_car original. Corpo do carro conferido contra o construtor nativo, dia,
prize_cars, disponibilidade, PCM e recarga verificados; consulta posterior
não duplica o prêmio. Negativo retorna 1. Escopo C; a referência EE para
entrega completa não foi comparada. gtmode_award_menu mantém sua prova
isolada de 768 registros contra award-slot07/08. 56 casos do event_cycle e
22/22 CTest passaram. Ciclo completo continua A, campeonato F.

### RNG inicial e grids consecutivos (04/10/2026)

Corrigida a leitura do zero armazenado no CORE como estado inicial: a tabela
de construtores em 0061736c aponta para 003BB820, que chama
003BB7E8(1,0xffff), depois 005792D8 e grava 0x12345678 em 00621AE8.
A afirmação histórica acima sobre o valor carregado não descrevia o estado
após os construtores. career_event_rng_load agora valida o CORE do disco e
aplica essa inicialização nativa; nenhum código EE é executado no aplicativo.

O oráculo executou o construtor real em 4096 estados anteriores diferentes
e conferiu a semente nativa. Em modo encadeado, manteve o estado entre seis
montagens para cada uma de cinco séries: Sunday Cup, Paris, neve, gelo e
Grand Canyon. Usa labels consecutivas existentes no catálogo e volta à
primeira quando não existe a próxima; não simula progressão de campeonato.
Compara cada setup completo e o RNG final contra as rotinas originais de
sorteio/pintura/pneu, com as fronteiras de construção/disco já declaradas.
Na Sunday Cup, RNG final nas cinco provas e na repetição da primeira:
00951950, f05daa70, 5031bd85, 609857c1, cc05d96b, c7fad104.

```powershell
cmake --build build --config Release --target gtmode_grid_paint_oracle pregrid_probe opengt4
build/Release/gtmode_grid_paint_oracle.exe extracted/core.bin '<ISO>'
$env:GT4_CAREER_GRID_CHAIN='1'
build/Release/gtmode_grid_paint_oracle.exe extracted/core.bin '<ISO>'
$env:GT4_CAREER_RNG_CHAIN_NEGATIVE='1'
build/Release/gtmode_grid_paint_oracle.exe extracted/core.bin '<ISO>'
Remove-Item Env:GT4_CAREER_RNG_CHAIN_NEGATIVE
Remove-Item Env:GT4_CAREER_GRID_CHAIN
$env:GT4_CAREER_RNG_NEGATIVE='1'
build/Release/gtmode_grid_paint_oracle.exe extracted/core.bin '<ISO>'
Remove-Item Env:GT4_CAREER_RNG_NEGATIVE
```

Os dois negativos retornaram 1, alterando início e segundo RNG final.
pregrid_probe --career agora usa career_event_rng_load, não semente 17:
1201 ticks congelados, passo da física, replay e reinício preservaram os seis
setups e RNG; --career-negative detectou alteração de setup. Capturas locais
build/career-rng-pregrid.png e build/career-rng-negative.png.
Nota E no início/continuidade do RNG desse grid; integração da sessão A.
Campeonato F, entrega completa de carro-prêmio C e outros consumidores do
RNG na sequência permanecem fora desta prova.


## Campeonato: suspensão e retomada (04/10/2026)

`main.c` grava o contexto ao sair de quick-championship. Como
00119618/00119BD8, uma etapa concluída avança antes de suspender; última
etapa concluída e Retire limpam somente a flag. O save contém os 0x88 bytes
de dados do objeto vivo de 0x90 bytes (primeiros oito são cabeçalho da classe).
A ponte expõe isSuspended, suspended_racecode/session e abandonRace, e
retoma pelo carro equipado e cinco prepared keys/pinturas salvos (00119AE8),
sem sortear novos adversários. A saída também captura o perfil de
quick-championship, antes omitido pelo caminho exclusivo de quick-event.

Provas: gtmode_championship_cycle, 34 campeonatos/200 etapas com resultados
injetados, preservação dos blocos e pontos após save/reload e execução do
sureNoSuspended original até o pedido championship. pregrid_probe
--championship carrega três pistas, restaurando a última pelo contexto salvo;
seis setups de 0x178 bytes e RNG conferidos sem tolerância. Controles
GT4_CHAMPIONSHIP_RESUME_NEGATIVE (pontos),
GT4_CHAMPIONSHIP_RESTORE_NEGATIVE (pintura) e
GT4_CHAMPIONSHIP_MENU_STACK_NEGATIVE (seleção) retornam 1. Regressão:
56 casos de gtmode_event_cycle_oracle, oráculos do campeonato e 22 CTest
passam. opengt4.exe atualizado após bloqueio temporário do linker.

Nota C para integração do contexto; regras isoladas permanecem E.
saveMenuStack/loadMenuStack persistem a pilha nativa menuPush/menuRef no
namespace championship e permitem a confirmação original de retomada.
Essa pilha permanece A: contrato exercitado, mas formato, limites e falhas
PS2 ainda sem comparação nativa; a pilha não é recuperada do save PS2 importado.
Não elevar o ciclo completo a E antes de provar opções da sessão,
apresentação, pilha e corrida completa. Não houve commit/push.


## Pilha do menu: formato original (04/10/2026)

A nota A da pilha anterior foi substituída por E nas operações e C na
ponte/save. Registro nativo 00169214..00169568 associa menuPush, menuPop,
menuRef, menuClearStack, saveMenuStack e loadMenuStack aos wrappers
00165EE0, 00165FC0, 00166100, 00166278, 001662B8 e 001662F8.
Estes usam o objeto game+0x3A368 e as funções 0042FD00, 0042FD78,
0042FDF0, 0042FB48, 0042FE88 e 0042FF48.

O original contém texto separado por zero em um buffer de 256 bytes:
+0xA68 comprimento, +0xA6C texto, +0xBAC comprimento salvo e +0xBB0
texto salvo. Push exige comprimento+strlen <255 e inclui o terminador.
Save copia todos os 256 bytes, incluindo o restante não usado; load
retorna falso se o comprimento salvo é negativo e deixa o atual intacto.
Clear apenas zera os primeiros bytes/comprimento e invalida o checkpoint.
A lista anterior com máximo de 32 itens não reproduzia estes limites.

8192 comparações automáticas de memória e retorno no oráculo do campeonato
passam; controle GT4_MENU_STACK_NEGATIVE retorna 1. Runtime usa os mesmos
primitivos, preserva checkpoint de 260 bytes no save nativo e deixa o
cursor atual fora do save. Teste de retomada nos 34 campeonatos/200 etapas
preserva seleção e executa a confirmação original. Strings retornam texto,
inclusive entrada numérica convertida a texto, como o wrapper original.
Importação do checkpoint de um save PS2 e comparação da conversão de
objetos arbitrários pela VM permanecem fora desta prova.


Ampliação da prova: os mesmos 8192 estados são fornecidos à ponte
`gt4_game_menu_stack_action`, que usa uma VM nativa; o teste compara com
as seis funções EE os 0x248 bytes de estado, checkpoint de 260 bytes,
textos/nil de pop/ref e booleano de load. A ponte também recebe E para
argumentos textuais e profundidades, com fronteira explícita na conversão
de objetos para texto. GT4_MENU_STACK_BRIDGE_NEGATIVE altera o buffer
extraído da ponte e retorna 1. Persistência do arquivo nativo e retomada
completa continuam C. Passaram 34 campeonatos/200 etapas, 22 CTest, build
opengt4 e diff --check. Sem commit/push.

### Metadados da sessão no pré-corrida do campeonato

`MQuickWork` agora publica `Laps` e `CourseLength`, ausentes antes. Fonte: `00119AA0..00119AB4`, quick +0x4f4/+0x4f0, e `00395400`/`00396800` para runway +0x14. A ponte usa RACE +0, que em eventos vem do RACE do disco +0x43; não usa o padrão de voltas de `gt4_race_desc`.

`pregrid_menu` e `pregrid_probe --championship` passam; controles de erro deliberado dos dois testes falham (exit 1). Build Release de `opengt4.exe` passa. Nota A: arredondamento com `nearbyintf` ainda precisa de oráculo EE para `cvt.w.s`. Secura da pista e opções de sessão continuam pendentes; nenhuma promoção do campeonato completo a E.

### Correção e prova EE da conversão de extensão

A hipótese anterior de arredondamento nearest-even estava errada: `00119AB0` trunca/satura. `gt4_career_course_length` substitui `nearbyintf` na ponte. O teste executa a rotina completa `00119918`, mantendo `00395400`/`00396800`; catálogo/rótulos/modo são fronteiras explícitas e zero participantes isola os campos quick +0x4f0/+0x4f4. 16.384 combinações de bits/voltas, incluindo limites, decimais, sinais, subnormais, NaN e infinitos, passam bit a bit; `GT4_COURSE_LENGTH_NEGATIVE` falha (exit 1). Conversão E, integração dos campos C (tolerância zero).

`pregrid_probe --championship` passa nas três etapas reais após a correção. Compilação dos testes passou; link de `opengt4.exe` bloqueado por LNK1104 (arquivo em uso), sem encerrar o processo do usuário. Campeonato completo permanece A por opções/apresentação pendentes.

### Retorno das opções à sessão correta

`main.c` não força mais ChampionshipRoot/RaceEventRoot após Option. O projeto volta sem raiz imposta e `switch_to` usa `menu_enter_quick_start` (getStartPage original), antes de inicializar qualquer raiz. Isso mantém AfterRoot no pós-corrida e cursorPosition do botão Option. `pregrid_menu` confere página, foco e comando nos dois projetos, pré/pós, tolerância zero; controle GT4_CAREER_OPTION_RETURN_NEGATIVE falha. Nota C: teste headless da ponte, não ciclo completo SDL. Resultado com zero participantes isola desenho dos tempos, pois getGridTime ainda falta.

Botão Setting original emite COMMAND_SETTINGS e salva foco; o teste passa e GT4_CHAMPIONSHIP_SETTINGS_NEGATIVE falha. Original 00118EF0: despacho enum15 em 001192F0..00119308 abre projeto setting; Option enum22 abre option em 00119678..0011968C. Não ativado setting: teste de entrada revelou funções de carro ausentes (linhas 331..819 do script) e OptionMenu.setInsensitive. Pending F explícito. Spectator enum14 e FreeQualify enum20 são sessões diferentes (001191E8 e 00119310), não aliases de Start.

Build Release opengt4.exe passou nesta etapa; arquivo anteriormente bloqueado ficou disponível.

### Tempos e ordem de chegada no pós-corrida

`getGridTime` implementado: `00127DF8` lê quick +0x454+4*i; `00114390` preenche com car +0x718 se car +0x5b6 !=0, ou 0x157529ff caso contrário. Helper gt4_career_grid_time E: 4.096 listas de 1..6 carros comparadas às duas rotinas EE; catálogo e ordenação explícitos como fronteiras. GT4_GRID_TIME_NEGATIVE falha.

Nova ponte race_game_quick_results reordena pelo rank estável, mapeia driver ao grid_slot original antes de copiar carro/pintura/tempo, atualiza slot do jogador. main usa a ponte nos eventos individuais e campeonato; antes os eventos preservavam ordem inicial e campeonato pressupunha slot==driver. Native getGridTime fornece palavras ao script original.

Ponte C: pregrid_menu agora executa pós-corrida com seis entradas, compara getters e textos 2'03.456/marcador; pregrid_probe --championship confere grade real com ordem invertida, pintura, tempos e não-finalizados por HUD injetado e restaurado integralmente. Não prova corrida física completa ou pixels. GT4_GRID_TIME_BRIDGE_NEGATIVE e GT4_GRID_TIME_RESULT_NEGATIVE falham. Três etapas reais passam; opengt4.exe Release compilou. Pendências de Setting, Spectator e FreeQualify permanecem.

### Máscaras de indisponibilidade da tela Setting

Implementado OptionMenu.setInsensitive(context, array): 002C7A80 converte lista numérica; 002C9B68 altera somente min(lista, itens lógicos), deixando cauda intacta e ignorando decoração (bit 13). Ao bloquear o item selecionado, 002C9898 chama 002C9740(start=0): primeiro habilitado, ou preserva índice quando não existe.

ui_insensitive_oracle compara host nativo a 002C9B68/002C9740 com 4.096 casos, seis itens e decoração intercalada, listas 0..9, flags iniciais e índices variados. Métodos de traversal e redraw são fronteiras explícitas; getters/setter de flags originais executam. GT4_OPTION_INSENSITIVE_NEGATIVE falha com exit 1. Máscaras/seleção E; ponte de inteiros C com tolerância zero (conversão EE completa de outros tipos não exercitada).

Tela Setting ainda depende de métodos de peças/regulagens do carro; nenhum getter ausente foi preenchido com valor fictício. Sua entrada pelo campeonato permanece pendente. Também corrigido erro de tipo encontrado em nat_pad_buttons: retorno ah_val volta a ah_int(0), em vez da chamada inválida ah_num(0). Build opengt4.exe e regressões verificados nesta etapa.

### Propriedade slot da regulagem A/B/C

`MCarData.slot` ausente produzia Setnil na linha 701 de setting.ad. Propriedade nativa agora usa o corpo real: getter 00140C70 lê live +0x490 e setter 0043E2E8 grava a palavra; o corpo serializado remove class header de 8 bytes, logo +0x488. Sem clamp inventado: palavra baixa inteira preservada; consumidores que endereçam setup validam limites separadamente.

Host owned car persiste membros normais do módulo e delega qualquer outra propriedade. `gtmode_championship_oracle` compara 4.096 setters a 0043E2E8, corpo inteiro preservado, getter igual à palavra original, valores signed/long e seleções 0..2 em gt4_career_active_setup. GT4_SETTING_SLOT_NEGATIVE falha. Nota E somente para slot; não prova fluxo da tela ou getters de peças/regulagens. Build Release e 22 CTest passam.

Inventário dos acessos restantes de setting.ad inclui beginSetting/endSetting, setupPartsBuffer, getEquipedParts/changeParts, getCurrentPower, curvas motor/câmbio, isExistParts/isRemovableType/isRemovedNormalType/isDefaultNoParts e getters/setters/ranges de suspensão, LSD, NOS, distribuição, freios, lastro, ASCC/TCSC, aerodinâmica e marchas. Nenhum foi substituído por dummy.

### Saída da tela de regulagens: endSetting

`0013FF38` chama `00441350`: três folhas de setup, chave da transmissão
em setup+0x60, tipo em linha TRANSMISSION+0x3a (`004462E0`).
`00441300` mapeia tipos 0..5 para 0, 6..10 para 1, 11 para 2,
12 para 3 e demais para -1. Copia live+0x470+8*índice para
live+0xf8+0x178*folha. O índice -1 lê live+0x468, inclusive chave
ausente; não se inventou uma substituição.

Implementado em `gt4_career_end_setting`, `MCarData.endSetting` e consulta
SpecDB no binder de carros em menu.c. Consultas terminam antes da gravação;
dados ausentes geram erro explícito.

Prova **E** da atualização do corpo e método nativo: 4.096 corpos de
0x4b8 bytes contra `00441350`, com `004462E0` e `00441300` originais.
`00443ED0` é fronteira explícita da consulta: cobertura de todos os 256
tipos, chave ausente UINT64_MAX, três folhas, preservação e retorno nil.
`GT4_END_SETTING_NEGATIVE=1` modifica body+0xf0 e falha com exit 1.
Consulta real do disco implementada, mas ainda sem prova própria.

Oráculo completo passou; opengt4.exe Release compilou; 22/22 CTest e
pregrid_menu (29 páginas originais) passaram. Tela completa continua F:
ainda faltam beginSetting, listas de peças, setters/ranges, curvas de
motor/câmbio e entrada/apresentação da tela.

### Preparação das peças da folha selecionada: setupPartsBuffer

`beginSetting` (`0013FED8`) chama duas etapas: `00147D08` carrega os
metadados do carro e `00147D50` prepara as linhas de peças. Esta última
é também o contrato `setupPartsBuffer` (`0013FEA8`). Implementada a segunda
etapa, mantendo a primeira e a entrada completa da tela pendentes.

`004401C0` executa 11 getters (`0043FE58..00440170`), que calculam
0x178*slot usando live+0x490. Chaves no corpo sem header:
0x28, 0x48, 0x40, 0x80, 0xb0, 0x50, 0x58, 0xe8, 0xe0, 0x100, 0xf8.
Destinos: 0, 0xc, 0x3a, 0x50, 0x70, 0x84, 0xf4, 0x110, 0x11a,
0x134, 0x13a. NOS.dbt, extraído localmente do disco, informa largura
8 no header+12; portanto a última linha termina em +0x142.

Prova **E** da seleção/transferência: 4.096 buffers completos contra o
original, três slots válidos, combinações de linhas ausentes, bytes
sintéticos e retorno nil da chamada Adhoc; corpo do carro preservado.
`00443ED0` é a fronteira explícita da obtenção das linhas.
`GT4_SETTING_BUFFER_NEGATIVE=1` altera o primeiro byte e falha (exit 1).
Não há prova da inicialização do buffer: método exige blob pré-existente
`_setting_parts_ps2`, sem inventar zeros para permitir abertura da tela.

A consulta real `_getSettingPartRow` usa namespace alto32 e id baixo32
sign-extended como `00443ED0/00443F00`; a busca da transmissão agora
reutiliza essa consulta, removendo a truncagem anterior para 16 bits.
Essa integração com banco real ainda não tem comparação própria.

Oráculo, 22/22 CTest e pregrid_menu (29 páginas) passaram. Core e harnesses
compilaram. Link do opengt4.exe bloqueado por LNK1104: executável aberto;
a versão em disco ainda não inclui este passo. Não fechamos o jogo.
Pendentes: inicialização do wrapper, metadados de beginSetting, listas e
troca de peças, setters/ranges, curvas e fluxo/apresentação da tela.

### Inicialização real do wrapper de regulagens

`00148030` aloca 0x190 bytes e chama `00147D88`. Este construtor inicializa
a classe base (`0030A678`), cria o corpo do carro de 0x4c0 (`0043DC20`),
inicializa o cache em wrapper+0x18 (`00449D58`) e os auxiliares
(`00146628`). Não há memset do buffer de peças em wrapper+0x30.
Logo, a pendência não deve ser resolvida fabricando um buffer zerado.

Implementado helper `gt4_career_setting_wrapper_reset`: chave do cache
UINT64_MAX, ponteiro/tamanho zero, ponteiro de tipo fornecido explicitamente;
seis palavras auxiliares +0x174..0x188 são zero exceto +0x180 = -1.
Todos os outros bytes ficam preservados. **E** nesses writes: 4.096 wrappers
com bytes aleatórios comparados integralmente com as duas rotinas originais.
Identidade do ponteiro de tipo é fronteira do teste, fornecida pelo resultado
do oráculo; não é prova da representação do cache no host.
`GT4_SETTING_WRAPPER_NEGATIVE=1` altera +0x180 e acusa erro (exit 1).

O helper ainda não está integrado à construção dos carros Adhoc. Faltam
a representação do cache, carregamento via `00449E10`, ligação de
`beginSetting` e entrada completa da tela. A preservação do buffer passa;
nenhum padrão inicial de memória foi inventado. Oráculo completo passou.
Link opengt4.exe permanece bloqueado por LNK1104 (jogo aberto).

### Lote: estágio equipado, regras de remoção, existência e linhas do disco

Métodos ligados aos carros: `getEquipedParts` (grafia original),
`isRemovableType`, `isRemovedNormalType`, `isExistParts`. Os três primeiros
passaram em 8.192 casos contra `0043F4C8`, `0044A250` e `0044A288`,
incluindo as chamadas Adhoc nativas. Removíveis: 13,18,19,20,22,23,27,28;
sem normal após remoção: 20,22. São consultas, não a implementação de
instalar/remover peças.

O estágio usa as chaves da folha selecionada e o byte específico de cada
linha, com `lb` signed só na suspensão (tipo 3). Tipo 31 lê setup+0x168:
zero vira -1. Estágio negativo recebe normal 0 somente quando autorizado
pela tabela CORE.GT4 0x6a7c08 (`0044A1B8`). A tabela agora é carregada
pelo leitor de dados de peças. Categorias sem branch passam pelo mesmo
fallback; tipos inválidos e três slots válidos cobertos.
Bytes das linhas são uma fronteira explícita do oráculo, sem substituir
lógica de seleção/stágio. Nota E nesses três contratos signed32.

Ponte do disco, nota C, tolerância zero: `pregrid_menu` verifica 64 linhas
reais de 32 namespaces (primeira/última) contra o leitor SpecDB, incluindo
preservação de todas as colunas e id ausente 0x10000. São consultas reais
à ISO. O leitor de linha é compartilhado pela referência dessa ponte,
logo não se declara uma prova independente do descompressor.

`isExistParts` lê o CARS do carro, delega ao lookup já comparado com o
original e testa resultado diferente de UINT64_MAX. A ponte passou em
2.890 consultas (17 carros, tipos -1..32, estágios -1..3), C. O teste
não implementa nem prova a construção do cache original.

Controles negativos (todos exit 1): GT4_EQUIPPED_STAGE_NEGATIVE,
GT4_PART_PREDICATE_NEGATIVE, GT4_SETTING_DISC_NEGATIVE,
GT4_PART_EXIST_NEGATIVE. Oráculo completo e 29 páginas de menu passaram.
Executável Release compilou e foi atualizado (bloqueio anterior cessou).

Continuam pendentes: integração da inicialização do wrapper/cache,
carregamento dos metadados e beginSetting completo, listas de peças,
changeParts, isDefaultNoParts, setters/ranges, curvas e fluxo da tela.
Este lote não modifica física. Não houve commit nem push.


## Remoção de peças e consulta DEFAULT_PARTS

`00446AE0` reduz o tipo a uint16 e escreve somente a chave -1 na folha
selecionada. Os 30 offsets vêm da jump table 006A6E60. Comparação de
4.096 corpos completos e três folhas: E. A ponte changeParts segue a
condição de 001403A0 (chave ausente, estágio zero, categoria removível):
C, com consulta da chave como fronteira e tolerância zero no corpo.
Chave existente retorna erro explícito; não fabrica ajustes de instalação.

`0013F3A8 -> 004455B0` consulta GENERIC_CAR -> DEFAULT_PARTS e só para
muffler (tipo 20) testa a chave ausente em DEFAULT_PARTS+0x98. Ausência
na chave ou no registro devolve falso. 4.096 consultas contra a rotina
original e ponte Adhoc: E, lookup/existência como fronteiras explícitas.

A ponte SpecDB agora aceita os 39 namespaces originais. Teste do disco:
66 linhas de 33 tabelas (incluindo DEFAULT_PARTS), 2.890 consultas de
existência e 17 consultas da peça original em carros do catálogo, C,
tolerância zero. Referência usa o mesmo leitor SpecDB; sem prova
independente do descompressor.

Controles GT4_REMOVE_PART_NEGATIVE, GT4_DEFAULT_PART_NEGATIVE e
GT4_DEFAULT_DISC_NEGATIVE falham com exit 1. Oráculo completo, 29 páginas
e 22/22 testes CTest passaram. opengt4.exe Release foi recompilado.

Pendentes: instalação completa de changeParts (00440B50 e inicialização
dos ajustes), integração do wrapper/cache e beginSetting, listas,
setters/ranges, curvas e fluxo completo da tela Setting. Física não foi
alterada. Não houve commit nem push.


## Instalação de chaves sem defaults

`00440B50` e jump table 006A6890: tipos 1,13,18..23,25,26,28 gravam
somente a chave na folha selecionada; tipos 8,15,16,17,29,30 gravam nas
três folhas. Tipos 6,7,9,12 não modificam nada. Namespaces fora de 1..31
não modificam nada. Todos os offsets vêm dos branches originais.

Helper e caminho Adhoc changeParts comparados em 8.192 corpos completos
contra a rotina original: E para esse contrato, três slots válidos,
chaves aleatórias, 17 categorias e branches sem operação. Lookup da chave
é entrada explícita. Nenhuma função de inicialização de parâmetros foi
substituída nesta prova: seus branches não são exercitados e continuam
retornando erro sem mutação no port.

GT4_INSTALL_KEY_NEGATIVE altera um byte e o teste termina com exit 1.
Oráculo completo e 22/22 CTest passaram. Executável Release atualizado.
Instalação com defaults, beginSetting/cache e fluxo completo da tela
continuam pendentes. Sem mudança na física, commit ou push.


## Defaults de instalação: tipos 2, 10 e 27

Portados 00440268, 004403C0 e 004408D8 chamados por 00440B50. A chave
é gravada na folha selecionada; o getter copia a linha ao buffer existente
(destinos 0, 0x3a e 0x13a). Linha ausente preserva buffer, como no original.
004452A8 aplica limite inferior antes do superior, inclusive bounds
invertidos. Defaults gravados em setup+0x12f/+0x130, +0x12e e +0x167.

4.096 comparações contra o original, corpo inteiro e buffer de 0x142 bytes,
três folhas para cada categoria e chamadas Adhoc: E para esse contrato.
Linhas são fronteira explícita sorteada; não prova lookup real nem tela
completa. Buffer precisa existir: beginSetting/construção continuam
pendentes, e a ponte não inventa inicialização quando ele falta.

GT4_INSTALL_DEFAULT_NEGATIVE altera um default e falha com exit 1.
Oráculo completo e 22/22 CTest passaram; opengt4.exe Release recompilado.
Outros defaults de instalação e fluxo Setting permanecem pendentes.
Sem alteração de física, commit ou push.

## Used stock state, 2026-10-05

0043B638 get / 0043B690 set match in 8192 differential cases, all 256 ids,
32 stock bits bytes and signed week, with reset on week change (0043B608).
GT4_USED_STOCK_NEGATIVE flips a byte and exits 1. Grade E for this contract.
getElapsedWeek now honors optional signed remainder, from 001331A0
+0x6c..+0x7c; used stock requests period 100. This wrapper change is source
backed, not a new E differential proof. makeUsed string-to-integer conversion
follows 0013E8E8 +0x38..+0x9c. Weekly ADC loading and actual visible shop
counts are C, scoped to week-zero data; full shop rendering remains unproved.

### Lojas: catálogos e defaults adicionais (2026-10-05)

`gtmode_purchase_menu ISO --shop-catalog`: C, 722 listas de preparadores/restrições, 100 estoques semanais com 12.192 entradas, quatro black cars na semana 99 e 2.829 seleções roda/asa no corpo completo. Controles negativos de catálogo, black stock e corpo falham. Compra completa, modelo visível, preços especiais e reabertura do save ainda não integram essa prova.

`gtmode_championship_oracle extracted/core.bin`: E para defaults de instalação 2/3/4/5/10/14/24/27 em 4.096 casos, corpo e buffer completos, folhas A/B/C, linhas ausentes e bounds invertidos. `GT4_INSTALL_DEFAULT_NEGATIVE=1` falhou (exit 1). Funções 004404B8, 00440820, 004407B0, 00440430 e 004406C8 executam no oráculo; SpecDB permanece fronteira explícita. Instalação de câmbio tipo 11, compra completa das lojas e renderer continuam pendentes.

### Black stock acquisition and model motion reader (2026-10-05)

`gtmode_purchase_menu ISO --special-purchase` opens JP/US/EU stock lists at
week 99, locates the four black cars and retains their authored garage objects
across page collection. The real script generates their seed, mileage and
paint. The harness calls acquisition/calendar/sold primitives explicitly,
then authored GetCarRoot/Auto Save: money, four complete 0x4b8 bodies and sold
bits survive slot reload. `GT4_SPECIAL_PURCHASE_NEGATIVE` corrupts a paint
byte and exits 1. Grade A: full BuyCarDialog click path and EE price comparison
are not exercised.

Leaving UsedCarRoot exposed missing Module.clearStatic. `00304820 +0x10`
calls `003063F8`, clearing the own static table. Native module loading now
resides in a host rather than that table, so it survives clearing and the
next stock loads. Sequential shop reopening passes; the unit check verifies
own removal, parent preservation and repeated clearing.
`GT4_CLEAR_STATIC_NEGATIVE` exits 1. Grade A without original VM comparison.

Mot1 decodes model translation, separate axis rotations and Euler channels
(5/6/7/8/19). Original vtable 00686900 +0x30/+0x38/+0x40 selects
0042C890/0042C8B8/0042C8E0; opcode 4 draws/captures the preceding transformed
frame (0042A660), so tracks belong to the following name marker.
`--shop-catalog` checks both entrance motions and both wash motions from
GTAutoRoot.gpb; `GT4_MODEL_CHANNEL_NEGATIVE` exits 1. Grade A for reader;
skeletal stack and entrance rendering were subsequently added (below).

Release opengt4.exe rebuilt. 23/23 CTest, --shop-catalog,
--special-purchase, --ui-regression and --customization-purchase pass.
At this stage customization still reported absent model animation methods and
short_name_no_year; these successful tests did not certify those paths.
No physics edits, commit or push in this continuation.

### GT Auto entrance mechanic (2026-10-05, continuation)

Mot1 now retains ordered push/pop/draw markers and computes accumulated
frames from 0042C818/0042C848/0042A660. Translation/scale multiply the current
matrix; Euler -> quaternion -> matrix follows 0048B888/0048A970, with the
existing native gt4_sincosf as an explicit math boundary. Four original
entrance/wash motions each produce all 20 bone frames at start/end. All
elements are finite, pelvis translation agrees exactly with local channels,
and entrance lower-spine translation matches parent composition within
0.000001. GT4_MODEL_FRAME_NEGATIVE exits 1. Grade A until EE comparison.

The first additional model/track is loaded from the project's GPB and drawn
as a resident skinned mesh (frame times inverse bind). Additional asset IDs
protect the cache from allocator pointer reuse between projects. Model play,
stop at end and loop are implemented; setModelMotionLoop applies to the
option camera as in 00153F28, preventing it from wrapping after the mechanic
stops. --customization-purchase checks stop/restart/loop, with
GT4_CREW_PLAYBACK_NEGATIVE exiting 1.

OPENGT4_PROBE_TEST_CREW=1 showroom_probe ISO ka_01 build/gtauto_crew_probe.png
finds 3450 pixels changed versus the same scene without crew, plus extra
draws. OPENGT4_PROBE_CREW_NEGATIVE removes the motion and exits 1 with zero
changed pixels. Actual application captures at 1/4 seconds:
build/gtauto_mechanic_1.png and build/gtauto_mechanic_4.png. Grade A: no PCSX2
pose/light comparison, own shadows and software equivalent; multiple models
and service sequence/alignment are rejected explicitly, still pending.

Release opengt4.exe/showroom_probe rebuilt. 23/23 CTest and shop catalog /
customization checks pass. No physics source changes, commit or push.

### GT Auto reported presentation defects (2026-10-06)

Removed the invented TextFace width squeeze. Original GTAutoRoot labels use
scale_x 0.95 even with Wash/Oil rectangles only 36/24 pixels wide;
00245408 +0x80/+0x100 submits the authored text style to 00490B98.
The customization harness records the four labels' glyph widths with their
original and enlarged rectangles; they match. GT4_LABEL_SCALE_NEGATIVE
halves a recorded width and fails (exit 1).

LightSet constructor 00210830 +0x9c..0x12c supplies world RGB zero,
direction (0,1,0,0), ambient bits 0x3e999999, diffuse/specular 0.5.
ui_light_defaults now supplies these values before authored overrides;
the old presenter supplied ambient 50/255 and no direct light. Tests check
normalized bits, direction and world; GT4_LIGHT_DEFAULT_NEGATIVE zeros
diffuse and fails (exit 1). Rendering remains A, without a lighting oracle.
Release, customization harness and 23/23 CTest pass. Capture:
build/gtauto_text_light_corrected.png.

The mechanic's wrong location was traced to omitted alignment in 00156AB0:
jtbl_0068FDC0[1] -> 00156D40 applies (f24,0,f22), using contact position
00364C40, tyre width 00364D78 and axle position 00364C60.
This translation is not yet implemented; do not substitute measured screen
offsets or model bounds. User also reports disappearance after waiting.
Ka remains visible behind the car after 30 seconds in
build/gtauto_defects_30.png; this does not disprove the reported S2000 case.
Disappearance, alignment and mechanic reflection remain pending.

Services remain F. Original wash has two models with two motions each,
alignments 7 and 1 (script L11767..11777); oil also has several tracks and
models. GTSE identifies car_wash bank 2/sequence 5, oil_jackup 2/7,
oil_jackdown 2/8 and oil 0/5. Original calls are at L11851 and
L12150/L12154/L12180. sound.c still rejects the pitch automation in oil,
already explicitly observed by test_sound.c. No generic substitute added.

### 06/10: ficha de compra sem valores nil

BuyCarDialog L6388..6409 exige métodos de catálogo e estatísticas atuais. A ponte só ligava getCatPs/Tq e drivetrain; novos/usados/importados agora recebem o contrato inteiro. Dados vêm do disco: ENGINE strings +0/+4/+8, CHASSIS dimensões +8/+10 e massa +12, RACINGMODIFY largura +8. DEFAULT_PARTS mapeia CHASSIS +0x28 e RACINGMODIFY +0x30 (00444480). Os usados chamam getCurrentPs/Tq/Ms, não getCatPs/Tq/Ms: cálculo nativo com folha selecionada e desgaste do corpo +0x498, baseado em 00146648/00364868/0034F3F8, preservando contexto de memória/FPU. A física não foi editada.

`gtmode_purchase_menu ISO --purchase-specs`: 456 chamadas em 38 carros do estoque inicial US e inicialização real de BuyCarDialog; confere widgets Line1/2/3/4/6/9. CR-X: L4:DOHC, 1595cc, 3800x1675x1270, 986kg; campos atuais preenchidos. `GT4_PURCHASE_SPECS_NEGATIVE=1` falha ao injetar nil:nil. Release recompilado e 23/23 CTest passam. Nota A: ainda faltam oráculo diferencial para cálculos atuais, arredondamento/unidades/formatação, catálogo equipado modificado e captura PCSX2. Nenhum valor ajustado visualmente.


### 06/10: garagem da Home ligada ao perfil nativo

Causa: GarageRoot exige garage.pickupCountry/pickupTuner/pickup e métodos estáticos de MCarGarage; o port não os fornecia. Agora o botão HomeRoot.Buttons.Garage executa o script original e consulta os carros salvos. País vem de GENERIC_CAR +0x0e e enum CORE.GT4 006A79C0 (004488A8/00448E50/00448E70), sem depender do catálogo de concessionárias. Tuner usa +0x10/006A7658. Enumeração ordena IDs únicos; seleção percorre vagas ocupadas. OptionMenu.selected_item expõe seu item lógico atual, usado pelos seletores das duas colunas.

MCarGarage é classe nativa com 13 ordenações. Power/mass/history/data vêm dos metadados compactos (00145B20, 00146860, 001468D8, 00146A20, 00146A98); preço/ano/país/tuner são chaves do catálogo. Cor segue 0043DD10/0043DDB0 com HSV; aspiração segue 001467A0/00445650/0013F528. CAR_NAME +8 fornece o nome curto; 0013FBF8 corta no primeiro backtick seguido de dois dígitos. CAR_NAME +12 fornece GetDescriptionID (0012FEA8). getCurrentPower arredonda PS em centésimos; getDate soma o epoch original; getCurrentPWR usa massa/PS em milésimos. popGarageCar/pushRidingCar adaptam o armazenamento PC dos corpos completos que 0016E7B0/0016EAA0 obtinham da memória/cartão PS2. O script mantém a troca, o Auto Save e a venda.

A garagem gráfica oferece a vista vazia com índice -1 quando não há carro selecionado: GarageRoot L12612 sempre consulta getGarageIndex, inclusive após vender o selecionado. Não altera o contrato nil do núcleo sem UI. Ao religar o perfil, o índice de garagem já conhecido tem prioridade sobre slot, que no host do carro é a folha de acerto (00140C70), não o endereço da vaga.

Prova: `gtmode_purchase_menu ISO --home-garage` passa botão original, 3 carros salvos, 30 textos, 13 ordenações em ambas direções, filtros de país/tuner, abertura de CarViewRoot, Change/Auto Save/reload, venda pelo diálogo original com soma de créditos e paginação de 13 carros (3 visíveis na página 2). O presenter de modelos é explícito no harness; não certifica renderização. `GT4_HOME_GARAGE_NEGATIVE=1` altera a contagem conferida e retorna 1. Purchase-specs e customization-purchase também passam.

Nota A. Ordenações usam inserção estável em empates, sem oráculo PS2; normalização de nomes UTF-16, filtros inválidos, modelos/anos desconhecidos e estatísticas compactas após tuning/desgaste ainda exigem comparação diferencial. Sem captura visual deste fluxo. A inicialização da Home ainda registra ausências de dayToWeekDay/MCalendar.getEvent, GetFormatString/getBspecTotalExperience e car_model no harness; o registro é reiniciado antes do botão Garage. Não declarar a Home inteira pronta ou E por este teste.

Build Release completo passa, incluindo build/Release/opengt4.exe. CTest recompilado: 23/23. git diff --check sem erros. Nenhum push ou commit feito.

### 06/10: requisitos comuns e percurso Sunday Cup

Correções genéricas, sem páginas especiais para cada série:

O teste SDL com --start também revelou IsASpec ausente em MQuickWork:
quick-event.ad:311 falhava antes de COMMAND_START e deixava o presenter
esperando. Ligado ao cálculo 00110498 via RACE +0x45, que 00114334..14340
grava em quick +0x490 (getter 005C4610). Não é complemento de IsBSpec:
exige série de licença, l0b/l0a/lib/lia/l0s, conforme jtbl_0068D2F0.
Usa race_record.isLicensePassed já existente no perfil nativo. Oráculo:
8192 combinações de seletor/séries aprovadas contra 00110498, com
004312B0 como fronteira. GT4_ASPEC_ADMISSION_NEGATIVE retorna 1.
Ponte confere 709 provas sem licenças. Regra E; ponte/Start C.

- MRaceData.getNeedCountry segue 001A1C60 (RACE +0x96); inclusive array vazio e ordem DE/BE/SE/FR/IT/GB/NL/ES da Europa.
- MRaceData.getAllowedCarList segue 001A2BB0/00448CD0, chave RACE +8, 60 qwords em ALLOW_ENTRY; -1 e resolução inválida são pulados, não terminam a lista. GENERIC_CAR e ENEMY_CARS vêm dos IDI do disco.
- MCarData.IsTestCar segue 0012F258 -> 004489A0(5), bit 5 de GENERIC_CAR +0x12.
- SpecDB rejeitava qualquer linha acima de 256 bytes. ALLOW_ENTRY possui 480, enquanto o limite de 255 se refere ao stream comprimido, não à saída. Removido apenas esse limite host; capacidade e spans permanecem.
- Transições internas de apply_pages preservam main.game como 0022F6A0. Antes, menu_enter recarregava o perfil de outra VM ao fechar BuyCarDialog; BuyCarRoot L6246..6278 retomava com referência inválida da garagem. GetCarRoot.start_phase1 falhava em setRidingCar apesar da inserção. Entradas entre projetos continuam com refresh.

Prova E isolada: `gtmode_admission_oracle extracted/core.bin ISO` executa o código original recompilado para 256 seletores de país, 709 provas (256 restritas, 2698 labels), incluindo métodos registrados, e 722 carros para IsTestCar. VM/boxing/append e IDI são fronteiras explícitas. Prova adicional de decodificação executa 004490F8 e 004493F8, inclusive Huffman e patches, nas 58 linhas de 480 bytes: zero diferenças e guardas intactas; buffer de 479 bytes rejeitado. Só memcpy substituído nessa parte. GT4_ADMISSION_NEGATIVE, GT4_ADMISSION_BINDING_NEGATIVE e GT4_WIDE_ROW_NEGATIVE retornam 1.

`gtmode_sunday_playable ISO` cria perfil isolado de 10000 créditos, navega UsedCarRoot original, compra lancer_evo_gsr_92 por 9582, confirma aquisição/seleção e Auto Save/reload. Correção do modal: C, valores exatos da transação autoral; GT4_MODAL_REFRESH_NEGATIVE reintroduz refresh no modal e reproduz erro de índice. GT4_SUNDAY_PLAYABLE_NEGATIVE altera débito e retorna 1. Não fornece captura/estado completo da VM PS2.

Em seguida o teste termina cinco corridas físicas reais via gt4_race_run, sem chegada/posição injetadas; usa takeover original 003454A8 modo 5 como controlador, pad ocioso explícito. PrizeRoot recebe resultado vivo e executa makePrizeFast/onPrizeCheck original. Registra, retorna e salva/reabre em cada etapa:

| Prova | Pista | Frames de física | Posição | Créditos |
|---|---|---:|---:|---:|
| am_sunday_0001 | r_mini | 8103 | 3 | 400 |
| am_sunday_0002 | 20r60r | 9536 | 6 | 100 |
| am_sunday_0003 | highspeedring | 22310 | 6 | 100 |
| am_sunday_0004 | r_route5_club | 13936 | 6 | 100 |
| am_sunday_0005 | motegi_west | 13211 | 6 | 100 |

Saldo final 1218. Perfil local: build/career-native/sunday-playable-1791301957-133. Log: build/sunday-playable.log. Nenhuma vitória nesse controlador; não certifica carro-prêmio com resultados de corridas físicas. `gtmode_award_delivery` continua passando entrega/persistência/não duplicação com resultados injetados, prova C separada.

`python tests/ui/run_sunday_entry.py ISO --save-dir PERFIL` copia o perfil para diretório temporário e navega o executável de produção até quick-event/RaceEventRoot, com seis carros montados e nenhum erro dos contratos novos. Verifica que o perfil de origem fica byte a byte intacto. Captura local build/ui-audit/sunday-entry/pregrid.png; --negative exige quick-tt e retorna 1.

Com --start, ativa A-spec, confere COMMAND_START e menu não desenhado,
captura a corrida/HUD em build/ui-audit/sunday-entry/start.png. Nenhum
undefined/erro de script. run_gtmode_entry_smoke.py passa os cinco fluxos
de criação/cancelamento/carga do perfil. A integração utiliza os adaptadores
de carreira ainda locais; suas alterações não foram incorporadas ao commit.

Percurso completo: A. Fronteiras: presenter headless da compra/pagamento, início/retorno chamado pela API e nomes de grid placeholders no pagamento. O teste não passa pelo bridge estático de resultado da main nem verifica desgaste/distância. UI/metadados ainda precisam de comparação PS2 (captura inicial mostra CourseLength 0.00 antes da atualização). Ainda faltam vitória física + prêmio, todas as séries, Setting/serviços/licenças/missões/B-spec. Não foi feito ajuste de valores por aparência. Física não editada. Release e 23/23 CTest passam.


## Licenças: regra de resultado isolada (06/10/2026)

`career_license.c/.h` porta a parte de registros de `001105A8` e os
argumentos de saída de `00110800`. A primeira recebe o estado já produzido
pela sessão: 2/3/4 significam ouro/prata/bronze (grades 9/10/11), demais
estados não escrevem records. O melhor tempo desta visita só muda por
comparação unsigned estrita; empate conserva a grade da visita. Todo sucesso
atualiza o record persistente por `00430A58`, mesmo sem melhorar o tempo.
Cada sucesso também deve entrar no ranking; conteúdo de ranking/ghost não
é portado por essa primitiva. Ao sair, grade 13 usa `0042F198`; outra grade
usa `0042F2E0`, low byte da grade, método 14 e melhor tempo da visita.

O oráculo executa as duas funções originais e `00430A58`. 16640 casos:
256 estados byte e quatro estados signed/large, record e best grade/time
sorteados, tempos zero/UINT32_MAX/iguais/sorteados. Inclui visitas sem
sucesso (grade 13) e sequência bronze -> falha -> prata empatada -> ouro
mais lento, onde record melhora e melhor tempo/grade da visita permanecem.
Fronteiras explicitadas no harness: work concluído, lookup do record,
construção de replay/ghost, inserção do ranking e calendário. Teste compara
o record inteiro, best grade/time, frequência e argumentos das chamadas.
Não é oráculo da classificação do tempo nem da chegada/reprovação física.

Build Release de `gtmode_license_result_oracle` passou. CTest específico:
3/3 (positivo e dois WILL_FAIL). `GT4_LICENSE_RESULT_NEGATIVE=1` corrompe
um bit do record; `GT4_LICENSE_HISTORY_NEGATIVE=1` altera o argumento/saída
de histórico, ambos falham. Nota E somente para os campos e argumentos
isolados provados; exame jogável e persistência integrada continuam F.
Nenhuma estimativa, nenhum unlock/medalha automática no jogo.

Caminho original localizado para continuar:
- gtmode.ad LicenseDetailRoot.onActivate (L10762 no dump) preenche race_code,
  retorna a LicenseDetailRoot e pede next_sequence = license; último exame
  consulta canTryLastLicense antes da saída.
- `0011CB38` constrói RaceLicense por `003BBF90`; vtable `006807A8`.
  `0011D0F8` escolhe carro/paint pelo RACE em vez do carro da garagem.
- `0011D1B8` abre quick-license; `0011D270` abre quick/GuidePage.
- quick-license.ad Start/Demo produzem COMMAND_START/COMMAND_DEMONSTRATION.
- Loop `0011CB38` chama `001105A8` após tentativa e `00110800` ao sair;
  este loop inicializa grade 13 e best time 0x157529ff.
- Ainda falta a sessão/conductor License e suas condições originais de
  largada, checkpoint, chegada, saída de pista/colisão e limite de tempo.
  menu/main ainda não roteiam license, nem carregam quick-license.

Nesta investigação inicial, src/core/physics não foi editado. A antiga
reserva por sessão foi revogada pelo usuário em 06/10/2026.
Não substituir exame por Time Trial.
