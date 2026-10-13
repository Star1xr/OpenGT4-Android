# Inventario das telas de corrida do disco

Conferido no NTSC-U v2.00: **10 projetos quick, 10 paginas principais de pre-corrida, 3 paginas adicionais de pos-corrida e 3 paginas de premio**. Sao **13 layouts distintos de menus pre/pos**, porque sete projetos reutilizam sua pagina principal apos a corrida. Somando premios, sao 16. Os layouts completos tem **29 RootWindow**, incluindo rankings, guia e popups; 29 nao e a quantidade de pre/pos-race.

Essa contagem e por projeto + pagina. Nomes repetidos em projetos diferentes contam separadamente. Estados e modos que compartilham a mesma pagina nao criam outro layout. GuidePage e uma introducao de licenca, contada a parte das dez paginas principais.

## Paginas e estados por modo

| Projeto / modo | Pre-corrida principal | Pos-corrida | Premio | Outras paginas |
|---|---|---|---|---|
| quick-arcade / corrida Arcade | ArcadeRoot | ArcadeRoot, canReplay | -- | QuickTunePopup |
| quick-tt / Time Trial Arcade | TimeTrialRoot | TimeTrialRoot, canReplay/canSaveGhost | -- | QuickTunePopup, RankingRoot, PasscodePopup |
| quick-event / evento GT Mode | RaceEventRoot | AfterRoot | PrizeRoot | -- |
| quick-championship / campeonato | ChampionshipRoot | AfterRoot | PrizeRoot, premio da corrida ou GrandPrize | -- |
| quick-mission / missoes | MissionRoot | AfterRoot | PrizeRoot | -- |
| quick-freerun / Free Run | FreeRunRoot | FreeRunRoot, canReplay/canSaveGhost | -- | RankingRoot, PasscodePopup |
| quick-practice / treino | PracticeRoot | PracticeRoot, canReplay | -- | -- |
| quick-mt / teste de maquina | QuickRoot | QuickRoot, canReplay e recorde de tempo/velocidade | -- | RankingRoot |
| quick-license / licenca | LicenseRoot | LicenseRoot, canReplay/canSaveGhost/canLoadGhost | -- | RankingRoot, PasscodePopup |
| quick / outra apresentacao de licenca | LicenseRoot | LicenseRoot, opcoes ficam insensitive conforme os flags | -- | RecordsRoot, StoreHouse, GuidePage, PasscodePopup |

Fontes do disco: todos os RootWindow em /projects/<projeto>/<projeto>.mproject; scripts compilados originais, executados pelo interpretador no teste. **A existencia dessas telas nao comprova que todos os seus modos estejam implementados no port.**

## Quem escolhe o projeto no original

As chamadas a 0010FE40 abaixo carregam o projeto; o endereco da string tambem fica registrado para distinguir as duas licencas e Photo Drive.

| Funcao original | Instrucao que passa o projeto | String / projeto |
|---|---|---|
| 001119A0 | 001119F4 | 0068D3B8 quick-arcade |
| 00112DC0 | 00112E28 | 0068D4A8 quick-tt |
| 001141C8 | 00114210 | 0068D558 quick-event |
| 00115290 | 001152D4 | 0068D608 quick-license |
| 00115FE8 | 00116030 | 0068D6B8 quick-mt |
| 00117668 | 001176C8 | 0068D7D0 quick-freerun |
| 00118670 | 001186B4 | 0068D8A8 quick-practice |
| 00119890 | 001198D4 | 0068D968 quick-championship |
| 0011B248 / Photo Drive | 0011B290 | 0068DA28 quick-freerun |
| 0011BF60 | 0011BFA8 | 0068DAC8 quick-mission |
| 0011D1B8 | 0011D224 | 0068DBB8 quick-license |
| 0011D270 | 0011D2E0 | 0068DBC8 quick |

Photo Drive reutiliza quick-freerun: nao acrescentar um projeto inventado. A-spec/B-spec mudam estado/commands; o icone Spectator depende de IsBSpec, nao de ser pre ou pos. Quick-practice e a apresentacao de treino; qualificacao/campeonato pode emitir COMMAND_FREE_QUALIFY no quick-championship.

## Condicoes de pre/pos e premios

- quick-arcade.ad L73..80: canReplay=0 esconde Replay/SaveReplay; depois mostra. L84..90: Spectator depende de IsBSpec. Com IsBSpec=0: quatro icones antes, seis depois.
- quick-tt.ad L62..77 e L88..93: flags independentes para replay, ghost e B-spec. Ranking, Logger e LoadGhost existem na pagina original; disponibilidade do backend no port ainda e F.
- quick-event.ad getStartPage L22..39 e quick-mission.ad L22..39: IsSessionFinished=0 -> pagina principal; terminou com prize != 0 -> PrizeRoot; terminou sem premio -> AfterRoot. O premio conduz ao pos; nao usar a pagina pre como substituta.
- quick-championship.ad getStartPage L26..50: pagina principal antes; premio da corrida tem prioridade; depois GrandPrize; depois AfterRoot. O mesmo PrizeRoot distingue os premios por grand_prize_mode=0/1: quatro ramos, tres layouts.
- quick-freerun.ad L89..116: replay, ghost e B-spec condicionais. onLoad L21 entra em FreeRunRoot.
- quick-practice.ad L67..74: replay condicional.
- quick-mt.ad L99..112: replay condicional; L152..175 distingue mtmax_testcourse/velocidade maxima dos testes com tempo.
- quick-license.ad L166..190: carregar ghost, replay e salvar ghost condicionais. onLoad L17 entra em LicenseRoot.
- quick.ad getStartPage L15..20: dispGuidePage escolhe LicenseRoot ou GuidePage; L328..349 controla insensitive de replay/ghost. GuidePage nao e uma tabela de resultados.

## Resultados, dois jogadores e rede

Resultados nao sao RootWindow dos quick menus. CORE tem quatro controladores de resultado: ResultArcade (RTTI 006A3808), ResultLicense (006A3938), ResultChampionship (006A4598) e ResultLinkBattle (0069F4B0). Isso conta classes, nao quatro imagens finais: cada uma tem ramos/animacoes.

/menu/common/result.strb e a tabela de resultado usada no port; o original tambem referencia result_license.strb em 006A3908. Reproduzir os resultados das licencas e batalhas ainda e F. Nao somar essas classes aos 29 RootWindow.

Arcade.ad doGame L1387/1430/1437/1444 distingue single_race, lan_race, time_trial e split_battle. CORE contem RaceSplitBattle, RaceNetBattle, RaceNetRallyBattle e RaceNetSinglePlayer, alem das strings online_p2p_race/online_rally_race/online_bspec_race. **A contagem de variantes de pre/pos desses fluxos nativos de batalha/rede ainda precisa ser fechada pela maquina de estados; nao esta garantida pelo catalogo quick.** Attract/autodemo, demonstration e load_replay tambem nao sao novos quick menus.

## Prova e limites

pregrid_menu disco.iso confere os nomes/ordem dos 29 RootWindow, executa dez estados dos seletores originais de evento/missao/campeonato, e testa antes/depois no mesmo objeto Arcade/Time Trial. --inventory-negative, --selector-negative, --tt-negative e --negative adulteram expectativas distintas e precisam falhar.

Nota C para visibilidade/selecao dos estados exercitados; inventario estrutural verificado contra o disco. Catalogo superior do Time Trial fica A ate o oraculo de conversao/arredondamento/formatacao. Tela nativa capturada em build/time-trial-pregrid.png, ainda sem comparacao de pixels PCSX2. Ghost, ranking, logger, replay, premios e demais corridas continuam com suas notas F onde nao implementados.
