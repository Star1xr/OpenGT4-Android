# GT4: segredos, eventos e requisitos de fidelidade

Pesquisa realizada em 01/10/2026 no código do OpenGT4, nos scripts do disco do usuário e na internet. Referência principal: GT4 retail NTSC-U, SCUS-97328; não misturar Prologue, Online Public Beta, Spec II ou outros mods com a versão original. O disco US também contém tabelas regionais EU, JP e KR; sua presença não certifica o comportamento completo dessas edições.

Este é um inventário verificável do material examinado, não uma garantia de que nenhum segredo ainda desconhecido existe. Código presente, caminho acessível e comportamento fiel são três verificações diferentes. Não foram implementados novos recursos durante esta pesquisa.

## Evidência e arquivos

- `tools/analysis/audit_gt4_secrets.py`: extrator somente de leitura de metadados de scripts locais.
- [GT4_EVENT_INVENTORY.md](GT4_EVENT_INVENTORY.md): todas as 143 famílias de eventos extraídas das tabelas de halls e fabricantes do script examinado.
- `GT4_REWARD_INVENTORY.json`, gerado localmente do disco por `tools/analysis/audit_gt4_secrets.py`: 172 registros de condições/recompensas e 25 desbloqueios por tempo do catálogo US. Inclui carros e conteúdo adicional; não significa 172 carros diferentes.
- `build/secret-audit/inventory.json`: sequências de comandos, módulos, linhas Adhoc e instruções dos manipuladores ocultos.
- `build/secret-audit/usedcar-inventory.json`: metadados dos 400 estoques semanais extraídos do disco, incluindo ocorrências dos black cars.
- Os `.adc`, modelos, texturas e outros assets continuam locais em `build/`/no disco. Os arquivos de documentação não contêm os scripts compilados.

Reprodução: executar o extrator com os `.adc` extraídos de gtmode, option e US_carlist. O modo `--used-disc "F:\dump gt4.iso"` lê `/usedcar`, extrai suas tabelas locais e produz o inventário semanal. Os caminhos de entrada usados nesta auditoria constam do JSON.

## Black cars e carros raros

**Confirmado no disco:** nos 400 arquivos de estoque examinados, os quatro protótipos aparecem apenas nos arquivos regionais `*_used0099.adc`, a semana de índice 99. A cor indicada é `1`. US usa um identificador do Toyota diferente de EU/JP/KR.

| Carro | ID US | Estoque | Posição na semana 99 | Cor |
|---|---|---|---|---|
| Mazda 787B '91 | `_787b_91` | `usedcar_01_carlist` | 48, índice zero | 1 |
| Nissan R92CP '92 | `r92cp_92` | `usedcar_01_carlist` | 49 | 1 |
| Nissan R390 GT1 '98 | `r390_race_98` | `usedcar_02_carlist` | 48 | 1 |
| Toyota GT-One '99 | `gt_one_race_exxon_99` | `usedcar_02_carlist` | 49 | 1 |

Em EU/JP/KR o Toyota é `gt_one_race_99`. Posições e quantidade de carros por estoque também variam. Preservar essas tabelas em vez de inserir quatro carros pretos por uma regra genérica.

A documentação de jogadores relata disponibilidade nos dias exibidos 694–700 e repetição a cada 700 dias: 1394–1400, 2094–2100 etc. Relata Cr. 1.224.999 para cada um dos dois carros do início dos anos 90 e Cr. 2.924.999 para cada um dos outros dois, total Cr. 8.299.996. Preços devem ser confrontados com a rotina original e a tabela do disco; não foram confirmados pelo código nativo nesta auditoria. [Relato com preços e ciclo](https://www.gtplanet.net/forum/threads/how-i-get-secret-black-cars.100725/).

**Pendência concreta:** `UsedCarRoot.onInitialize`, Adhoc 5591–5593, chama `calendar.getElapsedWeek(100)` e carrega `usedcar/<game_zone>_used%04d`. Em `src/core/gtmode_game.c`, `elapsed()` retorna `days/7` sem usar o argumento. Falta reproduzir/validar o módulo de 100 semanas. A geração de quilometragem, variante, preço, estado de compra e renovação do estoque também precisa de testes próprios. Ter `addUsedCar` funcional não certifica todo esse caminho.

Outros requisitos de usados:

- Rotação das três lojas, sete dias por semana, compras removendo apenas os itens corretos e reaparecimento no ciclo seguinte.
- Carros de baixa quilometragem, cores raras e diferenças de estoque regional. A web relata versões de 10 km; confrontar com os dados e gerador originais. [Used Car Cycle](https://gran-turismo.fandom.com/wiki/Gran_Turismo_4/Used_Car_Cycle).
- Distinguir cor preta normal de variante black de protótipo.
- Distinguir os quatro black cars compráveis do **Formula Gran Turismo preto** de 100%.
- Preservar cores dos prêmios, incluindo recompensas com uma cor forçada e aquelas que deixam seleção/aleatoriedade ao motor original.

## Códigos secretos embutidos

As cinco sequências abaixo foram extraídas do `gtmode.adc` local. O manipulador `craft`, Adhoc 1010, exige **`getElapsedDate() > 365`**, com comparação estrita. Como o dia exibido normalmente começa em 1, a primeira disponibilidade corresponde ao dia exibido 367, não 365. Validar fronteiras 364/365/366 dias decorridos. A descoberta e a discussão da contagem estão documentadas pelo pesquisador Nenkai e por pez2k. [Pesquisa original dos códigos](https://www.gtplanet.net/forum/threads/gt4-has-actual-cheat-codes.417862/).

| Tela/módulo | Resultado do manipulador | Sequência |
|---|---|---|
| `TopRoot` | +10.000.000 Cr. | Select, Left, Right, Right, Down, Up, Up, Left, Down, Up, Right, Left, Down, L1, R1, Select |
| `LicenseRoot` | Aprovar licença selecionada | Select, R1, Select, R1, Select, L2, L2, R2, R2, L1, Select, L1, Select |
| `LicenseDetailRoot` | Ouro no teste selecionado | Select, Select, R1, R2, L2, L2, Select, L1, R1, Select, R2, L1, Select |
| `MissionRoot` | +10.000.000 Cr. | Select, R2, Select, R2, L1, L2, L2, Select, L1, R1, Select, R1, Select |
| `EventCourseRoot/RaceSelect` | Ouro no item de evento selecionado | Select, L1, Up, Up, Select, R1, Down, Down, Select, L2, Select, R2, Select |

**Bug original importante:** o código da Mission Hall chama `incMoney(10000000)` e `refreshMoney`, Adhoc 8920–8921. Não conclui uma missão. Para fidelidade retail, não transformar esse código em um desbloqueio de missões.

Validar também: reset da sequência com tecla errada, escopo da tela/foco, persistência do índice, sons, atualização da UI, registros, premiações e efeitos sobre percentual. Não inferir que forçar o ouro por cheat executa todos os mesmos efeitos de vencer uma corrida real.

**Estado OpenGT4:** scripts presentes; a interface pública `menu_input`/mapeamento de teclado examinados só expõem direções, aceitar e voltar. `event_call()` monta `keysym`, mas não acrescenta `event.port`. Falta certificar encaminhamento de Select, Start, L1/L2/R1/R2 e identidade do controle. Sem isso, sequências existentes podem ser inacessíveis ao jogador.

## Menu secreto de Options

Confirmado no `option.adc`: `sSecretCommand` = **L1, Up, L2, Down, R1, Left, R2, Right, Start, Start**. O `onKeyPress` de Options rejeita o primeiro controle (`event.port == 0`). O código deve ser digitado no controle 2, e Secret/Monitor precisam continuar ocultos antes dele. O comportamento observado de exigir novamente o código ao retornar é parte da referência. [Pesquisa direta de TheAdmiester, Dezent e MMRivit](https://www.gtplanet.net/forum/threads/xenns-gt4-research-topic.251394/page-16).

Secret inclui limitação de pistas, oponentes ligados aos Favorites e fabricante/tuner específico. O script usa `special_arcade_tuner`; o script Arcade examinado verifica esse campo e omite entradas como GT Mode em configuração especial. Monitor inclui Zoom, Distance, Width, Height, Aspect, Scale e bordas. Validar ranges, aplicação, cancelamento, persistência e impacto real na câmera/projeção. Não substituir esse comportamento por um menu de debug novo sempre visível.

**Pendências:** eventos da segunda porta; montagem e persistência desses campos; consumo real no gerador de oponentes/Arcade e no renderer. Conseguir abrir `SecretRoot` ou `MonitorRoot` diretamente não prova funcionamento.

## Recompensas e progressão

Condições extraídas do catálogo US: `license`, `all_silver`, `all_gold`, `win`, `mission_1st`, `achievement`. Todas as 172 linhas estão no JSON, inclusive o segundo conteúdo/variante.

Confirmado pelas tabelas:

| Condição | Prêmio | Conteúdo adicional |
|---|---|---|
| achievement 25 | Audi Nuvolari quattro '03 | `-` |
| achievement 50 | Jaguar XJR-9 Race Car '88 | `-` |
| achievement 100 | Formula Gran Turismo '04 | cor `11`, preto |
| win `en_nur24h_0000` | Formula Gran Turismo '04 | `-`, não a cor forçada de 100% |
| win `ex_formula_1500` | Sauber Mercedes C9 '89 | `-` |

Não confundir o ID de família mostrado nos halls (por exemplo, endurance terminando em `0001`) com o ID de registro/prêmio (que pode terminar em `0000`). Resolver pela tabela original, não por substituição textual indiscriminada.

Checklist obrigatório:

- Prêmios cumulativos de bronze/aprovação, prata e ouro nas cinco licenças; requisitos do teste final.
- 34 missões, seus grupos de prêmios, carros e tempos de largada originais; não aplicar ajustes de mods.
- Coffee Breaks e sua relação com licenças, estatísticas e percentual: resolver pelos registros originais, não assumir contribuição.
- Requisitos de GT World Championship, abertura de Extreme, Endurance a 25% e término/filme de encerramento; validar cada predicado do disco e cada medalha exigida.
- Pontuação separada de campeonato e corridas individuais; vitória na série não implica ouro em cada etapa.
- Flags que impedem/repermitem prêmios, apagamento de troféus e repetição de campeonatos/eventos.
- Ordem de processamento de prêmio, disponibilidade Arcade, dinheiro, estatísticas e avanço de dia; lotação da garagem.
- Carros apenas de prêmio e elegibilidade especial por evento; não torná-los compráveis universalmente.
- Diferenças regionais: prêmios, nomes, Mission 34, moedas, listas de carros e estoque. A web diverge até sobre o que é espera versus contagem de largada da Mission 34; usar configuração da corrida do disco como autoridade.

O projeto já possui rotinas originais de créditos, calendário, registros e bits de presentes em `gtmode_game.c`/`career_*`, com validações descritas em [GT_MODE_LOGIC_VALIDATION.md](GT_MODE_LOGIC_VALIDATION.md). Isso é fundamento parcial; não comprova a cadeia completa de corrida → vitória → recompensa → percentual.

## Desbloqueio de pistas por calendário

O catálogo local contém 25 entradas `PresentTimeData`, condição `weeks`. Datas abaixo são conversão da semana para dia exibido (`7*w+1`), a validar com a UI e com flags de entrega. A sequência de semanas foi extraída do disco, não copiada de guia.

| Semana decorrida | Dia exibido esperado | ID da pista |
|---|---|---|
| 2 | 15 | deepforest |
| 4 | 29 | paris_tarmac |
| 6 | 43 | fisco_old |
| 8 | 57 | route5 |
| 10 | 71 | suzuka |
| 12 | 85 | motegi_east |
| 14 | 99 | grandvalley |
| 16 | 113 | hongkong |
| 18 | 127 | suzuka_west |
| 20 | 141 | new_fisco_gt |
| 22 | 155 | icerace |
| 24 | 169 | apricot |
| 26 | 183 | montecarlo |
| 28 | 197 | tahiti_maze |
| 30 | 211 | motegi |
| 32 | 225 | paris_rally |
| 34 | 239 | yosemite_dirt |
| 36 | 253 | capri |
| 38 | 267 | sarthe |
| 40 | 281 | autumn |
| 42 | 295 | infineon_nascar |
| 44 | 309 | nursnow |
| 46 | 323 | new_fisco |
| 48 | 337 | tsukuba_wet |
| 50 | 351 | sarthe_old |

Algumas listas online colocam Infineon Stock Car no dia 309; a tabela local diz semana 42, que corresponde a 295. Preservar a informação do disco. Validar pistas inicialmente abertas, Reverse, diferenças entre Arcade/GT Mode, Photo Drive e Practice. Gymkhana, pistas de lição e conteúdo residual não devem virar corridas normais só porque o asset existe.

## Inventário completo de famílias de eventos examinadas

| Grupo | Famílias extraídas |
|---|---:|
| Beginner | 8 |
| Professional | 9 |
| Extreme | 7 |
| Endurance | 16 |
| Special Conditions | 33: 11 locais × Easy/Normal/Hard |
| Japanese | 6 |
| American | 4 |
| European | 11 |
| Fabricantes | 49 |
| Total dessas tabelas | 143 |

O [apêndice de eventos](GT4_EVENT_INVENTORY.md) lista cada ID. Missões, licenças, Coffee Breaks e os modos de circuito/Power & Speed não entram nessa soma. Não usar 143 como número de corridas: um evento pode conter várias etapas ou direções. A inspeção também encontrou `CourseEvents` e `PowerAndSpeedEvents`, incluindo Free Run, Family Cup, Practice, Photo Drive, 400 m, 1000 m e velocidade máxima; a lista por pista está no inventário local.

Para cada ID, a ficha final de implementação deve obter da RACE/SpecDB e configuração original: nome/localização, licença, restrições de fabricante/modelo/ano/país/tração/pneus/potência, circuito/direção, voltas/duração, grid e oponentes, A/B-Spec, penalidade, combustível/pneus/pits, dinheiro por colocação, prêmio de campeonato, registros e flags. Nesta rodada, todas as famílias foram catalogadas; esses campos não foram todos decodificados e certificados.

## Recursos discretos que também precisam ser preservados

| Recurso | Evidência / validação necessária |
|---|---|
| Linha de guia em testes B/A por R3, cores de aceleração/freio | Confirmada no guia oficial; há exceções por tipo de teste. Validar por licença e configuração original. |
| Segurar marcha com transmissão automática | Relato técnico de jogadores para R2/L2; conferir mapeamento original, não apenas uma trava global de AT. |
| Replay acelerado por L3 e BGM desligável por Circle | Relatos de uso; confirmar no script/runtime de replay. |
| Transferência de até 100.000 Cr. de GT3 | Recurso original relatado; distinguir de importar um save completo de GT4. |
| Licenças B/A de GT4 Prologue | Relatos regionais; disponibilidade depende da edição. Não atribuir licenças a GT3 por engano. |
| Today's Special em GT Auto | Rodas de outros carros com rotação; inventariar tabela e condições por dia/semana, persistência e compatibilidade. |
| Óleo, desgaste de motor/rigidez, quilometragem | Rotinas parcialmente portadas e validadas no projeto; conferir efeito antes/depois, display e geração de usados. |
| A/B/C de acerto e peças compartilhadas | Persistência e opções por carro; presets não equivalem a comprar a mesma peça novamente. |
| A-Spec points | Recorde e cálculo próprios, diferentes de percentual e de B-Spec. |
| B-Spec | Ritmo, ultrapassagem, pits, treino do piloto, monitor e tempo até 3×; não usar multiplicador global na física. |
| Carros especiais | Restrições de corrida, Time Trial, Photo Drive e número de oponentes; usar flags reais de cada modelo/modo. |
| LAN, Live e múltiplos monitores | Manual oficial documenta papéis e configuração; requer suporte além de abrir menus. |
| Photo Travel/Photo Drive/Photo Album | Câmeras, pontos, resoluções, exposição, saves e USB. |
| Impressão Epson | Função de hardware documentada no manual US; definir política de adaptação PC sem declarar hardware PS2 reproduzido. |
| Controles por pressão e volantes | O manual/guia oficial descreve entrada analógica; fidelidade não se limita a botões digitais. |
| 480p/1080i e 16:9 conforme edição | Comportamento do renderer/output PS2; separar suporte moderno de equivalência original. |
| Atalhos de mapa, menus, replays e telas ociosas | Inventariar `onKeyPress` por módulo e preservação de contexto, não adicionar atalhos de Spec II como se fossem originais. |

Fontes complementares deste quadro: [guia oficial Piggyback, amostra](https://www.piggyback.com/en/wp-content/uploads/sites/7/2020/04/GT4_E_SamplePages.pdf), [guia de licenças com controles observados](https://www.gtplanet.net/forum/threads/gt4-licenses-gold-medal-guide.64913/), [GameFAQs: controles e segredos](https://gamefaqs.gamespot.com/ps2/561066-gran-turismo-4/cheats), [transferência observada](https://www.gtplanet.net/forum/threads/transferring-game-saves.58080/), [Today's Special](https://gran-turismo.fandom.com/wiki/Today%27s_Special), [manual US](https://www.videogamemanual.com/PS2/Gran%20Turismo%204%20%28USA%29.pdf), [manual LAN oficial de Polyphony/Sony](https://www.gran-turismo.com/local/jp/data1/pdf/gt4_lan.pdf).

## Easter eggs visuais

Estes itens dependem de modelos/texturas/animadores e visibilidade de câmera; a ausência de uma string no código não demonstra ausência do objeto.

| Item | Grau de evidência | Próxima verificação local |
|---|---|---|
| Macaco na árvore sobre Trial Mountain | Fotografias de GT4 publicadas por jogadores | Inspecionar asset e câmera no circuito normal/reverse |
| Nessie no lago de Trial Mountain | Relatos/imagens de jogadores de GT4 | Preservar billboard/modelo e posição; conferir LOD |
| Dois pescadores/barco no lago | Relatos de observação de GT4 | Asset/posição/câmera |
| Nike ONE 2022 com mensagem Morse | Pesquisa contemporânea com fotos, decodificação e intervenção do designer no tópico | Preservar textura/UV; texto aponta para o site do designer Phil Frank |
| Assinatura de produtor em licenças | Relato com lista de achados de GT4 | Inspecionar imagem da licença e legibilidade |
| Fumaça distante, árvore gigante e árvore caída | Relatos de jogadores | Tratar como detalhes de cenário até inspeção, não inventar triggers |
| Aviões e fumaça colorida | Relato de observação | Localizar pista e animação; ainda não confirmado no asset nesta auditoria |
| Pessoas, piscina e escada rolante | Relato de observação | Identificar local exato e animação; ainda não confirmado no asset |
| Parte inferior de Skyline parecida com miniatura | Rumor explicitamente não confirmado na fonte | Não incluir como requisito confirmado sem textura/modelo |

Fontes: [fotos de macaco de GT4](https://www.gtplanet.net/forum/threads/has-anyone-found-the-monkey.405478/), [Nessie e fumaça discutidos em GT4](https://www.gtplanet.net/forum/threads/the-big-tree.116284/), [observações diretas no lago](https://www.gtplanet.net/forum/threads/non-race-content-in-gt.321237/), [pesquisa Morse contemporânea](https://www.gtplanet.net/forum/threads/morse-code-on-nike-one-2022.57094/), [lista de achados e dúvidas dos jogadores](https://www.gtplanet.net/forum/threads/gt4-have-i-found-a-new-easter-egg.343492/).

Não importar o gato de Photo Travel, cachorro de Bern, foguete de Daytona, OVNI de Bathurst ou outros achados de GT5/6/7 sem evidência específica de GT4. Muitos resultados da busca misturam jogos.

## Conteúdo oculto versus conteúdo não utilizado

Separar três classes: (1) acessível no retail com condição real; (2) comportamento de debug deixado no retail, como Secret/Monitor e craft; (3) assets ou código sem caminho normal. Apenas a existência de um circuito, carro, fabricante ou modo residual não justifica disponibilizá-lo ao jogador.

A pesquisa de Xenn e o OpenAdhoc são referências de engenharia reversa, mas comparar versão/origem antes de usar: [pesquisa de conteúdo não utilizado](https://www.gtplanet.net/forum/threads/xenns-gt4-research-topic.251394/), [OpenAdhoc](https://github.com/Nenkai/OpenAdhoc). TCRF foi localizada por busca e referenciada pelo descobridor dos cheats, porém sua página não pôde ser lida diretamente nesta sessão; não foi tratada como prova de conteúdo que não conseguimos examinar.

Preservar bugs originais observáveis quando o objetivo for equivalência retail. Se for oferecido um modo corrigido, documentá-lo separadamente. Exemplos que merecem investigação incluem cheat de Mission Hall, peculiaridades de cálculo/prêmio e diferenças regionais. Exploits físicos, wheelies, wall riding e glitches de Arcade precisam de casos reprodutíveis e versão identificada; não foram certificados nesta rodada.

## Prioridade de implementação e testes

1. **Calendário/estoque:** `getElapsedWeek(100)`, carregamento dinâmico de cada tabela regional, fronteiras 693/694/700/701 exibidas e repetição 1394; cor, preço, odômetro e remoção após compra.
2. **Entradas ocultas:** todos os botões originais, segunda porta, evento cru com `port`, estado da sequência e dia 367; executar exatamente os manipuladores do disco.
3. **Progressão/prêmios:** todas as 172 condições, 25 desbloqueios temporais, três marcos de percentual, flags, garagem cheia e recarregamento de save.
4. **Eventos:** conectar as 143 famílias aos registros e configurações reais, depois validar cada etapa, restrição, duração, pagamento e prêmio. Acrescentar 34 missões, 80 testes e Coffee Breaks ao inventário detalhado.
5. **Recursos especiais:** rodas rotativas, carros especiais, B-Spec, transferências e LAN/multimonitor.
6. **Cenários:** imagens comparáveis de cada easter egg, textura, LOD, posição, animação e câmera.

Limitação atual: esta pesquisa fornece cobertura das tabelas examinadas e uma lista de achados documentados. A declaração “100% fiel” ainda exige comparação com o original de gameplay, SpecDB/configurações, hardware e assets. Nenhuma dessas funcionalidades deve ser marcada completa só porque existe uma tela ou função com o nome correto.
