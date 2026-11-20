# TFTape

Aplicativo para gravar **somente a janela do Teamfight Tactics (TFT)** no Windows.

- **Core:** C++ (`recorder/`) — captura (WGC), áudio (WASAPI), encode/mux MP4 H.264/AAC (Media Foundation).
- **UI:** C# .NET 8 + WinUI 3 (`ui/`) — iniciar/parar, pasta de destino, 720p/1080p, 30/60 fps, áudio do jogo isolado por processo (WASAPI process loopback) por padrão, com opções de sistema/microfone.

> Execução/compilação completa do gravador e da UI requer **Windows 10/11** com Windows SDK/MSVC e .NET SDK/Windows App SDK.

## Estrutura
- `recorder/` — core C++ (CMake) e testes (`recorder/tests`, GoogleTest/CTest).
- `ui/` — aplicação WinUI 3 (`TFTape.Ui`), lógica testável sem WinUI (`TFTape.Ui.Core`) e testes xUnit (`ui.tests`).
- `docs/architecture.md` — visão da arquitetura.
- `docs/e2e-validation.md` — checklist e registro da validação final no Windows com o cliente TFT real.
- Plano: `/home/caio/.opencode/plan/TFTape.md`.

## Build (Windows)
### Core (C++)
```powershell
cmake -S recorder -B recorder/build -DTFT_BUILD_TESTS=ON
cmake --build recorder/build --config Release
ctest --test-dir recorder/build -C Release --output-on-failure
```
Esperado: **74 testes C++** passando.

### UI (WinUI 3)
```powershell
dotnet restore ui/TFTape.Ui.sln
dotnet build ui/TFTape.Ui.sln -c Release
```
O build copia automaticamente `recorder.exe` (de `recorder/build/Release`), `tft_window_rules.json` (se existir) e `tft_window_rules.sample.json` para a saída da UI. Se o core ainda não foi compilado, o build emite um aviso e a UI não encontra o gravador.

### Testes da UI (xUnit)
```powershell
dotnet test ui/TFTape.Ui.sln -c Release
```
Esperado: **14 testes xUnit** passando.
Os testes cobrem o `RecorderViewModel` com um `IRecorderService` falso (sem iniciar o processo do core): comandos, opções enviadas, eventos de início/aviso/fim/erro, exceções, `PropertyChanged` e o despachante injetado.

Para usar um core em outro diretório:
```powershell
dotnet build ui/TFTape.Ui.sln -c Release -p:RecorderBuildDir=C:\caminho\para\build\Release\
```
Para publicar (também inclui os arquivos do core):
```powershell
dotnet publish ui/TFTape.Ui/TFTape.Ui.csproj -c Release
```

## Logs do core
- Arquivo: `%LOCALAPPDATA%\TFTape\logs\recorder.log` (com rotação: `recorder.log.1`, `.2`, `.3`; até 1 MB por arquivo).
- Registra carregamento das regras de janela, todas as janelas avaliadas (processo/classe/dimensões), regra que selecionou ou excluiu cada uma, motivo de falha/ambiguidade, início/fim do recorder e exceções fatais. Títulos de janelas só aparecem na janela selecionada e no resumo de ambiguidade.
- Diagnósticos também ecoam no `stderr`, que a UI drena para evitar travamento por pipe cheio.

## Comportamento
- Comandos da UI são JSON-lines validados de forma estrita: JSON malformado, tipo errado, chave desconhecida/duplicada ou valor inválido (ex.: `fps: "45"`) geram erro detalhado no evento `error`, sem aplicar valores padrão silenciosamente. Escapes Unicode (`\uXXXX`) enviados pelo .NET são decodificados corretamente (pastas com acentos funcionam).
- A pasta de destino precisa ser absoluta e válida no Windows (sem `..`, caracteres reservados ou nomes como `CON`); o core verifica a escrita antes de iniciar.
- A última pasta utilizada é lembrada entre execuções e já aparece preenchida na próxima abertura da UI (`%LOCALAPPDATA%\TFTape\output_directory.txt`).
- Fechar a UI envia `shutdown` ao core e aguarda a finalização do MP4; se ele não responder, o processo é encerrado.
- Falha ao capturar o **áudio do jogo** aborta o início com erro claro. Falha em fontes **opcionais** (sistema/microfone) gera aviso amarelo e a gravação segue sem essa fonte.
- Diagnósticos do core são drenados do `stderr` pela UI (evita travamento por pipe cheio) e mostrados se o gravador encerrar com erro.

## Validação end-to-end (Windows)

Testes unitários cobrem a lógica pura; a validação final precisa ser executada no Windows com o cliente TFT real. O checklist completo está em [`docs/e2e-validation.md`](docs/e2e-validation.md) e cobre:

- Perfis 720p/1080p × 30/60 fps, com inspeção do MP4 via `ffprobe`.
- Combinações de áudio (jogo, sistema, microfone) e falhas de dispositivos opcionais.
- Casos de falha: janela ausente/ambígua, cliente LoL aberto, parada/fechamento durante a gravação, caminhos com acento e protocolo JSON inválido.
- Revisão dos logs de detecção e diagnósticos do core.

Comandos de pré-requisito (os mesmos das seções de build/teste):

```powershell
cmake -S recorder -B recorder/build -DTFT_BUILD_TESTS=ON
cmake --build recorder/build --config Release
ctest --test-dir recorder/build -C Release --output-on-failure

dotnet restore ui/TFTape.Ui.sln
dotnet build ui/TFTape.Ui.sln -c Release
dotnet test ui/TFTape.Ui.sln -c Release --no-build
```

> Status: validação concluída em 19/11/2026 no Windows; todos os casos do checklist passaram. Detalhes em [`docs/e2e-validation.md`](docs/e2e-validation.md).

## Git
- Repositório local na raiz. Cada funcionalidade/fix em branch própria (`feat/...`, `fix/...`).
- **Antes de criar uma nova branch:** obter feedback e realizar o merge/aprovação da branch atual na `main`.
