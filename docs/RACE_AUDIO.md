# Áudio da corrida

Atualizado em 02/10/2026. Notas em `docs/FIDELITY.md` (linhas "Áudio:" e
"Som da corrida"). Recursos vêm do disco do usuário; nenhum sample,
desmonte ou captura entra no repositório.

## Caminho

```
race_game.c  -- cada passo de 1/60 s --> race_audio_step
race_audio.c -- tick:
   race_car_sound.c  func_003B5E80 (quadro por carro) -> func_00392640 (pedidos)
                     func_00398B70 (ambiente: CRSS, túneis, helicóptero, reverb)
   contagem          func_0039E9B0 -> sequências do gt4count.ins
   sound_driver.c    objetos de efeito, vozes de hardware por prioridade,
                     slots ENGN, notas de sequência, registradores (0055D890)
   spu.c (callback)  46 vozes SPU2: ADPCM com laço, gaussiana, ADSR,
                     volume fixo; reverb Studio C por core; música PCM por cima
```

A câmera entra pelo `gt4_car_listener`: matriz de vista do quadro, olho
anterior e atual (Doppler), tipo de câmera (vt+0xA8: para-choque 0, atrás
2, teto 1, largada/livre 3).

## Provas

```
build/Release/race_car_sound_oracle.exe captures/physics/*.eeMemory.bin
    (--negative, --negative-ambient, --negative-voice falham)
build/Release/race_sound_oracle.exe extracted/core.bin disco.iso [--negative]
PROBE_COURSE=grandvalley_east build/Release/race_audio_probe.exe disco.iso saida.wav [--negative]
```

O oráculo roda 003B5E80, 00392640 e 00457F60 recompilados sobre 20
capturas e compara com o port: 92 quadros de carro, 2.392 pedidos ao
driver, grupos de ambiente em 3 quadros seguidos, 886 volumes e 492
pitches de vozes reais. O probe gera um WAV local de 40 s e confere nível,
vozes em uso, clipping e o callback.

## Backfire nativo (11/10/2026)

003D6D30 lê o sinal físico de 003F33A8, sem modificar o carro, e produz
o pulso usado por 003B5E80. Mantém quatro partículas e RNG próprio
00579330/00578598 (semente 54321098 de 003D6C18). O modelo residente
fornece a contagem de saídas CARINFO+38: 003B6B78 não toca sem saída.
Nenhum arquivo é aberto durante o tick. `race_backfire` compara 16809
casos contra o original (16800 frames e nove contagens), incluindo bordas
e negativo do pulso. Algoritmo E; integração C. Waveform e escolha global
backfire/backfire2 continuam A. Chama visual não implementada.

## Pendente

- Box: disparos pela animação da equipe (003CFA98 -> 00429740), que não
  existe no port. Refueling por 003D3358.
- Backfire: produtor nativo integrado; falta comparar a waveform e o
  sorteio global com PCSX2. Chama visual ainda ausente.
- Helicóptero: só com câmera de replay (vt+0x130).
- Corte de câmera (+0x16B) e segunda viewport.
- Reverb: filtros de meia taxa do hardware aproximados.
- Música: waveform SPU2, fades globais de menu/carregamento e importacao
  das preferencias de memory card; fila e fade da corrida/replay provados abaixo.
- Volume e caminho do play_sound("ok") dos resultados: callback não
  localizado.
- Comparação de ouvido com o PCSX2 para subir a voz SPU2 a V.

## Correcao de loops entre callbacks (03/10/2026)

O callback tratava sempre KOFF antes de KON. Se o loop fosse ligado e
desligado entre dois callbacks, ele recebia KON por ultimo e ficava tocando.
0055D688 +0x10..+0x18 adia KOFF quando KON esta pendente; 0055DF70
+0x10C..+0x11C manda esse KOFF no tick seguinte. Agora cada evento recebe
um serial global do driver, preservando qual comando veio por ultimo.
Reutilizar uma voz continua com KOFF antes do novo KON.

`PROBE_CAR=ford_gt_05 race_audio_probe disco.iso saida.wav` roda 40 s
com Ford GT como jogador e adversario (IA); testa loop que termina antes
do callback, reutilizacao e wrap do serial. `--key-negative` perde o KOFF
e falha. Resultado: passa, 46 vozes, 25 amostras saturadas/3.840.000.
Nota A: nao ha comparacao da waveform com PCSX2; o envio IOP adiado e
agregado pelo callback. Falta repetir manualmente a situacao do usuario.

## Jukebox e transicoes da corrida (03/10/2026)

O ADS racing tem 65 entradas. O fallback do mundo, 00336DE8 +0x24
-> 00437B10, exclui as 15 photo_ (006A5E00) e embaralha as 50 restantes.
A opcao inicial americana e diferente: 004379F0 conserva 65 IDs, mas
/config/config-main.adc, bgmplaylist.h init_raceplaylist e apply,
habilitam e ordenam 36 delas. O port executa essas funcoes do disco numa
VM isolada; nenhuma lista escolhida de ouvido foi usada.

O objeto de 0x810 bytes conserva count, cursor, modo, stopped, semente
u16, IDs u64 e consumed/enabled/order/original de cada entrada.
00437E98/00437F58 e 00438170 foram portados; fim de ciclo limpa os
consumos, nao embaralha de novo. 0019E900 +0x154..0x168 traduz o
play_data [original, consumed, enabled, order].

O save nativo guarda o objeto inteiro em option.bgm_playlist_ps2.
0033C180/0033C278 copiam as preferencias somente na corrida ou com
world+D6C ligado; o original usa uma fila de replay independente de 50.
Desde o pedido de 04/10/2026, o port copia os IDs/flags da fila da corrida
e embaralha a copia no replay, sem alterar o cursor salvo (politica P). race_audio_open_replay recebe esse modo;
race_audio_open conserva a API de corrida. O menu em cache preserva o
blob atualizado antes de capturar o perfil; o audio fecha antes do
save final do app. race_bgm/replay_bgm e seus volumes ficam separados;
o mute temporario do replay inverte a preferencia, inclusive desligada.

Fade: 003C0F40 arma session+D4/D8 e ignora sobreposicao. 003C04E8 reduz
por tick e emite evento1 ao zerar; 003C0FB0 calcula o ganho. 00389B08 arma
o ultimo segundo do replay. O trecho do temporizador em race_run.c foi
autorizado pelo usuario nesta sessao. 00547A88 e um setter de ganho,
nao de duracao: 00548060 clampa [-1,1] e converte para Q15 (RPC4).
0033D568 aplica volume da corrida somente ao enum12; jingles respeitam
o gate BGM e conservam o ganho base. A troca de stream interrompe o anterior
imediatamente; nenhum crossfade artificial foi criado.

Provas E registradas em FIDELITY.md:
- race_sound_oracle: 50 entradas de fallback, 350 comparacoes de jingles,
  0x810 bytes da opcao US, 10.000 filas e tres ciclos cada, roundtrip
  nativo do save e 108 escolhas, gates de replay/override.
- Mesmo oracle: 640.000 ganhos/ticks de fade, 10.000 starts/overlaps,
  131.073 ganhos Q15 e 7.380 disparos do fade de replay.
- Gate e ganho final: 40.000 casos contra 0033D4B8/0033D510, cinco
  modos e todos os flags race/replay/toggle, ganhos variados e zero
  com sinal. O produtor usa gt4_music_stream_gain; --stream-negative
  adultera o mute e falha. Waveform continua fora desta prova.
- race_audio_probe: produtor real, 15.860 ticks de fade e politica de
  volume/mute, open/close reais, writeback da corrida, isolamento do
  replay e recarga do perfil; seis carros audiveis e sem clipping.
- Negativos: --playlist-negative, --jingle-negative, --order-negative,
  --defaults-negative, --preferences-negative, --fade-negative,
  --gain-negative, --fade-event-negative, --replay-fade-negative e
  --music-negative e --persistence-negative (probe).

Reproducao completa permanece A: resampling linear e waveform SPU2 sem
comparacao, latencia do worker/IO e timer PC nos grupos aleatorios.
Fades globais de carregamento/menu e importacao de opcoes do memory card
PS2 nao fazem parte da prova E acima. Nao chamar todo o audio de exato.

## Replay e sequencia repetida (04/10/2026)

Pedido do usuario: mesmas faixas habilitadas da corrida, aleatorias no
replay. O replay pede racing enum12 desde a abertura e ignora jingles da
sessao de corrida reutilizada. A fila sequencial americana do perfil passa
a shuffle; perfil ativo conserva a ordem e o cursor. A semente usa o clock
PC, sem reiniciar sempre com 12345678h. Algoritmo E, entropia A, politica P
em relacao ao default US e ao fallback original de replay.

race_audio_probe compara todos os IDs/flags da copia, injeta autodemo26 e
confere que continua racing12, e compara ordens com clocks diferentes.
--replay-negative restaura os pedidos errados e falha;
--seed-negative repete o clock e falha. Snapshot salvo permanece intacto
apos fechar o replay. Oracle original completo continua passando.

main.c desliga o audio do decoder/previous quando o MovieFace sai da tela.
O decoder em cache podia manter a saida de musica em outra tela. Nota A:
smoke no app publicado em build/replay-audio-fix.png confirma PCM/racing na
corrida e no replay, mas falta repetir a transicao do video com referencia
sonora do PCSX2. Build Release atualiza Visualizar pistas.cmd.

## Voz ENGN e status antigo (10/10/2026)

Usuario relata picotado em RPM sustentado na B-3. Encontrado defeito
independente no transporte: ended[] sobrevivia a reutilizacao de slot e
podia liberar a nova voz depois de um tick, antes do proximo callback.
Agora ended_on[] registra o KON observado e completed_voices filtra pelo
KON atual. Fonte do consumidor: 0055D890 +2C..90, status atual do IOP.

race_audio_probe: ENGN real a 3000 RPM, 600 ticks sem callback, OFF do
ocupante anterior nao altera hardware/serial; OFF atual libera. Negativo
--completion-negative repoe OFF antigo e falha. Oracle 100000 estados
continua passando; pitch/camadas/ADSR e fisica nao alterados. Nota A da
ponte, nao E de waveform. Ainda falta repetir a B-3 do usuario e comparar
com PCSX2 para confirmar que este defeito explica a oscilacao relatada.

## Investigacao do timbre da CRX (10/10/2026)

Usuario relata piora e timbre de Fusca; nao foi aplicado ajuste de pitch,
ganho ou fisica por ouvido. Referencias locais PCSX2 SCUS-97328 .05/.06
(v2.5.165) mostram CRX del Sol em Deep Forest. ENGN original usa
sound/engine/47801 e sound/normal0/47801, ambos com sete camadas.
O probe nativo com PROBE_CAR=delsol_sir_92 carrega exatamente esses bancos.

Novo race_engine_bank_capture compara registros EE do controlador
00559470 +30/+34 e todos os bytes ADPCM usados com RAM SPU2 capturada.
Duas referencias passam: 17 camadas, 255616 bytes por captura; negativo
altera primeiro bloco e falha. Nota E apenas dos dados deste carro.
Repetido race_car_sound_oracle nas duas referencias: 48 pedidos,
70 volumes e 62 pitches identicos; negativo de voz falha.

Timbre final permanece A. A correcao anterior de status antigo nao prova
a causa do relato e nao deve ser anunciada como solucao sonora da B-3.
Usuario depois esclareceu que a CRX era de outra corrida automatica;
tambem percebe no Mustang. Falta waveform PS2 do mesmo carro/camera/RPM.
Capturas e fontes de consulta PCSX2 ficam exclusivamente em build/.

## Camera atrasada e Doppler (10/10/2026)

Usuario esclareceu: CRX Si em outra corrida automatica, tambem Mustang
em RPM constante; a del Sol da B-3 nao era a unica referencia relevante.
Encontrada incompatibilidade de cadencia: race_game_update fazia som em
cada passo, mas atualizava camera so ao terminar o quadro PC. Dois passos
usavam a mesma camera; o seguinte via o salto acumulado. 003B5E80
+1FC..37C calcula Doppler da camera atual e anterior, por quadro original.

Extraido o mesmo bloco de camera em race_camera_audio_update e chamado
antes de race_audio_step em cada passo. Formula, samples e fisica intactos.
Snapshot/primeira inicializacao sem passos ainda constroem a camera.

race_audio_clock usa race_game_update real, teclado deterministico e
render offline, sem thread de audio. CRX Si 1990 e Mustang GT 2005,
3500 RPM apenas na observacao sonora (sensor restaurado antes de voltar
para fisica), 240 passos por execucao: cameras, ENGN, pitches e PCM sao
identicos com 1,2,4 passos por desenho. Negativo segura camera precedente
e falha no passo 1. Dois CTest registrados quando ha ISO configurada.

race_car_sound_oracle novo --negative-listener segura a camera no nativo
e compara func_003B5E80 real: captura .05 acusa +50 original 3F800000
versus 3F748B9C, sem alterar memoria original. Execucao normal .05/.06
continua passando 48 pedidos, 70 volumes e 62 pitches. Nota E da formula
ja registrada; A da integracao/cadencia e do timbre final, sem waveform
PS2 de referencia. Build Release opengt4 atualizou o launcher normal.

Validacao final: 16/16 CTest de audio/backend, replay e abertura/demo de
licenca passaram; race_inspect_probe preserva troca/movimento/reset de
camera. Capturas do probe continuam locais. Sem commit ou push nesta etapa.

## Equipe de box e nova referência da CRX (11/10/2026)

race_pit_crew usa o pacote pit_001/pit_002 selecionado pelo COURSE; 003CFA98 +003D01AC..02CC consulta marcas
wheel_gun/wheel_nut/tire_sound2/jack_up e posta 43/3F/40/42. Jackdown 41
observa 200h sem consumir flag da física. Change 45 e fluxo 46 têm handle
compartilhado; pausa para o fluxo e o próximo frame o restaura. Prioridade
40h, ganho/pitch 1 de 00393428/003934A0, master SE já existente.

race_pit_audio/negative exige waves dos oito enums 3F..46, 120 ticks sem
novo KON no fluxo, pausa/retorno e stop. Negativo troca wave de pistola.
pit_scene_capture exercita o produtor num ciclo de 980 frames com timer
decrescente, RAM inteira preservada; negativo altera RAM e falha. Isso é
regressão PC. O controlador 003CFA98 agora confere bit a bit em 313.600
ticks (dois pacotes/4 layouts/10 ordens/4 disponibilidades), incluindo
a ordem de one-shots e o flag do fluxo; negativos de clocks/câmera/modelos
falham. Waveform, mixer e espacialização continuam A. Oiltank 44 existe
no driver, disparo sonoro ainda não comprovado: 004A2808 com 44 é blend
gráfico, não som. Fixture atual: 511 frames de fluxo, RAM preservada.

Slots atuais 4..6 fornecidos pelo usuário são Ka em Laguna Seca, 7 é
CRX Si 1990 em Tsukuba. race_engine_bank_capture ISO slot7/eeMemory.bin
slot7/SPU2.bin compara tabelas EE e todos os blocos usados: engine/normal0
48306 e roadnoiz, 11 camadas e 176192 bytes idênticos, negativo falha.
Dados E, timbre final A: sem waveform PS2 equivalente para fechar causa.
