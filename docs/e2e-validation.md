# Validação End-to-End — TFTape

Este documento registra a validação final no Windows, com o cliente **Teamfight Tactics** real.
Os testes unitários cobrem a lógica pura; esta checklist cobre captura WGC, WASAPI, Media Foundation,
mux MP4, protocolo JSON-lines e a UI WinUI 3 juntos.

> **Status atual:** validação concluída em 19/11/2026 no Windows. Todos os casos do checklist passaram (`OK`), incluindo os pré-requisitos automáticos.

## 1. Ambiente de referência

| Item | Valor usado como referência |
|---|---|
| Sistema | Windows 10 10.0.26100.9974 (24H2) |
| CPU / GPU | Ryzen 5 5600 / GTX 1060 5 GB (NVENC disponível) |
| Memória / disco | 16 GB RAM / NVMe |
| Cliente | TFT exclusivo (não o cliente LoL) |
| Core | `recorder/build/Release/recorder.exe` |
| UI | `dotnet build/publish` de `ui/TFTape.Ui.sln` |
| Opcional | `ffprobe` (ffmpeg) no `PATH` para inspecionar o MP4 |

## 2. Pré-requisitos

1. Build e testes automáticos passando:
   ```powershell
   cmake -S recorder -B recorder/build -DTFT_BUILD_TESTS=ON
   cmake --build recorder/build --config Release
   ctest --test-dir recorder/build -C Release --output-on-failure

   dotnet restore ui/TFTape.Ui.sln
   dotnet build ui/TFTape.Ui.sln -c Release
   dotnet test ui/TFTape.Ui.sln -c Release --no-build
   ```
   Esperado: **74 testes C++** e **11 testes xUnit** passando (na data da validação; os totais atuais são 84 C++ e 27 xUnit após a persistência da última pasta, o sidecar e a biblioteca de VODs).

2. Abra o TFT e deixe a janela visível. Evite janelas do cliente LoL com título/processo semelhante durante os testes positivos — elas são usadas nos casos de ambiguidade.

3. Crie uma pasta de destino com acento, por exemplo `C:\VodsÁudio`, para validar UTF-8 no protocolo.

4. Confirme que `tft_window_rules.json` (se usado) está ao lado do `recorder.exe` na saída da UI.

## 3. Matriz de execução

### 3.1 Perfis de vídeo

Para cada linha: iniciar gravação, esperar ~10 s, parar pelo botão **Parar**, conferir status/caminho e validar o MP4 com `ffprobe`.

| ID | Resolução | FPS | Bitrate esperado | Resultado | Arquivo/observação |
|---|---|---|---|---|---|
| V1 | 720p | 30 | ~4 Mbps | OK | |
| V2 | 720p | 60 | ~6 Mbps | OK | |
| V3 | 1080p | 30 | ~8 Mbps | OK | |
| V4 | 1080p | 60 | ~12 Mbps | OK | |

Verificações por perfil:
- A UI mostra `Detectando janela do TFT...` e depois `Gravando (TFT)`.
- Ao parar: status `Pronto - gravação finalizada` e caminho `.mp4` preenchido.
- `ffprobe` reporta `codec_name=h264`, dimensões e `r_frame_rate` compatíveis com a linha.

### 3.2 Combinações de áudio

| ID | Configuração | Resultado esperado | Resultado | Observação |
|---|---|---|---|---|
| A1 | Som do jogo apenas (padrão) | Grava com AAC 48 kHz/2 canais | OK | |
| A2 | Jogo + "outras janelas/sistema" | Sem duplicação/eco do jogo | OK | |
| A3 | Jogo + microfone | Microfone audível; sem erro | OK | |
| A4 | Jogo + sistema + microfone | Três fontes mixadas, sincronizadas | OK | |
| A5 | Microfone indisponível (desligar dispositivo) | Aviso amarelo; gravação segue sem mic | OK | |
| A6 | Som do jogo desativado na UI | Erro claro ao iniciar; **nenhum MP4 válido** | OK | |
| A7 | Nenhum endpoint de render ativo | Erro claro ao iniciar (áudio do jogo obrigatório) | OK | |

`A2` depende do isolamento por processo: sem `PROCESS_LOOPBACK_MODE`, o core usa loopback do dispositivo e deduplica jogo/sistema. O log indicou qual caminho foi usado.

### 3.3 Robustez e casos de falha

| ID | Cenário | Resultado esperado | Resultado | Observação |
|---|---|---|---|---|
| R1 | TFT fechado ao iniciar | Erro "janela do TFT não encontrada"; nada é capturado | OK | Log conferido com candidatos/regras avaliadas |
| R2 | Duas janelas TFT plausíveis | Erro de ambiguidade; nada é capturado | OK | |
| R3 | Cliente LoL aberto junto | Somente a janela TFT é detectada/capturada | OK | |
| R4 | Fechar a UI durante a gravação | `shutdown` finaliza o MP4; arquivo tocável | OK | |
| R5 | Parar durante a gravação | MP4 finalizado; aviso amarelo limpo | OK | |
| R6 | Pasta com acento (`C:\VodsÁudio`) | Caminho/correspondência sem mojibake | OK | |
| R7 | Sessão longa (~2 min) com logs ativos | Sem travamento por pipe de `stderr` cheio | OK | |
| R8 | Iniciar com `output_dir` relativo/inválido via protocolo | Erro detalhado, sem gravar | OK | JSON via stdin validado |
| R9 | JSON malformado/tipo errado via stdin | Evento `error` com mensagem detalhada | OK | JSON malformado e tipo errado validados |

### 3.4 Sidecar de metadados

| ID | Cenário | Resultado esperado | Resultado | Observação |
|---|---|---|---|---|
| S1 | Gravar ~10 s e parar pelo botão **Parar** | Além do MP4, existe `<arquivo>.mp4.tftape.json` com `rounds` coerente com a detecção (pode ser vazio se o contador não mudar), `durationMs` coerente e `createdAtUtc` válido | | |
| S2 | Fechar a UI durante a gravação | `shutdown` finaliza o MP4 **e** grava o sidecar | | Mesmo caminho R4 |
| S3 | Pasta com acento | Nome do sidecar e conteúdo JSON legíveis, sem mojibake | | Mesmo caminho R6 |
| S4 | Tornar a pasta de destino somente leitura após iniciar | Se o sidecar falhar, aparece aviso amarelo e o MP4 permanece tocável | | Ajustar permissão depois |
| S5 | Abrir o `.tftape.json` em editor | JSON único, sem arquivo `.tmp` residual e com `schemaVersion: 1` | | |

O evento `recording_stopped` também deve trazer `sidecar_path` e `duration_ms`; a UI expõe esses valores no ViewModel para o visualizador de VODs.

### 3.5 Biblioteca de VODs (aba VODs)

| ID | Cenário | Resultado esperado | Resultado | Observação |
|---|---|---|---|---|
| D1 | Abrir a aba **VODs** com a pasta de gravação já preenchida | Lista mostra os MP4 da pasta, mais recentes primeiro; primeiro VOD selecionado e tocando | | Sem clicar em nada |
| D2 | Clicar em outro VOD da lista | Player troca de vídeo; título e metadados acompanham a seleção | | |
| D3 | VOD com sidecar válido | Linha mostra data, perfil, duração e quantidade de rounds; sem aviso amarelo | | |
| D4 | MP4 sem sidecar ou com `.json` corrompido | VOD continua na lista, com aviso amarelo "Metadados inválidos" quando houver sidecar quebrado | | **Não pode travar a UI** |
| D5 | Trocar a pasta pelo botão **Escolher pasta** e clicar em **Atualizar** | Lista recarrega; nova pasta é lembrada ao fechar/reabrir o app | | `vod_directory.txt` separado de `output_directory.txt` |
| D6 | Fechar a janela com um VOD tocando | Player é liberado e o arquivo não fica travado; core recebe `shutdown` | | Importante no Windows |
| D7 | Verificar botões de rounds | Com `rounds: []`, aparece "Nenhum round detectado nesta gravação." e nenhum botão; com rounds, um botão por marcador | | |

### 3.6 Marcadores de round (ROI/HUD)

| ID | Cenário | Resultado esperado | Resultado | Observação |
|---|---|---|---|---|
| M1 | Gravar do início de um jogo até a primeira virada do contador | Sidecar contém `1-1` em `startMs: 0` e `1-2` no instante da virada; botões na aba VODs fazem seek para esses instantes | | Ajustar a ROI em `tft_round_detector.json` com um print real se necessário |
| M2 | Gravar com HUD/contador fora da ROI ou sem mudança | `rounds` pode ficar vazio, sem aviso de erro e sem afetar o MP4 | | Config inválida gera aviso amarelo e usa defaults |
| M3 | Abrir `tft_round_detector.json` inválido (ROI fora de 0..1) | Core registra aviso no log/UI e grava com a configuração padrão | | A gravação não pode abortar |

> Limitação conhecida: sem OCR, os rótulos são atribuídos pela sequência configurada. Se a gravação começar no meio de um jogo, ajuste `labels` no JSON para começar no round atual; a detecção em si continua válida.

## 4. Inspeção do MP4

Com `ffprobe` disponível:

```powershell
ffprobe -v error `
  -show_entries format=duration,size:stream=index,codec_type,codec_name,width,height,r_frame_rate,sample_rate,channels `
  -of default=noprint_wrappers=1 "C:\VodsÁudio\tft_....mp4"
```

Esperado:
- Um stream `video` `h264` com `1280x720`/`1920x1080` e `30/1`/`60/1`.
- Um stream `audio` `aac` com `48000` Hz e `2` canais (exceto em A6/A7).
- `duration` próxima do tempo gravado e `size` coerente com o bitrate do perfil.
- Reprodução sem erro e sem atraso perceptível entre áudio e vídeo.

## 5. Logs do core

Arquivo: `%LOCALAPPDATA%\TFTape\logs\recorder.log` (rotação `.1`, `.2`, `.3`).

Checar:
- Início/fim do recorder e exceções fatais.
- Carregamento das regras de janela e candidatos avaliados.
- Regra que selecionou/excluiu cada janela e motivo de falha/ambiguidade.
- Carregamento do `tft_round_detector.json`, avisos de configuração inválida e linhas `Round detectado: <label> em <ms> ms`.
- Avisos de fontes opcionais e erros de áudio do jogo.
- Linhas com acentos legíveis (sem `indisponÃvel`).

## 6. Critérios de aceite

- [x] 74 testes C++ e 11 testes xUnit passando na validação de 19/11 (22 testes C++ e 16 testes xUnit adicionados depois).
- [x] V1–V4: MP4 H.264 válido, resolução/fps/bitrate corretos.
- [x] A1–A7: áudio do jogo sempre presente quando habilitado; falhas opcionais viram aviso; falha do jogo aborta.
- [x] R1–R9: nenhuma captura indevida, finalização segura e mensagens claras.
- [ ] S1–S5: sidecar gravado em stop/shutdown, JSON íntegro e aviso em caso de falha. Geração básica confirmada manualmente; checklist formal pendente.
- [ ] D1–D7: aba VODs lista, reproduz, troca de vídeo, tolera sidecar inválido e libera o player ao fechar.
- [ ] M1–M3: marcadores de round gerados na ROI correta, seek na aba VODs e fallback seguro em configuração inválida.
- [x] `ffprobe` sem erros e sincronia A/V aceitável.
- [x] Logs completos e legíveis.

## 7. Registro de execução

| Campo | Valor |
|---|---|
| Data da validação | 19/11/2026 |
| Windows / build | Windows 10 10.0.26100.9974 (24H2) |
| Resultado geral | OK — todos os casos da validação de 19/11 (V1–V4, A1–A7, R1–R9) e pré-requisitos automáticos passaram. Os casos S1–S5 (sidecar), D1–D7 (aba VODs) e M1–M3 (marcadores) ficaram pendentes para a próxima validação no Windows. |
| Arquivos MP4 de referência | MP4 dos perfis V1–V4, conferidos na UI e inspecionados com `ffprobe` |
| Exceções documentadas | Nenhuma |

Registro fechado em 19/11/2026 com todos os casos daquela data marcados como `OK`. As seções 3.4 a 3.6 e os casos S1–S5/D1–D7/M1–M3 foram acrescentados depois com as features de sidecar, biblioteca de VODs e marcadores de round e ainda precisam de execução no Windows.
