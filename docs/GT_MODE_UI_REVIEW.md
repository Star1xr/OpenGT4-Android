# RevisÃ£o da UI do GT Mode â€” retomada em 2026-09-30

Pedido do usuÃ¡rio: interromper por enquanto a validaÃ§Ã£o da carreira e revisar
os elementos da UI em vÃ¡rios locais, incluindo telas que ele nÃ£o apontou.
Usar a apresentaÃ§Ã£o e o comportamento do GT4 original como referÃªncia, dando
preferÃªncia aos scripts, recursos e funÃ§Ãµes originais. As imagens mostram
aparÃªncia; nÃ£o determinam duraÃ§Ã£o nem curva de animaÃ§Ã£o.

## Ponto de retomada da carreira

A compra padrÃ£o jÃ¡ tem comparaÃ§Ã£o da aÃ§Ã£o original e do save/carregamento.
A transiÃ§Ã£o dos slots PCSX2 7 para 8 foi validada nos 768 registros de corrida.
Ambos jÃ¡ tÃªm Cr. 5.186: nÃ£o validam o crÃ©dito do prÃªmio em dinheiro.
A carreira inteira NÃƒO estÃ¡ concluÃ­da. Retomar a lista e os comandos em
[GT_MODE_LOGIC_VALIDATION.md](GT_MODE_LOGIC_VALIDATION.md), especialmente:

- Estado anterior Ã  chegada para comparar o crÃ©dito do prÃªmio.
- Entrega completa dos carros de prÃªmio, disponibilidade e dependÃªncias de
  `sound.startStreamEx` / `option.menu_bgm_volume`.
- ComposiÃ§Ã£o da progressÃ£o dos campeonatos, A-Spec, requisitos e adversÃ¡rios.

NÃ£o substituir a validaÃ§Ã£o byte a byte por uma comparaÃ§Ã£o apenas visual.
Manter as capturas existentes; nÃ£o pedir novamente os slots 7 e 8.

## ReferÃªncias preservadas

As quatro imagens enviadas foram copiadas do diretÃ³rio temporÃ¡rio para
`captures/ui/20260930-gtmode-review/`, ignorado pelo Git. `source.json`
registra os caminhos de origem e SHA256. NÃ£o incluir imagens do jogo,
savestates, ELF ou tabelas extraÃ­das em commits.

| Arquivo local | Tela / elementos a comparar |
|---|---|
| `01-beginner-events.jpg` | Lista Beginner Events: banners, tÃ­tulos, painel lateral A/B, trofÃ©us e cÃ­rculos, espaÃ§amento e recorte, descriÃ§Ã£o inferior |
| `02-sunday-cup-regulations.png` | Popup Sunday Cup: cabeÃ§alho, transparÃªncia, linhas e colunas, barra de rolagem, tabela de prÃªmios e botÃµes Cancel/OK |
| `03-common-header.jpg` | CabeÃ§alho comum: carro, Ã­cone Cr., dinheiro com separador, dia amarelo antes do mÃªs claro, alinhamento e sombra |
| `04-sunday-cup-course-selection.png` | SeleÃ§Ã£o de pistas: faixa descritiva, miniaturas, banner, carro 3D, voltas, logos, foco, trofÃ©u e descriÃ§Ã£o inferior |

## Comportamentos pedidos explicitamente

- **Texto longo inferior:** ao acabar a rolagem, hÃ¡ um delay antes de ele
  reaparecer com uma animaÃ§Ã£o de baixo para cima. NÃ£o reiniciar imediatamente
  nem reaparecer de uma vez. Conferir tambÃ©m velocidade de rolagem, pausa
  inicial, recorte e reset quando muda o foco; tempos ainda a medir no original.
- **Cursor dos popups:** a entrada e a saÃ­da do popup de confirmaÃ§Ã£o tÃªm
  movimento suave do cursor. Sincronizar com o painel e seu foco, sem salto
  instantÃ¢neo. O pedido anterior tambÃ©m descreve o painel entrando de baixo
  para cima, saindo para cima ao confirmar OK e para baixo ao cancelar.
  Conferir trajetÃ³ria, duraÃ§Ã£o e easing no original antes de dar como fiel.
- **RevisÃ£o geral:** conferir outros elementos e locais mesmo que nÃ£o tenham
  sido citados; nÃ£o limitar a correÃ§Ã£o Ã s quatro imagens.

## Roteiro da revisÃ£o

1. Inventariar as telas e widgets compartilhados. Para cada diferenÃ§a, anotar
   tela, recurso/script/funÃ§Ã£o original, reproduÃ§Ã£o e estado da correÃ§Ã£o.
2. Comparar primeiro o cabeÃ§alho comum, descriÃ§Ã£o longa e ciclo de popup/cursor;
   correÃ§Ãµes compartilhadas precisam ser verificadas nas telas consumidoras.
3. Comparar lista de eventos, regulamentos e seleÃ§Ã£o de pistas com estas
   referÃªncias: geometria, fontes, transparÃªncia, foco, navegaÃ§Ã£o, rolagem,
   transiÃ§Ãµes e SFX. Valores de prÃªmios mostrados nÃ£o substituem a validaÃ§Ã£o
   das regras de carreira.
4. Ampliar a comparaÃ§Ã£o para mapa/paÃ­ses/fabricantes, concessionÃ¡rias, garagem,
   licenÃ§as, Driving Missions, GT Auto/tuning, campeonatos, saves e opÃ§Ãµes.
   Conferir componentes ausentes e nÃ£o funcionais, alÃ©m da aparÃªncia.
5. Verificar 4:3 e 16:9 sem esticar os elementos, recortes e alinhamentos,
   descriÃ§Ãµes longas e troca de foco por teclado/controle/mouse.
6. Registrar evidÃªncia antes/depois e o que permanece provisÃ³rio. Imagens
   estÃ¡ticas nÃ£o certificam animaÃ§Ãµes; medir no script/cÃ³digo ou em execuÃ§Ã£o.

PendÃªncias anteriores a reconferir: retorno dos Ã­cones pelo caminho angular
mais curto, escala do Ã­cone de paÃ­s referido pelo usuÃ¡rio, eixo/sombra dos
Ã­cones inferiores, apresentaÃ§Ãµes de fabricantes, centralizaÃ§Ã£o dos botÃµes,
controles de Picture Quality, translucidez dos dropdowns, nitidez das fontes,
SFX com reverb e saÃ­da das opÃ§Ãµes sem tela preta.
NÃ£o assumir que todos ainda estÃ£o quebrados: verificar o estado atual.

## Implemented in this audit (2026-09-30)

This is no longer only a task note. The disc inventory contains 34 project
folders; 33 descriptions provide **213 RootWindows**. `language` does not
provide the expected description. Initial direct-root discovery covered 72
roots in GT Mode, Options and Arcade, producing 69 local screenshots. Another
34 direct roots in boot, setting, event, quick and quick-arcade produced 34
captures (one route returned a nonzero status). Total: 106 direct-root launches,
103 local captures. Direct
opening omits caller-specific arguments: inventory is not navigation coverage.

| Shared component | Original reference and native change | Evidence / limit |
|---|---|---|
| Long tooltip | `00248DD0`, `0024A198`, `0024A250`, `0024A2A8`, `0024A2F8`, `0057B088/0057B0F8`: separate targets, waits, scrolling and vertical reentry in ui_motion.c | **103,680 byte-identical original updates**; 120 Hz adapter matches 60 Hz ticks. Real font initialization/state 0, other locales and nonanimated mode are not certified |
| Popup/cursor | Share cursor state between projects; base `0022FFD0`; cursor.adc panel entry from Y=480, OK upwards, Cancel downwards | Real confirmation/cancellation routes pass; full actor/pixel timing not certified |
| Focus | Restore focused flag when a prefocused page is shown or a parent revealed; Button state includes bits 2 and 4 | Navigation tests, visible gold OK and cursor |
| Regulation layout | MBox/FBox requests/allocation from `002B82E8`, `002B85B0`, `002B8778`; ignore decorative faces in content requests | Four rule rows and six prizes visible; 120-frame stable layout test. Full constraints remain partial |
| SelectBox | Vertical/horizontal offsets from `002D6598`; default horizontal layout also works without explicit spacing | Course logos separated; selection/return pass. Easing, magnification and thumbs remain partial |
| Modal loops | `0024EB40`, `00251CE8`, `00251E80`: nested UpdateContext enter/leave, platform event pump and per-frame instruction budget | Two nested loops tested; four actual event routes pass |
| Resources / script text | countChild `002058E0/00206880`, getChild, setImagePath, existGpbBinary, String.build `00313900/00315B78`, course keys from original RACE/COURSE.idi | Reads runtime disc resources; malformed String.build and all getters not oracle-certified |
| Header | Money formatting `001B1050/0042E6B8`, region `0042E440` | Car name, grouped credits, day before month visible. Full formatting not oracle-certified |
| Text/filter | UI filter and extra sharpness default to zero; retain literal script text separately from wrapping cache; RT03 control characters do not become question-mark glyphs | Original am_sunday text has a leading tab. Text/Options tests pass; typography not pixel-certified |
| Dropdown | Authored popup/background/shadow/highlight/dot, selected-row alignment from `002C9098` (`002C9254`); preserve decorative faces when replacing PC choices | Screenshot shows translucency and current-choice dot; hover/commit state test and Options routes. Large-list clipping remains an adapter |
| CarFace | Garage selection adapter; syncLoading `00138F50/0013A470` waits for the PC loader; car_model getter `00138BA8` permits authored ry=120 and warp | Course initialization reaches pose/visibility/cursor statements. Explicit pose uses authored camera/rectangle, advanced yaw+180 and negated z from `00157378`. Presentation adapter, **not complete original model port/oracle** |

Validation used build/career-native/Release/opengt4.exe and the US v2.00 disc:
menu_navigation, ui_text, tooltip oracle; 13 GT Mode routes; four event routes;
video/Options suite (commit, cancel, reload, sliders, repeated exit and opening
return after inactivity). Flow tests do not certify appearance. Test profiles
are isolated copies under build/: authored Auto Save may update that copy,
while the input profile is checked unchanged.

Local evidence (ignored by Git):
- build/ui-audit/report.json: initial 72-root discovery.
- build/ui-audit/all-projects/inventory.json: 213 authored roots.
- build/ui-audit/auxiliary/report.json: 34 additional direct roots in boot, setting, event, quick and quick-arcade (caller parameters still not supplied).
- build/ui-audit/dropdown-authored.png: authored translucent resolution popup.
- build/ui-audit/event-routes/: regulations and course navigation.
- build/ui-audit/routes/: intermediate captures before the CarModel adapter.

## Remaining work before whole-UI equivalence

1. Exercise all 213 roots through their actual callers with appropriate state
   and parameters, in 4:3 and 16:9. Photo, network, results, championships,
   maintenance/tuning and auxiliary screens are not certified by this audit.
2. Complete missing widget families found in the disc inventory: InputTextFace,
   InputNumberFace, SceneViewFace, TextBoxFace, PhotoRenderFace, GraphFace,
   SlideShowFace, EyetoyFace, ColorWindow, LoggerFace, RaceCourseMapFace,
   VirtualFace, PhotoMapWindow and PhotoViewFace. Several need more than a
   renderer alias (input, scene, graph and photo controllers).
   Complete rounded scrollbar rendering/indicators, dividers/full packing,
   font metrics/shadows/alignment, model projection/scales, list clipping,
   magnification and exact animation curves.
3. Complete CarModel: all three rotation axes, motion/interpolation, lighting,
   materials, per-page environment and original reflection/composition.
   The current explicit pose fixes initialization, not final 3D fidelity.
4. Implement disc/original-backed getTunerList, MCarData.IsTestCar,
   MRaceData.getNeedCountry and getAllowedCarList still missing on real routes.
   Do not hide diagnostics with fabricated empty lists or copied tables.
   Missing answers also affect regulation content.
5. Compare full popup/cursor timing and SFX/reverb with original execution;
   reconfirm map icon axes/shadows/shortest return, notices, dropdowns,
   manufacturer slides and exit behavior across callers.
6. Record reproducible per-screen comparison for focus, cursor, long text,
   animations and keyboard/controller/mouse. Static screenshots cannot prove
   timing or identical behavior.

**The whole UI is not yet identical to original GT4.** Next work includes
CarModel/composition and remaining shared widgets, then actual caller routes
across all projects. Do not touch physics, race_view, physics tests/docs.
Preserve LF and other sessions' changes. Career validation remains deferred
and recorded separately in GT_MODE_LOGIC_VALIDATION.md.

### Header paint and course-description clipping (2026-09-30)

`refreshCar` in the original Adhoc calls `ColorChipFace.setData(garage.riding_car)` with one argument. The host now accepts that form, resolves `car_code`/`code`, and reads the owned car's `color_index`; explicit showroom paint indices still work. Paint RGB and enabled color bands come from the disc's VARIATION rows. No paint values are embedded in code.

The single-line `EventCourseRoot::FG1::info` description now centers its visible glyph bounds vertically inside the authored rectangle, preserving horizontal alignment and the script's width adjustment. This avoids clipping its descent against the short description band. This is a presentation adjustment verified visually, not an original-function oracle certification.

Validation: Release build, `menu_navigation` and `ui_text` passed; all four `run_event_ui_smoke.py` routes passed. Visually inspected `build/ui-audit/event-routes/courses_confirm.png`: the paint swatch matches the loaded BEAT's teal paint, and the description has clear margins above and below. Test profiles are copied into build; source saves remain unchanged.

### Cursor retention during transitions (2026-09-30)

The render-context cursor now remains drawn when focus disappears during a timed fade, retaining its current position and angle until a destination supplies a target. It remains under the existing scene fade, including the fully black interval; the new focus resumes the original chase. Project changes now transfer the same cursor state already shared with common dialogs. Pages without focus outside transitions do not gain a cursor.

Validation: synthetic textured-cursor rendering checks focus loss, partial fade, full black, project transfer without a position reset, subsequent movement toward a distinct destination, and no cursor leakage after clearing the transition. Release build and menu/text tests pass. Four authored event navigation routes pass. Timing and opacity have not been certified frame by frame against a recorded PS2 transition.

### Additional original references: Replay Theater and Photo Travel

User reference images are retained locally in `captures/ui/20260930-gtmode-review/05-replay-theater.png` and `06-photo-travel.png`, with source hashes in `replay-photo-sources.json`. These are comparison targets, not validation evidence for the current renderer.

Replay Theater: left sidebar with Demo/Replay/Playlist, selected Replay gold background and chevrons, helicopter background under a translucent gray layer, centered empty-state message, upper memory-card free-space status, disabled circular action icons at right, toolbox and green exit buttons at lower right. PC storage text can differ, consistent with the project's native save format.

Photo Travel: split light/dark background, title and description strip, common car/paint/money/date header, vertically scrolling destination cards (selected card enlarged), destination preview at right with country name, flag and map outline, airplane background and bottom scrolling description. Inspect navigation, clipping, card magnification and preview transitions against authored resources and scripts.

Pending: compare these routes in the current build; static references alone do not certify animation timing or interactive behavior.

### Additional original references: circuit categories and activities

Five user screenshots are retained locally as `07-world-circuits.png` through `11-original-courses.png` in `captures/ui/20260930-gtmode-review`, with source hashes in `circuit-sources.json`. These cover World Circuits (Tsukuba selection and activity submenu), Power and Speed (Test Course), City Courses (Seoul Central), and Original Courses (High-Speed Ring).

Compare the category-specific full-screen backgrounds; title and common car/paint/money/date header; dark description band; course photograph with name and translucent large course outline; foreground car and reflection; translucent lower horizontal selection strip with authentic course logos; and bottom course length/elevation text. City and Original categories show Normal with a downward triangle beneath the selected logo. World Circuits replaces the logo strip with Practice, Track Meet, Photo Drive and Family Cup icons after selecting a circuit. The screenshots show Cr. 5, Cr. 5 and Cr. 3 beneath the first three activities: these are reference observations, not certified rules or hardcoded prices. Read costs and availability from original scripts/runtime data.

Cars appear at different angles and translucencies in these static captures. Do not treat a single captured angle or opacity as the final pose: inspect the original motion and transition code. Verify selection scrolling, cursor positions, Normal/Reverse controls, preview changes and activity entry/return in actual routes. These screens remain pending visual and behavioral comparison; recording references does not mark them complete.

### Native first-run presentation and PC storage wording

Normal startup without an active native profile now opens a welcome page in the save UI. Four icon cards reuse Options framing/background and Home icons: New Player, Import Savegame, Player Profiles and Play as Guest. New Player selects an empty slot and opens the original boot name keyboard. Completion creates the native profile and proceeds to MovieRoot when opening playback is enabled, otherwise to the main menu. Guest follows that presentation without creating a save. Cancelling name entry returns to welcome; selecting existing profiles opens the normal slot page. Existing active profiles skip onboarding on normal startup. `--first-run` forces this route for local inspection/testing.

Removed the memory-card artwork from native profile cards and hid console Memorycard status composites. Common storage translation keys now describe player profiles and PC disk space rather than PS2 slots. This covers the mapped acquisition/retry/create prompts; all other memory-card-specific screens and localized strings still require an inventory, especially replay/photo storage status. PS2 save import remains supported as a file import.

Verification: inspected welcome and original keyboard screenshots; six disposable-profile smoke routes cover welcome, cancellation, guest, existing-profile list, creation via original keyboard, and returning-player startup. Native profile creation reaches the opening movie; normal Auto Save can update options on exit. Shared menu/text tests and four GT Mode event flows pass. Interactive import picker and animation equivalence to original boot are not certified by these tests.

### Cursor motion audit (2026-09-30)

The former elapsed-time exponential approximation moved X/Y and tilt simultaneously. The original update block in `func_0022FFD0` (`002303A0..002304C8`) first tests both positional differences against one pixel, moves X/Y using `0057B088`, and only updates tilt when both pre-update differences are below one pixel. The native `gt4_cursor_step` in `src/core/ui_motion.c` now preserves that gate, strict thresholds, float constant and operation order. `ui_animate` accumulates the original 60 Hz updates, and project/dialog transfers preserve the fractional update clock as well as position and angle.

Validation: `ui_cursor_oracle extracted/core.bin` runs the original update block through the isolated EE test runtime. 490,050 updates across five speed ratios, eleven X/Y differences (including both sides of the one-pixel boundary), nine initial tilts and 90 consecutive ticks match X/Y/angle float bytes. Generated oracle code remains in ignored `build/`; it is not linked into `opengt4`. Menu rendering tests also verify identical displayed position and tilt after one second at 30/60/120 Hz, delayed tilt, retained cursor through partial/full-black fades and cross-project return. Release build, menu/text tests and four authored event popup/course routes pass. The regulations screenshot was inspected as a route integration check, not an original pixel/timing comparison.

Still unvalidated: full recorded PS2 popup entry/exit and moving-focus trajectories, sprite opacity/placement throughout those transitions, custom CursorRoot.setIcon behavior, and per-screen targets under every animated layout. The motion update equivalence above does not certify every cursor animation in every screen.


### Popup motion and colour fade follow-up (2026-09-30)

Ported original move/fade scalar updates and cursor.adc opening/closing
sequence. Common dialog preserves unsnapped panel movement, context speed,
original delays, result-dependent exit direction and Type 3 initial No focus
(types 0/2 use OK, 1/4 use Yes). General move/fade actors now use the same
original-frame helpers. The cursor uses current ancestor translations after
actor updates; a rendering test checks a translated popup without a
preceding draw. Colour period is .25 s by default and applies to each leg;
project entry no longer overrides authored panIn with a .2 s fade.

Actor oracles: 38,400 original move updates and 38,400 original fade updates
match float bytes; cursor oracle still matches all 490,050 updates. These
are isolated original EE test blocks, not part of the shipping executable.
MCrossTransition and MBlockTransition now use separate native composition
paths. See MENU_PRESENTATION.md for original addresses and oracle case counts.
Recorded PS2 full-frame/opacity equivalence remains unvalidated; the documented
rendering and instance-state gaps prevent claiming complete visual equality.

Final checks for this follow-up: Release build, menu/text tests, moving-popup
30/60/120 Hz float-byte equality and four authored event routes pass.
