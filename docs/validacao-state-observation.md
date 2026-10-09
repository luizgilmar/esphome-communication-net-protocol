# Candidato de observação de estados

Branch de desenvolvimento; não é uma release estável. O HUB habilita observers opcionalmente e mantém os clientes antigos. O TX ainda requer integração com seu controlador existente antes de ativar o chaveamento automático.

O mecanismo de assinatura, expiração, coalescência e espera progressiva foi compilado e exercitado em ESP32 de bancada com ESPHome 2026.8.0. Os relatórios da bancada acompanham o projeto de integração. Compilação completa no HUB ESP32-S3 e compatibilidade com ESPHome 2026.9.1 ainda pendentes.

Instalação: external_components com source type git, URL deste repositório e ref fixado no SHA completo do commit candidato. Selecionar components: [communication_net_protocol] no HUB. Não copiar fontes para /config/esphome e não habilitar state_query_client ou state_mqtt_handover no HUB/TX: estes adaptadores foram utilizados na bancada; a integração do TX deve usar seu coordenador existente.

A dependência ESP-NOW testada corresponde ao commit edf71b1d240f64c38481cd32baae9140999810c4 de luizgilmar/esphome-espnow-net-protocol. Não houve alteração de código nessa dependência.
