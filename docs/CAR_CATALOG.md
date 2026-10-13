# Native car catalog preview

After selecting a road circuit in Arcade Single Race, the original scripts now
populate MakerRoot and CarRoot from the user's US v2.00 disc. Arrows navigate
manufacturers and the car strip, Escape returns. Car names, miniature images,
manufacturer logos and the current-car code update together. The viewport
scrolls to keep the focused manufacturer visible. The strip shows adjacent
cars, highlights the selected one and displays position/total.

Run with the existing `--preview-courses` flag, now labelled Catalog preview
in the window title. This previews the catalog; it does not simulate save
unlocks, favourites, a garage or race eligibility. Enter in the desktop frontend toggles colour browsing; Left/Right change
the original paint variant, Enter/Escape returns to car browsing. C/V are
shortcuts. Race confirmation remains unavailable. See MODELS.md for the
native 3D preview and fidelity limits.

## Implementation

- The project's `load` reads `/projects/arcade/US_carlist.adc`. `ah_run_module`
  executes it in the calling project, preserving the enclosing interpreter
  stack and instruction budget. The original `CarData` arrays drive ordering
  and grouping; no catalog strings or car arrays are embedded in source.
- `isCarAvailable` in preview checks the real GENERIC_CAR label table.
  GENERIC_CAR.dbt supplies model year (offset 12) and OpenModel (offset 20)
  in the supported 24-byte row. The generic table mapping was cross-checked
  with [PDTools](https://github.com/Nenkai/PDTools/blob/master/PDTools.SpecDB/Core/Mapping/Tables/GenericCar.cs).
- CAR_NAME_american.dbt's first string index resolves through american_StrDB.sdb
  (GTST). The bounded string reader validates the offset table, length and
  terminator. Names were checked against the local disc rows.
- Tuner/country labels come from membership in the authored MakerData and
  CarData lists. `getCarLabel` retains the selected disc label.
- `loadGpb` adds per-manufacturer image packages to the UI cache. An image
  missing from packages can resolve the corresponding `/piece/*.img` asset.
  Packages remain cached until project shutdown; unloadGpb needs no extra
  action in this ownership model.
- ListBox supports template copies, item count, item lookup, indexed arrow
  navigation and onValueChanged. Copies retain script bindings. Runtime name
  assignments now change the widget lookup name. adjustSize reads native image
  dimensions; resizing a one-image composite sizes that image with it.

## Limits and validation

The native showroom now renders the original body, wheel/tire meshes and
a provisional studio background, with Pat0 colour patches. Lighting, camera and wheel placement
are provisional; see MODELS.md. Drivetrain/power/torque, category, favourite
persistence and race eligibility remain unfinished. Unknown specification
fields show `--`, never invented numbers. Preview favourites are empty until
save support exists.
Manufacturer count labels, tooltips and some animation/scroll details remain
incomplete. By Era is not included in the validated route yet.

`python tests/ui/run_car_smoke.py <disc.iso>` checks Ford, Toyota and BMW,
next/previous cars, returning/reopening, horizontal manufacturer scrolling and
the unavailable confirmation boundary. Screenshots/logs remain in build/.
Synthetic tests cover scoped script loading, template isolation, ListBox
callbacks/wraparound, runtime widget names and invalid GTST strings. Existing
menu and course smoke tests cover regressions in earlier navigation.

Next: match the original material/reflection passes and wheel transforms,
complete specification joins, then continue to race setup.


## Showroom header correction (2026-09-27)

GetCountry now returns the authored `nation_list2` flag code (US/JP/DE/etc.),
not the long catalog name (usa/japan/german). GENERIC_CAR category byte 17 modulo
100 maps to the original RT2 keys NORMAL/RACING/TUNING/CONCEPT; this mapping was
cross-checked with the disc's road/race/tuner/concept entries. Unknown values
remain explicit. The logo and tuner logo follow the original crossfade script;
repeat fades honour the endpoint hold (`over`). Non-standalone GPB piece assets
that fail Tex1 decoding fall back to their complete /piece/*.img disc file.
Verified visually with the Ka logo, US flag and NORMAL label.
