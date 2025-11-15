[English](README.md) | [Português (BR)](README.pt-BR.md) 

# OBS GameDetector – Plugin para OBS Studio

O **GameDetector** é um plugin para o **OBS Studio** que detecta automaticamente jogos em execução no computador e permite acionar automações, trocar cenas, enviar mensagens no chat da Twitch e muito mais.

Ele facilita a vida de streamers ao identificar o jogo sendo jogado e executar ações configuráveis de forma automática.

## Funcionalidades

### Detecção Automática de Jogos
* Monitoramento contínuo de processos do sistema a cada 5s direto da memória.
* Suporte a lista **manual** e **automática** de jogos.
* Identificação de início, encerramento e mudança do jogo ativo.
* Processamento assíncrono para não travar o OBS.

### Integração com Twitch Chat
* Envio de mensagens automáticas quando um jogo inicia.
* Geração rápida de token via navegador.
* Armazenamento seguro do token no OBS.
* Permissões mínimas: `user:write:chat`.

## Como Usar

### 1. Instalação

Baixe o instalador da sessão de releases do Github:

[→ Clique aqui para ver as releases](https://github.com/FabioZumbi12/OBSGameDetector/releases)

Siga as instruções, ou baixe op zip, descompacte o zip e copie as pastas `data` e `obs-plugins` na pasta:

```
C:\Program Files\obs-studio\
```

Reinicie o OBS.

### 2️. Abrindo o painel de configuração
No OBS:

**Menu → Ferramentas → Configurações do Game Detector**

Aqui você pode:

* Editar a lista de jogos
* Escanear por jogos nas pastas conhecidas da Steam e Epic (não escaneia todas pastas do PC)
* Configurar token da Twitch
* Salvar alterações

### 3. Configurar mensagens automáticas na Twitch
1. Clique em **Gerar Token**
2. O navegador abre com permissões mínimas
3. Copie o `ACCESS TOKEN` e o `CLIENT ID` e cole nos campos da configuração

## Compatibilidade

* OBS Studio **29+**
* Windows **10/11**
* Powershell
* Compilado com:
  * OBS SDK
  * Qt 6
  * C++17

## Contribuindo

Pull requests e sugestões são bem-vindas!

Você pode:

* Reportar bugs  
* Sugerir novas funções  
* Melhorar documentação  
* Criar testes  
