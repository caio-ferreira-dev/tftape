# TFT VOD Recorder

Aplicativo para gravar **somente a janela do Teamfight Tactics (TFT)** no Windows.

- **Core:** C++ (`recorder/`) — captura (WGC), áudio (WASAPI), encode/mux MP4 H.264/AAC (Media Foundation).
- **UI:** C# .NET 8 + WinUI 3 (`ui/`) — iniciar/parar, pasta de destino, 720p/1080p, 30/60 fps, áudio do jogo isolado por processo (WASAPI process loopback) por padrão, com opções de sistema/microfone.

> Execução/compilação completa do gravador e da UI requer **Windows 10/11** com Windows SDK/MSVC e .NET SDK/Windows App SDK.

## Estrutura
- `recorder/` — core C++ (CMake) e testes (`recorder/tests`, GoogleTest/CTest).
- `ui/` — aplicação WinUI 3.
- `.agents/SKILLS.md` — diretrizes de testes unitários e manutenção do git.
- `docs/architecture.md` — visão da arquitetura.
- Plano: `/home/caio/.opencode/plan/tft-vod-recorder.md`.

## Build (Windows)
### Core (C++)
```powershell
cmake -S recorder -B recorder/build -DTFT_BUILD_TESTS=ON
cmake --build recorder/build --config Release
ctest --test-dir recorder/build -C Release
```

### UI (WinUI 3)
```powershell
dotnet restore ui/TftVodRecorder.Ui.sln
dotnet build ui/TftVodRecorder.Ui.sln -c Release
```
Copie `recorder.exe` para junto do executável da UI (ou configure o caminho) para permitir o controle via processo.

## Git
- Repositório local na raiz. Cada funcionalidade/fix em branch própria (`feat/...`, `fix/...`).
- **Antes de criar uma nova branch:** obter feedback e realizar o merge/aprovação da branch atual na `main`.
