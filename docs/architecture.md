# Arquitetura — TFTape

## Visão geral
- **`recorder/` (C++)**: core de captura, áudio, encode e mux. Expõe controle via protocolo JSON-lines (stdin/stdout ou pipe nomeado), com parser estrito (`util/Json.cpp`) que recusa JSON malformado, tipos errados, chaves desconhecidas/duplicadas e valores inválidos, devolvendo erro detalhado à UI.
- **`ui/` (C# WinUI 3)**: `TFTape.Ui` é a interface gráfica; `TFTape.Ui.Core` contém modelos, serviços e ViewModel sem dependência de WinUI; `ui.tests` cobre essa lógica com xUnit e um `IRecorderService` falso. A UI injeta um `IUiDispatcher` baseado em `DispatcherQueue`, permitindo testar o ViewModel sem inicializar a interface.

## Componentes principais
- **Capture**: detecta a janela do TFT por regras configuráveis e captura apenas essa janela (WGC no Windows).
- **Audio**: captura áudio do jogo isolado por processo via WASAPI process loopback (`PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE`, Windows 10 build 20348+); o toggle "outras janelas/sistema" usa o mesmo mecanismo no modo `EXCLUDE_TARGET_PROCESS_TREE` (evita duplicar o áudio do jogo) e o microfone usa o endpoint de captura padrão. Se o isolamento por processo não estiver disponível, cada fonte cai para loopback do dispositivo padrão (pode incluir áudio de outras janelas). Todo o áudio é convertido pelo engine para PCM 16-bit/48 kHz/estéreo e depois codificado em AAC.
- **Encode/Mux**: H.264 + AAC em container MP4 (Media Foundation no Windows), com perfis 720p/1080p e 30/60 fps. Responsabilidades separadas: `Mp4Muxer` (dono do `IMFSinkWriter` e ciclo de vida do arquivo), `VideoEncoderMf`/`AudioEncoderMf` (media types e amostras via `IEncoder`) e `MfMp4Writer` (fachada que compõe os três e implementa `IMp4Writer`, injetável na `RecordingSession`). O mapeamento de perfis é puro e testado sem Windows.
- **Output**: valida a sintaxe do caminho (absoluto, sem `..`, caracteres inválidos ou nomes reservados do Windows), confirma permissão de escrita, gera nomes únicos `.mp4` e garante finalização segura do arquivo.
- **Logger/Clock**: `util/Clock.h` fornece relógio do sistema e relógio falso para testes; `util/Logger.cpp` grava diagnósticos no `stderr` e em arquivo rotacionado em `%LOCALAPPDATA%\TFTape\logs`. A detecção registra candidatos avaliados, regra de seleção/exclusão e motivo de falha/ambiguidade.
- **Protocol/App**: máquina de estados da gravação e contrato de comandos/eventos com a UI.
- **Preferências da UI**: `IOutputDirectoryStore` (em `TFTape.Ui.Core`) abstrai a persistência da última pasta de VODs; `LocalSettingsOutputDirectoryStore` (WinUI) usa `ApplicationData.LocalSettings`, mantendo o ViewModel testável sem dependência do Windows.
- **Build/empacotamento**: o build da UI copia `recorder.exe` (padrão: `recorder/build/Release`) e as regras de janela (`recorder/config`) para a saída e para o publish, via `RecorderBuildDir`/`RecorderConfigDir` no `.csproj`. O processo do core roda com `WorkingDirectory` na pasta do próprio executável, então encontra `tft_window_rules.json` ao lado dele.

## Plataformas
- **Alvo de execução:** Windows 10/11 (captura WGC, WASAPI, Media Foundation, WinUI 3).
- **Testes unitários de lógica:** módulos puros (regras, settings, paths, protocolo, estado) são portáveis e testáveis independentemente das APIs de hardware/UI.

## Validação
- **Automática:** 74 testes C++ (GoogleTest/CTest) e 13 testes xUnit, executados com os comandos do `README.md`.
- **End-to-end:** checklist e registro em [`e2e-validation.md`](e2e-validation.md), executados manualmente no Windows com o cliente TFT real (captura WGC, WASAPI, Media Foundation, mux MP4, protocolo e UI juntos).
- Como as APIs de captura/áudio/encode e a WinUI 3 não existem no ambiente WSL/Linux, a validação E2E foi executada no Windows e seus resultados estão registrados no checklist; a documentação final incorpora esse registro.
