# Adhoc scripts

GT4's menus are driven by Adhoc, Polyphony's scripting language. Each UI
project has a compiled script next to its screens
(`/projects/<name>/<name>.adc`). The port runs these scripts natively with
its own interpreter, the way the game engine does, and supplies the engine
objects they call.

| Part | Where |
|------|-------|
| Loader (`ADCH005` → code frames) | `src/core/adhoc/adhoc_code.c` |
| Interpreter, values, modules, GC | `src/core/adhoc/adhoc_vm.c` |
| Engine side: widgets, `main::game`, actors, watchers | `src/core/menu.c` |
| Disassembler (study tool) | `tools/adhoc_dis.py SCRIPT.adc` |

Sources: the GT Modding Hub description of compiled Adhoc and the
instruction layouts of GTAdhocToolchain (MIT). Nothing from either is copied
into the repository beyond the opcode numbering and operand layouts.

## File format (version 5)

`"ADCH005\0"`, then one code frame:

| Field | Type |
|-------|------|
| source file name | symbol (`i16` length + EUC-JP bytes) |
| parameters | `u32` count + symbols |
| stack size, local count | `i32`, `i32` |
| instructions | `u32` count, then each: `u32` line, `u8` opcode, operands |

Operands per opcode are in the `layout` table of `adhoc_code.c`. Function
and method definitions carry a nested frame.

Symbol lists that name a module or variable path hold the parts followed by
the joined name: `["main", "game", "main::game"]`. A single-entry list is a
local variable.

## Execution model

Checked against GT4's own scripts (arcade, gtmode, boot, option, setting,
event, quick run without interpreter errors):

| Instruction | Behaviour |
|-------------|-----------|
| `VARIABLE_PUSH` | local: reference to slot `index`; path: reference to a module member, found through the current module and its parents |
| `EVAL` | replaces a reference with its value |
| `ATTRIBUTE_PUSH name` | pops an object, pushes a reference to `object.name` |
| `ASSIGN_OLD` | pops value and reference, assigns, pushes the value |
| `POP_OLD` | pops into the frame's result register |
| `SET_STATE_OLD 1` | returns; **the return value is the last value popped by `POP_OLD`** (inferred: `return x` compiles to push x, `POP_OLD`, `SET_STATE_OLD 1`) |
| `SET_STATE_OLD 0` | leaves the module or class entered by `MODULE_DEFINE`/`CLASS_DEFINE` |
| `LOGICAL_AND_OLD`, `LOGICAL_OR_OLD` | **do not pop** (inferred from the code: a `POP_OLD` follows on the fall-through path); jump keeping the operand as the result |
| `BINARY_OPERATOR "[]"` | element reference; on an object with a string key, the member |
| `CALL n` | pops n arguments and the function, pushes the result |
| `IMPORT` | at top level copies into the module; inside a function into the call's scope |

Modules are namespaces; module code runs when the script loads. Classes
(`CLASS_DEFINE`) create instances when called, with their `ATTRIBUTE_DEFINE`
members set to nil and `__init__` run. Methods fetched from an object are
bound to it. Arithmetic keeps integers as integers and floats as single
precision.

Not implemented (unused by the scripts tried so far): `TRY_CATCH`, `REQUIRE`,
`OBJECT_SELECTOR`, `MODULE_CONSTRUCTOR`.

## Engine objects

Every widget of every screen is a module, nested by name under the project
module (`ArcadeProject::TopRoot::StartScroll::SelectBox`), so the scripts'
module definitions add their functions to the widget. Widgets answer
`name`, `visible`, `x`, `y`, `w`, `h`, `opacity`, `image_path`,
`focus_index`, `parent`, `first`, `last`, `next`, `prev`, child names, and
the numbers of their screen description; methods `doCopy` and `appendChild`
change the tree.

TextFace also exposes native `text`, `key`, `scale_x`, `scale_y` and
`adjustScale()`. `context.translate(root, key)` / `(root, page, key)` resolve
RT03 text. See [TEXT_FORMAT.md](TEXT_FORMAT.md). Viewer selection previews
now dispatch through the host's focus/activation/page event bridge;
see [MENU_NAVIGATION.md](MENU_NAVIGATION.md).

A screen's `onInitialize(context)` runs when it is first shown.
`MScriptWatcher(context, func)` calls `func(context)` every `interval`
frames once appended.

Missing engine members are reported once per script line and evaluate to
nil, so a screen keeps running. Current stand-ins, to be replaced by the
real systems:

| Object | Now | Later |
|--------|-----|-------|
| `main::game.option` | fresh-game defaults (`special_arcade_tuner = ""`, `wide_mode = 0`, `game_zone = "us"`) | options from the save data |
| `main::sound.play` | native system-bank note effects | other banks, pitch automation, envelopes/reverb |
| `MFadeActor`, `MMoveActor`, `MBlinkActor` | native time-based fade/move/blink | exact easing, rotation/anchor actors |
| `MSystem.GetRegionCode` | `"us"` (the supported disc) | from the disc identification |
| `context`, `pushEvent`, `MScriptEvent`, `MFunctionEvent`, `setFocus` | basic page stack, deferred callbacks, focus/activation, native confirmation | complete input semantics, dialogs styled like the game |

Each script entry is limited to one million interpreted instructions,
including nested calls. An unsupported synchronous wait fails with a
diagnostic instead of freezing the application. The VM restores stack state
after failed calls so subsequent input remains usable. Modules now provide
`defined(name)` for optional-member checks used by the menu scripts.

Menu presentation implementation and known approximations: [MENU_PRESENTATION.md](MENU_PRESENTATION.md).

## Integer/String relational dispatch (09/10/2026)

The original Integer callbacks002FD0B0 (`<`),002FCF80 (`>`),002FD148 (`<=`)
and002FD018 (`>=`) invoke the RHS toInteger slot at vtable+58. String
00314A40 calls005A2F88 ->005A7170: decimal conversion, then truncation to
signed32 bits. Car.getLength0013E370 returns a decimal string; rejecting a
numeric/string comparison silently disabled the compact-car length rule.
The VM now handles this specific Integer-left/String-right dispatch.
`gtmode_integer_string_oracle CORE` compares336 results with the original,
including empty/invalid text, signs/suffixes and32-bit boundaries; negative
inverts a result and fails. Other mixed-type dispatch is outside this proof.
