# Arquitetura — TFT VOD Recorder

## Visão geral
- **`recorder/` (C++)**: core de captura, áudio, encode e mux. Expõe controle via protocolo JSON-lines (stdin/stdout ou pipe nomeado).
- **`ui/` (C# WinUI 3)**: `TftVodRecorder.Ui` é a interface gráfica; `TftVodRecorder.Ui.Core` contém modelos, serviços e ViewModel sem dependência de WinUI; `ui.tests` cobre essa lógica com xUnit e um `IRecorderService` falso. A UI injeta um `IUiDispatcher` baseado em `DispatcherQueue`, permitindo testar o ViewModel sem inicializar a interface.

## Componentes principais
- **Capture**: detecta a janela do TFT por regras configuráveis e captura apenas essa janela (WGC no Windows).
- **Audio**: captura áudio do jogo isolado por processo via WASAPI process loopback (`PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE`, Windows 10 build 20348+); o toggle "outras janelas/sistema" usa o mesmo mecanismo no modo `EXCLUDE_TARGET_PROCESS_TREE` (evita duplicar o áudio do jogo) e o microfone usa o endpoint de captura padrão. Se o isolamento por processo não estiver disponível, cada fonte cai para loopback do dispositivo padrão (pode incluir áudio de outras janelas). Todo o áudio é convertido pelo engine para PCM 16-bit/48 kHz/estéreo e depois codificado em AAC.
- **Encode/Mux**: H.264 + AAC em container MP4 (Media Foundation no Windows), com perfis 720p/1080p e 30/60 fps.
- **Output**: valida pasta, gera nomes únicos `.mp4` e garante finalização segura do arquivo.
- **Logger/Clock**: `util/Clock.h` fornece relógio do sistema e relógio falso para testes; `util/Logger.cpp` grava diagnósticos no `stderr` e em arquivo rotacionado em `%LOCALAPPDATA%\TftVodRecorder\logs`. A detecção registra candidatos avaliados, regra de seleção/exclusão e motivo de falha/ambiguidade.
- **Protocol/App**: máquina de estados da gravação e contrato de comandos/eventos com a UI.
- **Build/empacotamento**: o build da UI copia `recorder.exe` (padrão: `recorder/build/Release`) e as regras de janela (`recorder/config`) para a saída e para o publish, via `RecorderBuildDir`/`RecorderConfigDir` no `.csproj`. O processo do core roda com `WorkingDirectory` na pasta do próprio executável, então encontra `tft_window_rules.json` ao lado dele.

## Plataformas
- **Alvo de execução:** Windows 10/11 (captura WGC, WASAPI, Media Foundation, WinUI 3).
- **Testes unitários de lógica:** módulos puros (regras, settings, paths, protocolo, estado) são portáveis e testáveis independentemente das APIs de hardware/UI.
