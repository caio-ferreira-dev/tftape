# Skills do Projeto — TFT VOD Recorder

## 1) Testes unitários

### Princípios
- **Avaliar a cada mudança:** ao criar/alterar código, identificar lógica pura, regras, máquina de estados, parsing/serialização, validação ou transformações isoláveis. Se existir, criar ou atualizar testes unitários.
- **Isolar dependências:** código que acessa APIs de janela/GPU/áudio/hardware ou UI deve ser separado por interfaces/abstrações para permitir testes com fakes/mocks/stubs da lógica.
- **Determinismo:** testes unitários não devem depender de janela real do jogo, dispositivos de áudio, tempo real não controlado ou rede; cobrir esses casos com testes de integração/smoke manuais no Windows quando necessário.

### O que testar neste projeto
- **C++ (core):**
  - `TftWindowFinder`/regras de detecção: match correto do cliente TFT exclusivo, exclusão de janelas/LoL indevidos, casos sem candidato e ambíguos.
  - `RecordingSettings`: validação e mapeamento de perfis (720p/1080p, 30/60 fps) e flags de áudio.
  - `OutputPath`: geração de nomes `.mp4` únicos, sanitização e validação/criação de diretório.
  - `CommandProtocol`: parse/serialização de comandos e eventos (JSON-lines), casos inválidos.
  - `RecorderController`/sessão: transições de estado (idle/recording/stopping/error) e tratamento de falhas.
- **C# (UI):**
  - `RecorderViewModel` e serviços de controle com `IRecorderService` simulado: estados da UI, habilitação de ações, validação de pasta/opções e propagação de erros/eventos.

### Boas práticas
- Nomear testes pelo comportamento esperado (ex.: `ReturnsError_WhenNoTftWindowFound`).
- Cobrir caminho feliz, bordas e falhas relevantes.
- Manter testes rápidos e independentes entre si.
- Ao corrigir um bug, adicionar teste que reproduza o problema quando a lógica for unitariamente testável.
- Rodar a suíte de testes relacionada antes de solicitar feedback/merge.

## 2) Manutenção do Git

### Repositório e estado base
- Manter um **repositório Git local na raiz do projeto** versionando código-fonte, testes, docs e configurações do projeto (respeitando `.gitignore` para artefatos de build como `bin/`, `obj/`, `build/`, etc.).
- Manter a branch `main` em estado **estável** (compilável/testes relevantes passando quando aplicável).

### Branches por trabalho
- Para **cada nova funcionalidade ou correção (fix)**, criar uma **branch nova e dedicada** a partir da `main` atualizada.
  - Convenção sugerida: `feat/<descricao-curta>` ou `fix/<descricao-curta>`.
- Não misturar funcionalidades/fixes não relacionados na mesma branch.

### Feedback e merge (regra do projeto)
- **Antes de criar uma nova branch**, garantir que houve **feedback do usuário** sobre o trabalho atual e que a branch atual foi **revisada/aprovada e integrada (merge) na `main`** (ou que houve decisão explícita de não integrá-la).
- Não iniciar nova linha de trabalho sem esse alinhamento.

### Commits e histórico
- Fazer commits **pequenos, atômicos e descritivos**, focados na mudança da branch.
- Mensagens claras indicando intenção (ex.: `feat: add TFT window detection rules`, `fix: finalize mp4 on stop`).
- Evitar commitar artefatos gerados, arquivos temporários ou segredos.

### Integração
- Atualizar a branch com a `main` quando necessário para reduzir conflitos antes do merge.
- Realizar o merge para `main` somente após validação/feedback do usuário.
- Após o merge concluído e alinhado, criar a próxima branch para o próximo item de trabalho.
