# Communication NetProtocol — limites e configuração declarativa

Estado: proposta de arquitetura; os exemplos abaixo **não são schema implementado**.

## Objetivo

Um único modelo de transação de aplicação para comandos entre dispositivos, independentemente de MQTT ou ESP-NOW. Preservar os tópicos e fluxos já validados no TX Quartogian e no HUB enquanto o package MQTT existente é migrado recurso a recurso. O protocolo não substitui o MQTT de entidades do Home Assistant nem as ligações físicas do ESP-NOW.

## Responsabilidades

| Camada | É responsável por | Não é responsável por |
| --- | --- | --- |
| `mqtt:` nativo do ESPHome | Broker, credenciais, conexão e entidades normais do HA (sensores, luzes, switches). | Lifecycle de comandos entre dispositivos. |
| `espnow_net_protocol` | Canal, PMK, peers (MAC/LMK), frames e entrega confiável de rádio. Pode funcionar sozinho em usos simples. | Política MQTT/ESP-NOW ou confirmação de efeito da aplicação. |
| `communication_net_protocol` | Identidade de dispositivo, recursos e destinos, transações, sessões, correlação, resultados, deduplicação entre transportes, timeouts e fallback. | Configuração de rádio ou renderização UX. |
| Adaptadores MQTT/ESP-NOW | Traduzir mensagens entre o núcleo e o transporte; usar o cliente MQTT nativo ou o componente ESP-NOW existente. | Decidir separadamente lifecycle e significado de sucesso. |
| Dispositivo (TX/HUB/outros) | Declarar ações locais e recursos, executar ação, confirmar estado quando aplicável, consumir resultado para UX. | Reimplementar protocolo e política de retries. |

Entrega de rádio, aceitação da operação, `IN_PROGRESS` e resultado terminal são fatos diferentes. O resultado só é `SUCCEEDED` quando o executor confirma o efeito previsto, conforme o contrato do recurso; recebimento de frame ou publicação MQTT não bastam. Uma indisponibilidade final gera falha terminal; a UX do TX poderá mostrá-la em vermelho sem alterar o erro técnico.

## Tópicos MQTT

1. **Entidades normais:** seguem `mqtt:` e as plataformas nativas do ESPHome. O núcleo de comandos não passa a criar manualmente tópicos de cada sensor do HA.
2. **Comandos:** tópico de entrada do destino, inicialmente `tx/commands/quartogian/command`.
3. **Resultados:** tópico de resposta do solicitante, inicialmente `tx/results/tx-quartogian-integration`; contêm o mesmo identificador de transação usado no pedido.
4. **Sessões:** anúncio de identidade/boot, inicialmente `tx/sessions/tx-quartogian-integration`, para impedir que respostas tardias de outra inicialização sejam aceitas.
5. **Estado observado:** snapshot retido separado de resultados, inicialmente `tx/status/quartogian/lights`. Atualização de sensor/estado pode chegar depois de um resultado terminal e não deve ser tratada como confirmação automática da mesma transação.

O padrão deve derivar os tópicos de prefixos e IDs estáveis, com override explícito para migração. Configuração de QoS, sessão limpa e política de retenção deve distinguir **comandos momentâneos (não reexecutar após reboot)** de **snapshots de estado (retidos quando apropriado)**. Resultados e sessão exigem identidade/correlação verificáveis.

## Relação declarativa no YAML

Exemplo de **desenho de schema**, sujeito a validação em ESPHome e alterações antes da implementação:

```yaml
mqtt:
  id: device_mqtt
  broker: !secret mqtt_broker

espnow_net_protocol:
  id: espnow_network
  channel: 1
  pmk: !secret espnow_pmk
  peers:
    - id: quartogian
      address: !secret espnow_quartogian_mac
      lmk: !secret espnow_quartogian_lmk

communication_net_protocol:
  id: command_network
  device_id: tx-quartogian-integration
  mqtt:
    mqtt_id: device_mqtt
    command_prefix: tx/commands
    result_prefix: tx/results
    session_prefix: tx/sessions
  esp_now:
    espnow_net_protocol_id: espnow_network
  policies:
    - id: normal
      transports: [mqtt, esp_now]
  destinations:
    - id: quartogian
      policy: normal
      espnow_peer: quartogian
```

O peer é cadastrado **somente** em `espnow_net_protocol`; `espnow_peer` referencia seu ID, sem duplicar MAC ou LMK. O destino lógico fornece o nome para roteamento MQTT. Cada destino/ação usa uma política ordenada: MQTT primário/ESP-NOW contingência em produção; ESP-NOW primário/MQTT contingência em testes controlados. Se apenas um transporte for compilado, o núcleo precisa manter o mesmo contrato de lifecycle com a política válida para essa configuração.

No executor, os recursos vinculam comandos a ações/entidades locais de forma declarativa; o schema exato de `resources` será fechado após validar as APIs reais do ESPHome. Evitar lambdas de parsing, globals de sessão e construção manual de JSON no YAML. Não configurar simultaneamente uma binding direta `espnow_net_protocol` e uma binding do novo núcleo para o mesmo comando: um único dono deve despachar a operação.

## Garantias antes de habilitar fallback

- A identidade de transação permanece igual através das tentativas MQTT e ESP-NOW; o executor mantém deduplicação compartilhada por origem, sessão/boot e transação, com resultado terminal reutilizável.
- Um `toggle` não é idempotente: se a ação ocorreu mas a resposta se perdeu, o segundo transporte **não** pode executar outro toggle. Confirmar semântica de retry/fallback para `open`, `close`, `stop`, cor, brilho e cenas antes da migração de cada recurso.
- Separar timeout de entrega, rejeição de execução, execução em andamento e conclusão; só iniciar fallback quando a política permitir e quando não houver execução remota conhecida que possa causar duplicidade.
- Recursos que dependem do HA não devem prometer conclusão offline. Identificar explicitamente executor local, dependência externa e confirmação de efeito.
- Dimensionar filas e cache por capacidade de compilação e observar margem da `loopTask`, inclusive na falha terminal de ambos os transportes.

## Migração incremental do HUB

1. Definir e testar contrato de mensagem, correlação, sessões, deduplicação cruzada e tópicos; preservar compatibilidade com os tópicos atuais.
2. Migrar `light/spot_chuveiro` como piloto, comparando comando, estado final, duplicatas, timeout, fallback e recuperação. Remover apenas o trecho correspondente do package legado após validação.
3. Migrar fitas: ligar/desligar, brilho, cor e efeitos, verificando estado observado e resultado.
4. Migrar persiana: `open`, `close` e `stop`, `IN_PROGRESS`, prazo estimado, conclusão e parada segura.
5. Migrar cenas após seus recursos; registrar dependências de HA e confirmação local/remota.
6. Remover as lambdas/globals/scripts restantes do bridge MQTT somente quando cada recurso tiver teste de equivalência e contingência. Manter o package antigo para os recursos ainda não migrados, sem duplicar rota.

Critério de fechamento: comandos funcionam com MQTT disponível, com ESP-NOW como contingência, com ambos indisponíveis (falha terminal sem efeito duplicado ou reboot) e após recuperação, incluindo cold start sem infraestrutura para os recursos declarados offline-capable.
