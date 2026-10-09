# Candidato de observação de estados

Branch de desenvolvimento; não é uma release estável. O HUB habilita observers opcionalmente e mantém os clientes antigos. O TX ainda requer integração com seu controlador existente antes de ativar o chaveamento automático.

O mecanismo de assinatura, expiração, coalescência e espera progressiva foi compilado e exercitado em ESP32 de bancada com ESPHome 2026.8.0. Os relatórios da bancada acompanham o projeto de integração. Compilação completa no HUB ESP32-S3 e compatibilidade com ESPHome 2026.9.1 ainda pendentes.

Instalação: external_components com source type git, URL deste repositório e ref fixado no SHA completo do commit candidato. Selecionar components: [communication_net_protocol] no HUB. Não copiar fontes para /config/esphome e não habilitar state_query_client ou state_mqtt_handover no HUB/TX: estes adaptadores foram utilizados na bancada; a integração do TX deve usar seu coordenador existente.

A dependência ESP-NOW testada corresponde ao commit edf71b1d240f64c38481cd32baae9140999810c4 de luizgilmar/esphome-espnow-net-protocol. Não houve alteração de código nessa dependência.

## Integração no coordenador do TX

state_observation_client.h fornece apenas a máquina de controle, sem transporte, callbacks MQTT ou acesso ao rádio. O TX mantém seu coordenador e observador existentes. Consultas MQTT correlacionadas com snapshot v2 completo estabelecem atualidade; retained e pushes não estabelecem prova MQTT. Desconexão observada durante consulta invalida sua prova, inclusive após reconexão. Renovação usa validade fornecida pelo servidor; falhas exigem novo desafio e têm backoff com jitter. O cliente não substitui a política dos comandos normais.

A fixture tests/state_observation_client_test.cpp cobre silêncio MQTT, renovação, desafio vencido, falhas, resposta de sessão antiga e wrap de millis. Sua execução nativa ficou pendente: o Zig neste ambiente falhou ao localizar seu próprio executável. A integração do TX passou em validação de configuração e geração C++; isso não confirma compilação nem comportamento do firmware.
