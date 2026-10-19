#pragma once
#include <type_traits>
#include "esphome/components/communication_net_protocol/outbound_destination_registry.h"
#include "esphome/components/communication_net_protocol/outbound_espnow_model_adapter.h"
#include "esphome/components/communication_net_protocol/outbound_mqtt_command_transport.h"
#include "esphome/components/communication_net_protocol/outbound_espnow_command_transport.h"

#ifdef USE_TX_ULTIMATE_ESPNOW
#error Generic shared adapter fixture must compile without TX feature defines
#endif
static_assert(std::is_same_v<
    esphome::communication_net_protocol::outbound::EspNowNetModelAdapter::NetCommand,
    esphome::espnow_net_protocol::NetCommand>);
static_assert(std::is_same_v<
    esphome::communication_net_protocol::outbound::EspNowNetModelAdapter::NetResult,
    esphome::espnow_net_protocol::NetResult>);
