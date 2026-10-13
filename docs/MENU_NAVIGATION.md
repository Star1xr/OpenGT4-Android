# Native menu navigation

`menu.c` owns the active page stack and input dispatch. Destinations and
next-project names come from the disc's Adhoc scripts, not label-to-screen
mapping in the app. Cached projects preserve focus while visiting Options.

## Engine bridge

- SelectBox follows ordered children, skipping invisible/insensitive items.
  Other buttons use directional geometry (distance plus alignment penalty),
  a provisional native policy rather than a verified PS2 algorithm.
- `setFocus`, `index`/`focus_index`, `insensitive`, `onFocusEnter`,
  `onFocusLeave`, and `onValueChanged` connect scripts with native widgets.
- Activate/cancel bubbles from the focused widget to its parents. The original
  target remains `event.widget`. Nonzero returns consume an event; the observed
  scripts return 2 for consumed activation/cancel.
- Deferred `pushPage` preserves the underlying page; `startPage` replaces the
  top; `closePage` finalizes/pops the top and restores focus below. Reopening a
  finalized page runs initialization again. Capacity: 32 pages.
- `MFunctionEvent(fn, args...)` and `MScriptEvent(context, target, name, array)`
  queue callbacks and pin their objects through GC. Up to 64 calls run per
  tick; queue capacity is 128. Project changes occur after callbacks return.
- `context.finish()` exposes `game.next_menu_project` only for the `menu`
  sequence. Race/replay/movie sequences fail explicitly; races do not start.
- `ConfirmDialog.open` calls a synchronous platform dialog and returns the
  player's answer to the script. Message/title come from the game, while
  native buttons currently say Yes/No. Original art/custom labels are pending.
- `context.translate` preserves unmatched input for the nested translation
  in OptionRoot.onCancel: `Exit?` becomes `Exit Options?`, then stays unchanged.

## Controls and verified routes

Arrows: focus. In closed OptionMenu fields, Left/Right cycle values and
Enter advances to the next available value; Up/Down move to another field.
Escape cancels through the page script. F10: quit.
F1/F2: debug screen browsing. PageUp/PageDown: debug project browsing.

On US v2.00: Arcade opens GameRoot/SingleRace, directional focus reaches
TimeTrial/SplitBattle/LANBattle, and cancel returns to TopRoot repeatably.
Options opens OptionRoot. Declining its exit confirmation stays there;
accepting returns to TopRoot with Options selected. GT Mode stops at the
unimplemented save-data check; subsequent input still works. No save is faked.

Ordinary buttons get a temporary thin focus border until actors and original
visibility conditions are implemented. It is not the PS2 highlight. SelectBox
shows its selected child alone. Rendering shows the top page only, without
full modal compositing.

## Limits

GT Mode's newer render-context, fade, progress and shared-panel support is
documented in [GT_MODE_UI.md](GT_MODE_UI.md). That page supersedes the older
transition limitations below for the direct GT Mode presentation preview.

Raw onKeyPress/keysym mapping, mouse/controllers, complete shared state between
projects, save data and non-menu sequences remain incomplete. Fades, moves,
blinks and menu audio are covered in [MENU_PRESENTATION.md](MENU_PRESENTATION.md).
Short numeric `sync()` waits present animation frames without re-entering the VM;
other waits are not implemented. `transition` is nil, the cursor proxy accepts
icon changes without drawing. Only common ConfirmDialog is supported; progress
and memory-card dialogs are not. Automatic demo timeout is a native default of
zero. Options are not written to saves/disc. Further screens can still report
missing engine members; navigating to one does not imply its features work.

A one-million-instruction limit interrupts unsupported waits/loops and restores
VM state for subsequent input. It is a diagnostic bound, not coroutine support.
Missing noncritical members keep the existing nil-and-log behavior.

## Validation

CTest `menu_navigation` uses synthetic screens/native test callbacks only.
It checks disabled/hidden selection, focus/value callbacks, bubbling, deferred
pages, repeated initialize/finalize, focus restoration, events surviving GC,
dialog answers, project requests, unsupported sequence rejection, loop
interruption and recovery. No disc or BIOS is needed.

Optional integration test:

```
python tests/ui/run_menu_smoke.py <your-disc.iso>
```

Seven scenarios assert actual project/root/focus and save local captures/logs
under ignored `build/menu_smoke/`. `--input` uses the same input entry point as
SDL keys, not OS key simulation. `--dialog-answer yes|no` is an explicit test
override for that run; normal confirmation is interactive.

## Native option fields

OptionMenu now participates as a single focus target instead of exposing its
popup children. Logical indices exclude decorative faces. Left/Right wrap
through available values, skipping hidden/disabled entries, then dispatch
onValueChanged(context,index) and the parameter sound. Enter opens a dropdown over the page. Arrows select a pending value; Enter
commits it once, while Escape closes without changing the value. Script setIndex and index
assignments clamp to the value range without dispatching a user-input event.
Drawing and navigation use the same value lookup.

RaceRoot's original apply/reflect scripts now work with these controls. OK
updates the option module and setNeedSave marks unsaved state. Cancel returns
without applying edited widget values. Settings survive reopening the page in
the same cached project, but are not persisted to disk or yet shared with every
project. This does not implement game saves or race simulation.

`python tests/ui/run_option_smoke.py <disc.iso>` checks four changed fields,
OK/reopen, and Cancel/reopen using semantic indices in screenshot diagnostics.
Synthetic tests cover disabled/hidden choices, callbacks, index clamping and
focus recovery. Captures/logs stay in ignored build/option_smoke/.

## Course catalog preview

Arcade Single Race now reaches the disc-backed course selection in explicit
`--preview-courses` mode, including direction and confirmation to MakerRoot.
See [COURSE_CATALOG.md](COURSE_CATALOG.md) for controls, implementation, tests
and remaining car/save/presentation work.
