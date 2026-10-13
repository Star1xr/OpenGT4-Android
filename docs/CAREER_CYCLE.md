# Carreira: corrida, retorno e persistência

Conferido em 09/10/2026. Não declara o GT Mode completo. Nenhuma fórmula
ou parâmetro da física foi alterado. A continuação abaixo acrescenta só
retornos virtuais e o predicado de inscrição de conversíveis, comparados ao original.

## Correções de integração

- Primeira compra: GetCarRoot (`gtmode.ad` L6736..6737) copia o carro
  anterior mesmo com garagem vazia. O adaptador não tenta carregar um
  modelo com label vazio depois dessa cópia; 0013C148/0013BE08 distinguem
  esse caminho do carro válido de 00145970.
- Quilometragem: 00114044..00114078 e 0011975C..00119780 somam os metros
  A/B ao perfil antes de retornar ao GT Mode. A ponte entre as duas VMs
  captura primeiro o pagamento, soma uma vez e atualiza o snapshot do quick
  menu. Sair, repetir e avançar etapa não podem perder ou duplicar a soma.
- Repetir recebe os 32 bytes de desgaste atualizados antes de construir
  outra sessão, conforme 00113DC8/00113EC8. O solver permanece intacto.
- Odômetro/preço de venda leem o desgaste vivo do corpo (+0x498 na
  serialização), conforme 00146528 +1C..24 / 003434C8 e 00441128/004411F0;
  a quilometragem compacta da compra pode estar antiga.
- Páginas em cache reinicializam com o perfil recebido. TopRoot L1152
  verifica presentes por data; HomeRoot atualiza os dados do carro.
- GetCourseDialog.onLoad L15577..15580 cria os atores usados em
  open_motion L15585..15592, antes do pushPage em L15593. O carregamento
  nativo prepara esse recurso com a primeira página. A fase integral de
  carregamento original ainda é A; não foi ajustada visualmente.

## Provas

C, integração sem tolerância nos campos comparados:
`gtmode_sunday_playable ISO --career-cycle` começa com 10000 créditos,
compra o Lancer usado por 9582 pelo estoque/script original e corre as
cinco provas da Sunday Cup. Continua na primeira até o primeiro
desbloqueio por data: 13 corridas físicas completas, sem chegada/posição
injetadas, incluindo uma vitória. Entrada do jogador: takeover original
003454A8 modo 5, fronteira explícita de teste.

PrizeRoot original paga cada resultado. Confere corpo/desgaste, dinheiro,
contadores, calendário, metros A/B, Home, save/load e reabertura da VM a
cada prova: saldo4218, metros46864/0 e Deep Forest liberada após a 13ª.
Uma segunda chamada ao checkpoint não duplica metros.
GT4_CAREER_MILEAGE_NEGATIVE omite a primeira gravação e reprova.

`gtmode_course_dialog ISO` abre, confirma, fecha a janela autoral e retorna
à Home sem erros. Fixture tem carro; importação
GT3/GT4p, render e timing são fronteiras. GT4_COURSE_DIALOG_NEGATIVE remove
o ator necessário e reprova. Não compara pixels ou animação inteira PS2.

E, rotina isolada: `gtmode_event_wear_oracle ISO extracted/core.bin`
compara 4096 gravações de 0010FA58 e conversões A/B, incluindo corpo
completo e setups preservados. Extensão C confere odômetro/preço de venda
contra dados vivos, com cache antigo de propósito. GT4_EVENT_WEAR_NEGATIVE
e GT4_LIVE_MILEAGE_NEGATIVE reprovam.

Regressões: gtmode_championship_cycle passa 34 séries/200 etapas com
resultados injetados; gtmode_award_delivery confere entrega única do
carro-prêmio/persistência, também com vitórias injetadas. Não transferir
essas provas para um campeonato inteiro dirigido.

`tests/ui/run_sunday_entry.py ISO --save-dir DIRETORIO --start` usa cópia
do perfil do ciclo e chega ao Start no executável, sem alterar o save
de origem. Logs/capturas em build/career-cycle e
build/ui-audit/sunday-entry, fora do Git.

Verificação final: 9/9 CTest focados passam, com controles negativos.
Home entra em cache antes da tentativa seguinte e reinicializa no retorno.
Admissão (709 provas/722 carros/8192 combinações de licença), resultado
(56 casos), soma A/B (4096 casos), garagem e as regressões de campeonato/
prêmio também passam. Controles de compra incorreta e refresh prematuro
reprovam. Release atualizado; 42 arquivos da física idênticos à baseline.

## Pendências A/F

Ciclo inteiro contra PS2, todas as 527 etapas dirigidas com vitórias,
todos os desbloqueios numa carreira contínua e rotas reais de
repetição/próxima etapa no executável. As provas de integração novas
abaixo não certificam trajetórias ou apresentação integral.
Metadados/apresentação têm aproximações. Importação GT3/GT4p continua
ausente; B-Spec completo não é provado por este teste A-spec.

## Séries, bônus e endurance (continuação em 09/10/2026)

`gtmode_series_awards ISO` percorre todos os registros `win` de
PresentData: 144 séries, 527 etapas, 33 campeonatos e 16 endurance.
Resultados são injetados neste teste. O retorno passa por PresentRoot
original, testa bloqueio antes de completar a série, entrega automática,
corpo completo do carro-prêmio, contadores, calendário, save/load e
impedimento de segunda entrega. Confirma cada prêmio antes da próxima
série e confere o pedido do filme final; decodificação do filme é fronteira.
Nota C, tolerância zero nos campos verificados. O negativo substitui as
vitórias da primeira série por segundos lugares e precisa reprovar.

Achei e corrigi um bloqueio do bônus de campeonato: o adaptador ignorava
`startPage` para a página que já estava aberta. `0022F6A0 +104..184`
finaliza as páginas antigas e inicializa a solicitada mesmo nesse caso;
quick-championship.ad L1044..1045 reabre PrizeRoot para pagar o bônus.
`gtmode_championship_bonus ISO` confere prêmio 10000, bônus 20000,
AfterRoot, consumo único e save/load. Resultado é fixture. C; negativo
altera um bit do valor recebido na segunda abertura e reprova.

`gtmode_series_playable ISO --physical SERIE CARRO 1` dirige as provas
com a IA nativa e exige carro-prêmio após o resultado físico:

- Sunday Cup, Corvette Z06 '00: cinco vitórias, A112 Abarth '79 entregue.
- Tuning Car Grand Prix, Minolta Toyota 88C-V '89: cinco vitórias,
  pagamento 50000 mais bônus 20000 e Stream Z entregue.

Não injeta chegada, posição ou vitória. Confere valor pago pelos scripts,
desgaste, corpo completo do prêmio e save/load. C, tolerância zero nos
campos comparados. O negativo omite o writeback do resultado e reprova.
Fronteiras: carro inicial construído na fixture, apresentação sem janela,
takeover original003454A8 modo 1 após a largada e pedidos de pit abaixo de
10 L pelo evento003F19E0 caso6. Esse limiar é estratégia de entrada do
teste, não regra original de IA/B-Spec nem parâmetro da física.

Endurance encontraram dois retornos virtuais ausentes em `race_run.c`:
0035AF10 +0..4 retorna zero; 0038B980 +0..C lê objeto+E408. Só esses
retornos foram mapeados, por autorização explícita. Não calculam nem
alteram física. `gtmode_endurance_queries extracted/core.bin` executa as
duas funções originais recompiladas em 8192 casos, compara retorno
inteiro e preservação da palavra lida e exige rejeição de outra chamada
não portada. E nesse escopo; negativo altera o retorno e reprova.

`gtmode_endurance_catalog ISO` descobre os 16 eventos pelo próprio
PresentData, usa um carro original do grid como fixture legal e compara
limites do catálogo com sessão nativa: oito por tempo, oito por voltas.
Executa 600 frames de cada largada. C; negativo altera minutos e reprova.

`gtmode_series_playable ISO --endurance SERIE CARRO 1` termina a prova
pela sessão nativa, exige o limite de tempo original quando aplicável e
confere pagamento, desgaste e save/load. Não exige vitória; não se deve
confundir conclusão física de endurance com carro-prêmio conquistado.
Infineon 2h45 com Minolta passou: 602380 frames, sexto lugar,12000
créditos e três pit stops. C; negativo muda o limite observado e reprova.

`tests/gtmode/run_endurance_suite.py ISO --jobs 4` passou os 16 eventos
inteiros: limites originais, boxes, chegada física, pagamento, desgaste
e save/load. C, tolerância zero nos campos comparados. Os carros são
fixtures stock legais; os oito limites por tempo somam 99h45 de simulação,
além das oito provas por voltas. Le Mans antiga usa Minolta '89 por
escolha explícita de fixture: 5202529 frames, sexto lugar, 47 pits e 80000.
Logs por prova e results.json ficam em build/career-series/endurance-suite,
fora do Git. --reuse-passed reúne logs aprovados da mesma build/disco sem
repetir corridas; --car RACE=CAR permite outra fixture, ainda sujeita à
admissão autoral. O negativo remove um registro do catálogo e reprova.

A: a run alternativa com GT40 stock não terminou: parou na volta 155.
O render da RAM do frame2700000 mostra a frente presa na barreira de pneus,
fora do asfalto. Ainda há combustível e o modelo acelera, sem pit service.
A trajetória anterior e sua causa integral continuam sem prova.

E no escopo da continuação do step: `endurance_stall_step.py physics_run
RAM OUT` compara 600 chamadas de0034DB30 com o original recompilado: seis
carros, 601 estados de0x10a0 bytes por carro, 15347136 bytes sem diferença.
O odômetro continua sem progresso nos dois caminhos. A fixture põe
D_006208B0=1, gate original de câmera0035F658 +0..14, evitando CreateThread
na apresentação; o nativo usa PHYS_HEADLESS existente. Nenhum HLE,
fórmula, parâmetro, estado dos carros ou arquivo da física alterado.
Negativo GT4_ENDURANCE_STALL_NEGATIVE corrompe posição no primeiro frame
e reprova. Isso não declara o GT40 corrigido ou a trajetória inteira fiel.
Estado original, render e logs ficam em build/career-continue, fora do Git.

Verificação final: 14 casos focados (incluindo sete controles negativos)
passam, com as duas injeções negativas corrigidas antes de registrar a
prova; navegação e pregrid_menu também passam. Release atualizado.
Comparados os 42 arquivos da física com a baseline: só race_run.c mudou,
nas seis linhas de integração/comentários dos dois retornos autorizados.

Estimativas novas: nenhuma em fórmulas, tempos ou prêmios. Estratégia de
input e fixtures acima são fronteiras explícitas, não comparação das
trajetórias contra PCSX2. Render/UI completos continuam A.

## Continuação: todas as séries Beginner (09/10/2026)

C: 12 séries, 45 vitórias e carros-prêmio, sem chegada/rank injetados.
Cada série usa um perfil de teste independente; não é uma carreira contínua.
Além da Sunday Cup e do Tuning Car GP já listados, os casos novos são:

| Série | Fixture legal | Peças concedidas/instaladas (tipo=stage) | Etapas | Prêmio |
|---|---|---|---|---|
| FF | srt4_03 | 14=3,20=3,8=3 | 5 | mazda6_tmv_01 |
| FR | cerbera_speed12_00 | stock | 5 | skyline_2000gtb_67 |
| 4WD | zz2_00 | stock | 5 | mtr_gmv_04 |
| MR | zonda_c12s_gmv_02 | stock | 5 | esprit_turbo_hc_87 |
| Lightweight K Car | cappuccino_95 | 14=3,20=3,8=3,25=10,26=10 | 3 | ginetta_g4_64 |
| Spider & Roadster | viper_srt10_03 | stock | 3 | prowler_02 |
| Sport Truck | silverado_sst_02 | 13=3,20=3,8=3,25=10,26=10 | 3 | silverado_sst_02 |
| Grand Canyon Easy | rsc_tmv_lm_01 | 25=11,26=11 | 2 | rs200_84 |
| Chamonix Easy | rsc_tmv_lm_01 | 25=12,26=12 | 2 | infiniti_fx45_dmv_02 |
| Ice Arena Easy | rsc_tmv_lm_01 | 25=12,26=12 | 2 | rsc_tmv_01 |

Fixtures não contornam getViolatedRegulatoin. GT4_SERIES_PARTS é entrada
explícita do teste: posse concedida, getParts/changeParts/setupPartsBuffer/
endSetting executados e getEquipedParts conferido. Não testa compra nem
seus pré-requisitos. Valores de tipos/stages vêm do catálogo CARS do disco.
Antes de dirigir, o bloco equipado completo é comparado à sessão; na
volta, pagamento autoral, desgaste, prêmio0x4b8 e save/load são conferidos.
Tolerância zero nesses campos; trajetória/imagem integral permanecem A.

Exemplo: GT4_SERIES_PARTS=14=3,20=3,8=3, seguido de
`gtmode_series_playable ISO --physical am_ff_0000 srt4_03 1`.
CTest registra positivos e negativos de cada nova série. O negativo omite
o resultado físico antes do pagamento e reprova; não fabrica outra chegada.
Runs stock FF/SRT4, FR/Corvette, MR/Saleen, K Car e Sport Truck que perderam
etapas ficaram nos logs; não foram contadas como provas de prêmio.

E: corrigido isOpenCar no binder do carro. O script regulation.ad:328
chamava a API ausente e bloqueava a inscrição de Spider & Roadster.
Registro00141B90 +0x1D14 ->0013F040 +30..64, chave da configuração ativa por
00441248/004454C0, row00448820 e bit4 do byte+0x12 por004489A0.
`gtmode_admission_oracle CORE ISO`: 722 carros x três setups, 2166 retornos
conferidos com o original, inclusive chave ativa diferente de spec_id.
Mais 2558 chaves ENEMY_CARS resolvidas por00448A58 +8..48,
4724 retornos no total. GT4_OPEN_CAR_NEGATIVE e GT4_OPEN_TUNED_NEGATIVE
alteram o retorno genérico/tuned e reprovam. Lookup/boxing são
fronteiras, sem mutação do carro. Não altera física nem parâmetros.

25 casos novos (12 positivos, 13 negativos) passaram. Release atualizado.
As 132 séries/482 etapas restantes ainda não têm vitória física certificada;
auditoria de prêmio com resultados injetados cobre o catálogo inteiro.
Vitórias/carros-prêmio de endurance continuam pendentes: tentativa Laguna
200/Minolta stock terminou em segundo; com turbo do catálogo stage4 e
R5, em terceiro. Pagamento/save passam, mas não há carro-prêmio nessas runs.


## Bateria completa de vitórias (09/10/2026, continuação)

As tentativas anteriores são histórico. **144 séries / 527 etapas físicas /
144 entregas de carro-prêmio: C**, com 43.537.320 frames e 654 pits nas
tentativas aprovadas. Todas as 16 endurance foram vencidas, incluindo as
três de 24h: 25.935.205 frames, 489 pits e 16 carros-prêmio. Não são 527
primeiros lugares: campeonatos permitem posições menores nas etapas desde
que o título seja conquistado. Eventos independentes exigem todas as vitórias.

Chegada/rank são nativos; elegibilidade, pagamento, bônus e entrega usam os
scripts originais. Cada etapa confere a configuração equipada de 0x178 bytes
e a restrição autoral. Cada série confere entrega única, corpo completo de
0x4b8 do prêmio contra o construtor original, desgaste e save reaberto, sem
tolerância nos campos. Nenhum resultado ou rank foi injetado.

Perfis independentes e carros/posse de peças são fixtures por APIs originais,
não compras auditadas nem uma carreira única contínua. Pad, escolhas de
carro/peças, takeover e pedidos de pit são política explícita do teste,
não implementação completa de B-Spec. O pad pede boxes abaixo de 10 L ou
na faixa de desgaste da sessão (00356EF0; sessão+10; wheel+AC). Auto-gears
usa 00443240 já portado. Nenhuma fórmula, parâmetro ou fonte da física mudou.

`series_fixtures.c` constrói 722 carros, instala somente peças existentes
por getParts/changeParts/setupPartsBuffer/endSetting e consulta a
getViolatedRegulatoin autoral em cada etapa. O manifesto normal confere
144 séries/527 etapas em 29.911 consultas; o de chassis/terra/neve, 29.442.
`series_victory_inputs.json` registra as quatro escolhas adicionais:
Chamonix Hard (Escudo preparado), Ice Arena Hard (Elise e Escudo em provas
independentes), Formula GT (FGT stock, seleção de controlador por rodada)
e Speedster (peças originais, incluindo suspensão semi-racing na terceira
prova e R5 dianteiro/R4 traseiro na segunda). São inputs legais, sem alterar
coeficientes nem o catálogo do jogo. O controlador original 00362E00 e os
cinco comandos de ritmo foram conferidos contra a memória original, com
negativos; isso não certifica uma estratégia completa de B-Spec.

Reprodução, a partir da raiz (ISO é o disco local):

```powershell
build/Release/gtmode_series_fixtures.exe $ISO > build/fixtures.log
build/Release/gtmode_series_chassis.exe $ISO > build/chassis.log
.venv/Scripts/python.exe tests/gtmode/run_series_suite.py $ISO --fixtures build/fixtures.log --exclude-series rh_icerace_h_0000 --exclude-series rh_nursnow_h_0000 --exclude-series ex_formula_1500 --exclude-series de_opel_speed_0000 --jobs 4 --out build/victories/base
foreach ($series in @('rh_icerace_h_0000','rh_nursnow_h_0000','ex_formula_1500','de_opel_speed_0000')) {
  .venv/Scripts/python.exe tests/gtmode/run_series_suite.py $ISO --fixtures build/chassis.log --exe gtmode_series_pace.exe --victory-inputs tests/gtmode/series_victory_inputs.json --series $series --jobs 1 --out "build/victories/$series"
}
```

`series_receipts.py ISO --run RESULTS EXE MANIFEST` (repetir --run para as
cinco identidades) exige 144 séries, 527 etapas, 16 endurance, hashes de
exe/disco/manifesto/inputs e coerência de cada log físico. Prova consolidada:
`build/career-finish/receipts-144.json`; executáveis históricos preservados
nessa pasta. Logs originais em `build/career-series/victory-*`. Perdas não
contam. GT4_SERIES_PHYSICAL_NEGATIVE omite writeback, GT4_SERIES_SUITE_NEGATIVE
remove série, GT4_SERIES_RECEIPTS_NEGATIVE corrompe rank e
GT4_SERIES_FIXTURES_NEGATIVE remove candidato: todos reprovam.

Dois erros de integração foram corrigidos: consultas de aspiração e
getAspiration TURBINE+B0 (34.656 retornos E); comparação Adhoc Integer/String
na restrição de comprimento (336 retornos E). Referências e negativos em
FIDELITY.md. O build Release e os oito testes de consultas/controlador passam.

### GT40 preso: comparação da trajetória

**E no escopo medido:** de um checkpoint nativo no frame 3.600 foram
comparados 2.460.000 frames (11h23m20s simulados), seis carros, entre código
original recompilado, step nativo e chamador real `gt4_race_run_frame`.
Zero diferenças no SHA256 de cada estado completo: 62.818.585.536 bytes
brutos por caminho, 88.560.036 bytes de saída compacta por caminho. Avanço
de 148.292,846 m; último progresso no frame 489.142 da janela. Com os mesmos
inputs automáticos de teste, o original também alcança a barreira e para.
Não é uma afirmação sobre todas as corridas ou inputs do GT4.

Uma captura de outra tentativa no frame 2.440.800 também confere 15.000
frames e 383.065.536 bytes brutos nos três caminhos (6.464,5 m; último avanço
no frame 5.744 da janela). A captura já presa confere 600 steps. São
tentativas diferentes, não uma única trajetória de 24h. Os primeiros 3.600
frames e a corrida integral ficam fora da prova; a run perdida não virou
vitória. Câmera é excluída pelo gate original D_006208B0=1 de 0035F658 +0..14.

`endurance_trajectory.py physics_run RAM OUT --production
gtmode_trajectory_native --disc ISO --digest --frames 2460000 --timeout 10800`
produz a comparação longa; RAM e logs permanecem locais. O teste negativo
altera posição no frame 1 e reprova. `trajectory_digest.py` também confere
15.001 fingerprints CNG com hashlib dos estados brutos; corrupção reprova.
CNG é só saída compacta Windows; saída bruta funciona nas outras plataformas.
Comparação cobre os estados completos dos seis carros, não toda a EE RAM,
e não altera física nem inputs. Logs: `build/career-finish/trajectory-*.log`.
