# Home e perfil

Atualização de 09/10/2026. Esta etapa não altera física, IA ou parâmetros dos carros.

A Home, StatusRoot e LogRoot executam as telas, recursos e scripts originais
do disco. A inicialização da Home parava em getBspecTotalExperience e no
formato inteiro de MUtility; essas chamadas foram implementadas. CarModel
também existe com garagem vazia, como o script exige.

## Dados e prova E

`gtmode_home_oracle extracted/core.bin [ISO]` compara 4096 casos com as
instruções originais. O executável de teste contém o oráculo; o aplicativo
usa apenas C nativo e constantes lidas do CORE.GT4.

- B-Spec: 00467C58, 00467D90, 00467F60, 004680D8 e 00468B58. Matrizes
  0x180 bytes, pesos D_00623E40, limites e truncamentos originais. Inclui
  zero, saturação e entradas aleatórias. Compara quatro retornos e getters.
- Contadores: 0043B018 soma as sete posições com wrap de 32 bits;
  00163EF0 +0x50..0x60 seleciona a posição.
- Distância: 001640D0/00164268 +0x60..0x78, 0057F480/0057F4F8,
  00472C28 e 0057F360. Conversão assinada, mantissa truncada para valores
  grandes, fatores km/milhas e truncamento final; compara ambas as unidades.
- Histórico: 0042F060 +0x30..0x70; hoje e futuro retornam nil, dias anteriores
  dentro da janela retornam o registro. Inclui primeiro índice variável e
  volta do anel de 2922 dias.
- Pistas: 0042EA38/0042EAE8, lista ordenada de 128 chaves COURSE (namespace
  0x22, 00447C78 +0x14), inserção em lista vazia/cheia, duplicatas e override
  +0x2408. Não é um bitset. O teste compara a lista inteira e os retornos.

`GT4_HOME_NEGATIVE=1` corrompe um retorno B-Spec e falha. As duas regressões
CTest gtmode_home/gtmode_home_negative passam. Não há estimativas nesses cálculos.

## Ponte e apresentação A

`gtmode_purchase_menu ISO --home-status` verifica a inicialização dos três
scripts sem erro, quatro máximos de experiência, duas unidades de distância,
vitórias/troféus/prêmios, o escape %% do texto (00494198..004941C8), as nove
schemas de getEvent com labels reais do SpecDB e serialização/reabertura.
`GT4_HOME_STATUS_NEGATIVE=1` altera a contagem conferida e falha.

MCalendar é uma classe com getEvent, como LogRoot exige. O callback segue
00133370 +0x17c..0x1850; enums de tipo/aquisição/curso vêm de D_00622D58,
D_006A5138 e D_006A51F8. Não inventa registros de corridas.

Perfis novos recebem a lista de /projects/boot/US_carlist.adc, initAvailable
do boot: 43 nomes e reversos autorizados por CourseData.has_reverse, 60 chaves
no disco suportado. Perfis importados mantêm sua lista. setCourseAvailable
permite que os scripts originais de presentes por data liberem pistas.
A importação PS2 preserva agora a experiência B-Spec (payload +0x94, 0x180
bytes, 0043B168) e o byte de override da disponibilidade (0042EB20).

Capturas locais: build/home-native/home-final.png, status.png, diary.png.
As telas usam os recursos do disco, mas ainda falta comparação independente
da lista de desenho e da imagem final com o PS2. A prova dos getters não
certifica iluminação, enquadramento ou composição 3D da garagem. Evolução
da experiência durante B-Spec pertence à implementação desse modo, não a
esta leitura do perfil. A importação dos dois campos novos foi conferida
pelo layout desmontado; ainda falta um save PS2 com experiência não zero
para conferir a imagem resultante de ponta a ponta.
