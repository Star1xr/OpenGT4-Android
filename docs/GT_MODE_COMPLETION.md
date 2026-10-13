# GT Mode: fechamento pendente

Atualização de 08/10/2026. Esta lista não declara o modo completo.
Não alterar dinâmica/fórmulas/parâmetros físicos: ordem explícita do usuário.

| Área solicitada | Estado desta tarefa |
|---|---|
| Setting/tuning | E: instaladores/ranges. C: 722 carros, 108561 instalações, 99636 campos, 2166 entradas completas; curvas/marchas stock contra original e 41070 painéis autorais. ScaleBar/GraphFace e valores fechados corrigidos com slots 1/2 do Honda. Imagem GS integral/sincronismo e todas as combinações simultâneas de upgrades continuam A. docs/SETTING.md. |
| GT Auto | Lavagem e troca de óleo funcionam com os scripts originais, múltiplos modelos/tracks, sons, cobrança e Auto Save/reabertura em FF/MR/4WD. E: valores de sujeira/óleo/setter (12288 comparações). Apresentação/mixer/sincronismo A; sombras e composição original dos mecânicos ainda pendentes. docs/GT_MODE_UI.md. |
| B-Spec/missões | Implementação completa pendente. |
| Eventos/endurance | C: compra real, 13 corridas físicas, pagamento autoral, desgaste/quilometragem, calendário, Home, reabertura do save/VM e primeiro desbloqueio por data (Deep Forest). Checkpoints e janela de desbloqueio corrigidos. Todas as séries, carro-prêmio após vitórias físicas, campeonatos/endurance completos e comparação integral PS2 continuam A/F. docs/CAREER_CYCLE.md. |
| Home/desbloqueios | E: dayToWeekDay e getters de B-Spec/contadores/distância/histórico/disponibilidade. Home/Status/Diary funcionais, seed de pistas do boot, setters por data e save. Integração/apresentação A; falta imagem/lista de desenho PS2, importação com experiência e carreira contínua para todos os desbloqueios. docs/HOME.md. |
| Licenças | Auditoria de todas as 80 fitas como comandos de jogador executada; conjunto completo continua A. Ghost/Logger UI e sincronismo completo pendentes. |
| Fotos/apresentação | Álbum/slideshow/Photo Drive/efeitos ópticos e diferenças completas de apresentação pendentes. |

## Provas novas

- `setting_defaults_oracle extracted/core.bin`: 9216 retornos/mutações
  completos de corpo e buffer, nove categorias, três folhas, linhas completas,
  curtas e ausentes. Negativo altera o padding preservado da linha de 11 bytes.
  Fronteira explícita: consulta SpecDB fornece bytes/quantidade ao original.
  Nenhum original/recompilado vai para o aplicativo.
- `gtmode_setting_race ISO`: 722 CARS reais, 108561 instalações, 99636
  campos A/B/C, save roundtrip e 2166 entradas completas setup/desgaste.
  Prova C de integração, não trajetória; ownership é dado pela fixture.
  Negativo modifica a cópia recebida.
- `gtmode_setting_curve`: 4332 builders e 4332 callbacks originais nos
  722 carros stock; C floats absoluto 2e-5 + relativo 2e-6, maior erro
  1.19209289e-7. Negativos de pico/label; record é fronteira compartilhada.
- `gtmode_setting_catalog_menu`: 41070 painéis A/B/C, 38226 controles nos
  extremos, stock/upgrades do catálogo; negativo detecta apply prematuro.
- `gtmode_setting_reference`: slots 1/2 Honda BEAT; endpoints/espessura,
  cinco traços separados, oito campos indisponíveis e closed value normal.
  Negativos de vértice e labeli. Sem dados do jogo versionados.
- `gtmode_weekday_oracle extracted/core.bin`: 65536 resultados originais,
  low32 de inteiros largos, estouro/signed remainder, argumento extra/ausente.
  Boxing/refcount são fronteiras VM explícitas. Negativo altera o retorno.
- 18 testes selecionados de Setting, menus, dia da semana e B-1 passam,
  incluindo controles negativos. Release recompilado.

## Auditoria das licenças: não confundir ouro com sincronismo

`gtmode_license_teacher ISO [código]` roda todas as fitas do disco quando
não recebe código. Opções vêm da fita (00110230 +98..110); setup vem da prova
atual, sem restaurar identificadores históricos de carros. Avaliação como
jogador: DC=0, sem alterar fórmulas nem forçar notas. Cada frame consome o
comando de 32 bytes; após EOF mantém comandos neutros como host_read.

Resultado inicial: 23 ouros, quatro pratas, dois bronzes, 45 reprovações e
seis sem conclusão no limite do teste. Só cinco ouros também tiveram tempo
exato de GT4M: B-1, B-7, B-13, IB-6 e IB-7. Quinze fitas têm race_time=0;
esse campo não serve como referência de tempo nesses casos. O limite após
EOF é somente um timeout de teste, não uma duração inventada no jogo.

Há 68 cabeçalhos RaceLicense e 12 RaceTraining. A montagem agora escolhe
RaceTraining para B/A/IB/IA 5,10,15 (0010FBD8 +20..84), lê a quantidade
ativa de RACE +3e (003BB478), usa os carros de RACES e mantém o guia visível.
A capacidade seis do construtor não é a quantidade ativa; nas 12 provas
atuais são dois participantes. Grid de 003ECC70 e métodos próprios de
00685F50 foram ligados aos solvers nativos existentes. TrainingWorld usa
00683C20; o importador de replay é 003D5F00, diferente de 003EAD58 da licença.

O usuário autorizou apenas a montagem/integração; nenhuma fórmula ou
parâmetro de dinâmica foi alterado. Arquivos necessários: race_run.c/h,
session.c e license_session.h, mais preparação do disco e exibição no app.

E, escopo limitado: gtmode_training_oracle executa o classificador original
para as 80 provas, compara construtor/clamp com os RACE reais, grid das 12
Training e 28672 consultas/controle de largada com instruções originais.
Fronteiras explícitas: chamadas de menu, construtores-base, consulta da row,
tipo de mundo e replay; não é prova de garagem, trajetória ou apresentação.
Três negativos independentes alteram classe, getter e palavra de largada.

A nova auditoria continua com 23 ouros, quatro pratas, dois bronzes, 45
reprovações e seis timeouts. A falha imediata das 12 Training foi eliminada:
B-5 agora avança 4168 frames e reprova por sair da pista; aluno em
1316,335 m e guia em 1344,462 m. IB-5 reprova por contato após 4214 frames.
O usuário forneceu o slot 2 da B-5. O guia conservava ASM/TCS/flag +3458
do construtor 003B4B38; a montagem foi corrigida. C: setups completos dos
dois carros fora dos bytes de padding sem inicialização, opções de entrada
e 128 bytes de regras conferem com a memória. No tick 8, posição do aluno
difere no máximo 0,259 mm e do guia 6,866 mm; trajetória continua A.
O modo --trajectory do teste exige 1 mm e continua falhando; não relaxar
a tolerância nem alterar dinâmica para esconder essa divergência. Ghost de licença continua pendente: a row
normal pode começar com só um participante; criar o segundo exige a
sequência original de carregamento do melhor registro, não um clone cego.

12 testes de montagem/Training/License/Setting/B-1 mais sete regressões de
menu/ciclo passam, incluindo negativos (alguns se sobrepõem). O negativo
original de quantidade foi corrigido para zero: um já era a quantidade
válida de B-1. Snapshot final somente do índice compilou o aplicativo e passou 12/12
testes de Training/captura/B-1/Setting, incluindo os negativos.
Release recompilado. Resultados/capturas/baseline somente
locais: build/gtmode-completion/.

E, escopo limitado: `gtmode_training_step` parte da mesma RAM do slot 2
e compara 64 passos de 0034DB30 com o passo nativo: dois carros,
553280 bytes completos e idênticos, sem ignorar campos. Os comandos
variam com uma seed de teste; ambas as rotas usam a fronteira do
controller original para consumir os mesmos dados sintéticos. O
negativo corrompe um byte da posição no primeiro passo e falha.
Duas regressões CTest passam. Isso separa o solver da montagem:
não elimina a diferença inicial do guia nem prova a fita completa.
Um ensaio mais longo do oráculo para no frame 93, em 0048D4C8
(qmtc2.i do caminho VU0 de áudio); não substituir esse caminho
silenciosamente nem alterar a física para mascarar divergências.

## B-5: encerramento da Demonstration no original

O usuário informou em 08/10 que a Demonstration inicia, termina logo após
a largada e retorna ao pré-race no PCSX2. A duração e a causa ainda não
foram capturadas; não impor uma volta completa ou ouro como referência.
O slot 2 disponível é uma tentativa normal: world+CC8=0 e RaceInfo+DC=0.
A entrada original de Training é 00115290 +20..28: DC=1 e
00387EF0(world,2,1); ao voltar, +68 zera DC. RaceLicense entra por
0011D1B8, que também conserva temporariamente world+F050.
Training importa a fita em world+124A0 (003D5F00 +4..1c), mas o recorder
selecionado para o relógio do Scene é world+123CC (003D5EC0 +8..34).
Não substituir esse encerramento pelo tamanho da fita sem comparação.
A fita B-5 contém 6490 comandos; isso não prova que o Scene os reproduz
todos. A auditoria DC=0 acima é um diagnóstico de tentativa, não um teste
do ciclo da Demonstration ou prova de defeito na dinâmica. O retorno da
demo continua A; aguarda memória durante a contagem, sem exigir um
savestate no fim de uma volta que o original não completa nessa demo.
