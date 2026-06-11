\# Primeiro teste funcional do ICNP



Data: 2026-06-11



\## Hardware



\- 2 placas Heltec WiFi LoRa 32 V2

\- Frequência: 915 MHz

\- SF: 7

\- BW: 125 kHz

\- CR: 4/5



\## Fluxo validado



1\. Professor envia BEACON.

2\. Aluno recebe BEACON.

3\. Aluno envia DATA.

4\. Professor recebe DATA.

5\. Professor envia ACK.

6\. Aluno valida ACK.



\## Exemplo de pacote



BEACON:

ICNP;TIPO=BEACON;PROFESSOR=1;CICLO=22



DATA:

ICNP;TIPO=DATA;ALUNO=1;SEQ=11;CICLO=22;FC=72;SPO2=98



ACK:

ICNP;TIPO=ACK;PROFESSOR=1;ALUNO=1;SEQ=11;CICLO=22



\## Observação



Este teste valida a comunicação funcional mínima do protocolo ICNP em bancada, ainda com dados fisiológicos simulados.



\# Resumo do primeiro ensaio funcional do ICNP em bancada



Data: 2026-06-11



\## Objetivo



Registrar o primeiro ensaio funcional do protocolo ICNP em bancada, utilizando duas placas Heltec WiFi LoRa 32 V2, uma configurada como nó Professor e outra como nó Aluno.



O objetivo do ensaio foi verificar o ciclo mínimo de comunicação:



1\. Professor envia BEACON.

2\. Aluno recebe BEACON.

3\. Aluno envia DATA.

4\. Professor recebe DATA.

5\. Professor envia ACK.

6\. Aluno valida ACK.



\## Hardware utilizado



\- 2 placas Heltec WiFi LoRa 32 V2

\- Antenas LoRa compatíveis

\- Comunicação LoRa em 915 MHz

\- Computador com VS Code e PlatformIO



\## Configuração LoRa



\- Frequência: 915 MHz

\- Spreading Factor: 7

\- Bandwidth: 125 kHz

\- Coding Rate: 4/5



\## Arquivos de evidência



Prints salvos externamente:



\- TESTE\_BANCADA\_11062026\_ALUNO.png

\- TESTE\_BANCADA\_11062026\_PROFESSOR.png



Logs salvos no repositório:



\- dados/logs/dados\_icnp\_professor.csv

\- dados/logs/dados\_icnp\_professor\_limpo.csv



\## Resultado observado



Foram registrados 40 ciclos válidos do protocolo ICNP, do ciclo 35 ao ciclo 74.



Todos os ciclos apresentaram ACK positivo.



\## Síntese dos dados



| Métrica | Valor observado |

|---|---:|

| Total de ciclos registrados | 40 |

| ACKs positivos | 40 |

| Taxa de ACK | 100% |

| RSSI mínimo observado | -65 dBm |

| RSSI máximo observado | -49 dBm |

| SNR mínimo observado | 9,25 dB |

| SNR máximo observado | 10,25 dB |



\## Interpretação preliminar



O ensaio confirmou a operação funcional mínima do ICNP em bancada. O nó Professor iniciou os ciclos por meio de mensagens BEACON, o nó Aluno respondeu com mensagens DATA, e o Professor confirmou o recebimento por meio de mensagens ACK.



Os dados registrados indicam estabilidade no enlace LoRa durante o ensaio, com 100% de confirmações positivas nos 40 ciclos observados. Os valores de RSSI e SNR são compatíveis com uma condição de bancada próxima, sem obstrução relevante e com comunicação direta entre os dispositivos.



Este resultado não representa validação fisiológica do sistema, pois os valores de FC e SpO2 utilizados no pacote DATA ainda são simulados. O resultado deve ser interpretado como validação funcional inicial da comunicação ICNP sobre LoRa em ambiente de bancada.

