# Arcade UI review — 2026-09-30

## Scope and evidence

User bug screenshots are retained locally in `captures/ui/20260930-arcade-review/`. Direct-root discovery of all 17 Arcade roots and both quick-arcade roots produced 17 captures out of 19 attempts; report and diagnostic logs are in `build/ui-audit/arcade-review/report.json`. Direct launch does not provide caller state, so its failures are discovery findings rather than proof of broken production navigation. MatchingRoot and MonitorRoot timed out in missing LAN services.

## Corrections

- Rounded-button ink centering now applies to the `label` child, preserving the separate authored `info` rectangle. Manufacturer/era car counts no longer overlap the tab titles. Their locations come from `arcade.mproject`, not replacement layouts.
- MakerRoot's Nation column uses authored clipping. The extra footer cut was removed at the user's request on 2026-10-01 to restore the partial next-country flag preview.
- HistoryRoot's `onLoad` (Adhoc lines 3653–3670) creates era cards with `insertChild(item, Surface)`. The host now inserts and reorders the actual child before the decoration, preserving root ownership and avoiding duplicates when moving existing children.
- Manufacturer and era data are loaded together before either page's first initialization. Each original `onLoad` runs once; the inactive page is not entered or focused. Both counters are therefore available at the first MakerRoot display. The test disc/catalog-preview run shows 719 manufacturer cars and 371 era cars; these are script-calculated values, not hardcoded counts.
- First-load lifecycle now runs authored `onLoadInitialize` before `onLoad` and `onInitialize`. SettingPopup's callback (Adhoc lines 4631–4633) populates transmission, assist and convertible choices that were previously blank. The real Ford Ka route shows AUTOMATIC, TCSOnly and close, with tires still populated separately by the original script.

## Verification

Release build, menu navigation and text tests pass. Child-insertion tests cover ordering and reinserting an already attached card. Car navigation tests cover 14 routes: manufacturer selection, next/previous car, color selection, driving options, returning/reopening, scrolling, era page, selecting a car by era, and the empty favorites dialog. Driving-options checks assert populated choice labels. Nine course routes cover locked categories, next/previous, returning, selecting Normal/Reverse, cancelling direction selection and confirming either direction. Screenshots of Manufacturer, Era, course selection and driving options were inspected visually. Four GT Mode event routes also exercise the shared lifecycle after this change.

## Still requiring verification/correction

- Pixel and animation comparison against original Arcade captures beyond the two reported bugs.
- Driving-options layout/motion and reopened dropdown state throughout repeated routes; initial choices now work, but that is not full visual certification.
- Favorites with actual entries and imported-save garage selection.
- Two-player/pregrid flows with their real caller state.
- LAN Matching/Monitor depends on missing network services.
- quick-arcade race-result/quick-tuning UI with a real prepared race state. Direct-root diagnostics include missing stream-volume services and Grid0 dependencies.
- OptionPopup diagnostics include replay_bgm, language_for_unit and OptionMenu.Count. Resolve original semantics before marking that screen complete.
- 3D car motion, materials/reflections and all original animation timings remain outside this static review's certification.

No game resources, captures or generated recompilation output are committed. Physics and race_view remain outside this task.


### Course list cursor regression (2026-09-30)

The parked Normal/Reverse controls retained focus while outside the viewport.
Their draw-time cursor target replaced the visible course-card target and
clamped the cursor to the bottom edge. An off-screen control now cannot
replace a visible target already selected during that draw. A sole moving
popup target remains eligible outside the viewport, preserving its original
entry/exit chase. Fully transparent controls do not provide cursor targets.
City/Amalfi screenshot confirms the cursor on the card. Menu/text tests,
including popup movement at 30/60/120 Hz, and nine course routes pass.
