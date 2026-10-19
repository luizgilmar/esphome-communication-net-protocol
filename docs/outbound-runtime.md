# Servico de comandos compartilhado — candidato 19/10/2026

`communication_outbound` provisiona o mesmo servico de admissao e os adaptadores MQTT/ESP-NOW extraidos do TX. Configuracao declarativa: identidade de origem, politicas e destinos associados a peers autenticados. Nao executa automacoes especificas do ambiente.

Ha duas vagas normais para MQTT e uma vaga separada para interrupcao. Politicas que incluem ESP-NOW usam apenas uma vaga normal; a segunda capacidade fisica do endpoint fica reservada para interrupcao (por exemplo STOP). Nao ha fila de repeticao automatica por chamadas da interface. O endpoint deve ter um unico responsavel pelos comandos de saida; nao combinar com TX/state_query_client no mesmo radio nem usar comandos diretos concorrentes.

O NSPanel pode associar `outbound_service` ao cadastro de recursos. A admissao de um comando nao publica estado confirmado nos sinalizadores. As respostas sao acompanhadas pelo servico e registradas em log; a conexao entre snapshots recebidos e sinalizadores continua pendente.

Validacao desta etapa: 113 testes Python passaram; ESPHome 2026.9.1 gerou codigo das fixtures do servico e da integracao NSPanel. A compilacao C++ completa e o ensaio de comandos no hardware ainda estao pendentes. As duas compilacoes anteriores dos adaptadores compartilhados foram confirmadas pelo usuario.

Fixtures somente para compilacao, sem credenciais reais e sem instalacao: `examples/outbound_runtime_compile.yaml` neste repositorio e `examples/outbound-runtime-check.yaml` no repositorio NSPanel. Nenhuma alteracao no firmware instalado ou nos pins Git de producao.
