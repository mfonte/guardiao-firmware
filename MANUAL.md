# Guardião — Manual do Usuário

**Versão do firmware:** 1.2.1

---

## Índice

1. [Visão Geral](#1-visão-geral)
2. [Botões](#2-botões)
3. [Ligando o dispositivo](#3-ligando-o-dispositivo)
4. [Primeira configuração de Wi-Fi](#4-primeira-configuração-de-wi-fi)
5. [Telas](#5-telas)
6. [Navegando entre telas](#6-navegando-entre-telas)
7. [Alarme de temperatura](#7-alarme-de-temperatura)
8. [Reconfigurar o Wi-Fi](#8-reconfigurar-o-wi-fi)
9. [Redefinição de fábrica](#9-redefinição-de-fábrica)
10. [Comportamento de envio para a nuvem](#10-comportamento-de-envio-para-a-nuvem)

---

## 1. Visão Geral

O Guardião é um sensor de temperatura conectado à internet. Ele lê a temperatura ambiente em tempo real, exibe os dados no display e os envia automaticamente para a nuvem.

---

## 2. Botões

O dispositivo possui dois botões:

```
┌─────────────────────────┐
│                         │
│       [display]         │
│                         │
│   [S1]          [S2]    │
└─────────────────────────┘
```

| Botão             | Pressão rápida | Pressão longa                 |
| ----------------- | -------------- | ----------------------------- |
| **S1** (esquerda) | Tela anterior  | Abrir portal Wi-Fi (5 s)      |
| **S2** (direita)  | Próxima tela   | Redefinição de fábrica (10 s) |

> **Pressão rápida:** toque e solte em menos de 1 segundo.  
> **Pressão longa:** mantenha pressionado até a barra de progresso completar.

---

## 3. Ligando o dispositivo

Ao ligar, o display exibe a tela de boot com uma barra de progresso enquanto o dispositivo se inicializa:

```
┌─────────────────────────┐
│                         │
│        Guardião         │
│                         │
│    Conectando WiFi...   │
│  [████████░░░░░░░░░░░]  │
│                         │
└─────────────────────────┘
```

Etapas exibidas durante o boot:

| Progresso | Etapa                  |
| --------- | ---------------------- |
| 0%        | Iniciando              |
| 20%       | Conectando ao Wi-Fi    |
| 40%       | Sincronizando horário  |
| 60%       | Conectando ao servidor |
| 80%       | Autenticando           |
| 100%      | Pronto!                |

Ao concluir, o buzzer emite três bipes curtos crescentes e o display passa para a tela principal.

---

## 4. Primeira configuração de Wi-Fi

Na primeira vez que o dispositivo é ligado (ou após uma redefinição de fábrica), ele não possui rede Wi-Fi configurada. O display exibirá:

```
┌─────────────────────────┐
│                         │
│    Connect to WiFi:     │
│                         │
│     Guardiao-XXXX      │
│                         │
│    using your phone     │
└─────────────────────────┘
```

**Passos:**

1. No celular ou computador, abra a lista de redes Wi-Fi disponíveis.
2. Conecte-se à rede chamada **`Guardiao-XXXX`** onde XXXX são os 4 últimos caracteres do ID do dispositivo, exibidos no display.
3. Uma página de configuração abrirá automaticamente. Se não abrir, acesse **192.168.4.1** no navegador.
4. Selecione sua rede Wi-Fi doméstica e insira a senha.
5. Salve. O dispositivo reiniciará e se conectará automaticamente.

> Após a configuração bem-sucedida, o buzzer emite dois bipes confirmando a conexão.

> **Limitações das credenciais Wi-Fi:** O dispositivo armazena até **3 redes** na memória interna. O nome da rede (SSID) pode ter no máximo **32 caracteres** e a senha no máximo **64 caracteres**. Credenciais que excedam esses limites serão truncadas silenciosamente.

---

## 5. Telas

O dispositivo possui **6 telas**, navegáveis com os botões S1 e S2. Uma barra de ícones fixa no topo indica o status atual em todas as telas.

### Barra de status (topo de todas as telas)

```
┌─────────────────────────┐
│ ≋  ⇅  🔔       23.6°   │  ← barra de status
├─────────────────────────┤
│        (conteúdo)       │
└─────────────────────────┘
```

| Ícone   | Significado                                        |
| ------- | -------------------------------------------------- |
| `≋`     | Wi-Fi conectado                                    |
| `≋̶`    | Wi-Fi desconectado                                 |
| `⇅`     | Sincronizado com o servidor                        |
| `⇅̶`    | Sem sincronismo com o servidor                     |
| `🔔`   | Alarme de temperatura ativo                        |
| `23.6°` | Temperatura atual (visível fora da tela principal) |

---

### Tela 1 — Temperatura

Tela principal. Exibe a temperatura atual com o nome do dispositivo e uma seta à direita indicando a tendência.

```
┌─────────────────────────┐
│ ≋  ⇅  🔔               │
├─────────────────────────┤
│        Casa 01          │  ← nome do dispositivo
│                         │
│        23.6°C ↑         │  ← temperatura + seta de tendência
│                         │
│    ◆  ·  ·  ·  ·  ·    │  ← paginação (tela 1 de 6)
└─────────────────────────┘
```

**Setas de tendência:**

| Seta | Significado          |
| ---- | -------------------- |
| ↑    | Temperatura subindo  |
| ↓    | Temperatura descendo |
| →    | Temperatura estável  |

---

### Tela 2 — Limite de temperatura

Exibe a posição da temperatura atual dentro da faixa configurada.

```
┌─────────────────────────┐
│ ≋  ⇅  🔔               │
├─────────────────────────┤
│        Threshold        │
│                         │
│  [══════▐░░░░░░░░░░░░]  │  ← posição atual na faixa
│                         │
│  -10       23.6°C   40  │  ← mínimo / atual / máximo
│    ·  ◆  ·  ·  ·  ·    │
└─────────────────────────┘
```

Se nenhum limite estiver configurado, será exibido "Not configured".

---

### Tela 3 — Relógio

Exibe a hora e data atuais sincronizadas automaticamente pela internet (horário de Brasília).

```
┌─────────────────────────┐
│ ≋  ⇅                   │
├─────────────────────────┤
│                         │
│        14:32:07         │  ← hora atual (BRT)
│                         │
│       2026-04-11        │  ← data atual
│    ·  ·  ◆  ·  ·  ·    │
└─────────────────────────┘
```

> O horário é sincronizado automaticamente ao ligar. Se o dispositivo estiver sem Wi-Fi, a tela exibirá "Syncing..." até a conexão ser restabelecida.

---

### Tela 4 — Status do sistema

Informações de operação do dispositivo.

```
┌─────────────────────────┐
│ ≋  ⇅                   │
├─────────────────────────┤
│ Firebase: OK            │
│ Sent: 142               │  ← total de leituras enviadas
│ Up: 2h 34m              │  ← tempo ligado
│ Heap: 38420B            │  ← memória interna disponível
│    ·  ·  ·  ◆  ·  ·    │
└─────────────────────────┘
```

---

### Tela 5 — Rede Wi-Fi

Detalhes da conexão de rede.

```
┌─────────────────────────┐
│ ≋  ⇅                   │
├─────────────────────────┤
│ SSID: MinhaRede         │
│ IP: 192.168.1.105       │
│ Host: guardiao-casa01   │
│ RSSI: -62 dBm           │
│    ·  ·  ·  ·  ◆  ·    │
└─────────────────────────┘
```

> **RSSI:** indica a força do sinal. Valores entre -30 e -70 dBm são normais. Abaixo de -80 dBm pode causar instabilidade.

---

### Tela 6 — Alimentação

Tensão interna do dispositivo.

```
┌─────────────────────────┐
│ ≋  ⇅                   │
├─────────────────────────┤
│                         │
│         3.28 V          │
│                         │
│       USB Powered       │
│    ·  ·  ·  ·  ·  ◆    │
└─────────────────────────┘
```

| Mensagem       | Significado                         |
| -------------- | ----------------------------------- |
| `USB Powered`  | Alimentado por USB (acima de 4.5 V) |
| `Battery OK`   | Bateria em nível adequado           |
| `LOW BATTERY!` | Bateria baixa — recarregar          |

---

## 6. Navegando entre telas

```
        ← S1                                                    S2 →

  [Temp] → [Threshold] → [Relógio] → [Status] → [WiFi] → [Bateria]
    ◆           ·              ·           ·         ·         ·
```

- **S2 (pressão rápida):** avança para a próxima tela.
- **S1 (pressão rápida):** volta para a tela anterior.
- Os pontos na parte inferior indicam qual tela está ativa.
- Um bipe curto confirma cada navegação.

---

## 7. Alarme de temperatura

Quando a temperatura ultrapassa os limites configurados, o dispositivo aciona o alarme:

```
┌─────────────────────────┐
│                         │
│       ! ALARM !         │
├─────────────────────────┤
│                         │
│        ALARM ON         │
│                         │
│  Press any button stop  │
└─────────────────────────┘
```

- O buzzer soa de forma intermitente e crescente por até **30 segundos**.
- Pressione **qualquer botão** para silenciar o alarme imediatamente.

---

## 8. Reconfigurar o Wi-Fi

Use este recurso para trocar a rede Wi-Fi sem redefinir o dispositivo.

**Como fazer:**

1. Mantenha **S1** pressionado. Uma barra de progresso aparecerá:

```
┌─────────────────────────┐
│                         │
│    Open WiFi Portal     │
│  [████████████░░░░░░░]  │
│       Hold...           │
│    Release to cancel    │
└─────────────────────────┘
```

2. Segure até a barra completar (~5 segundos).
3. O dispositivo criará a rede **`Guardiao-XXXX`** e aguardará nova configuração.
4. Siga os mesmos passos da [Primeira configuração](#4-primeira-configuração-de-wi-fi).

> **Solte o botão antes de completar** para cancelar sem alterar nada.

---

## 9. Redefinição de fábrica

Apaga a configuração de Wi-Fi e reinicia o dispositivo do zero.

> ⚠️ **Atenção:** esta ação não pode ser desfeita. O dispositivo precisará ser reconfigurado.

**Como fazer:**

1. Mantenha **S2** pressionado. Uma barra de progresso aparecerá:

```
┌─────────────────────────┐
│                         │
│      Factory Reset      │
│  [████████████████░░░]  │
│       Hold...           │
│    Release to cancel    │
└─────────────────────────┘
```

2. Segure até a barra completar (~10 segundos).
3. O display exibirá a tela de reinicialização:

```
┌─────────────────────────┐
│                         │
│      Restarting...      │
│                         │
│        Casa 01          │
│                         │
└─────────────────────────┘
```

4. O dispositivo reiniciará e aguardará nova configuração de Wi-Fi.

> **Solte o botão antes de completar** para cancelar sem alterar nada.


## 10. Comportamento de envio para a nuvem

No firmware **1.2.1**, o envio para a nuvem ocorre por gatilhos independentes.

### 10.1 Envio por intervalo (regular)
- O dispositivo envia leituras em um intervalo fixo (ex.: a cada 5 minutos).
- Esse envio é contínuo e **não depende** das medições programadas por horário.

### 10.2 Medições programadas (por relógio)
- Além do intervalo regular, é possível configurar medições em horários exatos.
- Exemplo: início às **00:00** com frequência de **60 min** → envia em 00:00, 01:00, 02:00...
- O cálculo respeita o fuso local configurado no dispositivo.

### 10.3 Envio por evento de alerta
- Se a temperatura ultrapassar limites configurados, o dispositivo força envio imediato.
- Quando a temperatura volta à faixa normal, um envio de recuperação também pode ser disparado.

### 10.4 Falha de rede/Firebase e fila pendente
- Se não for possível enviar na hora, a leitura é salva na memória interna (fila pendente).
- Quando a conexão volta, as leituras pendentes são reenviadas automaticamente.
- Leituras de **medição programada** têm prioridade de retenção quando a fila está cheia.

### 10.5 Logs técnicos (diagnóstico)
Em serial/debug, os envios aparecem com origem explícita:
- `INTERVAL`
- `SCHEDULED`
- `THRESHOLD`
- `RECOVERY`

Também existem logs de espera:
- `INTERVAL-HOLD`
- `SCHEDULED-HOLD`

Esses rótulos ajudam a identificar rapidamente por que uma leitura foi enviada (ou adiada).


---

*Manual gerado para o firmware Guardião v1.2.1*
