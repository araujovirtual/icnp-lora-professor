# ICNP/LoRa - Arquitetura vestível experimental para monitoramento fisiológico

Projeto de dissertação de mestrado desenvolvido no PPGCC/UTFPR, com uma arquitetura vestível experimental baseada em comunicação LoRa ponto-a-ponto, protocolo ICNP e visualização local em navegador para acompanhamento técnico-operacional de sinais fisiológicos estimados.

> **Importante:** este projeto realiza ensaios técnico-operacionais de bancada e integração funcional. As leituras de frequência cardíaca e SpO2 são estimativas experimentais obtidas por aquisição PPG. Este repositório **não** deve ser interpretado como sistema clínico, dispositivo médico, ferramenta de diagnóstico ou solução clinicamente validada.

---

## 1. Visão geral

O sistema é organizado em uma arquitetura **Professor-Aluno**:

- **Nó Professor:** coordena o ciclo ICNP, envia `BEACON`, recebe pacotes `DATA`, responde com `ACK`, registra os dados em formato CSV e disponibiliza uma API local para visualização.
- **Nó Aluno:** coleta dados do sensor PPG, estima frequência cardíaca e SpO2, mede bateria, exibe informações no OLED e transmite os dados ao Professor por LoRa.
- **API local:** roda no nó Professor e permite acompanhar os últimos dados dos Alunos por navegador, notebook, celular ou TV conectada à mesma rede.
- **API Admin Wi-Fi:** permite configurar SSID e senha do Wi-Fi do Professor sem recompilar o firmware.

O protocolo ICNP foi implementado para organizar a comunicação entre os nós, reduzir transmissões desordenadas e registrar métricas como RSSI, SNR, ACK, ciclo, sequência, bateria e qualidade operacional da aquisição PPG.

---

## 2. Hardware usado

### 2.1 Nó Professor

| Item | Função |
|---|---|
| Heltec WiFi LoRa 32 V2 | Placa principal com ESP32, LoRa SX127x e OLED integrado |
| Antena LoRa 915 MHz | Comunicação LoRa ponto-a-ponto |
| Cabo USB | Alimentação, gravação do firmware e monitor serial |
| Notebook/celular/TV | Acesso ao painel web e à página administrativa |

### 2.2 Nó Aluno

| Item | Função |
|---|---|
| Heltec WiFi LoRa 32 V2 | Placa principal do Aluno com ESP32, LoRa SX127x e OLED integrado |
| Sensor MAX30102/MH-ET LIVE | Aquisição PPG para estimativa experimental de FC e SpO2 |
| Antena LoRa 915 MHz | Comunicação LoRa ponto-a-ponto com o Professor |
| Bateria Li-ion/LiPo ou alimentação USB | Alimentação do nó Aluno |
| Jumpers/fios Dupont | Ligação I2C do sensor PPG |

### 2.3 Ligações principais do MAX30102 no Aluno

| MAX30102 | Heltec WiFi LoRa 32 V2 |
|---|---|
| VIN/VCC | 3V3 ou 5V, conforme o módulo utilizado |
| GND | GND |
| SDA | SDA do ESP32 |
| SCL | SCL do ESP32 |

> Verifique o módulo MAX30102 utilizado antes de ligar em 5 V. Alguns módulos aceitam 3,3 V e 5 V; outros exigem cuidado com nível lógico.

---

## 3. Software necessário

- Visual Studio Code
- PlatformIO IDE
- Git
- Driver USB da placa, se necessário
- Navegador web para acessar a API local

---

## 4. Estrutura principal do projeto

```text
src/
├── professor/
│   ├── professor_main.cpp
│   ├── api_professor.cpp
│   ├── api_professor.h
│   ├── config_wifi.cpp
│   └── config_wifi.h
│
├── aluno/
│   └── aluno_main.cpp
│
└── comum/
    ├── configuracao_lora.h
    ├── radio_lora.cpp
    ├── radio_lora.h
    ├── protocolo_icnp.cpp
    ├── protocolo_icnp.h
    ├── sensor_fisiologico.cpp
    ├── sensor_fisiologico.h
    ├── bateria.cpp
    ├── bateria.h
    ├── oled.cpp
    └── oled.h
```

A estrutura pode variar ligeiramente conforme a versão do projeto, mas a organização lógica é esta: código do Professor em `src/professor`, código do Aluno em `src/aluno` e módulos compartilhados em `src/comum`.

---

## 5. O que cada arquivo faz

### 5.1 Arquivos do Professor

#### `src/professor/professor_main.cpp`

Arquivo principal do nó Professor.

Responsabilidades:

- inicializar LoRa;
- iniciar o ciclo ICNP;
- enviar mensagens `BEACON`;
- aguardar pacotes `DATA` dos Alunos;
- validar campos recebidos;
- responder com `ACK`;
- calcular/registrar RSSI e SNR;
- imprimir logs no Serial Monitor;
- gerar linha CSV dos dados recebidos;
- atualizar o estado usado pela API local.

Exemplo lógico do fluxo:

```text
Professor inicia
Professor envia BEACON
Aluno recebe BEACON
Aluno envia DATA
Professor recebe DATA
Professor envia ACK
Professor atualiza API e CSV
```

#### `src/professor/api_professor.cpp`

Implementa o servidor HTTP local do Professor.

Responsabilidades:

- criar o dashboard web;
- expor o endpoint `/api/status`;
- exibir os últimos dados dos Alunos;
- desenhar gráficos em esteira temporal;
- aplicar cores operacionais para FC, SpO2 e bateria;
- manter a visualização em navegador/TV;
- disponibilizar a página `/admin` ou `/api/admin` para configuração Wi-Fi.

Endpoints principais:

| Endpoint | Função |
|---|---|
| `/` | Dashboard local com dados dos Alunos |
| `/api/status` | Retorna os últimos dados em JSON |
| `/admin` | Página administrativa Wi-Fi |
| `/api/admin` | Página administrativa Wi-Fi |

#### `src/professor/api_professor.h`

Cabeçalho da API do Professor.

Responsabilidades:

- declarar estruturas de dados usadas pela API;
- declarar funções públicas da API;
- permitir que `professor_main.cpp` atualize os dados exibidos no dashboard.

Estrutura típica:

```cpp
struct EstadoAlunoAPI {
    int aluno;
    int ciclo;
    int seq;
    int fc;
    int spo2;
    float batAluno;
    float energiaProf;
    int rssi;
    float snr;
    bool dedo;
    String qualidade;
};
```

#### `src/professor/config_wifi.cpp`

Implementa a configuração Wi-Fi do Professor.

Responsabilidades:

- ler SSID e senha salvos na NVS do ESP32;
- tentar conectar em modo station;
- criar fallback automático se não houver rede configurada ou se a conexão falhar;
- criar a rede `ICNP_PROFESSOR_SETUP`;
- manter o IP fallback `192.168.4.1`;
- salvar novas credenciais Wi-Fi;
- escanear redes próximas;
- fornecer lista de redes para a página admin.

#### `src/professor/config_wifi.h`

Cabeçalho da configuração Wi-Fi.

Responsabilidades:

- declarar funções de inicialização Wi-Fi;
- declarar funções de leitura/salvamento de credenciais;
- declarar funções usadas pela API Admin.

---

### 5.2 Arquivos do Aluno

#### `src/aluno/aluno_main.cpp`

Arquivo principal do nó Aluno.

Responsabilidades:

- inicializar LoRa;
- inicializar OLED;
- inicializar sensor MAX30102;
- ler aquisição PPG;
- estimar frequência cardíaca experimental;
- estimar SpO2 experimental;
- medir bateria;
- aguardar `BEACON` do Professor;
- montar pacote `DATA`;
- enviar pacote `DATA` por LoRa;
- aguardar `ACK`;
- atualizar OLED local com FC, SpO2, bateria, sinal e qualidade.

Formato atual do pacote `DATA`:

```text
ICNP;TIPO=DATA;ALUNO=<id>;SEQ=<seq>;CICLO=<ciclo>;FC=<bpm>;SPO2=<%>;BAT=<V>;IR=<valor>;RED=<valor>;DEDO=<0|1>;QUAL=<OK|RUIM|NA>
```

Campos principais:

| Campo | Significado |
|---|---|
| `ALUNO` | Identificador do nó Aluno |
| `SEQ` | Sequência local do pacote |
| `CICLO` | Ciclo ICNP informado pelo Professor |
| `FC` | Frequência cardíaca experimental |
| `SPO2` | Estimativa experimental de SpO2 |
| `BAT` | Tensão da bateria do Aluno |
| `IR` | Valor bruto infravermelho do MAX30102 |
| `RED` | Valor bruto vermelho do MAX30102 |
| `DEDO` | Indica presença de contato/sinal óptico |
| `QUAL` | Qualidade operacional da amostra |

---

### 5.3 Arquivos comuns

#### `src/comum/configuracao_lora.h`

Centraliza parâmetros do rádio LoRa.

Normalmente contém:

- frequência de operação;
- largura de banda;
- spreading factor;
- coding rate;
- potência de transmissão;
- pinos LoRa da placa Heltec.

#### `src/comum/radio_lora.cpp` e `src/comum/radio_lora.h`

Camada de abstração do rádio LoRa.

Responsabilidades:

- inicializar rádio;
- enviar mensagens;
- receber mensagens;
- retornar RSSI;
- retornar SNR;
- isolar detalhes da biblioteca LoRa do restante do firmware.

#### `src/comum/protocolo_icnp.cpp` e `src/comum/protocolo_icnp.h`

Implementam funções auxiliares do protocolo ICNP.

Responsabilidades:

- montar mensagens `BEACON`, `DATA` e `ACK`;
- interpretar mensagens recebidas;
- extrair campos do pacote;
- validar tipo de mensagem;
- manter o formato textual padronizado do protocolo.

Mensagens principais:

```text
ICNP;TIPO=BEACON;PROFESSOR=1;CICLO=41;ALVO=1
ICNP;TIPO=DATA;ALUNO=1;SEQ=27;CICLO=41;FC=70;SPO2=88;BAT=3.43;IR=91740;RED=24692;DEDO=1;QUAL=OK
ICNP;TIPO=ACK;PROFESSOR=1;ALUNO=1;SEQ=27;CICLO=41
```

#### `src/comum/sensor_fisiologico.cpp` e `src/comum/sensor_fisiologico.h`

Módulo do sensor fisiológico experimental.

Responsabilidades:

- inicializar MAX30102;
- ler canais IR e RED;
- detectar presença de dedo/contato;
- calcular frequência cardíaca experimental;
- calcular SpO2 experimental;
- classificar qualidade operacional da amostra;
- fornecer os dados prontos para o pacote ICNP.

> Este módulo deve ser interpretado como aquisição PPG experimental, não como medição clínica.

#### `src/comum/bateria.cpp` e `src/comum/bateria.h`

Módulo de leitura de bateria.

Responsabilidades:

- ler tensão da bateria;
- converter leitura ADC para volts;
- classificar nível operacional de bateria;
- fornecer valor para OLED, DATA e API.

#### `src/comum/oled.cpp` e `src/comum/oled.h`

Módulo de exibição local no OLED.

Responsabilidades:

- mostrar estado do Aluno ou Professor;
- exibir FC, SpO2, bateria e qualidade;
- mostrar estado de BEACON, DATA e ACK;
- evitar sobrescrita visual entre linhas do display.

---

## 6. Como implementar do zero

### Passo 1 - Clonar o repositório

```bash
git clone <URL_DO_REPOSITORIO>
cd <PASTA_DO_REPOSITORIO>
```

### Passo 2 - Abrir no VS Code

Abra a pasta do projeto no Visual Studio Code com a extensão PlatformIO instalada.

### Passo 3 - Conferir o `platformio.ini`

Confirme se existem ambientes separados para Professor e Aluno. Exemplo:

```ini
[env:professor]
platform = espressif32
board = heltec_wifi_lora_32_V2
framework = arduino
monitor_speed = 115200

[env:aluno]
platform = espressif32
board = heltec_wifi_lora_32_V2
framework = arduino
monitor_speed = 115200
```

A nomenclatura exata dos ambientes pode variar conforme a versão do projeto.

### Passo 4 - Configurar o rádio LoRa

Abra:

```text
src/comum/configuracao_lora.h
```

Confirme os parâmetros usados nos ensaios:

```text
Frequência: 915 MHz
BW: 125 kHz
SF: 7
CR: 4/5
```

Use a frequência permitida para sua região e para seu módulo.

### Passo 5 - Gravar o firmware do Professor

Conecte a placa Professor via USB.

No PlatformIO, selecione o ambiente do Professor e envie o firmware.

Via terminal:

```bash
pio run -e professor -t upload
```

Depois abra o monitor serial:

```bash
pio device monitor -b 115200
```

### Passo 6 - Gravar o firmware do Aluno

Conecte a placa Aluno via USB.

No PlatformIO, selecione o ambiente do Aluno e envie o firmware.

Via terminal:

```bash
pio run -e aluno -t upload
```

Depois abra o monitor serial:

```bash
pio device monitor -b 115200
```

### Passo 7 - Ligar o Professor

Ao iniciar, o Professor tenta conectar ao Wi-Fi salvo.

Se não houver Wi-Fi configurado, ele cria a rede fallback:

```text
SSID: ICNP_PROFESSOR_SETUP
Senha: icnp12345
IP: 192.168.4.1
```

### Passo 8 - Configurar o Wi-Fi pela API Admin

Conecte o notebook ou celular na rede:

```text
ICNP_PROFESSOR_SETUP
```

Acesse no navegador:

```text
http://192.168.4.1/admin
```

ou:

```text
http://192.168.4.1/api/admin
```

Na página administrativa:

1. clique em **Carregar lista** ou **Atualizar lista**;
2. selecione a rede Wi-Fi desejada ou digite o SSID manualmente;
3. informe a senha;
4. clique em **Salvar rede Wi-Fi**;
5. aguarde o Professor reiniciar;
6. verifique no Serial Monitor o IP recebido em modo station.

### Passo 9 - Acessar o dashboard

Com o Professor conectado ao Wi-Fi, abra no navegador:

```text
http://IP_DO_PROFESSOR/
```

Exemplo:

```text
http://192.168.254.12/
```

Para consultar os dados em JSON:

```text
http://IP_DO_PROFESSOR/api/status
```

### Passo 10 - Ligar o Aluno

Com o Professor ativo, ligue o Aluno.

O fluxo esperado é:

```text
Professor envia BEACON
Aluno recebe BEACON
Aluno coleta FC, SpO2, IR, RED, bateria e qualidade
Aluno envia DATA
Professor recebe DATA
Professor envia ACK
Aluno valida ACK
Dashboard atualiza os dados
```

### Passo 11 - Conferir o Serial Monitor

No Professor, devem aparecer mensagens semelhantes a:

```text
Recebido: ICNP;TIPO=DATA;ALUNO=1;SEQ=27;CICLO=41;FC=70;SPO2=88;BAT=3.43;IR=91740;RED=24692;DEDO=1;QUAL=OK
RSSI DATA: -42 dBm
SNR DATA: 9.50 dB
Enviando ACK: ICNP;TIPO=ACK;PROFESSOR=1;ALUNO=1;SEQ=27;CICLO=41
CSV;CICLO=41;ALVO=1;ALUNO=1;SEQ=27;FC=70;SPO2=88;IR=91740;RED=24692;DEDO=1;QUAL=OK;RSSI=-42;SNR=9.50;BAT_ALUNO=3.43;ENERGIA_PROF=3.55 V;ACK=1
```

### Passo 12 - Conferir o OLED do Aluno

O OLED deve mostrar, conforme a versão do firmware:

- FC;
- SpO2;
- bateria;
- presença de sinal/dedo;
- qualidade operacional;
- estado de comunicação;
- ACK recebido ou falha.

Sem contato adequado no sensor, o esperado é:

```text
FC=NA
SPO2=NA
DEDO=0
QUAL=NA
```

---

## 7. Fluxo ICNP implementado

### 7.1 BEACON

Mensagem enviada pelo Professor para abrir o ciclo.

```text
ICNP;TIPO=BEACON;PROFESSOR=1;CICLO=<ciclo>;ALVO=<aluno>
```

### 7.2 DATA

Mensagem enviada pelo Aluno com dados operacionais.

```text
ICNP;TIPO=DATA;ALUNO=<id>;SEQ=<seq>;CICLO=<ciclo>;FC=<bpm>;SPO2=<%>;BAT=<V>;IR=<valor>;RED=<valor>;DEDO=<0|1>;QUAL=<OK|RUIM|NA>
```

### 7.3 ACK

Mensagem enviada pelo Professor confirmando recebimento.

```text
ICNP;TIPO=ACK;PROFESSOR=1;ALUNO=<id>;SEQ=<seq>;CICLO=<ciclo>
```

---

## 8. API local

### 8.1 Dashboard

```text
http://IP_DO_PROFESSOR/
```

Mostra:

- Alunos ativos;
- ciclo ICNP;
- sequência;
- FC experimental;
- SpO2 experimental;
- bateria do Aluno;
- energia do Professor;
- RSSI;
- SNR;
- idade da última atualização;
- gráficos em esteira temporal;
- cores operacionais.

### 8.2 Status JSON

```text
http://IP_DO_PROFESSOR/api/status
```

Retorna os últimos estados dos Alunos em JSON.

### 8.3 Administração Wi-Fi

```text
http://IP_DO_PROFESSOR/admin
```

ou:

```text
http://IP_DO_PROFESSOR/api/admin
```

Permite:

- visualizar modo atual de Wi-Fi;
- listar redes próximas;
- selecionar SSID;
- salvar senha;
- reiniciar configuração sem recompilar firmware.

---

## 9. Faixas visuais operacionais

As cores do dashboard são apenas apoio visual operacional. Elas não representam diagnóstico.

### Frequência cardíaca experimental

| Faixa | Interpretação operacional |
|---|---|
| 50 a 120 bpm | Verde |
| 40 a 49 bpm ou 121 a 160 bpm | Atenção |
| Menor que 40 bpm ou maior que 160 bpm | Crítico operacional |

### SpO2 experimental

| Faixa | Interpretação operacional |
|---|---|
| 95% ou mais | Verde |
| 90% a 94% | Atenção |
| Menor que 90% | Crítico operacional |

### Bateria do Aluno

| Faixa | Interpretação operacional |
|---|---|
| 3,50 V ou mais | Verde |
| 3,20 V a 3,49 V | Atenção |
| Menor que 3,20 V | Crítico operacional |

---

## 10. Logs e CSV

O Professor imprime linhas CSV no Serial Monitor para análise posterior.

Exemplo:

```text
CSV;CICLO=41;ALVO=1;ALUNO=1;SEQ=27;FC=70;SPO2=88;IR=91740;RED=24692;DEDO=1;QUAL=OK;RSSI=-42;SNR=9.50;BAT_ALUNO=3.43;ENERGIA_PROF=3.55 V;ACK=1
```

Campos úteis:

| Campo | Uso |
|---|---|
| `CICLO` | Identificação do ciclo ICNP |
| `ALVO` | Aluno chamado pelo Professor |
| `ALUNO` | Aluno que respondeu |
| `SEQ` | Sequência do pacote |
| `FC` | Frequência cardíaca experimental |
| `SPO2` | Estimativa experimental de SpO2 |
| `IR` e `RED` | Leituras brutas do MAX30102 |
| `DEDO` | Presença de contato óptico |
| `QUAL` | Qualidade operacional |
| `RSSI` | Força do sinal LoRa |
| `SNR` | Relação sinal-ruído |
| `BAT_ALUNO` | Bateria do Aluno |
| `ENERGIA_PROF` | Energia/tensão do Professor |
| `ACK` | Confirmação de recebimento |

---

## 11. Como testar

### Teste mínimo de comunicação

1. Grave o firmware do Professor.
2. Grave o firmware do Aluno.
3. Abra o Serial Monitor do Professor.
4. Ligue o Aluno.
5. Verifique se o Professor envia `BEACON`.
6. Verifique se o Aluno envia `DATA`.
7. Verifique se o Professor envia `ACK`.
8. Verifique se o Aluno confirma o `ACK`.

### Teste com sensor PPG

1. Ligue Professor e Aluno.
2. Coloque o dedo no MAX30102.
3. Aguarde estabilização das leituras.
4. Observe `DEDO=1`.
5. Observe `QUAL=OK` quando a amostra estiver estável.
6. Confira FC, SpO2, IR e RED no Serial/OLED/API.
7. Retire o dedo.
8. Confirme se os campos mudam para `FC=NA`, `SPO2=NA`, `DEDO=0`, `QUAL=NA`.

### Teste da API

1. Configure o Wi-Fi do Professor pela página `/admin`.
2. Abra o dashboard `/`.
3. Ligue um ou mais Alunos.
4. Acompanhe a atualização dos cards e gráficos.
5. Abra `/api/status` para verificar o JSON.

---

## 12. Limitações conhecidas

- As leituras de FC e SpO2 são estimativas experimentais por PPG.
- O sistema não realiza validação clínica.
- O sistema não deve ser usado para diagnóstico.
- O desempenho do PPG depende de contato, pressão, posição do sensor e movimento.
- Ensaios com mobilidade intensa ainda exigem avaliação adicional.
- A autonomia energética ainda deve ser medida em campanha específica.
- A latência fim a fim ainda deve ser medida formalmente.
- A escalabilidade com muitos nós deve ser avaliada por novos ensaios ou emulação controlada.

---

## 13. Comandos Git úteis

```bash
git status
git add .
git commit -m "adiciona API admin Wi-Fi com fallback e scan assincrono"
git push
git tag v13-api-admin-wifi
git push origin v13-api-admin-wifi
```

No Windows, o Git pode exibir avisos como:

```text
LF will be replaced by CRLF the next time Git touches it
```

Esse aviso normalmente indica conversão de final de linha e não impede o commit.

---

## 14. Estado atual da dissertação

A versão mais recente documentada é a V13, com:

- template oficial UTFPR/ABNT;
- PDF recompilado;
- ICNP formalizado;
- autômatos operacionais;
- evidências de bancada;
- API local em navegador/TV;
- API Admin Wi-Fi;
- provisionamento sem recompilação;
- roteiro de reprodução;
- linguagem segura para ensaio técnico-operacional.

---

## 15. Próximos passos

- Revisar visualmente o PDF final.
- Finalizar commit/push/tag do firmware.
- Versionar o pacote final da dissertação.
- Preparar apresentação da defesa com base na V13.
- Coletar novo ensaio curto apenas se for necessário como evidência complementar.

---

## 16. Aviso de uso

Este projeto é acadêmico e experimental. Ele demonstra integração funcional entre sensor PPG, protocolo ICNP, LoRa e visualização local. As informações exibidas devem ser usadas apenas para avaliação técnica e operacional do protótipo.
