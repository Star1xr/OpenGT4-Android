# Modos: Classic e Enhanced

Decidido em 29/09/2026; **alterado em 02/10/2026**: não são mais dois
executáveis, são **dois modos do mesmo jogo**. Direção de longo prazo; nada
aqui muda o trabalho atual além das regras da seção "O que vale desde já".

## A ideia que guia tudo (02/10/2026)

O objetivo não é "rodar o jogo no PC igual ao original". É: **como a
Polyphony teria feito, e mantido vivo, se ela mesma tivesse lançado o GT4
para PC.** Consequências:

- O Classic é o ponto de partida provado; o Enhanced é o jogo "vivo".
- Tudo o que for acrescentado tem de parecer feito pelo estúdio: mesma
  linguagem visual, mesmas fontes e sons, mesma lógica de menus. Nada com
  cara de remendo ou de mod colado por cima.
- Recursos de PC são tratados como parte do jogo (opções de vídeo,
  teclado e mouse, proporções de tela), não como ferramentas externas.

## Três camadas

1. **O jogo**: conteúdo e comportamento do GT4. Tem de ser o GT4; é o que
   o registro de fidelidade mede.
2. **A camada de PC**, presente nos dois modos: o que precisa existir
   porque o jogo não está mais num PS2. Tela de saves (no lugar do memory
   card), nova carreira, opções de vídeo, mapeamento de controles, sair
   para o sistema. Não é "melhoria": o Classic também tem. É desenhada como
   o estúdio faria, no estilo do jogo. No registro de fidelidade entra como
   P (diferente de propósito), cada item com a sua justificativa.
3. **A camada Enhanced**, opcional: render moderno, mods, sistemas com
   versão nova, itens extras de interface.

### Telas e peças novas (02/10/2026)

- Primeiro, usar o que o jogo já tem: fontes, molduras, ícones, sons,
  transições e o comportamento do cursor dos menus originais.
- Quando faltar uma peça, **pode-se criar asset novo e original**, feito
  como o estúdio teria feito na época (mesma linguagem visual, mesma
  resolução e paleta do jogo). Vale para a camada de PC nos dois modos e
  para conteúdo novo do Enhanced (por exemplo, uma concessionária do
  Brasil).
- Asset novo é o único conteúdo que o projeto distribui. Tem de ser
  realmente original: nada copiado ou derivado do disco, e sem logos de
  marcas (do jogo ou de fabricantes) sem que o usuário decida esse risco.
- Conteúdo novo de jogo (carros, pistas, concessionárias) é só do
  Enhanced. No registro de fidelidade, peça nova entra como P.
- Conteúdo com marcas reais (por exemplo a concessionária do Brasil) é
  distribuído **como mod, separado do projeto**, não como parte do
  sistema. O projeto entrega a estrutura que o mod usa.
- A arte nova é feita à mão pelo usuário (designer), não gerada.

Qual modo abre por padrão **não está decidido**. Uma saída é o jogo
perguntar na primeira execução, numa tela no estilo do jogo.

## Os dois modos

- **Classic**: vem primeiro. O mais parecido possível com o original:
  comportamento, visual e controles do PS2, sem melhorias. O Classic
  **imita de propósito o aspecto antigo** (a imagem do PS2), mesmo num PC
  que poderia desenhar melhor.
- **Enhanced**: vem depois. É a mesma base do Classic, melhorada, no
  espírito do OpenTTD: render novo (sombras e reflexos de verdade,
  iluminação nova), teclado e mouse nativos, itens novos de interface,
  **mods**. As opções vêm ligadas por padrão.

## Um código, um executável, dois modos

- Um repositório, **um executável**. O modo é escolhido em tempo de
  execução (configuração ou opção do jogo), não na compilação.
- A base é uma só. O Enhanced **não é uma base diferente**: é a base do
  Classic se comportando de outro jeito, com camadas a mais que podem ser
  ligadas e desligadas.
- A lógica do jogo (física, IA, carreira, interpretador Adhoc, leitura do
  disco) é a mesma nos dois modos: um conserto vale para ambos.
- O que muda entre os modos fica na fronteira: renderer, entrada, itens
  extras de interface, carregamento de mods, padrões de configuração. O
  código do jogo consulta o modo só nesses pontos; nada de `#ifdef` nem de
  `if (enhanced)` espalhado pela lógica.
- **Mods só são aceitos no Enhanced.** No Classic o jogo usa apenas o
  conteúdo do disco.
- **Mods de GT4 existentes (decidido em 07/10/2026).** O OpenGT4 terá
  um formato oficial de mod (pacote com manifesto: nome, versão, autor,
  dependências; arquivos que substituem caminhos do VOL e conteúdo
  novo), e os mods novos são feitos nele. Para mods feitos para o PS2
  (Spec II e outros) haverá um **importador só de assets**: carros,
  pistas e sons, lidos dos arquivos que o próprio usuário tem e
  convertidos para o formato oficial; o projeto não distribui nada
  deles. O que esses mods mudam no código do jogo (câmeras, Event
  Synthesizer, randomizer, motor de som) não é importado: o usuário
  faz essas mecânicas direto no Enhanced, como recursos nativos. O
  Spec II parte do GT4 Online Public Beta NTSC (SCUS-97436), outro
  executável; seus assets precisam ser lidos ou traduzidos para o
  v2.00 (SCUS-97328) que o port usa.
- Na corrida, a cena (o que desenhar: pista, carro, rodas, luz da pista,
  `src/platform/sdl/race_view.c`) é uma só; o Classic a desenha com as
  regras e o aspecto do PS2, o Enhanced com um renderer moderno.
- A fidelidade (`docs/FIDELITY.md`) é medida **no modo Classic**.

### Como o Classic imita o PS2 (decidido em 02/10/2026)

O renderizador é um só e não é piorado nem desfeito para o Classic. O
aspecto antigo vem de **filtros aplicados por cima** da imagem pronta:

- resolução interna de 640x448 (o renderizador já desenha numa resolução
  interna e amplia para a janela);
- pontilhado de cor (dithering) como filtro;
- entrelaçamento **simulado como efeito visual**, sem reproduzir o
  mecanismo de vídeo do PS2.

Cada filtro é uma opção. O Enhanced é a mesma imagem sem esses filtros,
com as melhorias ligadas. Os parâmetros dos filtros (matriz do
pontilhado, como os campos são misturados) devem ser tirados do que o
jogo programa no chip gráfico, não escolhidos a gosto. Quedas de quadro e
tempo de carregamento do PS2 não são imitados.

No registro de fidelidade, um filtro é imitação: vale no máximo C
(confere, com tolerância declarada contra uma referência), nunca E.

### Tela larga (16:9)

O jogo original já tem um modo de tela larga: uma flag (D_006184F0) que
desloca os elementos do HUD em 96 unidades (`func_003AFD18`, bits 4 e 8 do
alinhamento) e corrige a proporção do que é desenhado. Portanto 16:9 é
**renderizado de verdade**, não esticado, e deve seguir esse caminho do
próprio jogo, não uma adaptação inventada. Por existir no original, vale
também no Classic e entra no registro de fidelidade. Proporções que o
original não tem (21:9 e outras) são do Enhanced.

### Sistemas com versão nova

Quando um sistema ganhar uma versão melhorada para o Enhanced (o primeiro
candidato é a IA dos rivais):

- a versão original fica **intacta e provada**, num módulo separado; a
  nova fica ao lado, não é uma modificação da antiga;
- o Classic usa sempre a original;
- no Enhanced a troca é uma opção visível ("original / nova"); o padrão é
  decisão do usuário;
- replays, ghosts e recordes guardam qual versão estava ativa, porque são
  re-simulados;
- a física do carro não entra nessa regra: é uma só nos dois modos.

## Visual livre, jogabilidade igual

- Melhorias visuais e de interface podem ser diferentes à vontade.
- A física e as regras de corrida são as do original nas duas edições
  (validadas bit a bit, `docs/PHYSICS_STATUS.md`). Assim tempos, ghosts,
  replays e saves valem nos dois modos. Uma mudança de jogabilidade no
  Enhanced, se houver, é uma opção separada e marcada, nunca o padrão
  silencioso.
- Saves e configurações valem nos dois modos. Um save alterado por mods
  fica marcado, para não se misturar com o do Classic.

### Melhorias visuais anotadas para o Enhanced

Coisas que o Classic reproduz como o PS2 faz e que podem ganhar uma versão
melhor depois, como opção ligada por padrão no Enhanced:

- **Sombra dos carros na corrida** (anotado em 02/10/2026). No original a
  base do volume projetado tem borda reta e, somada à mancha de contato,
  forma um bloco quase preto de contorno duro embaixo do carro, que parece
  um objeto (`docs/RENDERER.md`, "Car shadows in the race"). No Enhanced:
  borda suavizada (penumbra) e opacidade menos chapada. O Classic fica como
  está.

## Multiplayer competitivo (futuro, Enhanced)

Ideia de longo prazo: corridas online competitivas, no estilo do GT7, sem
que alguém consiga levar vantagem alterando o jogo.

- O servidor não confia no cliente. Ele roda a mesma física sem gráficos
  (o modo headless que já existe) e confere cada corrida pelos inputs do
  jogador: um tempo que o replay não reproduz é recusado.
- Dois saves: o da carreira offline fica no PC (livre para mods); o do
  multiplayer (garagem, créditos, carros online) fica só no servidor, e
  compras, prêmios e ajustes são pedidos que o servidor valida.
- No online só entra conteúdo verificado (hash dos dados de carro e pista).
- Para o servidor reproduzir uma corrida, a física tem de dar o mesmo
  resultado em qualquer CPU, sistema e compilador. Isso pede testes de
  determinismo em outras plataformas e opções de compilação que não
  reordenem contas de ponto flutuante.
- Risco a pesar na hora: servidores online dão ao projeto muito mais
  visibilidade do que um port offline em que cada um usa o próprio disco.

## O que vale desde já

- A física continua determinística e dirigida só pelos inputs do
  controle; nada de estado escondido que dependa de tempo real ou do
  quadro de vídeo.
- O modo headless da física continua funcionando e testado: é ele que
  um dia vira o juiz no servidor.
- Código de apresentação (render, áudio, entrada) fica fora da lógica do
  jogo, para cada modo usar o seu.
