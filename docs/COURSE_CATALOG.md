# Native course catalog preview

The original Arcade scripts now navigate TopRoot -> GameRoot -> CategoryRoot
-> CourseRoot. Left/Right selects a course, updating the original map, logo,
thumbnail and name. Enter selects Normal/Reverse when the script offers it;
Escape returns to the course list or category. Confirming reaches MakerRoot,
and the native car catalog can be browsed; this does not start a race.

Run the native executable with the user's own US v2.00 disc:

```
build/Release/opengt4.exe "path/to/disc.iso" arcade TopRoot --preview-courses
```

`--preview-courses` explicitly exposes the disc catalog without a save. The
window title identifies preview mode. No unlock state is invented or written;
without this flag, availability remains false until save loading exists.
Start from Arcade/TopRoot so original script state is initialized in order.

`MCourseData.GetMaxCarNum` reads COURSE.dbt/COURSE.idi in
`/specdb/GT4_US2560/`, resolves the script's course code, decompresses its row
and reads EntryMax at byte 17 of the 22-byte row. Missing or malformed data
returns zero capacity. The script applies its own category, Arcade and player
count filters. The bridge is specific to US2560; other disc schemas are not
claimed to work.

`src/core/specdb.c` is an original bounded GTDB/GTID reader supporting raw rows,
LSB-first Huffman data, template rows and bitmask patches. Layout references:
[PDTools database reader](https://github.com/Nenkai/PDTools/blob/master/PDTools.SpecDB/Core/Formats/DBT_DatabaseTable.cs),
[COURSE mapping](https://github.com/Nenkai/PDTools/blob/master/PDTools.SpecDB/Core/Mapping/Tables/Course.cs).
No game table or script bytes are included in the repository.

The widget bridge implements callFocus/outFocus/getFocusedWidget, preserving
the selected child of a SelectBox. Directional navigation stays inside the
current carousel. Adhoc strings support bounded `%d`, `%i`, `%s`, `%%` and
integer width/zero padding. Other format conversions fail explicitly.
MUnit uses the script's metre values and provisional metric formatting (km/m),
not saved unit preferences.

## Validation and remaining work

- `specdb_tests`: synthetic raw, Huffman fast/long codes, template/patch rows,
  label lookup and truncated/invalid input. No disc required.
- `menu_tests`: formatting, focus transfer, carousel isolation and existing
  navigation/option callbacks.
- `python tests/ui/run_course_smoke.py <disc.iso>`: nine real-disc routes,
  selected code checks, reverse confirmation and screenshots. Includes a
  negative check that normal mode does not unlock the catalog.
- Existing menu/options smoke checks and CTest remain passing.

The native manufacturer/car catalog now works for browsing; see
[CAR_CATALOG.md](CAR_CATALOG.md). Remaining work includes save records,
tooltips, full scroll geometry, some actors and native car 3D rendering.
Movie backgrounds now play (see MOVIES.md). Default score text is not a loaded
player record. Confirming a car does not start a race.

Keep the accepted audio settings: 44100 Hz source samples -> 48000 Hz mixer,
tempo 1.0 and prepared PCM. No audio change was needed for this milestone.

SelectBoxes with authored spacing now render a clipped horizontal card strip,
with the selected card centred and highlighted, dimmed neighbours, and a native
position/total badge. This replaces the earlier single-card approximation;
exact original scroll animation remains pending.
