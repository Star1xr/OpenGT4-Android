# Setting nativo do GT Mode

08/10/2026. Física preservada: 42 arquivos rastreados idênticos à baseline
build/setting-native/baseline. Não incluir sujeira anterior da física.

Projeto original /projects/setting no interpretador; riding_car da garagem,
três folhas 0x178 e slot em body+0x488. onLoad/onUnload montam cache e
sincronizam o carro antes do save. Páginas internas não restauram snapshot
sobre os edits. Peças continuam sendo as possuídas; o teste concede NOS
apenas à fixture. F4 envia Triângulo (atalho PC, P): no Home abre Settings;
no Setting abre o gráfico. OK aplica o painel, Cancel descarta, Left/Right ajustam.

## Fontes

- setting.ad L28..70: entrada, saída, carro/corrida/pista.
- 00140C70 / 0043E2E8: slot; corpo serializado omite header +8.
- 0043E2F0..0043F248: freio, suspensão, LSD, marcha, NOS, VCD, lastro,
  ASM, TCS e downforce; 001490B0..0014D638: ranges de nove famílias.
- 0014A738..878: wing soma contribuição a ambos os limites de downforce.
- 00147D08 -> 00449E10: CARS; 004401C0: linhas das peças equipadas.
- 0013FF38 -> 00441350: transmissão nas três folhas ao sair.
- 00149538 / 003553A0 / 00355278: limites entre relações signed.
- 0014A2D0: auto preserva reverse, salva final e escreve forward.
- 00148B60 / 0034F3F8: pontos do motor e normalização.
- 00148940 / 0034B808: gráfico de marchas.
- 002DFA10: range também atualiza display_min/display_max.
- 0029F798: GraphFace count/pairs +C0/+C4, capacidade original 256.

## Verificação

```
cmake --build build --config Release --target opengt4 gtmode_setting_menu gtmode_setting_oracle
ctest --test-dir build -C Release -R gtmode_setting --output-on-failure
```

Oráculo usa extracted/core.bin e splat/asm/core/text.s somente em teste;
nenhum código recompilado é ligado ao aplicativo. 180936 comparações:
reads/writes/ranges e 4608 conjuntos de limites de marchas. Negativos
GT4_SETTING_ORACLE_NEGATIVE/RANGE_NEGATIVE/RATIO_NEGATIVE alteram um
byte/limite/hi[1] e falham. setting_menu testa todas as páginas, A/B/C,
input nos sliders de lastro, apply, saída/reentrada. FLOW_NEGATIVE altera
folha salva; CONTROL_NEGATIVE exige dois passos em vez de um.
setting_save faz store/load em diretório isolado da build. O comando
Triângulo de HomeRoot é testado isolado dos callbacks de exibição do Home
(carregador 3D/calendário não fazem parte desta fixture). O executável
Release abriu Settings vindo do Home na captura home-setting.png. A
inicialização visual do Home ainda registra serviços ausentes
(dayToWeekDay/getBspecTotalExperience); isso não está coberto pelo teste
do comando de entrada nem recebe E/C nesta tarefa.

## Limites

E restrito aos valores/ranges/intervalos comparados. Curvas e marchas agora
recebem C no catálogo stock: comparação independente descrita abaixo.
A imagem inteira continua A: rasterização/antialias GS, composição de todas
as páginas e sincronismo completo não possuem oráculo de desenho integral.
GraphFace recebe pares float em vez de BinaryBuffer na fronteira PC.
Nenhum valor foi calibrado por imagem. Capturas somente locais.

A corrida preparada recebe o corpo para a próxima tentativa. Pré-grid é
reconstruído; depois de terminar, resultado/replay ficam até próximo Start.
Sessão completa, RNG, composição/pixels e sincronismo ainda requerem prova.
Nenhum asset/referência é commitado; nenhum push.

## 08/10: linhas reais e entrada na corrida

BRAKECONTROLLER.dbt do disco tem 11 bytes. O espaço de 12 bytes reservado
no wrapper não é a largura exigida da linha. 00440268 lê até +0x0a;
00443ED0 copia a quantidade decodificada e preserva padding. Instaladores
agora copiam a linha recebida, validando apenas os bytes que realmente leem.
Validade do buffer também marca somente os bytes recebidos.

E: setting_defaults_oracle compara 9216 corpos/buffers contra nove funções
originais, com três folhas, linhas completas/curtas/ausentes e padding.
Negativo modifica o padding preservado de uma linha de 11 bytes.
C: gtmode_setting_race percorre os 722 carros e todas as peças presentes
no CARS: 150054 instalações, 99636 acessos a campos A/B/C, roundtrip e 2166
entradas com setup 0x178/desgaste 32 bytes completos; corpo chamador preservado.
Ownership de upgrades é concedido somente pela fixture, sem custo de loja.
Não prova a trajetória nem cada combinação simultânea de todas as peças.
dayToWeekDay do Home foi implementado; os demais serviços do Home seguem
fora do teste de entrada em Settings.

## 08/10: catálogo completo, curvas e referências de Setting

- `gtmode_setting_curve CORE ISO`: 4332 builders originais 0034B960/
  0034B8E8 e 4332 callbacks de normalização/labels 00148B60, 722 carros
  stock, folhas A/B/C e modos normalizado/expandido. Contagens/RPM exatos;
  floats C, limite absoluto 2e-5 + relativo 2e-6, maior erro 1.19209289e-7.
  Montagem inicial do record/SpecDB é fronteira compartilhada explícita;
  engine/drive builders e aritmética do callback executam instruções originais.
  Negativos alteram pico ou label e falham. Não certifica todas as curvas
  de todas as combinações possíveis de upgrades.
- Corrigidos apenas dados de exibição: pico elétrico/híbrido (0034B9EC),
  até nove marchas (0034B088), RPM inicial de cada marcha (0034B6A0..750),
  preservando o campo anterior à substituição pelo conversor do race params.
  Labels retornam picos medidos (00148EC0/00148F1C), não escalas do gráfico.
- `gtmode_setting_catalog_menu ISO`: 45414 painéis (incluindo EngineCurbePopup) autorais nas três folhas,
  36750 controles ajustáveis nos dois extremos, 722 carros stock e com
  upgrades disponíveis concedidos pela fixture. C só da integração; verifica
  ausência de erro de VM, edits pendentes até apply e isolamento das folhas.
  Negativo modifica lastro antes de apply e falha. Não é oráculo GS.
- Slots 1/2 fornecidos em 08/10: Honda BEAT Version F, corpo em EE+A20550;
  slot 2 GraphFace em EE+C7E710, vtable 0066C5F8, type B8=1,
  width BC=2, count C0=10, pares C4. Somente referências locais em
  extracted/handoff/setting-user-states-20261008, nunca no Git.
- `gtmode_setting_reference ISO eeMemory.bin`: corpo real capturado,
  10 endpoints C em 2e-5, 5 strokes, largura original/centros C em 1e-4,
  ordem convexa TL/TR/BL/BR conforme API PC, oito campos Standard `--`
  e zero widgets pulados. Retorna normal closed value mesmo quando o item
  da lista popup está bloqueado, como o clone 002C8198/002C9B68.
  Negativos deslocam vértice ou tornam visível labeli e falham.
  Prova do adaptador de desenho, não da cobertura/antialias GS.
- Renderer passou a percorrer ScaleBar, aplicar default GraphFace width=2
  e type=1/pares (0029E9D0..EA04). Binding do Composite chamado popup
  deixou de abrir o controle indevidamente. Vértices dos strokes corrigidos
  para a ordem da API; sem alocação por stroke/frame.
- Referências opcionais no CMake: OPENGT4_SETTING_REFERENCE aponta para o
  eeMemory.bin local do slot 2 e habilita teste/negativos. Capturas do app
  Honda em build/setting-completion/beat-{gears,setting-fixed}.png.
  Nenhuma edição na pasta physics; commits sem push.

Verificação final desta etapa: catálogo e entrada na corrida completos passam
com as contagens acima; 17 testes selecionados de Setting/defaults e depois
10 de UI/menus/persistência passam, incluindo negativos. O snapshot só das
alterações do commit também compila o app Release e os quatro harnesses;
8/8 comparadores/negativos passam nele, incluindo curvas nos 722 carros.
Smoke isolado: três carros/nove entradas e um carro/57 painéis. A captura
GearRoot isolada fica em build/setting-completion/staged-gears.png.
Nenhum arquivo de physics ou referência do jogo integra este commit.

## 08/10: fechamento funcional; acabamento adiado pelo usuário

Auditoria anterior usava type em type_max, mas 0044A1E0 +0..18 e a UI
usam type-1. A contagem antiga de 108561 instalações era incompleta.
Nova auditoria: 150054 instalações/99636 campos/2166 entradas. Catálogo
com popup detalhado: 45414 painéis/36750 controles, três folhas, stock e
upgrades da fixture. Não é produto cartesiano de todas as peças.
A inicialização GT Mode da fixture registra withdrawn/withdrawnGT4p
não implementados; ausência de erro de VM se restringe a Setting.

NIKE ONE tem oito marchas. Adapter aceita 0..9 como 00445780; arrays
signed usam 16 shorts como 00149538. 4608 conjuntos contra 00355278;
com getters/ranges são 180936 comparações. Relação extra escrita em
setup+110+2*i (0043E2F0), sem confundir índice 8 com seletor Final.
E: auto oracle, 20394 arrays/6417 corpos 0x4b8 idênticos, 722 carros,
todas as transmissões, A/B/C, Auto min/max/meio e override -1/0/1.
SpecDB/record inicial/VM são fronteiras; solver/bounds são originais.
Caixas fixas não recebem chamadas Auto inacessíveis. 32 limites de
estágio contra 0044A1E0; negativos RANGE/BODY/STAGE detectados.
C: stock + duas configurações simultâneas de upgrades, 12996 builders
originais e 12996 callbacks; abs 2e-5 + rel 2e-6, pior 1.19209289e-7.

EngineCurbePopup parava em GetTorqueForDisplay. Implementados método e
GetUnitOfTorque, 001AF760/001AF810 -> 00472A08/00472758, bits 40E773EA/
411CCCCC, +0.5 e inteiro signed. E: 196608 conversões/três labels contra
original, negativo detectado. Captura beat-engine.png só local.
Power/Torque ainda ignoram rotate=90 do mproject: apresentação A.
Usuário não pode fornecer referência agora; pediu focar funcionamento e
passar para outra área. Não ajustar essa transformação por aparência.

E: 3240 updates dos dois MMoveActors contra 002C1BB0, ambas as direções,
60/120/240 Hz; negativo no quadro 17. C: reference com RECOMP_TESTS
executa 002E88F8/0025B578/0025BAA0 no slot 2: seis retângulos/dez pontos,
type/width/RGBA. Geometria 1e-4/cor 1e-6; negativo detectado. Render
context gravado, GS fora. Sincronismo/cursor/loader e pixels completos A.

24/24 testes focados passam, incluindo negativos. Catálogos completos,
logs e baseline em build/setting-final, somente locais. Nenhum arquivo
de physics foi alterado nesta tarefa. Sem push.

Snapshot isolado do índice: app e cinco harnesses compilam; 16/16
testes novos/oráculos/negativos passam. 42 arquivos de physics continuam
idênticos à baseline, nenhum está no índice desta tarefa.
