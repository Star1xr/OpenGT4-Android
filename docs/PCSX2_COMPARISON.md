# Comparação com o PCSX2

Lista do que ainda precisa ser comparado com o jogo original no PCSX2, como
fazer cada comparação e como ela vira automática. A nota V do
`docs/FIDELITY.md` continua sendo do usuário: a automação só prepara o lado a
lado e a medida; quem aprova é você.

## 1. A ideia: o savestate é a referência

Um savestate do PCSX2 (`.p2s`) é um zip que já guarda tudo que a comparação
precisa, sem passo manual extra:

| Arquivo dentro do `.p2s` | Para que serve aqui |
|---|---|
| `Screenshot.png` (640x480) | a imagem de referência daquele quadro |
| `eeMemory.bin` (32 MB) | o estado do jogo: corrida, carros, câmera, HUD, menus, save |
| `GS.bin` | a memória de vídeo (texturas, framebuffer) |
| `SPU2.bin` | o estado do som (vozes, reverb) |

O OpenGT4 já sabe desenhar uma corrida a partir de uma memória capturada
(`OPENGT4_RACE_SNAPSHOT`, `gt4_race_run_restore`). Então, para tudo que é
corrida, **um savestate vira um teste automático**: tirar a memória, desenhar
o mesmo quadro no nosso motor e comparar com o `Screenshot.png` do próprio
savestate.

Você só precisa fazer o savestate no momento certo. O resto a ferramenta faz.

## 2. Configuração do PCSX2 (uma vez)

Para as imagens serem comparáveis com as nossas:

- Gráficos: resolução interna **nativa (1x)** durante as capturas. Hoje está
  em `upscale_multiplier = 3`. O `Screenshot.png` sai em 640x480 de qualquer
  jeito, mas com 3x o serrilhado e o filtro mudam e a comparação fica injusta.
- Proporção **4:3**; sem patch de widescreen; sem "no-interlace" patch.
- Desentrelaçamento: deixe como está (`deinterlace_mode = 0`, automático) e
  não mude entre capturas.
- Filtro de textura: "Bilinear (PS2)", o padrão. Sem filtragem anisotrópica,
  sem hacks de upscaling.
- PINE ligado na porta 28011 (já está: `EnablePINE = true`).
- Para o som: Ferramentas > Captura de vídeo (com áudio), 60 fps, quando o
  item pedir áudio ou animação.

Salve cada referência com nome, em vez de usar os slots numerados: menu
Sistema > Salvar estado > Para arquivo, em
`captures/ref/<id-do-caso>.p2s` (por exemplo `captures/ref/hud-tsukuba-reta.p2s`).
A pasta `captures/` é local e nunca vai para o git.

## 3. Os seis tipos de comparação

| Tipo | O que compara | Referência | Automação |
|---|---|---|---|
| **A. Quadro da corrida** | imagem 3D e HUD num instante | savestate com `Screenshot.png` | total: nossa memória restaurada do mesmo savestate, desenho, diferença por pixel/SSIM |
| **B. Estado em números** | posições, tempos, voltas, câmera, valores do HUD | dois savestates (início e fim) de uma sequência determinística | total: rodamos do primeiro até o quadro do segundo e comparamos a memória campo a campo |
| **C. Tela 2D (menus)** | layout, textos, ícones, cores | savestate parado na tela | quase total: chegamos à mesma página com `OPENGT4_ACTIVATE`/`--input`, captura, diferença com máscara das partes animadas |
| **D. Animação e tempo** | transições, fades, contagem, câmeras em movimento | captura de vídeo 60 fps | semi: alinhar pelo primeiro quadro que muda e comparar quadro a quadro |
| **E. Som** | músicas, motores, efeitos, vozes | áudio da captura de vídeo | semi: onsets/duração/espectro medidos; a nota final é de ouvido |
| **F. Fluxo e comportamento** | o que acontece ao apertar X, ordem de telas, saves | você jogando os dois | manual, com roteiro curto por item |

O tipo B é o mais forte para física, IA e tempos: as **demonstrações do disco**
(Replay Theater e licenças) rodam com os mesmos comandos nos dois, então um
savestate no começo e outro alguns segundos depois bastam para conferir tudo
número a número.

## 4. A lista, por tipo

Cada linha cita o item do `docs/FIDELITY.md` (nome igual ao de lá) e o que
capturar. **Prioridade**: 1 = aparece toda hora para quem joga; 2 = aparece
em modos específicos; 3 = detalhe.

### A. Quadro da corrida (savestate no meio da corrida)

Capture em Tsukuba e numa pista grande (Nürburgring ou Laguna Seca), com
carro de rua e de corrida. Para cada câmera, um savestate.

| Pri | Itens do FIDELITY | Capturar |
|---|---|---|
| 1 | Câmera de trás, Câmera do teto, Câmera de para-choque, Projeção das câmeras | um savestate por câmera, numa reta e numa curva |
| 1 | HUD: posição, volta, tempos, cronômetro; painel simples; painel de bordo; mini mapa; passo fixo dos dígitos; unidade de velocidade | reta com HUD completo, em km/h e mph |
| 1 | Pista: cor; céu e fundo; conjuntos que não são da vista; brilho/reflexo no asfalto; névoa; Texturas: filtro | uma vista ampla por pista (dia), uma com névoa (Nürburgring, Grand Canyon) |
| 1 | Carros: sombra; luzes da pista; reflexo da pista; cor do reflexo; pintura; UVs; aros dos modelos de corrida; freios atrás dos aros; descarte de faces; luz de freio; retângulos do Ka | carro na câmera de replay/TV, parado e freando |
| 1 | Corrida: linhas de tempo do display; HUD: imagem do carro ao lado da diferença | logo após passar um setor atrás de outro carro |
| 2 | Pilotos nos carros; fechamento dos dedos; freios, rodas, conversíveis | conversível em câmera de TV perto |
| 2 | Rodas: borrão por acumulação; fumaça dos pneus; folhas levantadas; chama do backfire | derrapando na terra/grama, em alta rotação |
| 2 | Pistas: camadas de brilho (tipo 4) | pista à noite ou com sol baixo (Côte d'Azur, Tokyo R246) |
| 2 | Box: equipe, ferramentas, iluminação e sombras; Box: animação da equipe | parado no box durante o pit stop |
| 2 | Mensagens START/FINISH, do sinal e de penalidade; Largada: textos A-spec, logo, 3-2-1 | um savestate por mensagem |
| 2 | B-Spec: Race Monitor e vista ao vivo; Menu de box A-Spec/B-Spec | corrida em B-Spec, monitor aberto; menu de box aberto |
| 2 | Battle2P: imagem dos espelhos; corrida local, câmeras e apresentação | 2 jogadores, os dois carros na tela |
| 3 | Licenças: imagem dos objetos de pista; carro na câmera antes da contagem; luz dos fiscais | B-1 e um teste com cones |

### B. Estado em números (sequências determinísticas)

| Pri | Itens | Capturar |
|---|---|---|
| 1 | Replay: reprodução; Replay Theater: demos do disco | uma demo do Replay Theater: savestate no 1º quadro e outro 10 s depois |
| 1 | Licenças: auditoria das fitas; encerramento da Demonstration | demonstração da B-1: savestate no início e no fim |
| 1 | Time Trial: largada lançada; corrida do TT; carro fantasma | TT com fantasma, savestate na largada e após uma volta |
| 2 | Largada: câmera alta/baixa, campo de visão, HUD aos 4,5 s; Largada lançada: câmeras | savestates em 0 s, 2 s, 4,5 s da largada (memória dá câmera e tempo) |
| 2 | Carreira: grid pelo disco, entrada do carro equipado, Sunday Cup jogável | grid da Sunday Cup antes do 3-2-1 |
| 2 | Treino livre / Free Run e Practice | Practice em Tsukuba: savestate no box e outro 30 s depois |
| 2 | Endurance: run automática do GT40 | B-Spec numa endurance, dois savestates distantes |
| 3 | Missões: resultado e saída; Missões de pilotagem | uma missão: início e fim |

### C. Telas 2D (savestate parado na tela)

| Pri | Itens | Capturar |
|---|---|---|
| 1 | Menus do Arcade; Showroom; Home: garagem, Status | cada página principal |
| 1 | Resultados: linhas, texto, ícones; Replay: abertura | tela de resultado de uma corrida de 6 carros |
| 1 | Pré-race: projeção, retângulos, pista ao fundo; Time Trial: catálogo superior, Session Record | pré-corrida do Arcade e do TT |
| 1 | Pausa, reiniciar, sair | menu de pausa aberto |
| 2 | Tune Shop (janela de compra, todas as categorias); GT Auto (textos, luz, câmera, rodas, asa); Setting | uma página por loja |
| 2 | GT Mode: ficha de compra; black cars; usados; Mapa: aviso de tuning | concessionária e usados (semana 99 se possível) |
| 2 | Licenças: menu com demonstração, HUD e resultado, tempos no menu | escola de licença, resultado de aprovação e de reprovação |
| 2 | Logger; Jukebox; Photo Travel; Photo Develop; álbum e slideshow | cada tela |
| 2 | Regulamentos: divisórias do popup; Campeonato: tela Setting | popup de regulamento da Sunday Cup |
| 3 | Vídeos do menu: faixa de cor do IPU; Menus em 16:9 | quadro do vídeo do Arcade; menu com 16:9 ligado |

### D. Animação e tempo (captura de vídeo)

| Pri | Itens | Capturar |
|---|---|---|
| 1 | Carregamento: apresentação completa; Menus: startPage com várias páginas | do menu até a corrida começar |
| 1 | Largada completa (câmeras, textos, 3-2-1, START) | da tela preta até 5 s depois do GO |
| 2 | Resultados: liberar o botão de pular; Replay: fim e lista | da linha de chegada até o replay |
| 2 | GT Auto: animações e mecânico; Box: animação da equipe | um serviço inteiro e um pit stop inteiro |
| 2 | Licenças: fiscais e câmeras do resultado; troféu 3D | resultado de licença e de campeonato |
| 3 | Tela inicial: corrida automática e retorno | deixar o menu parado até a demo e voltar |

### E. Som (áudio da captura de vídeo)

| Pri | Itens | Capturar |
|---|---|---|
| 1 | Áudio: música da corrida; Pré-race: duas músicas PCM; Menus: música ao trocar de projeto, início após loading | 30 s de cada tela |
| 1 | Áudio: contagem da largada; START e som da largada | a largada inteira |
| 2 | Áudio: voz SPU2, reverb, alocação de vozes, Doppler, derrapagem | volta com derrapagem e passagem por outro carro |
| 2 | Áudio: box (wheelgun, macaco, abastecimento); GT Auto: sons dos serviços | um pit stop; um serviço |
| 3 | Corrida: sons na pausa; Resultados: som de confirmação; Photo Travel: obturador | cada um |

### F. Fluxo e comportamento (você jogando)

| Pri | Itens | Roteiro |
|---|---|---|
| 1 | Controle: gatilhos, analógico, olhar para trás; teclas entregues aos scripts | mesmas teclas nos dois, conferir resposta |
| 1 | Arcade: volta do menu rápido para a escolha do carro; Pausa/reiniciar/sair | fazer o caminho nos dois |
| 2 | Tune Shop: compra e instalação; GT Auto: compras de rodas/asas; Tuning: relações após câmbio; Setting: presets | comprar e conferir saldo/efeito |
| 2 | Opções gravadas ao fechar; Home: ver/trocar/vender carro | fechar e abrir de novo |
| 2 | Jukebox; Free Run e Practice; Photo Drive | usar cada um do começo ao fim |
| 3 | Black cars: aquisição e save; usados: limpeza e recarga | depois do calendário dos usados ficar pronto |

## 5. Como fazer agora, sem a ferramenta pronta

1. Rode o PCSX2 com a configuração da seção 2.
2. Para cada linha que for capturar, salve o estado em
   `captures/ref/<id>.p2s` (nome curto, sem espaço).
3. Me passe a lista de ids que você salvou. Eu extraio as imagens e a
   memória, rodo o nosso lado e mando o lado a lado com a diferença.
4. Você dá a nota (V ou não) e eu registro no `docs/FIDELITY.md`.

Dica para render mais: numa mesma corrida dá para salvar vários estados em
sequência (câmeras, HUD, sombra, fumaça); cada savestate leva um segundo.

## 6. Plano de automação

**Fase 1: savestate para teste (tipos A e C).**
`tools/compare/` com:
- `cases.yaml`: um caso por linha da seção 4: id, item do FIDELITY, tipo,
  arquivo de referência, como rodar o nosso lado (variáveis/argumentos),
  máscara de regiões animadas, limite de diferença.
- `p2s.py`: tira `Screenshot.png` e `eeMemory.bin` do savestate.
- `run.py`: para cada caso com referência, roda o OpenGT4 (corrida:
  `OPENGT4_RACE_SNAPSHOT=<eeMemory>`; menus: o caminho de `OPENGT4_ACTIVATE`),
  compara (`tests/ui/png_diff.py`, SSIM com numpy) e gera um relatório HTML
  local com referência, nosso quadro, diferença e nota.
- Os casos que passarem viram teste do `ctest` com a referência local, como
  os testes com o disco já fazem (ficam de fora quando o arquivo não existe).

Prova da ideia (12/10/2026), slot 03 do usuário (Ka em Laguna Seca, câmera
de trás): `eeMemory.bin` do `.p2s` em `OPENGT4_RACE_SNAPSHOT`, `--drive
--race-course laguna --window 640x480`, quadro lado a lado com o
`Screenshot.png`. Pista, céu, fundo e carro saem no mesmo lugar. Falta para
a ferramenta: escolher a câmera pela memória capturada (hoje é
`OPENGT4_RACE_VIEW` à mão: 0 deu a vista sem o carro, 1 a de trás, 2 o
céu), desenhar o HUD nesse modo congelado (hoje não aparece) e tirar a pista
do argumento, lendo-a da memória. Já aparece uma diferença real: a nossa
câmera de trás está mais alta e mais longe que a do PCSX2.

**Fase 2: números (tipo B).**
Sequência determinística: restaurar a memória do primeiro savestate no nosso
motor sem janela, rodar até o mesmo relógio da corrida do segundo, comparar
os campos (carros +0x5B0.., posição, velocidade, câmera, relógio). O PINE
(`tools/pcsx2_capture.py`) também carrega savestates pelo número do slot e
lê memória com o PCSX2 rodando; dá para pedir vários pontos de uma vez sem
você salvar à mão.

**Fase 3: vídeo e som (tipos D e E).**
Gravação de quadros do nosso lado (`--shot` em série) e do PCSX2 (captura de
vídeo). Alinhamento pelo primeiro quadro diferente, tabela de tempos de cada
transição; para o som, início e duração de cada evento e espectro médio.
A nota continua sendo de ouvido.

**Fase 4: navegação no PCSX2 sem você.**
Mandar teclas para a janela do PCSX2 (o teclado já está ligado ao controle)
para chegar às telas e salvar o estado pelo atalho. Só vale depois que as
fases 1 e 2 estiverem rodando, porque o tempo de cada tela varia.

## 7. Ordem sugerida

1. Uma sessão de capturas de corrida (tipo A, prioridade 1): umas 20
   savestates em Tsukuba e Nürburgring cobrem câmeras, HUD, pista e carros.
2. As demos do Replay Theater e da B-1 (tipo B): 2 savestates cada.
3. As telas principais (tipo C, prioridade 1).
4. Vídeo da largada e do carregamento (tipo D) com áudio (tipo E).
5. O restante por prioridade.
