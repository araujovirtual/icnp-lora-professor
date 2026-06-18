# ICNP/LoRa Professor

Arquitetura vestível experimental baseada em **LoRa ponto-a-ponto**, protocolo **ICNP** e visualização local em navegador para acompanhamento técnico-operacional de dados fisiológicos estimados por PPG.

O projeto foi desenvolvido para reproduzir uma bancada com um nó **Professor** e um ou mais nós **Aluno**. O Professor coordena a comunicação LoRa, recebe os pacotes ICNP, confirma o recebimento por ACK e disponibiliza os últimos dados recebidos em uma API HTTP local. Os nós Aluno realizam aquisição PPG experimental com MAX30102/MH-ET LIVE, estimam frequência cardíaca e SpO2, exibem informações no OLED e enviam os dados ao Professor.

> **Aviso importante:** este projeto é acadêmico e experimental. As leituras de frequência cardíaca e SpO2 são estimativas técnico-operacionais obtidas por aquisição PPG. Este repositório não é um dispositivo médico, não realiza diagnóstico, não substitui equipamento clínico e não possui validação clínica.

---

## 1. Visão geral da arquitetura

A arquitetura segue o modelo **Professor-Aluno**.

| Componente | Papel |
|---|---|
| Nó Professor | Coordena o ciclo ICNP, envia `BEACON`, recebe `DATA`, responde com `ACK`, mede RSSI/SNR e disponibiliza uma API local |
| Nó Aluno | Aguarda `BEACON`, verifica se é o alvo do ciclo, lê o sensor PPG, monta `DATA`, transmite por LoRa e valida o `ACK` |
| ICNP | Protocolo textual de aplicação usado sobre LoRa para organizar o ciclo `BEACON -> DATA -> ACK` |
| API local | Interface HTTP executada no Professor para visualizar os últimos estados recebidos dos Alunos |
| API Admin Wi-Fi | Página local para configurar SSID e senha do Professor sem recompilar o firmware |

Fluxo operacional básico:

```text
Professor envia BEACON com CICLO e ALVO
Aluno correspondente ao ALVO coleta os dados
Aluno envia DATA por LoRa
Professor recebe DATA e mede RSSI/SNR
Professor envia ACK
Aluno valida ACK
Professor atualiza API local
```

---

## 2. Hardware usado

### 2.1 Nó Professor

| Item | Função |
|---|---|
| Heltec WiFi LoRa 32 V2 | Placa principal com ESP32, rádio LoRa SX127x e OLED integrado |
| Antena LoRa 915 MHz | Comunicação LoRa ponto-a-ponto |
| Cabo USB | Alimentação, gravação do firmware e monitor serial |
| Notebook, celular ou TV com navegador | Acesso ao painel local e à página administrativa |

### 2.2 Nó Aluno

| Item | Função |
|---|---|
| Heltec WiFi LoRa 32 V2 | Placa principal do Aluno com ESP32, rádio LoRa SX127x e OLED integrado |
| MAX30102/MH-ET LIVE | Sensor óptico para aquisição PPG experimental |
| Antena LoRa 915 MHz | Comunicação LoRa ponto-a-ponto com o Professor |
| Bateria Li-ion/LiPo ou alimentação USB | Alimentação do nó Aluno |
| Jumpers/fios Dupont | Ligação I2C entre Heltec e MAX30102 |

### 2.3 Ligações principais do MAX30102

| MAX30102/MH-ET LIVE | Heltec WiFi LoRa 32 V2 |
|---|---|
| VIN/VCC | 3V3 ou 5V, conforme o módulo utilizado |
| GND | GND |
| SDA | GPIO4 / SDA |
| SCL | GPIO15 / SCL |

> Antes de alimentar o MAX30102 em 5 V, verifique o módulo utilizado. Alguns módulos possuem regulador e conversão de nível; outros exigem alimentação e nível lógico em 3,3 V.

---

## 3. Software necessário

- Visual Studio Code
- Extensão PlatformIO IDE
- Git
- Driver USB da placa, se necessário
- Navegador web para acessar a API local

---

## 4. Estrutura principal do projeto

Estrutura lógica usada no projeto:

```text
src/
├── comum/
│   ├── display_oled.h
│   ├── led_sync.cpp
│   ├── led_sync.h
│   ├── protocolo_icnp.cpp
│   ├── protocolo_icnp.h
│   ├── radio_lora.cpp
│   ├── radio_lora.h
│   ├── sensor_fisiologico.cpp
│   └── sensor_fisiologico.h
│
├── professor/
│   ├── api_professor.cpp
│   ├── api_professor.h
│   ├── config_wifi.cpp
│   ├── config_wifi.h
│   └── professor_main.cpp
│
├── aluno/
│   └── aluno_main.cpp
│
├── test/
├── .gitignore
├── platformio.ini
└── README.md
```

A organização separa o código específico do Professor, o código específico dos Alunos e os módulos compartilhados em `src/comum`.

---

## 5. O que cada arquivo faz

### 5.1 Arquivos do Professor

#### `src/professor/professor_main.cpp`

Arquivo principal do nó Professor.

Responsabilidades:

- inicializar o rádio LoRa;
- inicializar a API local;
- iniciar o ciclo ICNP;
- selecionar o Aluno alvo;
- enviar `BEACON`;
- aguardar `DATA`;
- validar campos recebidos;
- medir RSSI e SNR;
- enviar `ACK`;
- imprimir registros estruturados no monitor serial;
- atualizar o estado exibido pela API local.

Fluxo lógico:

```text
inicializa Professor
seleciona ALVO
envia BEACON
aguarda DATA
valida DATA
mede RSSI/SNR
envia ACK
atualiza API
inicia próximo ciclo
```

#### `src/professor/api_professor.cpp`

Implementa o servidor HTTP local do Professor.

Responsabilidades:

- disponibilizar o dashboard web;
- expor o endpoint `/api/status`;
- exibir os últimos dados dos Alunos;
- desenhar gráficos em esteira temporal;
- aplicar cores operacionais para FC, SpO2 e bateria;
- disponibilizar `/admin` e `/api/admin` para configuração Wi-Fi.

Endpoints principais:

| Endpoint | Função |
|---|---|
| `/` | Dashboard local com os dados dos Alunos |
| `/api/status` | Estado atual dos Alunos em JSON |
| `/admin` | Página administrativa Wi-Fi |
| `/api/admin` | Página administrativa Wi-Fi |
| `/api/config` | Salvamento de SSID/senha |
| `/api/scan` | Lista de redes Wi-Fi em cache |
| `/api/scan/atualizar` | Solicita nova varredura de redes |

#### `src/professor/api_professor.h`

Cabeçalho da API do Professor.

Responsabilidades:

- declarar estruturas de dados usadas pela API;
- declarar funções públicas da camada HTTP;
- permitir que `professor_main.cpp` atualize os dados exibidos no painel.

Exemplo de estrutura usada pela API:

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
- tentar conexão em modo Wi-Fi station;
- criar fallback automático quando não houver rede configurada;
- criar a rede `ICNP_PROFESSOR_SETUP`;
- manter o IP fallback `192.168.4.1`;
- salvar novas credenciais Wi-Fi;
- escanear redes próximas;
- fornecer a lista de redes para a página administrativa.

#### `src/professor/config_wifi.h`

Cabeçalho da configuração Wi-Fi.

Responsabilidades:

- declarar funções de inicialização Wi-Fi;
- declarar funções de leitura e salvamento de credenciais;
- declarar funções usadas pela API Admin.

---

### 5.2 Arquivos do Aluno

#### `src/aluno/aluno_main.cpp`

Arquivo principal do nó Aluno.

Responsabilidades:

- inicializar LoRa;
- inicializar OLED;
- inicializar MAX30102/MH-ET LIVE;
- ler IR e RED;
- estimar frequência cardíaca experimental;
- estimar SpO2 experimental;
- medir bateria;
- aguardar `BEACON`;
- verificar o campo `ALVO`;
- montar `DATA`;
- enviar `DATA` por LoRa;
- aguardar `ACK`;
- validar `ACK`;
- atualizar OLED local.

Formato atual do pacote `DATA`:

```text
ICNP;TIPO=DATA;ALUNO=<id>;SEQ=<seq>;CICLO=<ciclo>;FC=<bpm>;SPO2=<%>;BAT=<V>;IR=<valor>;RED=<valor>;DEDO=<0|1>;QUAL=<OK|RUIM|NA>
```

Campos principais:

| Campo | Significado |
|---|---|
| `ALUNO` | Identificador do nó Aluno |
| `SEQ` | Sequência local do pacote |
| `CICLO` | Ciclo informado pelo Professor |
| `FC` | Frequência cardíaca experimental |
| `SPO2` | Estimativa experimental de SpO2 |
| `BAT` | Tensão operacional do Aluno |
| `IR` | Canal infravermelho bruto do MAX30102 |
| `RED` | Canal vermelho bruto do MAX30102 |
| `DEDO` | Presença de contato/sinal óptico |
| `QUAL` | Qualidade operacional da amostra |

---

### 5.3 Arquivos comuns

#### `src/comum/radio_lora.cpp` e `src/comum/radio_lora.h`

Camada de abstração do rádio LoRa.

Responsabilidades:

- inicializar o rádio;
- enviar mensagens;
- receber mensagens;
- retornar RSSI;
- retornar SNR;
- isolar detalhes da biblioteca LoRa do restante do firmware.

#### `src/comum/protocolo_icnp.cpp` e `src/comum/protocolo_icnp.h`

Funções auxiliares do protocolo ICNP.

Responsabilidades:

- montar mensagens `BEACON`, `DATA` e `ACK`;
- interpretar mensagens recebidas;
- extrair campos;
- validar tipo de mensagem;
- manter o formato textual padronizado do protocolo.

Mensagens principais:

```text
ICNP;TIPO=BEACON;PROFESSOR=1;CICLO=41;ALVO=1
ICNP;TIPO=DATA;ALUNO=1;SEQ=27;CICLO=41;FC=70;SPO2=88;BAT=3.43;IR=91740;RED=24692;DEDO=1;QUAL=OK
ICNP;TIPO=ACK;PROFESSOR=1;ALUNO=1;SEQ=27;CICLO=41
```

#### `src/comum/sensor_fisiologico.cpp` e `src/comum/sensor_fisiologico.h`

Módulo de aquisição PPG experimental.

Responsabilidades:

- inicializar o MAX30102;
- ler canais IR e RED;
- detectar presença de dedo/contato;
- estimar frequência cardíaca experimental;
- estimar SpO2 experimental;
- classificar qualidade operacional da amostra;
- fornecer dados prontos para o pacote ICNP.

> Este módulo realiza aquisição PPG experimental. Ele não deve ser interpretado como medição clínica.

#### `src/comum/display_oled.h`

Funções de exibição local no OLED.

Responsabilidades:

- exibir estado do Professor ou Aluno;
- mostrar FC, SpO2, bateria e qualidade;
- mostrar estado de `BEACON`, `DATA` e `ACK`;
- evitar sobrescrita visual entre linhas do display.

#### `src/comum/led_sync.cpp` e `src/comum/led_sync.h`

Sinalização visual por LED.

Responsabilidades:

- indicar atividade de transmissão/recepção;
- sinalizar ciclo ICNP;
- auxiliar depuração de bancada.

---

## 6. Configuração do PlatformIO

Exemplo de `platformio.ini` com três ambientes: Professor, Aluno 1 e Aluno 2.

```ini
[env]
platform = espressif32
board = heltec_wifi_lora_32_V2
framework = arduino
monitor_speed = 115200
lib_deps =
    sandeepmistry/LoRa
    thingpulse/ESP8266 and ESP32 OLED driver for SSD1306 displays
    sparkfun/SparkFun MAX3010x Pulse and Proximity Sensor Library
build_flags =
    -I src/comum

[env:professor]
build_src_filter =
    +<professor/>
    +<comum/>
    -<aluno/>
upload_port = COM5
monitor_port = COM5

[env:aluno1]
build_src_filter =
    +<aluno/>
    +<comum/>
    -<professor/>
upload_port = COM9
monitor_port = COM9
build_flags =
    ${env.build_flags}
    -DID_ALUNO_CONFIG=\"1\"

[env:aluno2]
build_src_filter =
    +<aluno/>
    +<comum/>
    -<professor/>
upload_port = COM12
monitor_port = COM12
build_flags =
    ${env.build_flags}
    -DID_ALUNO_CONFIG=\"2\"
```

As portas `COM5`, `COM9` e `COM12` são exemplos do computador usado nos testes. Em outro computador, substitua por `COMX`, conforme a porta detectada pelo Windows:

```ini
upload_port = COMX
monitor_port = COMX
```

No Linux, o formato normalmente será semelhante a:

```ini
upload_port = /dev/ttyUSB0
monitor_port = /dev/ttyUSB0
```

ou:

```ini
upload_port = /dev/ttyACM0
monitor_port = /dev/ttyACM0
```

---

## 7. Como implementar

### Passo 1 - Clonar o repositório

```bash
git clone https://github.com/araujoivirtual/icnp-lora-professor.git
cd icnp-lora-professor
```

### Passo 2 - Abrir no VS Code

Abra a pasta do projeto no Visual Studio Code com a extensão PlatformIO instalada.

### Passo 3 - Conferir o `platformio.ini`

Confirme:

- placa `heltec_wifi_lora_32_V2`;
- bibliotecas em `lib_deps`;
- ambientes `professor`, `aluno1` e `aluno2`;
- portas seriais corretas para cada placa.

### Passo 4 - Gravar o Professor

Conecte a placa Professor via USB.

Via PlatformIO:

```bash
pio run -e professor -t upload
```

Para abrir o monitor serial:

```bash
pio device monitor -p COMX -b 115200
```

Substitua `COMX` pela porta da placa Professor.

### Passo 5 - Gravar o Aluno 1

Conecte a placa do Aluno 1 via USB.

```bash
pio run -e aluno1 -t upload
```

### Passo 6 - Gravar o Aluno 2

Conecte a placa do Aluno 2 via USB.

```bash
pio run -e aluno2 -t upload
```

### Passo 7 - Ligar o Professor

Ao iniciar, o Professor tenta conectar ao Wi-Fi salvo.

Quando não há rede configurada, ele cria a rede fallback:

```text
SSID: ICNP_PROFESSOR_SETUP
Senha: icnp12345
IP: 192.168.4.1
```

### Passo 8 - Configurar Wi-Fi pela API Admin

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
2. selecione a rede desejada ou digite o SSID manualmente;
3. informe a senha;
4. clique em **Salvar rede Wi-Fi**;
5. aguarde o Professor reiniciar;
6. verifique no monitor serial o IP recebido em modo station.

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

### Passo 10 - Ligar os Alunos

Com o Professor ativo, ligue os nós Aluno.

Fluxo esperado:

```text
Professor envia BEACON
Aluno alvo recebe BEACON
Aluno coleta FC, SpO2, IR, RED, bateria e qualidade
Aluno envia DATA
Professor recebe DATA
Professor envia ACK
Aluno valida ACK
Dashboard atualiza os dados
```

---

## 8. Fluxo ICNP implementado

### 8.1 BEACON

Mensagem enviada pelo Professor para abrir o ciclo.

```text
ICNP;TIPO=BEACON;PROFESSOR=1;CICLO=<ciclo>;ALVO=<aluno>
```

### 8.2 DATA

Mensagem enviada pelo Aluno com dados operacionais.

```text
ICNP;TIPO=DATA;ALUNO=<id>;SEQ=<seq>;CICLO=<ciclo>;FC=<bpm>;SPO2=<%>;BAT=<V>;IR=<valor>;RED=<valor>;DEDO=<0|1>;QUAL=<OK|RUIM|NA>
```

### 8.3 ACK

Mensagem enviada pelo Professor confirmando recebimento.

```text
ICNP;TIPO=ACK;PROFESSOR=1;ALUNO=<id>;SEQ=<seq>;CICLO=<ciclo>
```

---

## 9. Saída esperada no monitor serial

O monitor serial do Professor deve mostrar o ciclo ICNP, o pacote recebido, RSSI, SNR e o ACK enviado.

Exemplo:

```text
Recebido: ICNP;TIPO=DATA;ALUNO=1;SEQ=27;CICLO=41;FC=70;SPO2=88;BAT=3.43;IR=91740;RED=24692;DEDO=1;QUAL=OK
RSSI DATA: -42 dBm
SNR DATA: 9.50 dB
Enviando ACK: ICNP;TIPO=ACK;PROFESSOR=1;ALUNO=1;SEQ=27;CICLO=41
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
| `BAT` | Tensão operacional do Aluno |
| `ACK` | Confirmação de recebimento |

---

## 10. API local

### 10.1 Dashboard

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

### 10.2 Status JSON

```text
http://IP_DO_PROFESSOR/api/status
```

Retorna os últimos estados dos Alunos em JSON.

### 10.3 Administração Wi-Fi

```text
http://IP_DO_PROFESSOR/admin
```

ou:

```text
http://IP_DO_PROFESSOR/api/admin
```

Permite:

- visualizar o modo atual de Wi-Fi;
- listar redes próximas;
- selecionar SSID;
- salvar senha;
- reconfigurar o Professor sem recompilar firmware.

---

## 11. Faixas visuais operacionais

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

## 12. Como testar

### 12.1 Teste mínimo de comunicação

1. Grave o firmware do Professor.
2. Grave o firmware de pelo menos um Aluno.
3. Abra o monitor serial do Professor.
4. Ligue o Aluno.
5. Verifique se o Professor envia `BEACON`.
6. Verifique se o Aluno envia `DATA`.
7. Verifique se o Professor envia `ACK`.
8. Verifique se o Aluno valida o `ACK`.

### 12.2 Teste com sensor PPG

1. Ligue Professor e Aluno.
2. Coloque o dedo no MAX30102.
3. Aguarde estabilização das leituras.
4. Observe `DEDO=1`.
5. Observe `QUAL=OK` quando a amostra estiver estável.
6. Confira FC, SpO2, IR e RED no monitor serial, OLED ou API.
7. Retire o dedo.
8. Confirme se os campos mudam para `FC=NA`, `SPO2=NA`, `DEDO=0`, `QUAL=NA`.

### 12.3 Teste da API

1. Configure o Wi-Fi do Professor pela página `/admin`.
2. Abra o dashboard `/`.
3. Ligue um ou mais Alunos.
4. Acompanhe a atualização dos cards e gráficos.
5. Abra `/api/status` para verificar o JSON.

---

## 13. Limitações conhecidas

- As leituras de FC e SpO2 são estimativas experimentais por PPG.
- O sistema não realiza validação clínica.
- O sistema não deve ser usado para diagnóstico.
- O desempenho do PPG depende de contato, pressão, posição do sensor e movimento.
- Ensaios com mobilidade intensa exigem avaliação adicional.
- A autonomia energética deve ser medida em campanha específica.
- A latência fim a fim deve ser medida formalmente.
- A escalabilidade com muitos nós deve ser avaliada por novos ensaios ou emulação controlada.

---

## 14. Comandos Git úteis

```bash
git status
git add .
git commit -m "atualiza firmware ICNP LoRa"
git push
```

No Windows, o Git pode exibir avisos como:

```text
LF will be replaced by CRLF the next time Git touches it
```

Esse aviso normalmente indica conversão de final de linha e não impede o commit.

---

## 15. Licença e uso

Este projeto é acadêmico e experimental. Ele demonstra integração funcional entre sensor PPG, protocolo ICNP, comunicação LoRa e visualização local. As informações exibidas devem ser usadas apenas para avaliação técnica e operacional do protótipo.
