# Classic: recursos funcionais ainda ausentes

Auditoria local de 12/10/2026, depois da integração do Photo Drive.
F identifica ausência funcional; A identifica implementação com comparação
incompleta. E/C certificam somente o item delimitado na FIDELITY.

## Estado atual dos recursos auditados

| Recurso | Nota | Evidência e limite |
| --- | --- | --- |
| Power & Speed: 0–400 m, 0–1000 m, velocidade máxima | A | quick-mt e sessão15 ligados ao GT Mode. gtmode_machine_test completa as três distâncias/velocidade com comandos nativos, conserva tempos e máxima; negativo troca mundo e falha. Las Vegas e apresentação integral ainda sem prova ponta a ponta. |
| Ambiente do Photo Travel | E | photo_ambient: todos os cinco grupos CRSS nos 15 cenários são vazios; 120 saídas originais de 00457F60 têm volume zero e pitch1. Negativo altera um volume e falha. A ausência de stream CRSS não é recurso faltando; mixagem/música de menus conservam sua nota própria. |
| Persistir seleção musical personalizada do slideshow | C | album_music_bind/store mantém o blob0x810 por perfil. outside_menu confere escolha/ordem/shuffle, reabertura e captura antiga; negativo corrompe byte de forma garantida. playlist_cache conserva ambos bgm/slide após duas capturas obsoletas, com negativo. Backend PC P; som/composição A. |
| Passcode do ranking | A | Algoritmo0044B258 E:1.024 códigos32dígitos contra EE. course_ranking prova geração/partes/save-load; negativo falha. Caller A por bit de pilha não inicializado escolhido0; relógio do host P. Não há chave do jogo embutida. |
| Volante com force feedback | A | SDL WHEEL, comando4 e publicação do byte original00353EE8. wheel_input confere256 forças, pedais/steer/hats, stop e desconexão, negativo falha. Driver SDL P; eixos/calibração e ensaio G29 pendentes. Sem alteração das fórmulas da física. |
| Importar replays/fotos e revelar filmes no formato PS2 | A | Import Media aceita arquivo/diretório/PSU/card FAT com/sem ECC; fonte preservada. Filme aparece em Photo Lab/Film; commandDevelop abre PhotoSave PreviewRoot, reconstrói a cena e exporta PNG. photo_film_format: 1.024 blocos originais E, leitura/câmera/CPRM/cor. film_menu e film_develop: integração nativa C, com negativos. Filme real, snapshots de piloto/rodas/ambiente e imagem PS2 ainda sem comparação; apresentação A. JPEG e replay têm provas específicas em ps2_media_test. |
| Controles/apresentação completos de PhotoPause | A | racephoto.strb original ligado ao Photo Drive; photo_pause_movie confere getters, ajuda/aspecto e exportação PhotoSave, negativo falha. Câmeras predefinidas não enumeradas e composição/controle original integral pendentes; editor ainda usa ShootRoot. |

## Implementado com prova ou apresentação pendente

- Photo Travel/Drive: lente, filtros, exposição e borrão temporal ligados;
  coeficientes/tempos E, compensação de foco C, imagem A. Câmera editada,
  partículas/objetos/box e composição GS integral pendentes.
- Licenças, missões, B-Spec, carreira/endurance, ajustes, GT Auto e box têm
  implementações/provas específicas. Consultar os ciclos/pixels pendentes
  na FIDELITY; antigos F não significam ausência de uma modalidade inteira.
- Demos automáticas de corrida, Replay Theater, slideshow, remapeamento,
  trilhas A/B do Logger, espelho/replay/garagem 2P, Free Run e Practice
  estão ligados. Não reutilizar listas antigas sem conferir o código atual.
- Importação de progresso GT4 PS2 existe. Adaptadores de herança US GT3
  estão em menu_legacy_import.inc; comparação com save real ainda pendente.
  inheritGT4pData tem mapeamento regional; não presumir Prologue em NTSC-U.

## Escopo e diferenças deliberadas

PNG, arquivos/perfis locais e atalhos de teclado são P. LAN, online,
EyeToy e impressão estão N no escopo Classic atual (FIDELITY 4.4/4.5).
Não são implementados; não são bloqueadores da carreira local prevista.
Empacotamento/instalador ficou para depois.

Esta lista vem dos scripts/entradas/adaptadores atuais; não certifica
cada ramo de UI ou periférico do original. Fidelidade integral de render,
UI e mixagem continua pendente mesmo nos recursos já disponíveis.
