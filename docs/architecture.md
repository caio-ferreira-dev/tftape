# Arquitetura — TFT VOD Recorder

## Visão geral
- **`recorder/` (C++)**: core de captura, áudio, encode e mux. Expõe controle via protocolo JSON-lines (stdin/stdout ou pipe nomeado).
- **`ui/` (C# WinUI 3)**: interface gráfica para configurar e controlar gravações, iniciando/monitorando `recorder.exe`.

## Componentes principais
- **Capture**: detecta a janela do TFT por regras configuráveis e captura apenas essa janela (WGC no Windows).
- **Audio**: captura áudio do processo do jogo (padrão) e opcionalmente loopback do sistema e microfone; realiza mixagem e sincronização.
- **Encode/Mux**: H.264 + AAC em container MP4 (Media Foundation no Windows), com perfis 720p/1080p e 30/60 fps.
- **Output**: valida pasta, gera nomes únicos `.mp4` e garante finalização segura do arquivo.
- **Protocol/App**: máquina de estados da gravação e contrato de comandos/eventos com a UI.

## Plataformas
- **Alvo de execução:** Windows 10/11 (captura WGC, WASAPI, Media Foundation, WinUI 3).
- **Testes unitários de lógica:** módulos puros (regras, settings, paths, protocolo, estado) são portáveis e testáveis independentemente das APIs de hardware/UI.
