# GT4 UI projects

Parser: `src/core/mproject.c`. Renderer: `src/core/ui.c`. Program:
`opengt4 <disc.iso> [project] [screen] [--shot out.png]`.

## Files

Each menu area is a folder under `/projects/<name>/` in `GT4.VOL`:

| File | Content |
|------|---------|
| `<name>.mproject` | **plain text**: every screen (`RootWindow`) as a widget tree |
| `US/<Screen>.gpb` | the screen's images: `gpb2` package of `Tex1` files named like `image/arcade/top/gt.png` |
| `<name>.adc` | compiled Adhoc scripts: behaviour, menu items, transitions (docs/ADHOC.md) |
| `US/rtext.rt2` | RT03 translated texts, looked up by page and key |

Projects seen: `boot`, `arcade`, `gtmode`, `option`, `setting`, `event`,
`quick-*`, `network`, `online`, `labomode`, `photo_*`, ...

`gpb2`: magic, pair count at `0x0c`, 16-byte pairs from `0x10`:
`{name offset, data offset, size, 0}`. An asset may be stored compressed
like a GT4.VOL file (magic `C5 EE F7 FF`, negated size, raw deflate):
MakerRoot.gpb's NL/ES/BE/AU flags on the US disc. `gpb_load` inflates it.

## .mproject grammar

```
Type { ... }              object: a widget, or a value such as RGBA {r g b a}
key type { values }       property: geometry rectangle {x y w h}, name string {"..."}
key [n] { ... }           list: children [3] { ... }, color [4] { RGBA ... }
```

Widget types in `boot` and `arcade`: `RootWindow`, `Composite`, `ColorFace`,
`ImageFace`, `FrameImageFace`, `TextFace`, `VBox`, `ScrollBox`, `SelectBox`,
`OptionMenu`, `Button`, `ListBox`, `SliderBar`, `ToolTipFace`, `MovieFace`,
`FlashFace`, `CarFace`, `ColorChipFace`, `InputTextFace`, and more.

## What the renderer does, and what is assumed

UI space: 640×480 (several widgets use `0 0 640 480`), scaled to the window.

| Rule | Status |
|------|--------|
| `geometry` is relative to the parent | fits every screen drawn so far |
| no `geometry` → the parent's area | assumption |
| `visible {0}` hides the widget and its children | assumption |
| `opacity` multiplies alpha down the tree | assumption |
| `ColorFace`: one colour fills; four colours are corners TL, TR, BL, BR | assumption (TopRoot's fade looks right) |
| `ImageFace` with `geometry` fills it; without, natural size; `scale` multiplies about the image's centre | centre: matches the PCSX2 TopRoot logo (0.9 in a 400x240 frame); fill/natural size still assumption |
| `FrameImageFace`: corners at natural size, top/bottom edges stretched | assumption (buttons look right) |
| images searched in the screen's package, then the project's other packages | works for boot and arcade |
| `clip {1}` restricts the children to the widget's area | assumption |
| vertical `SelectBox` (`direction 1`, no `spacing` key): items stacked at the first item's height + spacing, x = `border_width`, scrolled to centre the focus (ends clamped at 0.3 item), focus ± box/pitch items drawn, focus last | CORE 0x002D6598 (offset), 0x002D69C0 (draw), 0x002D6B40 (layout); easing rate is ours (18/s) |
| other `SelectBox` without `spacing` shows only its focused item, centred | assumption |
| SelectBox moves stop at the first/last item (no wrap) | CORE 0x002D7468 / 0x002D7528 |
| a widget's fade/blink actor (`widget.actor = ...`) advances only while the widget is drawn; it holds while hidden by `visible` or its state faces | inferred: actors belong to the widget |
| a ScrollBox's `inc_arrow` face fade is the complement of the same face's fade on `dec_arrow` (one fully shown while the other is gone) | observed in PCSX2 (TopRoot); the mechanism is not traced |
| `Button` faces choose by state: 4 = focused, 8 = insensitive (GameRoot: static `bg` frame unless focused, blinking `bgf` when focused, icon `bg0` hidden when disabled) | state nibble +0x98, setters 0x00266060..0x002661D8; bit meanings from the authored faces |
| `ScrollBox` children `inc_arrow`/`dec_arrow` get state 1 while its list can still move down/up; their faces pick by `visible_compare`/`visible_condition` (TopRoot: static `bg` at an end, blinking `bga` otherwise, via the script's repeating MFadeActor, 0.5 s) | CORE 0x002CFBE0 |

Scripts run on the loaded widget tree (`src/core/menu.c`): they choose
which logo is visible and copy the menu items into their lists. Not drawn
yet: `MovieFace`, `FlashFace`, `CarFace`, tool tips,
animations and original focus effects. Basic native focus is implemented;
see [MENU_NAVIGATION.md](MENU_NAVIGATION.md). Checking rendering rules against PCSX2 screenshots is
pending.

TextFace now uses the disc's FT01 fonts and RT03 text table. Details,
provisional layout rules and remaining limits: [TEXT_FORMAT.md](TEXT_FORMAT.md).
