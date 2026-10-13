# Pré-race do Arcade

Estado em 02/10/2026. Notas e provas: seção 4.2 de `FIDELITY.md`.

Commit `2f3728d` guarda o banco de câmeras, passo, hook SceneViewFace,
passe de pista e oráculos. A integração em `main.c`/`menu.c`,
`pregrid_menu.c` e seu target CMake ainda estão locais: usam mudanças
de carregamento e áudio ainda não commitadas.
Delta dessa integração guardado em
`extracted/handoff/pregrid-integration-local.patch`; não contém assets.

O preto atrás da grade vinha de `SceneViewFace` sem implementação em
`ui.c`. A cena agora entra na ordem do widget, antes da grade e dos botões.
`001119A0 +0x24..+0x80` retira a corrida do conductor para o menu;
`00388880` seleciona câmera modo 5. Por isso o fundo desenha só a pista,
sem carros, sombras ou passes de reflexão dos carros.

A câmera vem do REP4 em curso +0xC0, com entradas de 0x7C, não do Mot1
da largada em +0xD0. `003747B8 +0x220` seleciona `0037BF00`;
`00375980` troca e repete as tomadas. `course_pregrid.c` porta as curvas
de `003DDBC8`, relógio de `003FC9A0`, projeção de `003794A8` e molas de
`00385098`. Só bancos time-driven com eye/target em coordenadas do mundo
são aceitos. Os bytes pertencem ao curso e ficam vivos até fechar o banco.
O passo usa arredondamento EE e devolve o modo anterior ao chamador.

Dois erros impediam a interface de terminar de inicializar: faltava
`sound.startStreamEx` (registro `0023ED48 +0x6F4`) e a legenda buscava o
nome do widget, ignorando `tooltip_key` do mproject. Agora o script pede
`pcm/default_quick`, e a tradução usa as chaves `TOOLTIP|icon_text_*`
originais. O curso também é passado para `main.game.course_code`, lido
pelo popup QuickTune em `quick-arcade.ad L601`.

Opções abre e retorna ao mesmo grid. A cena é mantida depois da corrida
ou de Escape; Iniciar novamente monta outra corrida com a mesma seleção.
O relógio do fundo não avança a física. Não há geração de cubemaps ou
borrão de rodas durante essa apresentação.

## Conferência

Compilar `opengt4`, `pregrid_menu`, `pregrid_probe`; para o oráculo,
configurar `OPENGT4_RECOMP_TESTS=ON` e compilar `pregrid_oracle`.
O desmonte e `extracted/core.bin` são dependências locais do oráculo,
nunca do executável do jogo.

```text
pregrid_menu.exe disco.iso
pregrid_menu.exe disco.iso --negative
pregrid_probe.exe disco.iso extracted/handoff/pregrid-probe.png
pregrid_probe.exe disco.iso extracted/handoff/pregrid-negative.png --negative
pregrid_oracle.exe extracted/core.bin extracted/handoff/pregrid-grandvalley.crs
pregrid_oracle.exe extracted/core.bin extracted/handoff/pregrid-grandvalley.crs --negative
```

Os controles negativos têm de sair com código 1. Curvas/tempo foram
conferidos em Grand Valley, Grand Valley East, Tsukuba e Nürburgring.
A projeção é comparada na fronteira de `tanf`, não contra a trigonometria
PS2. A matriz final VU, o near clip herdado e o fade interno de
SceneViewFace ainda não têm prova; não chamar o conjunto de exato.

Captura de desenvolvimento sem navegar o catálogo:

```text
opengt4.exe disco.iso arcade TopRoot --pre-race --race-course grandvalley_east --race-car ka_01 --no-config --window 960x720 --shot extracted/handoff/pregrid.png --shot-delay 1
```

`--pre-race` usa um grid de diagnóstico fixo, como `--drive`. O fluxo
normal do Arcade continua usando os adversários escolhidos pelo script.
`--quick-input right,right,accept,back --dialog-answer yes --shot-delay 7`
confere o percurso Opções → confirmação → pre-race.

## Pendências

QuickTune aplicado (10/10): `gt4_race_desc.quick_*` leva potência, peso,
pneus por eixo e velocidade máxima do câmbio; 00111DC0 grava na ficha do
jogador no Start (a corrida montada para o grid é remontada) e o mundo do
Arcade (00387348) encaixa os pneus. Teste: `quick_tune_tests` (ctest
`arcade_quick_tune*`). Detalhes em FIDELITY.md.

B-spec, replay e salvar replay continuam F. A referência com sete ícones
é do pós-corrida. Por pedido explícito de 02/10/2026, os sete agora aparecem
também antes da corrida: os três pendentes recebem foco e legenda, mas aceitar
não sai da tela, não para a música e não grava dados. Visibilidade é P; modos F. Fonte de fallback e rasterização de menus seguem o estado
geral da interface. A imagem final do fundo usa o renderer existente,
incluindo as aproximações documentadas nele.

## Nomes, ícones e áudio (02/10/2026)

O runtime confundia `abbreviate()` com `adjustScale()`: o nome comprido era
encolhido. Agora executa a abreviação original com `...`, preserva escala
.875 e corta no retângulo do TextFace. Fonte padrão hn_14, gap extra zero e
kerning por contornos FT01 vêm do executável; não foram ajustados a olho.
O construtor original também confere RGB padrão 212 e alinhamento 4.
Os ícones usam o cliente 72x68 herdado dos botões, em vez da textura natural
(`002A2608 +0x7C..+0xC0`). A rasterização completa continua A.

`pcm/default_quick` estava entrando no carregador MIDI e ficava silencioso.
`pcm_music.c` lê /bgm/us/ads.inf, grupo 3, com as duas faixas gt4qm-01 e
gt4qm-02. O worker decodifica antes de publicar o stream; o callback não
aloca nem acessa o disco. O volume usa o raw /128 do script. Seleção usa o
RNG original, mas com relógio PC; ADPCM, resampling linear, loop e fade ainda
não foram comparados ao áudio SPU2, portanto nota A.

```text
pregrid_text_oracle.exe extracted/core.bin disco.iso
pregrid_text_oracle.exe extracted/core.bin disco.iso --negative
pregrid_pcm.exe disco.iso extracted/handoff/pregrid-pcm
pregrid_pcm.exe disco.iso extracted/handoff/pregrid-pcm --negative
pregrid_audio_probe.exe disco.iso
pregrid_audio_probe.exe disco.iso --negative
```

Positivos retornam 0; negativos retornam 1. O oráculo compara 32.841 passos
FT01, 32 larguras, 20.480 abreviações e 10.000 estados RNG. `pregrid_menu`
também confere nomes compridos e os sete quads dos ícones.
Para capturar com reprodução real, acrescentar `--with-music` ao comando
`--shot`; sem essa opção a captura desliga o áudio por padrão.

A ponte PCM agora está integrada em `src/app/main.c` através de
`src/app/pregrid_audio.c`, separada do worker MIDI. `menu_pcm_request()`
entrega o grupo e o volume pedidos pelo script; `stopStream` e a saída da
tela removem somente o callback pertencente a esse worker. Reentrar escolhe
uma faixa pelo RNG original e reinicia a reprodução; um pedido cancelado
durante a carga não publica a faixa antiga. Os dados são preparados fora do
loop principal; uma faixa já decodificada é reutilizada se for escolhida
novamente. Shutdown aguarda o worker antes de liberar os dados.

`pregrid_audio_probe` confere as duas faixas, volume 120/128, mute, saída,
reentrada, cancelamento durante a carga e preservação de áudio de outra tela.
Seu controle negativo espera saída não zero com volume zero e retorna 1.
A ponte foi testada com os próprios trechos isolados; a build local e a
execução com `--with-music` também passaram. A build limpa do executável
continua bloqueada por uma dependência anterior: `main.c` já inclui e usa
`career_vehicle.h`, ainda fora do git com suas dependências locais.
Eles não fazem parte deste commit. É integração funcional, ainda A pela
ausência de comparação SPU2.
O delta antigo `extracted/handoff/prerace-pcm-integration-local.patch` foi
substituído por essa ponte e não deve ser reaplicado. As alterações gerais
de loading/MIDI e `pregrid_menu.c` continuam locais, da outra integração.
