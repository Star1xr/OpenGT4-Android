# Física e corrida: estado

Atualizado em 30/09/2026.

## Mapa de arquivos

Por decisão do usuário em 06/10/2026, há uma única sessão ativa e nenhum
arquivo está reservado por área. Este mapa serve para localizar o código,
sem exigir autorização para editar física, corrida ou apresentação.

- `src/core/physics/` (todo o diretório)
- `src/core/spectables.c/.h`, `src/core/specdb.c/.h` (a parte de linhas por
  índice e strings do SpecDB que a física usa)
- `tests/physics/` (harnesses `physics_run`, `setup_run`, `opengt4_drive`,
  scripts de comparação)
- `docs/analysis/PHYSICS.md`, este arquivo
- `src/platform/sdl/race_view.c/.h` (a pista com o carro, as rodas; o
  `car_model.c/.h` é compartilhado com o showroom dos menus)
- As linhas de física no `CMakeLists.txt` (`opengt4_physics`, alvos de
  `tests/physics`).

Preserve as mudanças locais existentes. Commits incluem apenas as mudanças
da tarefa; use stage_hunk.py quando houver outros trechos locais no mesmo
arquivo. Nunca use git add -A. Nada de push sem pedido do usuário.

## Onde está

A documentação técnica completa está em
[analysis/PHYSICS.md](analysis/PHYSICS.md). Resumo:

- **O step (func_0034DB30) roda em C**: motor, transmissão, pneus,
  suspensão, colisão, paredes, piloto, auxílios, voltas, pit lane, relação
  entre carros. O que só alimenta a apresentação (som, replay, vibração,
  câmera) é pulado no modo headless.
- **Trem de força inteiro em C**, sem cair no original: todas as
  trações (FF, FR, 4x4 com centro dividido, viscoso, acoplamento, LSD e
  travável), turbo de 1 ou 2 estágios, câmbios manual, automáticos com
  conversor de torque, o híbrido com bateria e o power split (câmbio 3),
  e todos os modos de troca de marcha. Validado nos 722 carros por
  amostra de cada tipo e com peças enxertadas por `PHYS_POKE` e
  `PHYS_POKE_CAR` (como o tuning faz).
- **Do disco, em C**: parâmetros do carro (SpecDB), garagem, pista (RNW4) e
  linhas (ADLN), o init da corrida (func_0034D490) e de cada carro
  (func_00363B10).
- **Sessão Time Trial**: a maioria dos métodos em C (`session.c`).
- **Corrida contra carros (Arcade)**: a física e a IA inteiras em C. Num
  step headless o original só é chamado para o driver do controle
  (func_00426CE0, leitura; func_00426D50, estado do volante), que é a
  fronteira com a plataforma. Em C também: modelo de piloto (inclusive
  na linha do pit), catch-up da sessão (func_00365790), pit stop com
  troca de pneus (func_0035C0B8, precisa do SpecDB e de memória de
  rascunho do host: `gt4_phys_specdb`, `gt4_phys_scratch`), desgaste e
  calor dos pneus, desgaste do motor, macaco do pit, carro-ventilador,
  controle de guinada, motor dianteiro do híbrido, todos os
  diferenciais, carros de 3 rodas, vibração do controle e force feedback.
- `PHYS_BRIDGE_LOG=arquivo` no `physics_run` lista cada chamada que o C
  ainda faz ao original.
- Tudo validado bit a bit contra o código original recompilado, em
  snapshots do PCSX2 (`captures/`, local) e em variantes sorteadas.

## O que falta (nesta ordem)

1. ~~Tempo de volta da sessão TT~~: relógio, posições e melhor volta em C;
   eventos, ghost e recordes ficam no original (no headless são pulados) e
   serão portados junto com o modo de jogo.
2. ~~Sair do snapshot~~: `opengt4_drive <disco> - --car X --course Y`
   monta a corrida a partir do próprio disco (o `CORE.GT4` é descompactado
   em `tests/physics/race_build.c`; `extracted/core.elf` não é mais
   necessário), sem `a_parado.eeMemory.bin`. Toda a montagem da corrida
   Time Trial está em C (métodos 0xD0 e 0xD8 da sessão, configurações da
   IA, tabelas de direção, solver carro-contra-carro): com o controle
   pelo host (`gt4_phys_pad`), o `opengt4_drive` não chama nenhuma função
   original, nem na montagem nem nos passos. A sessão Time Trial ainda é
   montada à mão pelo `race_build` (os objetos que o menu criaria).
3. ~~IA, carro contra carro, force feedback~~ (em C, ver acima).
   O controle do jogador entra pelo host: `gt4_phys_pad` (leitura,
   estado do volante, saídas de vibração e força), com SDL no
   `opengt4_drive` (teclado como pad digital, gamepad como analógico,
   rumble).
4. Ainda no original: o layout de duas rodas (moto, herança do Tourist
   Trophy, nenhum carro do GT4 usa), linhas no formato antigo (não-ADLN,
   sem uso conhecido nos dados), e o que só alimenta a apresentação:
   eventos e mensagens da corrida, replay, som do motor (func_003F71C8),
   câmera (func_0035F658), efeitos de fumaça e marcas (func_00395C70/98).

## Passagem de bastão (30/09/2026)

Para quem continuar a física. Estado: física, IA e montagem da corrida
Time Trial em C; o `opengt4_drive` monta e dirige sem chamar o original.

**Como trabalhar:**
- Compilar: `sh tests/physics/compare.sh build` (MSBuild direto nos
  projetos da física; alterações locais no CMakeLists podem impedir
  a regeneração).
- Antes de mexer: `sh tests/physics/compare.sh old` (guarda um
  `pr_old.exe` do HEAD para comparar).
- Conferir contra o original:
  `sh tests/physics/compare.sh arcade 1500` (as seis capturas do Arcade,
  todos os carros e a RAM; aceita `PHYS_POKE_CARS=...` etc. para forçar
  casos raros) e `sh tests/physics/compare.sh built 15 tsukuba 300`
  (corrida montada do disco, RAM inteira). O resultado tem de ser
  `cars SAME ram words differ 0`.
- Casos que nenhuma captura tem: `PHYS_AB=<caso>:<n>` no `physics_run`
  (estados sorteados, original contra C; os casos estão em `ab_test` no
  `tests/physics/step_run.c`: finish, orders, model, event, drvset,
  tyre, frame...).
- O que ainda vai ao original: `PHYS_BRIDGE_LOG=<arquivo>`.
- Regra de sempre: portar lendo o assembly (`splat/asm/core/text.s`),
  comparar bit a bit; o que nenhuma captura exercita não é portado "no
  olho": ou se força por poke/A-B, ou fica caindo no original.

**Próximos passos, nesta ordem:**
1. Terminar a sessão da corrida Arcade (tabela 0x00679CE8, `race()` em
   `session.c`): largada, fim de corrida e resultado; os eventos que só
   alimentam o HUD ficam como fronteira da apresentação. Dá para fazer
   com as capturas `ar_*` que já existem.
2. Levantar pelo executável as tabelas de sessão do GT Mode (campeonato,
   endurance, licença, missão, B-spec): o que cada uma chama, o que já
   está em C por ser compartilhado, o que falta.
3. Pedir ao usuário capturas do PCSX2 no meio de corridas do GT Mode
   (campeonato, endurance com pit stop, licença, B-spec), extraídas como
   as `ar_*`, e portar/validar essas sessões com elas.
4. Classes do "mundo": `world_is_arcade` (`session.c`) só conhece
   RaceArcadeSingle e RaceFreeRun; listar as outras classes (os nomes
   estão no executável) e completar.
5. Porta de entrada: hoje o `tests/physics/race_build.c` monta à mão os
   objetos que o menu criaria (sessão Time Trial, corrida, mundo,
   perfis, garagem) em endereços fixos de captura. Para o jogo nativo, a
   física precisa expor "comece uma corrida com estes parâmetros" e o
   menu chamar isso pela interface da corrida.
6. Ligar no `opengt4.exe` (`main.c`, plataforma): o controle já tem a interface (`gt4_phys_pad`); a
   plataforma daria leitura, rumble e força do volante (G29 ainda não é
   lido como volante).
7. Apresentação ainda original (fica para quando áudio/vídeo chegarem):
   som do motor, câmera, eventos e HUD, replay, fumaça e marcas.

## Armadilhas já conhecidas

- O repositório usa finais de linha LF. O Python do Windows grava CRLF em
  modo texto: use `newline=''`. Antes de commitar, compare
  `git diff --cached --stat` com `--stat -w`.
- O MSVC troca `x < 0 ? -x : x` por fabs mesmo com `/fp:strict`, e isso
  perde o sinal de um zero. Onde o original usa branch + neg.s, escreva
  `if (x < 0) x = -x;`; onde usa abs.s, `fabsf`.
- Heredoc no Git Bash transforma `\n` dentro de strings C em quebra de
  linha real. Para C, use as ferramentas de edição.
- `rc 127` sem saída ao rodar exe pelo Git Bash era um estouro de pilha
  (0xC0000409) na árvore de colisão, corrigido em a3bb5ed. Se voltar a
  aparecer, é bug, não o shell.

## Endurance: duas consultas de integração (09/10/2026)

Autorização do usuário limitada a integração comprovada, sem fórmulas ou
parâmetros. `race_run.c` aceita dois retornos antes de rejeitar callbacks
não portados: 0035AF10 +0..4 retorna zero; 0038B980 +0..C lê o inteiro em
objeto+0xE408 no caminho dos boxes. Nenhum writeback ou cálculo do solver.

E no escopo do retorno inteiro: `gtmode_endurance_queries CORE` compara
8192 chamadas com as duas funções originais recompiladas, preserva a
palavra consultada e prova que chamada desconhecida ainda falha.
GT4_ENDURANCE_QUERY_NEGATIVE muda um bit e reprova. Detalhes dos ciclos
físicos e fronteiras de input em docs/CAREER_CYCLE.md; essas provas C não
declaram trajetória de endurance idêntica à do PS2.

## Continuação do GT40 capturado (09/10/2026)

E restrito a0034DB30: endurance_stall_step.py compara 600 steps, seis
carros e 15347136 bytes com o original recompilado, sem diferença. Partida
é a RAM nativa do frame2700000; não prova a trajetória anterior. Câmera
excluída pelo gate original D_006208B0=1 (0035F658 +0..14), caminho nativo
headless existente. Negativo corrompe posição no frame1 e reprova.

Render mostra frente na barreira de pneus; odômetro sem progresso nos
dois caminhos, combustível ainda positivo e AI acelerando. Run inteira
continua A, não foi corrigida. Nenhum arquivo, fórmula ou parâmetro da
física mudou nesta continuação. Inscrição de conversíveis corrigida em
menu.c com getter original, fora do solver. Provas de carreira e suas
fronteiras de input/peças em docs/CAREER_CYCLE.md.


## GT40: trajetória conferida e vitórias completas (09/10/2026)

Nenhum arquivo de src/core/physics, fórmula ou parâmetro mudou. Todas as
144 séries/527 etapas físicas e 144 carros-prêmio passaram C; as 16 endurance
incluem três de 24h, 25.935.205 frames/489 pits/16 carros-prêmio. São perfis
independentes, peças originais concedidas por API e comandos explícitos de
teste; não é prova de carreira única ou estratégia completa de B-Spec.

GT40 E nesta janela: checkpoint nativo frame 3.600, 2.460.000 frames/seis
carros, original recompilado versus step nativo versus gt4_race_run_frame.
SHA256 de cada estado completo sem diferença: 62.818.585.536 bytes brutos
por caminho, 88.560.036 bytes compactos por caminho. Avanço 148.292,846 m,
último progresso no frame 489.142 da janela. O original também bate e para
com os mesmos inputs automáticos de teste. Primeiros 3.600 frames, câmera
e corrida integral de 24h ficam fora do escopo.

Outra tentativa, checkpoint frame 2.440.800: 15.000 frames e 383.065.536 bytes
brutos exatos (6.464,5 m; último avanço 5.744). Captura já presa: 600 steps.
Não são partes de uma única corrida. Gate original D_006208B0=1 exclui câmera
0035F658 +0..14; pad/pit são entradas iguais. Corrupção de posição reprova.
Saída compacta Windows CNG confere com hashlib de 15.001 estados brutos e
negativo; somente saída muda, não EE/inputs. Estados dos seis carros, não
toda a EE RAM. Logs locais trajectory-*.log; reprodução em CAREER_CYCLE.md.

Seleção de controlador 00362E00 (256 valores) e cinco comandos de ritmo
(2.000 eventos) E contra memória original, com negativos. Isso confere a
fronteira usada pelas fixtures, não todo o modo B-Spec. Recibos finais:
build/career-finish/receipts-144.json; build Release e oito testes de
consultas/controlador aprovados. Fórmulas e parâmetros preservados.
