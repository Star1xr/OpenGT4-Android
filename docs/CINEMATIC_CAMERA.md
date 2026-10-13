# Câmera cinematográfica PC (P)

F9 ativa/desativa durante corrida ou replay 1P. Esc sai da câmera antes de
abrir a pausa/sair do replay. Não está disponível na tela dividida 2P.
O HUD fica oculto inicialmente. Não grava vídeo automaticamente.

| Tecla | Ação |
|---|---|
| Mouse | Olhar; no modo Seguir com mira ligada, orbitar o carro |
| WASD | Mover em relação à câmera, com aceleração/desaceleração suave |
| Q / E | Baixar / subir |
| Shift / Ctrl esquerdo | Movimento rápido / preciso |
| Roda do mouse | Zoom da lente |
| F | Seguir o carro / câmera livre no mundo |
| G | Prender à carroceria como GoPro / soltar |
| Tab | Próximo carro, mantendo o enquadramento relativo |
| T | Ligar/desligar mira no carro; no modo GoPro passa para Seguir |
| Z / C | Inclinar o horizonte |
| B | Profundidade de campo: desligada → f/2.8 → f/5.6 → f/11 |
| [ / ] | Diminuir/aumentar distância de foco manual |
| R | Restaurar foco automático no carro selecionado |
| P | Congelar/retomar a cena, continuando a mover a câmera |
| H | Mostrar/ocultar HUD |
| Home | Voltar à câmera traseira do carro selecionado |
| F5 | Entrar no Photo Drive com o enquadramento atual |

Para uma GoPro, posicione a câmera perto da carroceria e aperte G. Ela
acompanha a posição, direção e inclinação do carro. WASD/QE e mouse ainda
permitem ajustar o suporte. Para uma tomada orbital, F liga Seguir e a mira
no carro; mouse faz a órbita e WASD aproxima/afasta/reposiciona a câmera.

O controle continua dirigindo o carro. Enquanto a ferramenta está ativa,
o teclado pertence à câmera; use replay ou controle para filmar movimento.
P usa a retenção de apresentação já existente, sem mudar o passo da física.
Ao sair, restaura a retenção anterior e libera o mouse. Nenhum cursor é
desenhado no quadro. Desfoque faz oito desenhos por quadro e pode reduzir o
desempenho; começa desligado.

Ferramenta PC de propósito, nota P: sensibilidade, suavização e presets
não são recursos do GT4 original. A lente reutiliza gt4_photo_lens, com
compensação da projeção real da corrida; acumulação fica na GPU. Foco
automático usa o plano do carro selecionado; [/] permitem puxar foco.
Renderer/óptica integral conservam as notas anteriores de fidelidade.

race_cinematic testa seed, teclado isolado da direção, movimento,
órbita/mira, troca de carro, montagem sem salto, eixos ortonormais,
zoom/foco, pausa/reset/cleanup e 32 MiB da física inalterados, inclusive
desenho com desfoque. O negativo troca um byte e acusa a diferença.
Também verifica projeção do plano focal entre amostras (<1e-6) e pixels
diferentes entre desfoque ligado/desligado. Capturas locais em build/.
