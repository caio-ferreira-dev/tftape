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
   Esperado: **74 testes C++** e **11 testes xUnit** passando (na data da validação; o total atual é 14 xUnit após a persistência da última pasta).

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
- Avisos de fontes opcionais e erros de áudio do jogo.
- Linhas com acentos legíveis (sem `indisponÃvel`).

## 6. Critérios de aceite

- [x] 74 testes C++ e 11 testes xUnit passando na validação de 19/11 (3 testes adicionados depois).
- [x] V1–V4: MP4 H.264 válido, resolução/fps/bitrate corretos.
- [x] A1–A7: áudio do jogo sempre presente quando habilitado; falhas opcionais viram aviso; falha do jogo aborta.
- [x] R1–R9: nenhuma captura indevida, finalização segura e mensagens claras.
- [x] `ffprobe` sem erros e sincronia A/V aceitável.
- [x] Logs completos e legíveis.

## 7. Registro de execução

| Campo | Valor |
|---|---|
| Data da validação | 19/11/2026 |
| Windows / build | Windows 10 10.0.26100.9974 (24H2) |
| Resultado geral | OK — todos os casos (V1–V4, A1–A7, R1–R9) e pré-requisitos automáticos passaram |
| Arquivos MP4 de referência | MP4 dos perfis V1–V4, conferidos na UI e inspecionados com `ffprobe` |
| Exceções documentadas | Nenhuma |

Registro fechado em 19/11/2026 com todos os casos marcados como `OK`.
