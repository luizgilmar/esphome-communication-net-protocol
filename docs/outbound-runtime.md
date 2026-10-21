# Servico de comandos compartilhado — candidato 19/10/2026

`communication_outbound` provisiona o mesmo servico de admissao e os adaptadores MQTT/ESP-NOW extraidos do TX. Configuracao declarativa: identidade de origem, politicas e destinos associados a peers autenticados. Nao executa automacoes especificas do ambiente.

Ha duas vagas normais para MQTT e uma vaga separada para interrupcao. Politicas que incluem ESP-NOW usam apenas uma vaga normal; a segunda capacidade fisica do endpoint fica reservada para interrupcao (por exemplo STOP). Nao ha fila de repeticao automatica por chamadas da interface. O endpoint deve ter um unico responsavel pelos comandos de saida; nao combinar com TX/state_query_client no mesmo radio nem usar comandos diretos concorrentes.

O NSPanel pode associar `outbound_service` ao cadastro de recursos. A admissao de um comando nao publica estado confirmado nos sinalizadores. As respostas sao acompanhadas pelo servico e registradas em log; a conexao entre snapshots recebidos e sinalizadores continua pendente.

Validacao desta etapa: 113 testes Python passaram; ESPHome 2026.9.1 gerou codigo das fixtures do servico e da integracao NSPanel. A compilacao C++ completa e o ensaio de comandos no hardware ainda estao pendentes. As duas compilacoes anteriores dos adaptadores compartilhados foram confirmadas pelo usuario.

Fixtures somente para compilacao, sem credenciais reais e sem instalacao: `examples/outbound_runtime_compile.yaml` neste repositorio e `examples/outbound-runtime-check.yaml` no repositorio NSPanel. Nenhuma alteracao no firmware instalado ou nos pins Git de producao.


## Encaminhamento de publicacoes — candidato 20/10/2026

Runtime inclui registro limitado de PushSnapshotObserver e um unico callback do endpoint para resultados espontaneos. Filtra peer configurado, namespace de transacao (bit 63), consistencia e sucesso, converte pelo mesmo EspNowNetModelAdapter e entrega destino/identidade ao observador. Nao publica estado do dispositivo por ACK de comando. O modo de consulta anterior continua disponivel; nspanel_snapshot e responsavel por assinatura e versoes.

StateObservationClient admite limite opcional de renovacao para respeitar o frescor do consumidor. Zero preserva a cadencia anterior validity/3 do TX; configurado usa min(validity/3, limite). Cinco verificacoes C++ em tests/state_observation_timing_compile.h e limite de 64 bytes acompanham a compilacao candidata. Detalhes e ensaio em bancada-cc2/docs/signal-subscription.md. Os pins de firmware instalados nao mudaram.


## Policy de observacao e decoder MQTT — candidato 21/10/2026

CommandDraft pode indicar observation_policy configurada somente para state/snapshot ou state/observation/v1. O servico rejeita override para atuacoes e mantem o vinculo destino/device_id; request.policy_id registra a policy efetiva. Isso permite provar MQTT isoladamente e manter controles de lease no radio, sem duas identidades para o mesmo destino. Chamadas antigas sem override usam a policy padrao.

state_fields_mqtt_decoder.h converte o payload v2 de LightStateSnapshot em state-fields/v2 binario, com limite de entrada 447 bytes e validacao tipada, apenas quando USE_NSPANEL_SNAPSHOT_MQTT esta habilitado. nspanel_snapshot controla prova, sessao MQTT, versao e aplicacao. Ensaios em bancada-cc2/docs/signal-handover.md; compilacao completa e execucao deste candidato ainda pendentes.
