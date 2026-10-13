# Mods: ideias para o futuro (Enhanced)

Anotado em 07/10/2026. Nada disto está em andamento: o trabalho atual é o
Classic. Regras que já valem estão em `docs/EDITIONS.md` (mods só no
Enhanced; mods de GT4 existentes por um importador só de assets).

## Formato oficial de mod do OpenGT4

- Pacote com manifesto: nome, versão, autor, dependências e o rótulo do
  carro (ou da pista).
- Arquivos que substituem caminhos do VOL (`/car/...`, `/specdb/...`) e
  conteúdo novo.
- Tabelas (specdb) em texto legível (JSON); o jogo as converte nas linhas
  que o código original lê.

## Mods existentes (Spec II e outros)

- Importador só de assets: carros, pistas e sons, dos arquivos que o
  usuário tem; o projeto não distribui nada deles.
- As mecânicas desses mods (câmeras novas, Event Synthesizer, randomizer,
  motor de som) são feitas direto no Enhanced, como recursos nativos.
- Spec II (v1.10, sobre o GT4 Online Public Beta NTSC, SCUS-97436):
  carros novos Mini Cooper 1.3i '98, Mazda MX-5 '05 (da demo do MX-5),
  Hyundai Click Type-R '04, Tiburon Turbulence '99, Tuscani CCS '03,
  Lexus IS200 GT-1 Race Car '04 (os quatro da versão coreana do GT4);
  pista nova Circuit Valencia (do Tourist Trophy); sons de motor refeitos
  em mais de 400 carros. Esses carros já vêm em `CAR4` e o importador só os
  reempacota. Quase tudo existe em discos oficiais (coreano, demo do MX-5,
  Tourist Trophy), que o importador também poderia ler.

## O que um carro precisa ter (como o port lê um carro do disco)

1. Identidade: rótulo (`GENERIC_CAR.idi`), linha do `GENERIC_CAR` (o tipo
   de teto escolhe o modelo aberto/fechado), nome (`CAR_NAME`, banco de
   textos), fabricante, país, ano, categoria.
2. Modelos `CAR4`: menu, corrida (`/car/lod`) e teto aberto (`/car/open`);
   carroceria, 4 rodas, pneus, freios, volante, sombra, casco, sombra
   projetada, luz de freio, pontos de câmera (cockpit, para-choque,
   teto), lugar e animações do piloto.
3. Pinturas: `VARIATION` (nomes e cores) e o `.pat` do modelo.
4. Física: `DEFAULT_PARTS` -> motor, tração, chassi, suspensão, freio,
   pneus, câmbio, direção, LSD... O motor de física é o mesmo para todos.
5. Peças de tuning com preço nas tabelas de peças.
6. Som: o banco de motor do carro.
7. Imagens: fotos da concessionária, miniaturas, o ícone lateral 96x64 do
   HUD.
8. Onde aparece: concessionária, usados, prêmios, restrições de evento.

A ler antes de fechar o formato: como o jogo escolhe o banco de som do
motor, de onde vêm as fotos da concessionária, que tabelas a IA usa.

## Modelos 3D: glTF para criar, `CAR4` no jogo

- O modder exporta glTF 2.0 (Blender etc.) com regras de nome: `body`,
  `body_open`, `wheel_FL/FR/RL/RR`, `brake_*`, `tire_*`, `steering_wheel`,
  `lamp_brake`, `shadow`, `hull`, pontos `camera_cockpit`,
  `camera_bumper`, `camera_roof`, `driver_seat`; pinturas como variantes
  de material (`KHR_materials_variants`); um nível de detalhe para o menu e
  outro para a corrida.
- Uma ferramenta do SDK converte glTF -> `CAR4`. O pacote do mod leva só
  `CAR4` e o jogo só lê `CAR4`: o carro de mod passa pelo mesmo leitor e
  pelo mesmo desenho dos carros da Polyphony.
- Perfil GT4, para não destoar de um jogo de 2005: o conversor reduz
  texturas aos tamanhos do disco (256x256 ou menos) e à paleta de 256/16
  cores do `Tex1`, grava posições em ponto fixo, transforma cada material
  num dos tipos do GT4 (pintura com reflexo, vidro, cromado, borracha,
  farol, luz de freio; mapas PBR ignorados) e recusa modelo acima do
  limite de triângulos (na corrida os carros do disco têm uns 4.500 a
  5.000; o Ka 4.582, o Ford GT 5.072). Um carro "fora do padrão" de
  propósito só leva aviso.
- Validador e teste: peças obrigatórias presentes, rodas batendo com a
  ficha (entre-eixos, pneus), o carro no showroom e numa corrida sem
  janela, foto ao lado de um carro original.
- Custo estimado do escritor `CAR4` só para o OpenGT4: 4 a 6 sessões
  (`Tex1` ao contrário do leitor, `MDLS` com tiras, materiais e o programa
  de desenho, peças à parte, `Pat0`). Teste: ler e reescrever os 807 carros
  do disco e comparar. Um `CAR4` que o PS2 real aceite é bem mais caro e
  não é o objetivo.
- Antes de começar: ver se as ferramentas de modelo do Nenkai (PDTools)
  já escrevem `CAR4`, e sob que licença.

## Pistas

O que o port já lê de uma pista (`/crs/<nome>` e `/crs/<nome>.ad`): o
mundo (MDLS), o céu (+0x54), o horizonte (+0x58), o céu e o cenário dos
reflexos (+0x5C, +0x14), a camada de brilho do asfalto; o RNW4
(triângulos de colisão com o piso de cada um e o bit 0x20 das folhas,
grid, traçado com setores e chegada, linhas); o Env0 (+0x80: luz,
névoa, classes de fumaça); os Tex1 da fumaça (+0x68) e das partículas
(+0x9C); as câmeras de TV (REP4, +0xC0) e de largada (+0xD0); o logo
(+0xC4); as linhas da IA (`.ad`); o mapa do HUD; a linha `COURSE`.

Ideia de mod de pista:
- Criação em glTF com regras de nome: `world` (cenário) e `collision`
  (malha de colisão; o material marca o piso: asfalto, grama, terra,
  folhas...), pontos de grid, chegada e setores, uma curva central do
  traçado.
- Conversor do SDK: cenário no formato do disco com limites de pista de
  2005 (texturas com paleta, materiais do GT4, polígonos como os das
  pistas originais); RNW4 com a árvore de busca da colisão montada pelo
  conversor; linha da IA gerada da curva central e ajustável; Env0 de um
  preset (dia, tarde, neve) tirado de uma pista do disco.
- Opcionais: câmeras de TV e de largada (sem elas, o replay usa as
  câmeras do carro).
- Validador e teste: um carro da IA dá uma volta sem janela; conferir que
  completa sem sair, que os setores disparam e que o grid cabe 6 carros.
- Importadas: pistas de discos do mesmo motor (Circuit Valencia do Tourist
  Trophy) já vêm no formato e só são reempacotadas. Pistas de outros
  jogos, não.
- O mais difícil: a linha da IA (`.ad`, ainda não lido por inteiro).
