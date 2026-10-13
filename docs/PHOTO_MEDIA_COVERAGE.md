# Cobertura de filmes e replay PS2 — 12/10/2026

| Família | Estado | Prova |
|---|---|---|
| Arcade/Single/Demo | Base/GT4M, uma fita | adaptador C, comandos E |
| FreeRun/FreePractice | uma fita | adaptador C |
| GTmode/Championship | RACE key/row e setup | adaptador C |
| License/Training/Mission | sessão e RACE key | adaptador C, comandos das demonstrações E |
| MachineTest | world mode15 | adaptador C, sessão integral A |
| SplitBattle | dois recorders/checkpoints independentes | adaptador C, trajetória A |
| Melhor volta PS2 | FileReplayLicense (001CFE20/001D0360) | F: importar como ghost não implementado; formato PC não prova PS2 |
| LAN/NetBattle/NetRally/NetSinglePlayer | produtores 00338548/003D5F30 | F: conductor de rede ausente; não converter para Arcade |
| RaceBasic/SinglePlayer/Solitaire/TrainingBase | classes internas/base | produtor de save público não confirmado |
| PhotoMode/PhotoDevelop | mundo de foto | FILM/RHDR/SFRM suportado; não prova GT4M universal |

00346BB8/00346CA0 aceita count0..3. replay_types_test produz 48 arquivos,
compara índices/checkpoints, duas fitas, eventos independentes do nome,
CRC/truncamento e negativo de steps. disc_replay_oracle compara 93 arquivos
reais, 556738 comandos de 32 bytes; negativo altera steering. Não compara
trajetória integral dos doze adaptadores.

FILM: header, PCAM, CPRM v0..3, esqueleto DRVR, interpolação de vinte ossos,
registros SMOK/PTCL, ângulo e limite da roda têm E delimitado no oracle.
DRVR v1 lê grasp; bloco inteiro/dedos não comparados ao original.
CDTL wheel/wing e dimensões dos quatro pneus preparam todos os carros.
Modelo de corrida carrega piloto; pose/dedos aplicados. SMOK/PTCL restauram
pools sem simular. film_develop retira cada detalhe separadamente e confirma
mudança de pixels em Tsukuba/Brooklyn: integração C, imagem A.

Pendências: body/LOD CDTL, recorte por plano SMOK, partículas entre snapshots,
GDGT, develop ambiental, far clip, retrato e tiles GS. Não há filmes pessoais
ou captura PS2 de referência. Fontes preservadas, classes recusadas skipped.
Logs locais build/photo-pause-audit/*final* e film-isolated.log. Nenhum asset,
captura ou desmonte entra no Git.
