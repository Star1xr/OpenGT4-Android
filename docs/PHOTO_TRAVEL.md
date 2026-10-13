# Photo Travel

O fluxo parte do Photo Travel do GT Mode. O script original seleciona
`game.course_code`, conserva o carro da garagem e pede a sequência
`photo_shoot` (gtmode.ad L13585..13596). O app instala a cena estática antes
de entrar em ShootRoot. Não inicia sessão de corrida nem toca no solver.

## Uso no PC

- Setas escolhem o controle ou ícone; Enter ativa/confirma.
- WASD movimenta o carro/câmera ou aplica o controle focado (Pan, Roll,
  Move, Climb). Q/E giram o carro quando seu ícone está selecionado.
- FocalLen aceita esquerda/direita com o passo do script.
- F5 aciona o botão Shoot; também dá para escolher esse botão e usar Enter.
- Num replay 1P, F5 abre Photo Drive na cena atual. A corrida e o áudio
  ficam congelados; Esc em ShootRoot ou Exit retoma o mesmo replay.
- Aperture, ManualFocus, Filter, exposição, contraste e balanço de branco
  agora afetam a foto. ManualFocus mede o ponto central da imagem.
- Na prévia, Enter salva PNG; Esc retorna à câmera. Q/E controlam o zoom
  da inspeção pelos bits L1/R1 do script imageControl.ad.
- As fotos vão para a subpasta `photos` da pasta de saves, com data e número.
  A captura não inclui a UI. A prévia não muda quando a cena ao vivo muda.

## Fontes

- `0039674C..64`: cabeçalho COURSE+CC -> CRPM.
- `003C65E8 +50..D4`: regiões de 64 bytes e câmeras de 16 bytes.
- `0036FAE8 +190..248`: poses iniciais X/Z/ângulo, limites e dimensões.
- `00371220 +D0..FC`: mira da câmera; `00370DD0 +90..C4`: altura.
- `003FE358 +2C/+E8..12C`: consulta do chão, zero quando não há superfície.
- `00370D08/0036F4E0`: distância focal em décimos de mm e sensor de 24 mm.
- `0036F5C0/00370D70`: abertura lida de CORE.GT4, tabela 00620E6C.
- `003716A8`, `00371800`, `003719D0`, `00371A58`: pan, mira, altura, roll.
- `00194DB0 +90..124`: mapa MDLS em CRPM+14.
- `00239018/20`: ScaleBar.value float e clamp pelo min/max.
- `00462EC8 +B4..10C`: banco `/sound/gt4shutter<tipo>.ins`; o registro
  shutter de gt4se.inf seleciona bank 4 / sequence 0.
- ShootRoot, PreviewRoot e seus diálogos vêm de photo_shoot.mproject/.adc.

Nenhuma tabela ou imagem do jogo está embutida no código/repositório.

## Conferência

Configurar `OPENGT4_TEST_DISC` com o caminho do próprio disco e executar:

```powershell
cmake --build build --config Release --target opengt4 photo_mode_tests photo_travel_probe
ctest --test-dir build -C Release -R '^photo_' --output-on-failure
```

`photo_mode_data` compara os campos CRPM de todos os 15 destinos com uma
leitura independente dos bytes originais, rejeita tabelas quebradas e
truncamentos e exercita os 19 bancos de obturador. O negativo desloca a pose
original e falha. A nota E cobre somente os campos CRPM comparados.

`photo_travel_flow` exercita o script Shoot, prévia, três exports, retorno,
nova foto, mudança Brooklyn -> Gion e controle numérico da focal. Confere
dimensões/assinatura PNG, nomes separados e hash do congelamento. O negativo
corrompe o hash esperado e falha. Isto testa o adaptador, não os pixels PS2.

Validação de 2026-10-08: build Release do índice isolado passou; os quatro
testes acima passaram (incluindo os dois negativos). A entrada direta em
ShootRoot/Brooklyn também gerou captura. Os 42 arquivos da física presentes
no baseline permaneceram byte a byte intactos durante esta tarefa.

Para abrir diretamente sem alterar a garagem, somente para diagnóstico:

```powershell
$env:OPENGT4_PHOTO_COURSE='ss_brooklyn'
$env:OPENGT4_PHOTO_CAR='ka_01'
build/Release/opengt4.exe '<seu disco.iso>' photo_shoot ShootRoot
```

## Pendências de fidelidade

**A:** matriz/trigonometria host, near PC 0,2 m, escolha inicial da região 0,
mapa/ícones e zoom sem seletor/clipping dos polígonos internos, movimentos do
carro no mapa, modelo completo do menu, luz/material e relógio de apresentação.
Ainda falta comparação visual com PCSX2. Retrato/aspectos, visibilidade do
carro em can_shoot e pan da inspeção também precisam de conferência.

**F:** música/ambiente específico de Photo Travel. Exposição temporal do
Photo Drive ligada em 12/10, amostrando histórico de render sem executar
novos passos físicos.

12/10: efeitos escalares de Photo Develop ligados: exposição, white balance,
contraste e mature/monochrome. `photo_optics` executa 003EFC10/003EFD80:
3366 combinações E, coeficientes bit a bit. 1024 amostras do disco de lente
(003323B0, MT 00579438/00579548), E. Deslocamento e compensação de foco de
00332550, 512 matrizes C, tolerância 1e-7. Três negativos detectam alterações.
O número de amostras (32) vem de 003C4D98/9C, não do campo JPEG quality (95).
CLUT e mistura reutilizam os passes já conferidos de license_image.

**A:** resultado óptico completo. Autófoco lê um pixel do depth GL;
near/far, diagonais do sensor, pose da câmera e viewport nativos substituem
os contextos GS. A média de 32 cenas usa alpha contínuo na GPU (P), sem os
tiles 2x2 e o truncamento FIX do PS2. Imagem PCSX2 ainda não comparada.

Photo Drive reutiliza os carros, poses, rodas, piloto, objetos e pista
residentes do replay. `race_resources` confere PNG, bloqueio de update,
carro 0x10A0/cursor da fita intactos, restauração da câmera e retomada,
além da volta para Photo Travel com mapa visível. Negativo remove o bloqueio
e detecta avanço. Integração C nativa; composição/pixels A, teclado/PNG P.

**P:** teclado, PNG na resolução da janela e armazenamento local substituem
pad e Memory Card/USB. A seleção dos efeitos e os rótulos vêm do disco, mas
a saída de áudio ainda usa o mixer aproximado que já existia.

## Photo Drive: sessão e exposição temporal, 12/10

GT Mode -> CourseEntryRoot -> Photo Drive atende agora a sequência
photo_drive do próprio script. Carro equipado, pista selecionada, RACE
photo_drive do disco (um carro, mode 0, zero voltas), GT world 003EDE50
como 0011AC54/0011ACB4. O menu é quick-freerun (0011B288); não há lista
RACES de adversários. photo_drive_menu confere callback original, retorno,
corpo, cor e regras; negativo troca a rota e reprova. C de integração.

Na sessão ao vivo ou replay 1P: F5 abre câmera; [ / ] selecionam os 19
obturadores originais, F5 fotografa, Enter salva PNG, Esc retorna.
Teclado/PNG/UI emprestada de Photo Travel são P; PhotoPause STRB completo
continua pendente. Sem avanço da corrida enquanto a foto está aberta.

Tempos de 00620EF0 e 0036F628/0036FA20; limite por 00333680, distribuição
por 00334730 e interpolação por 0033B358. photo_optics compara 736 casos
bit a bit e três limites, negativo TIME: E somente para rotinas escalares.
photo_motion confere oito carros/rodas/sombras/câmera, não cumulativo,
sem histórico e restauração; negativo detecta alteração. race_resources
confere fotos diferentes em 1/60 e 1/4000, 32 MiB intactos, câmera/visibilidade,
sessão GT ao vivo e retomada/replay: C nativa.

Imagem completa A: câmera editada com delta host, partículas/objetos/box
mantêm estado atual; média GL/viewport P sem equivalência integral GS.
Amostras reutilizam geometria residente e só alteram estado de render.
Nenhum arquivo da física foi editado por esta tarefa.

## Revelação de filmes PS2, 12/10

Pedido do usuário: implementar agora e comparar depois. Import Media aceita
F#### de PSU/cartão/diretório, decifra sem escrever na fonte e preserva o
stream em `films/*.ogt4film`. Photo Lab -> Film -> Develop executa
`labomode.ad` L1574..1595; a fronteira nativa abre PhotoSave/PreviewRoot.
Esc retorna ao Photo Lab; a exportação salva um PNG novo em `photos/`.

`00334FC8` escreve PMDI9 e FILM82 (v2), sem padding. Dimensões são s16;
tiles/amostras são s8; boolean usa um byte; RHDR frame count usa s32.
PCAM tem 17 floats; CPRM embaralha os blocos e expande um boolean byte
para a palavra nativa +114. O último SFRM é atual, o anterior é histórico.
Carros e câmera são reconstruídos sem executar uma sessão física.

**E delimitado:** `photo_film_format`: 256 FILM originais, 256 PCAM,
256 CPRM (versões0..3) e 256 blocos de desenvolvimento/filtros comparados
bit a bit com 00334FC8/0033ADE8/0045A4A0/00334B28. Negativo altera width;
todos os truncamentos do fixture rejeitados sem alterar a saída.

**C nativo:** `film_menu` executa a lista/confirm/load/commandDevelop e
consumo único; `film_develop` passa por PreviewRoot real, reconstrói Ka e
BEAT em Tsukuba e Brooklyn, prova presença do carro por diferença de PNG,
preview congelado, exportação e fonte intacta. Negativos suprimem dispatch
ou diferença de pixels e falham. Fixtures autorais, sem assets no git.

**A integral:** sem filme real e comparação PCSX2. Body/LOD CDTL,
planos de recorte SMOK, partículas entre snapshots, GDGT, develop ambiental,
far clip, retrato e tiles GS pendentes. PNG no viewport/acumulação GL P.


### 12/10: detalhes de filmes e classes de replay

DRVR (20 ossos + grasp v1), quatro rodas CPRM (hub/ângulos/intervalo,
limite e dimensões gravadas), CDTL wheel/wing para todos os carros,
SMOK/PTCL restaurados. Piloto usa modelo de corrida; filme não simula.
photo_film_oracle: 7/7 CTests incluindo seis negativos; E restrito aos
headers/bancos, esqueleto/interpolação, registros, ângulo e limite.
film_develop retira rodas/piloto/fumaça/folhas separadamente e confirma
pixels alterados em Tsukuba/Brooklyn; dois negativos falham. race_resources,
film_menu e ps2_media_fixture passam. Imagem integral A.
Replay: 12 classes/48 fixtures, count0..3 e dois streams independentes;
negativo detecta steps alterado. 93 arquivos reais/556738 comandos E.
Cobertura e limites em docs/PHOTO_MEDIA_COVERAGE.md. Física intacta.
