# Missões de pilotagem (GT Mode)

Estado: fluxo de resultado/retorno implementado (11/10/2026).
Regras isoladas E, integração C; apresentação integral PS2 ainda A.

Feito:
- Preparação pelo disco (`gt4_career_mission_prepare`, em
  `career_license_disc.c`): 34 missões (`usl0m0001..0034`), capacidade 6
  limitada por RACE +0x3E, carro do jogador = entrada 0 da lista RACES.
- Sessão `00685DA0` em `session.c`: a do treino com as entradas 0x08
  (`race_gaps`, 00359238), 0xA0 (licença + car +0x718 zerado), 0xC0
  (evento de chegada uma vez por carro, máscara cond +0xCBE3), 0x110
  (00357800), 0x160..0x180 (constantes), 0x190 (penalidade comum
  0035FCC8). Oráculo `gtmode_mission_session`: 6144 casos idênticos ao
  original, memória inteira; controle negativo falha.
- Mundo RaceMission no `race_run` (vtable 006815E0, modo 10; tipo
  005FB728 sobre RaceTrainingBase, não Arcade) e as entradas como no treino.
  `gtmode_mission_disc`: as 34 montam e avançam sem o original.
- `menu`/`main`: `exit_menu("mission")` vira pedido de corrida; o `main`
  prepara, abre `quick-mission:MissionRoot` (pré-corrida conferida na
  tela) e corre a missão com o HUD comum. `race_game` separa licença
  (1, 2) de missão (3).
- Resultado (0011C328/0011C2E0) em `career_original.c` e `gtmode_game.c`:
  prêmio acumulado, registro campo 3 e A-spec, evento de calendário
  método 15; o script paga o prêmio (onPrizeCheck/incMoney).

- Resultado/retorno: `race_mission.c` lê a fila da sessão sem alterá-la.
  Evento 5 do jogador abre o resultado; evento 10 encerra sem esse painel,
  com prioridade sobre o evento 5 no mesmo frame. Oráculo
  `gtmode_mission_flow`: 4096 casos contra 003E05A0/00389638, incluindo
  rivais terminando antes, prioridade e permissão de saída imediata.
  Negativo troca uma decisão e falha. E apenas para essas decisões;
  desenho e alocação são fronteiras explícitas.
- Encerramento: 003ED188 -> 0033CCF0 arma world+D80; 0033C490
  encerra no tick seguinte, antes de avançar a corrida. `race_mission.c`
  agora preserva esse intervalo e o último estado físico. Oráculo
  `gtmode_mission_exit`: 2048 casos normais/especiais/demo, clock de fade
  inalterado e nenhum passo adicional; negativo encerra cedo e falha.
  E para timing/decisão; os serviços de cleanup e scheduler são fronteiras,
  sua ordem original é rastreada, não uma prova de áudio/render completos.
  Correção da nota anterior: esse caminho usa duração zero, não um fade
  ausente. O executável da missão 10 pede saída e para no frame 9150;
  `mission_app.py --negative-step` soma um passo no log e falha.
- O original não tem uma classe ResultMission: 003BF6E8 +0x80..0xCC
  constrói ResultArcade em world+0x22778. 003DF898 -> 003EC2B8 carrega
  `/menu/US/result.strb`. A ponte agora escolhe esse arquivo para missões,
  avança pelo clock 003DF978 +0xB0 e permite sair sem esperar todos os
  rivais (003DFE20 +0x28). Arcade conserva seu caminho existente.
- `main.c`: retorno para QuickWork/quick-mission como em 0011BB60;
  o replay só começa por comando explícito, não automaticamente.
- `gtmode_mission_cycle`: missão 9 vencida em primeiro, 1280 frames de
  controle nativo; prêmio 1000, PrizeRoot -> AfterRoot, recorde e dinheiro
  persistidos após save/reload. Missão 10 perde em quarto, sem prêmio nem
  painel sobre a corrida, retornando a AfterRoot. Nenhum resultado ou
  estado físico foi injetado; política do piloto de teste é P e fornece
  somente comandos de controle (`mission_pad_fixture.h`). Integração C.
- Textos: a quebra multiline existente no renderer é exercitada durante
  o desenho, nas 34 descrições do Hall e nas 34 do pré-corrida; largura
  máxima é a do widget do disco, tolerância 0,001 unidade. Controle
  negativo excede a largura e falha. Não prova pixels/posição integral.
- `mission_app.py`: executável real, missão 9 -> PrizeRoot, missão 10 ->
  AfterRoot, com fita de controle gerada pela física nativa. Capturas e
  perfis isolados em build/ui-audit/mission-app; negativo exige página
  inexistente e falha. C para esse caminho, não para todas as missões.
- `gtmode_mission_catalog`: 136 ciclos de resultado controlado no limite
  corrida/menu (abortar, perder, vencer, perder após a vitória), nas 34
  missões. Scripts originais PrizeRoot/AfterRoot, Cr. 272500 pagos,
  melhores registros conservados e save/reload em cada ciclo, tolerância
  zero. O teste usa o botão original para acelerar o contador de dinheiro
  (`prize_action.ad` L27..35, `quick-mission.ad` L562). Negativo muda a
  classificação esperada e falha. C na integração; resultados forçados
  são explícitos, não provam 34 vitórias físicas nem pixels PS2.

Pendente:
- Comparar integralmente composição, câmeras, áudio e animação do resultado
  com o PCSX2; apresentação continua A. Usuário não consegue fornecer os
  estados de resultado/falha agora (11/10). Não ajustar a apresentação no
  olho: FIDELITY regra 5. A ausência de fade na saída por evento 10 é
  comprovada no código, não uma pendência visual a inventar.
- Vencer e repetir o ciclo completo nas outras 33 missões. Todas as 34
  montam e a sessão tem oráculo E, mas isso não prova cada corrida inteira.

Atalhos de teste:
`OPENGT4_MISSION=l0m0001 opengt4 <iso> gtmode MissionRoot` pede a missão
como se a checagem de licença tivesse passado. `OPENGT4_MISSION_TEST_PAD`
lê uma fita de comandos de 32 bytes/frame; ausente, usa controle normal.
Não disponibilizar esses overrides como regra de carreira.

```powershell
ctest --test-dir build -C Release -R gtmode_mission_ --output-on-failure
.venv/Scripts/python.exe tests/gtmode/mission_app.py build/Release/opengt4.exe <iso>
.venv/Scripts/python.exe tests/gtmode/mission_app.py build/Release/opengt4.exe <iso> --case fail
.venv/Scripts/python.exe tests/gtmode/mission_app.py build/Release/opengt4.exe <iso> --verify-log build/ui-audit/mission-app/win/app.log --negative
```

## Como o jogo abre uma missão

- `gtmode.ad` MissionRoot.onActivate: confere garagem e licença
  (`need_license`), grava `game.race_code = "l0m%04d"` com o `idx` do item e
  chama `exit_menu("mission")`. Mesmo caminho da licença.
- `func_0011BB60`: o laço da missão (o da licença é `0011CB38`).
  - `0011BEC0` prepara a descrição da corrida (chama `003BB440`, `00447400`,
    `00447458`, `003D6300`).
  - `005C1498(0x4F8)` cria o QuickWork; `00127E60`/`001242B8` o ligam.
  - `003B7850` monta a corrida; `005C1498(0x22C80)` + `003BF6E8` constroem
    o mundo **RaceMission** (nome em `003BF670`, vtable do mundo
    `006815E0`, base `003ECD40`, não a da licença `003E9BF0`).
  - O mundo grava em +0x10140 a sessão de vtable **`00685DA0`**: classe
    própria, métodos em `003F5FA8..003F7150` (cerca de 1.000 instruções).
  - `0011C138` e `0011BFE8` ligam mundo e QuickWork.
  - `0011BF60` abre o projeto **`quick-mission`** (MissionRoot, AfterRoot,
    PrizeRoot) e roda a corrida.
  - Comandos do menu pela tabela `jtbl_0068DA70` (QuickWork +0).
  - Resultado: estado do mundo +0xCE0 == 4; carro +0x5B6 (terminou);
    `0011C328` (sucesso), `0011C1D8`, `0011C2E0` (registro e prêmio).
  - Saída: `001103E0`, `003ECB00`.

## Ordem de trabalho

1. Preparar as entradas da missão pelo disco (linhas RACE `l0m0001..`,
   lista RACES), como `career_license_disc.c` faz para as licenças.
2. Sessão `00685DA0` em C, comparada método a método contra o original
   (oráculo do recompilador), como as outras sessões de `session.c`.
3. Mundo RaceMission e montagem em `race_run` (tipo de largada, posições).
4. Ponte do `quick-mission` no `main.c`/`menu.c`: pré, corrida, AfterRoot,
   PrizeRoot, repetir e sair.
5. Resultado, registro (`isMissionClear`), prêmio em dinheiro/carro e save.
6. Teste de ponta a ponta pelo exe, como `tests/gtmode/license_app.py`.
