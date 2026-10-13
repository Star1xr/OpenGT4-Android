# GT Mode UI â€” 2026-09-29

This work is restricted to native menu presentation. Physics and race
simulation are outside its scope.

Run `build/Release/opengt4.exe "<disc.iso>" gtmode` to open the map directly.
Arrows move the focus, Enter opens a page, Escape returns. The original Arcade
entry still encounters the unimplemented save check; use this direct preview.
An optional fourth argument selects a debug screen, such as `LicenseRoot`.

## Implemented

- GT Mode's single render context shares the page/event state and transition
  object. `createRenderContext(1)` / `getRenderContext(0)` now work; unsupported
  counts and indices fail explicitly.
- Transition objects implement panIn, panOut, panOutIn, syncIn and syncOut.
  Elapsed-time fades cover the composed 2D and ModelFace output. Numeric waits
  continue presenting without recursively executing scripts. A redundant page
  request does not leave the screen black.
- `MProgress.value` drives ProgressFace's clipped texture/colored fill, clamped
  to 0â€“1. Objects remain live through normal VM references and garbage collection.
- appendChild accepts both authored forms, with or without a context argument.
  removeChild detaches only children of the specified parent. This supports
  attaching/removing the shared Common header and tooltip panel on page changes.
- Reparented subtrees adopt the destination page for focus/actors while retaining
  their authored package/translation scope (`resource_root`). Cycles are rejected.
- Optional widget labels return nil when absent, avoiding spurious map warnings.

The ModelFace/OpenGL integration draws the map's original models. Legacy
`onEnter`/`onLeave` focus handlers now drive the authored `target_mode` and
`rotate_v`/`model_rotate_v`. Selection turns at 2 degrees per 60 Hz tick,
integrated in elapsed time; deselection eases back to `model_rotate`. The
return easing rate remains provisional. Initial legacy focus entry is deferred
until the page has initialized its data. Map scrolling centers its landmark.

## Icon framing correction

The user's PCSX2 reference exposed oversized icons, clipped cone/roof tops,
and a wrong rotation order. CORE function `0x0021AB38` applies translation,
then Z, X, Y rotations; the host UI uses row-major **Rz * Rx * Ry**.
The attempted world-Y heading adjustment was reverted after the user's
License Center reference exposed displaced pivots across the map icons.

Framing is the original projection (10/10/2026): `0x00217058` takes the
widget rectangle into the 640x448 framebuffer (`0x00215FE0`, mode 0 of
`D_006184D0`), centres the frustum on it (`0x004A59B8`) and uses one focal
length, half the framebuffer height over tan(view_angle / 2). In 640x480 units
that is x * 0.5 * h * 448/480 / tan and y * 0.5 * h / tan. The old factors
(focal 0.8, vertical 0.85, centre 0.15 h lower) are gone. Compared with the
user's PCSX2 print of Beginner Events at equal text width: same size and tilt
(model X +15 confirmed; 0 and -15 tested worse). The padded viewport (the
rectangle grown twice about its centre) still intersects the parent scissor.
The Dutch flag keeps its 2.5 factor, not yet explained by the original.
Current comparison capture: `build/gtmode_icons.png`.

## Map navigation and notice balloons

The map uses its authored `navigate_source` / `navigate_target` regions as
local left/top/right/bottom bounds. These are deliberately different from
the visible icon rectangles. Directional selection now prefers targets whose
perpendicular intervals overlap, then the closest forward gap. Targets outside
the authored lane are rejected, so map edges do not jump diagonally. This ranking is a native implementation,
not yet an instruction-for-instruction reconstruction of the original search.
Menus without these properties retain their existing geometric selection,
and SelectBox retains ordered navigation. Home goes left to GT Auto, down to
License Center and up to Italy; reversing each returns to Home.
Map movement now uses the ScrollWindow's authored `interpolate_ratio` (0.07)
with elapsed-time easing, instead of the global 0.15 fallback.

The original scripts already populated and faded `NoticeBase`. The renderer
was skipping its `FBox` subtree. FBox now draws and packs its content before
each frame, measuring text and applying minimum dimensions/padding; backgrounds
without geometry fill the computed box. The script controls the notice text,
position, visibility and fade, including hiding the small selected `ex` marker.
Packed labels without an explicit font use `hn_14`, calibrated to the supplied
PCSX2 reference (original default face still unverified). Home's notice measures
221x96 logical pixels; License Center's measures 249x76. Explicit fonts win.
Captures: `build/gtmode_smoke/home_notice.png`, `license_notice.png`.

## Validation

`menu_navigation` now checks context identity, transitions surviving GC, fade
intermediate/end values, same-page fade recovery, shared progress values,
cross-page reparenting, retained resource scope, legacy focus events and
frame-rate-independent model rotation at 30/60/120/144 Hz.
It also checks authored navigation bounds, direction rejection and invariance
under scrolling. `ui_text` validates the underlying font decoding/metrics.

`python tests/ui/run_gtmode_smoke.py "<disc.iso>"` validates Home/map round trips,
License Center selection/reentry and explicit debug-page entry.
It additionally checks directional round trips, notice visibility, translated
text and packed dimensions, and hiding the balloon on a destination without one.
Images and full logs go to ignored `build/gtmode_smoke/`. Known unsupported game-state and
presentation calls remain in those logs; passing means navigation works, not
that every original effect or feature is implemented.

## Remaining work

- MCrossTransition and MBlockTransition currently use a black color fade;
  retained surfaces/crossfade and the original block pattern are not implemented.
- MChaseActor now drives LicenseRoot's selected cone through the authored
  doStart/doStop focus callbacks. CORE 0x00283618 supplies x range 0, y range
  16 and phase step 8.5 degrees; 0x00284270 adds sin(phase) * x range and
  subtracts abs(sin(phase)) * y range from the captured widget position.
  The original truncates phase to integer degrees after each update (8 degrees
  per tick with defaults); a fixed 60 Hz actor clock preserves this at different
  presentation rates. doStart/doStop reset phase (0x002841B0), and stop restores
  the baseline. Native sinf replaces the game's polynomial sine approximation;
  visual comparison of timing against PCSX2 is still pending. Constructor,
  period and floating_range_x/y properties are supported. Tests cover idle,
  stop/restart, GC and consistent movement at 30/60/120/144 Hz.
- MColorObject RGB/RGBA construction and interpolation now work. Text, shadow,
  image and panel colors honor script changes; captured originals can be restored.
- LicenseRoot's animated FlashFace background now works: five disc images,
  authored fades and a roughly 25-second loop. See [STROBE.md](STROBE.md).
  General Flash/STRB content beyond the bitmap-rectangle subset is pending.
- Home statistics and calendar were connected to the native profile on
  2026-10-09. The data primitives have original-code oracles; UI presentation
  remains A. See [HOME.md](HOME.md) for the current scope and proofs.
- Home/GTAuto CarFace, service animations, purchases, license tests, progress
  dialogs and saving are not completed by this UI work.

Useful captures: `build/gtmode_smoke/home.png`, `license.png`, `home_return.png`.
For subsequent work, extend FlashFace as needed and the Home profile display,
and compare the selected cone's timing against PCSX2. Keep physics/race changes
separate.

## UI continuation — 2026-09-29

The UI-only selection stack now implements `main.game.menuPush`, `menuRef`
(optional depth from the top) and `menuPop`. It is distinct from the visible
page stack and is not serialized into player profiles. This lets NationRoot
receive its country, DealerRoot receive its manufacturer, and license detail
pages receive their category. Returning from a dealer restores the original
country and selected manufacturer. String substr/toInt and numeric toString/
toInt complete the label operations used by these scripts.

GT Mode loads the disc's US_carlist.adc into its project before its main script.
This restores the authored new/classic/prize/test-car catalogs, including
LicenseCarData. GetCarName resolves the existing SpecDB display names.
The license test numbers and test-car names now render. The actual CarFace
3D test-car preview is still missing; no substitute image is used.

DealerRoot's top presentation reads each manufacturer's original
`/piece/tuner_screen/<manufacturer>.strb`. Its final frame issues exactly
GotoFrame(0), Play, End; the bitmap reader now validates and honors this
specific loop, including its original frame rate and alpha fades. This does
not implement general ActionScript. All 96 manufacturer clips in the user's
USA v2.00 disc decoded and passed loop-clock validation. Alfa Romeo and Fiat
were also checked through the actual map/country/dealer navigation.

Validation: menu_navigation, native_media and ui_text; extended
run_gtmode_smoke.py exercises license details and return, dealer entry,
country/manufacturer selection and return. To scan the original presentations:
`build/Release/media_tests.exe dealers "<disc.iso>"`.
Captures are in ignored build/gtmode_smoke. Remaining game-state diagnostics
and absent CarFace previews mean this is UI progress, not completed GT Mode.

## Detached ground shadow — 2026-09-30

The static hall_M conversion produces a ground quad centered at X 2.738,
while the Driving Missions building is centered at X 0. Its shadow therefore
orbits outside the building when the whole mesh rotates. ModelFace now aligns
a two-triangle ground quad to the body when the center displacement exceeds
a quarter of the body span. Its size and texture are preserved; small offsets
are retained. This is a presentation compatibility fix, not a reconstruction
of the original anchor adjustment. Car/world conversion is unchanged.
Selected Mission captures at different yaw angles stay centered. The GT Mode
smoke suite includes map selection of Driving Missions.

## Stateful GT Mode contracts — 2026-09-30

`src/core/gtmode_game.c` replaces the empty garage/license/calendar queries
with native methods backed by the player's serialized state. Bind these
after restoring a profile. The dirty callbacks must be attached to existing
modules: `submodule()` creates a replacement and would discard the state.
The integration smoke suite now rejects missing methods for these contracts.

Current equivalence status is in `GT_MODE_LOGIC_VALIDATION.md`. The original
credit primitive has since been ported and compared on GT Mode snapshots;
the other contracts below are still provisional.

Implemented: integer credit transactions with original 64-bit wrap;
up to 1,000 garage entries, stable/reusable indices, selection, deletion and
deleting all except the selected car; the original 16-test license categories
and bronze/silver/gold queries, with the first 15 required for the final test;
record units with NONE defaults; numeric prize IDs and claimed-prize queries;
car availability flags; elapsed days/weeks, Julian-day conversion and dated
calendar events. Native fresh profiles start at Cr. 10,000 on 2 April 2005.
These values do not replace existing profiles' credits or dates.

Earlier PS2 imports retained their original slot number in a dense car list.
Binding now restores sparse indices before resolving `riding_index`, avoiding
selection of the wrong imported car. Imported raw car parameters and record
IDs still require conversion to the native catalog/record schema.

This is a state foundation, not completed GT Mode gameplay. New-car insertion
does not yet complete the authored purchase flow, which also needs MCarGarage
and SpecDB-backed car parameters. Used-car turnover, tuning/maintenance,
race-entry eligibility, result recording, payouts, prize selection and
championship progression are not implemented here. RaceView currently renders
a preview and does not produce a complete race result. No wins or license
medals are awarded by entering or leaving a menu. Calendar event timing
(one day per call) and mission group queries still need comparison with the
original engine before they can be described as exact original behavior.

Validation: gtmode_game tests cover credit boundaries, garage mutation and
selection, license thresholds, leap days, numeric prizes, car availability,
serialization/rebinding and sparse imported garage slots. Actual-disc smoke
checks cover map/Home/license/dealer navigation and the original notices;
the native-save entry suite covers creation, loading and cancellation.

Map presentation adjustments (2026-09-30): the Dutch dealer ModelFace has a
local 2.5x projection adjustment to match the neighboring flags' footprint.
This is visual calibration, not a port of the original projection. Returning
icons use the shortest signed angular difference from the authored starting
orientation (below/above 180 degrees changes the return direction).
The header uses the original day-first layout and groups credits with commas;
runtime TextFace alignment now honors the script's assignments, restoring
the no-car `--` label. Help text rises into view over 0.25 seconds, holds for
one second and scrolls at 100 logical pixels/second when wider than the screen;
it repeats by rising again instead of entering from the right. These timings
are visually chosen and have not been measured against the PS2 runtime.

## Revisão solicitada em 2026-09-30

A próxima revisão visual e de animações está registrada em
[GT_MODE_UI_REVIEW.md](GT_MODE_UI_REVIEW.md), com referências locais
preservadas e o ponto de retomada da validação da carreira.


## Original UI audit - 2026-09-30

See [GT_MODE_UI_REVIEW.md](GT_MODE_UI_REVIEW.md) for current scope, original
addresses, evidence and unresolved differences. The disc inventory contains
213 authored RootWindows in 33 descriptions. Direct-root discovery of 72
screens is not full navigation coverage or visual certification.

Shared fixes include the original tooltip state machine (103,680 exact oracle
updates), nested modal loops, focus restoration, MBox/FBox layout, SelectBox
allocation, String.build, disc-backed course logos, literal text separate from
wrapping, and CarFace loading/pose delivery. CarModel rendering remains partial:
the course script can apply ry=120 and warp, but all axes/motion, lighting,
reflections and projection still require comparison. Default UI filter and
extra sharpness are zero; existing stored PC preferences are preserved.

Flow tests passing does not certify pixels or animation curves. Career-rule
validation is deferred; its authoritative status is GT_MODE_LOGIC_VALIDATION.md.

## Visible regressions corrected, 2026-10-05

Ticket ImageFace now uses inherited client geometry (002A2608 +0x7c..+0xc0),
so 640x206 event textures occupy their authored 296x95 cards. Volkswagen's
three rows pass the zero-tolerance quad test and deliberate height mismatch.
UsedCarModule retains its authored object and loads weekly ADC stock; makeUsed
converts string painting indices. Actual shops show 24/38/38 entries in week 0.
World/Original course lists show 19/10 entries without a debug preview flag.
Saved course-unlock bits (00166670 -> 0042EAE8, state +0x2008) remain pending:
course availability is A, not exact.

ModelFace now takes tan of half the full FOV, as 00217350..00217390 and
002179B0..002179F4 do. Existing focal/vertical/center calibration remains A.
Captures: build/map_fixed.png, used_flow.png, original_courses_fixed.png and
vw_cards_fixed.png. Test: gtmode_purchase_menu ISO --ui-regression; negative
controls GT4_VISIBLE_MENU_NEGATIVE and GT4_CARD_SIZE_NEGATIVE both exit 1.
Release executable rebuilt; 23/23 CTest pass. Optional actor nil reads in
course script lines 3293/3295 and incomplete tuner queries remain documented
limitations outside these rectangle/stock tests.

### Continuação GT Auto: rodas no modelo (2026-10-05)

`CarFace.wheel` passa o label original à apresentação; loader lê `/wheel/menu/<label>` como 00454270 e substitui os aros sob os transforms já existentes. `car_garage` recupera a roda normal do corpo +0x468. Nenhuma mudança na seleção/modelos da corrida. Nota A: sem comparação do original para o encaixe customizado, motions/câmera do GT Auto pendentes. Captura da loja `build/gt_auto_custom_wheel2.png` evidencia carro fora de quadro; não aceitar essa tela como concluída. Probe `build/wheel_changed.png` isola o modelo. Teste contrato/negativo: OPENGT4_PROBE_TEST_WHEEL / OPENGT4_PROBE_WHEEL_NEGATIVE no showroom_probe, quatro carros. Spoiler visível, câmbio automático, avisos e fluxo completo compra/save continuam pendentes.

### Continuação GT Auto: câmera do disco (2026-10-05)

loadModelOptionMotion (001391D0 -> 0013A678) lê os arquivos Mot1 dos GPBs, com cache por projeto. Widgets guardam referência e tempo; frontend leva look-at/FOV ao backend GPU. Captura build/gt_auto_camera_wait.png, com --shot-delay 3, mostra a roda selecionada e o carro; a captura imediata ocorria durante a recarga assíncrona. Nota A: falta comparação PCSX2 de matriz/tempo e backend software. Teste --shop-catalog compara 22 amostras/cache/ausência com leitura direta dos GPBs; negativo GT4_OPTION_CAMERA_NEGATIVE falha. Ainda pendentes loadModelSet/pitmanFix, loadModelMotion/GTAutoEntranceMotion, playModelMotion, loop da animação, spoiler visível e fluxo completo compra/save.

### Continuação GT Auto: asa visível (2026-10-05)

Modelo externo `/wing/menu/<label>` carregado como 00454378. Montagem 004533A8: CarInfo+0x50 -> geometry+0xd0 posição e geometry+0xdc*0.5 escala uniforme. Callback 47 seleciona branch de suporte 0/1 conforme flag instalada, como 00453950/00453488. CarFace.wing recarrega apresentação; car_garage recupera WING da folha ativa +0x100. p000 remove; corrida mantém seus loaders existentes. Contrato validado em BEAT, Ka, 120i e Focus RS com OPENGT4_PROBE_TEST_WING, negativo OPENGT4_PROBE_WING_NEGATIVE falha (exit1). Capturas build/wing_probe.png e build/gt_auto_wing2.png. A: comparação PCSX2, animações do GT Auto e compra/save completos pendentes.

### GT Auto: eventos de compra (05/10/2026)

`putBuyWheelEvent` e `putBuyWingEvent` agora seguem o calendário binário original em vez do registro genérico: tipos 6/7, chaves nos namespaces 29/30, categoria/detalhe truncados para byte, avanço de um dia. Fontes: 0042F3C0, 0042F408, 004409D8, 00440B18. Nota A: bytes reservados do histórico de pilha são zerados na ponte Adhoc; o helper preserva os bytes recebidos. Teste `gtmode_purchase_menu ISO --shop-catalog` passou incluindo cinco eventos/dias; negativo GT4_CUSTOM_EVENT_NEGATIVE falhou na chave alterada. Build opengt4 passou e 23/23 CTest. Continua pendente validar compra/cancelamento/reabertura do save pelas telas e os avisos de tuning no mapa.

### Compra pela interface e reabertura (05/10/2026)

Teste `gtmode_purchase_menu ISO --customization-purchase`: scripts originais de rodas/asas, cancelamento sem mudar corpo/créditos/eventos, compra Bridgestone 001/p001, Cr 10000 → 9500 → 8300, instalação e dois eventos; save explícito e outra VM restauram corpo 0x4b8 integralmente. O harness substitui somente apresentação do diálogo e carregamento 3D (coberto separadamente por showroom_probe). Negativos GT4_PURCHASE_CUSTOM_NEGATIVE e GT4_TUNING_NOTICE_NEGATIVE falharam. TopRoot gera aviso de tuning Ford em DealerUSA e torna ex visível. Nota A, ainda sem oráculo EE/desenho e sem validar Auto Save original; tipos 11/31 e quatro chamadas de animação GT Auto continuam pendentes. Corrigido CarFace.wing: WingShopRoot L12436 passa default para restaurar modelo de fábrica, agora convertido para ausência de asa extra.

### Tipos 11/31 e Auto Save original (05/10/2026)

Rigidez tipo31 implementada (flag +0x168 nas três folhas), E contra 00440B50/004468F8 na bateria 8192; negativo GT4_RIGIDITY_NEGATIVE falha. Padrões câmbio tipo11: E em 1024 corpos/buffers contra 004402F0, linha real GEAR 45 bytes; GT4_GEAR_DEFAULT_NEGATIVE falha. Geração automática de relações integrada via callback gt4_career_vehicle_auto_gears, fonte 00443240; reutiliza física sem editá-la, ainda A na integração. Shop-catalog testa três estágios Ka, negativo GT4_AUTO_GEARS_NEGATIVE falha. Validade de bytes carregados de peças agora é marcada ao instalar. Teste customization-purchase deixa de salvar manualmente após as compras: botão voltar do GTAutoRoot passa por checkAutoSave e TopRoot, perfil limpo e reabertura conserva corpo/créditos/eventos. Ainda pendentes quatro chamadas/modelos de animação GT Auto, testes de compra dos black cars e comparação visual com PCSX2.

### GT Auto: lavagem e óleo funcionais (08/10/2026)

Usuário adiou acabamento visual de Settings e pediu seguir funcionamento.
CarWashRoot/OilChangeRoot já executam os scripts do disco: cobrança de 50,
Back antes do serviço, recusa por falta de saldo, relógios independentes,
done e Auto Save pela saída ao TopRoot. Teste gtmode_gtauto_menu passa em
Ka FF, BEAT MR e Lancer 4WD; body inteiro e dinheiro sobrevivem ao reload.
Negativo altera saldo. Getter de sujeira/oil life e setter visual: E restrito
a 12288 comparações com CORE, três negativos (docs/FIDELITY.md).

loadModelSet mantém modelos em ordem; loadModelMotion aceita alinhamento,
hidden e yaw como 00139098..F0/00154124..30. 00153FA8 consulta só a câmera.
GPU mantém mesh por asset/serial e desenha cada track; mecânicos skinned,
panos e carrinho rígido com nomes MDLS+58 (0042A660 +70..B8). Arrays clonadas
profundamente, liberadas com widgets. Geometria usa scratch privado do
construtor já existente; nenhuma edição na física.

Sons car_wash, oil, oil_jackup/down usam INST originais. Fallback Se atende
note-off/loop e B0/41 (0055AF48 +158..230), sem síntese no callback. Teste
sound_tests --services confere quatro PCM, rejeita truncamento e detecta
pitch ignorado; --service-negative falha. Efeitos simples ficam na rota
anterior. A: clock de áudio 240 ticks/s herdado, ganho .65/interpolação
linear/ramp contínuo; ADSR/reverb/quantização original ainda pendentes.

Apresentação A: composição dos mecânicos/alinhamentos e câmera precisam de
comparação PCSX2, sombras dos adicionais e backend software faltam. Não
ajustar offsets no olho. Probes wash/oil detectam cada modelo separadamente;
OPENGT4_PROBE_SERVICE_NEGATIVE remove tracks e falha. Capturas/provas em
build/gtauto-functional, só locais. App CarWashRoot completa serviço e
mantém Exit ativo. GT Auto integral e GT Mode completo não certificados.

Verificação final: snapshot só do índice compila app, renderer e oráculos;
10/10 CTest focados passam (serviços/valores/sons, controles negativos,
native_sound e gtmode_game). Probes wash/oil e negativos passaram no
renderer; app encerrou lavagem/óleo em Exit. Arquivos de física: 42 idênticos
à baseline, nenhum no commit. menu_navigation continua falhando no assert
da decoração do dropdown (test_menu.c:588); mesma falha reproduzida com
ui.c/ui.h/menu.c/test_menu.c idênticos ao HEAD anterior, sem os serviços.
Não anunciar suíte integral passando. Logs/capturas só locais em
build/gtauto-functional; sem push.


### GT Auto: relógios, reflexo e levantamento (09/10/2026)

Slots PCSX2 06/07 do usuário: BEAT Version F, lavagem a 2,299998045 s e
óleo MR a 1,599998713 s. Referências/EE só locais em
extracted/handoff/gtauto-states-20261008. L12062 usa oil_change_f_cam também
no MR (align==align2); L12088 manda ry=180. A aplicação já selecionava isso;
fixture do probe corrigida. Não trocar a câmera MR por oil_change_cam:
esta é usada quando align!=align2 (4WD), com os movimentos start/end.

Adicionais entram no reflexo. Hidden mantém Y achatado no passe regular e
omite o refletido, como 00156AA0. Canal Mot1 lift passa da main ao renderer:
corpo sobe, rodas/freios ficam no hub menos min(lift,raio) relativo ao corpo,
fonte 00156950/00459AD8/00459C10. Meshes separados preparados só na primeira
elevação; residentes e compartilhando texturas do corpo. Sem offsets novos
nos mecânicos para coincidir com o print.

E isolado: 4096 alinhamentos/passes, 4096 parâmetros de lift, 184320 estados
do controlador sem troca enfileirada. Negativos draw/lift/clock. Acumulador
UI entrega dt US 3C888888 a 60 Hz; fim estrito e uma subtração por
normalização seguem 0021B4F8/0021B528. Normalizar no update ainda difere da
ordem de desenho original: não prova sincronismo completo.

Áudio: 480 ticks/s, 120 BPM/240 ppqn, driver 60 Hz, substituindo 240/s.
Rampa quantizada e tabelas do CORE. Quatro serviços usam backend SPU nativo
já existente; PCM preparado uma vez/reutilizado, sem síntese no callback.
E escalar em 171953 estados, três negativos. sound_tests --services confere
PCM/automação/truncamento/cache; --service-negative conserva pitch e falha.

Apresentação/áudio integral A. Capturas wash-slot06/oil-slot07 com página e
luzes do disco mostram diferenças de luz/reflexo/pose e carrinho (óleo).
Não foram corrigidas no olho. Pendentes composição/curvas completas,
sombras, backend software e movimento 2 completo da pose (00153C90..A4).
Reverb offline não conserva estado compartilhado IOP; cauda de 1 s é A.
PCM e fase áudio/UI sem comparação. Não transferir E dos helpers para isso.

Probes: OPENGT4_PROBE_SERVICE=wash/oil, OPENGT4_PROBE_SERVICE_PAGE=1,
OPENGT4_PROBE_SERVICE_TIME no tempo acima; óleo MR com
OPENGT4_PROBE_SERVICE_ALIGNMENT=10 e carro beat_f_92, cor 1.
OPENGT4_PROBE_SERVICE_NEGATIVE remove tracks e falha.
Provas/logs em build/gtauto-presentation, locais; física preservada.


Amostragem Mot1 adicional: gtmode_service_motion executa 0042B440 nas dez
fitas de lavagem/óleo, start/end e panos, 2410 streams. C de matrizes com
tolerância 0,00002, erro máximo 9,53674316e-7; sin/cos e stack são fronteiras.
Controle negativo altera matriz e falha. A implementação anterior falhava
no fim da lavagem: clamp do último valor em vez de extrapolação do segmento
extremo (00486090/00486388); Mot1 agora segue Horner de 004861A8 +00..68.
Isso não corrige por si a composição rígida do carrinho: ainda deslocado
no slot 7; não acrescentar offset visual. Câmeras/timing integral e pixels
continuam A. Nenhum arquivo da física alterado.


Verificação deste trecho: aplicativo Release atualizado; snapshot só do
índice compila aplicativo, renderer e oráculos. 21/21 CTest focados passam,
incluindo controles negativos. Probes BEAT nos slots 6/7 e fim da lavagem
passam; remover os tracks é detectado. Physics: 42 arquivos idênticos à
baseline. Capturas, CORE e desmonte ficam locais, fora do commit.


### GT Auto: carrinho, luz, sombra e projeção (09/10/2026, continuação)

Esta continuação substitui as pendências escalares da nota anterior, sem
declarar apresentação integral fiel. Física não foi editada por esta tarefa.

- Carrinho: Mot1 rígido compõe raiz em colunas com rot do renderer em linhas;
  00156AA0 -> 0042A828. Sem offset visual novo. additionalModelScale=.95 do
  script vale para lavagem e óleo; BEAT MR mantém ry=180/f_cam.
- Luzes: canais Mot1 0C..0F de Light0/1/2, sombraOpacity/fade/lift seguem a
  câmera. Sombra de contato 004524B8 usa -light no referencial do carro e
  substitui coluna Y, sem dividir por lightY; fica no chão durante lift.
- Levantamento: 00364B30 usa axle[2]-axle[4], limite de suspensão, não raio.
  Body/brake meshes e texturas permanecem compartilhados/residentes.
- Relógios: update avança; desenho normaliza câmera viva (00156F18), mas
  adicionais normalizam cópia (00273AC8). playModelMotion não reinicia tempo.
  Studio usa conductor próprio; compatibilidade do primeiro track não soma
  dt duas vezes. motion=2 e fade contra EE em 92160 frames exatos.
- Projeção: RenderCtx vt663BE0+1FC -> 00215FE0 converte widget640x480 para
  framebuffer640x448. 002173AC chama 00107328, que aplica correção vertical
  1/0x3F8C8B43 do modo NTSC2 (001060F8); antes ela faltava no GT Auto.
  E: 16 modos/escala e 3 offsets GS, com negativos independentes.
- Composição: ALPHA0x68/FIX(opacity*.3) refletido, 0x64 regular, dois samples
  regulares/um refletido (00217058/00216E78), acumulados com FIX(1/(i+1)).
  Contexto adicional separado abaixo de bodyopacity=.99 (00217430..48).
  FBOs/meshes/texturas residentes; nenhuma captura/readback na apresentação.
  GPU C:132 casos, tolerância1/255 simples,2/255 acumulado; máximo.719/255.
  Negativos alterando FIX e índice de sample são detectados.
- Mot1 C:3133 streams (dez fitas+três câmeras), tol.00002, max9.53674316e-7;
  matrizes, look/FOV, fade/lift/shadow e luzes. Negativos matriz/canais.
- 25/25 CTest focados passam. Compra/entrada --customization-purchase e seu
  negativo conferidos. Probes BEAT slots6/7, fim e remoção de tracks passam.
  Referências/logs somente em build/gtauto-finish e extracted/handoff.

Apresentação integral A: diferenças de cobertura/rasterização, recorte da
página e iluminação/skin final; backend software sem os passes. Relógios e
canais provados não certificam fase áudio/UI/IOP nem pipeline GS inteiro.
Usuário não consegue GS dump; seguir evidência do código e savestates,
não pedir novamente nem calibrar enquadramento no olho. Não anunciar E/C
para a imagem inteira. Sem push; assets/capturas/CORE/desmonte fora do Git.

Verificação final desta continuação: snapshot do índice compila aplicativo,
renderer e oráculos; 25/25 CTest passam. Composição GPU e negativo de
amostra passam no snapshot; compra/entrada também passa. Release atualizado.
Nenhum arquivo de physics/asset/captura foi staged nesta tarefa.
