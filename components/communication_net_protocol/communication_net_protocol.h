#pragma once

#include "esphome/core/component.h"
#include "transaction_tracker.h"
#ifdef USE_COMMUNICATION_NET_INBOUND
#include "inbound_route_registry.h"
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
#include "route_admission.h"
#endif
#include "declarative_inbound_binding.h"
#include "passive_inbound_inspector.h"
#endif
#ifdef USE_COMMUNICATION_NET_ESPNOW_OBSERVER
#include "esphome/components/espnow_net_protocol/espnow_net_protocol.h"
#endif
#include "mqtt_command_decoder.h"

#ifdef USE_MQTT
#include "mqtt_wire_transport.h"
#include "mqtt_mailbox.h"
#endif

namespace esphome {
namespace communication_net_protocol {

// Optional receive-only MQTT probe. No peer ownership or commands are started.
class CommunicationNetProtocolComponent : public Component
#ifdef USE_COMMUNICATION_NET_ESPNOW_OBSERVER
    , public espnow_net_protocol::NetCommandIdentityObserver
#endif
{
 public:
  void set_device_id(const char *device_id) {
    this->device_id_ = device_id;
#ifdef USE_COMMUNICATION_NET_INBOUND
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
    this->route_admission_.set_local_device(device_id);
#endif
#endif
  }
#ifdef USE_COMMUNICATION_NET_ESPNOW_OBSERVER
  void set_espnow_observation_source(
      espnow_net_protocol::EspNowNetProtocolComponent *endpoint) {
    if (endpoint != nullptr) endpoint->set_verified_command_observer(this);
  }
  void on_command_identity(espnow_net_protocol::PeerIndex peer,
                           const espnow_net_protocol::NetCommand &command) override;
#endif
  void setup() override {}
  void loop() override;
  void dump_config() override;

  TransactionTracker<4> &transactions() { return this->transactions_; }
#ifdef USE_COMMUNICATION_NET_INBOUND
  void add_inbound_route(const char *id, const char *resource,
                         const char *command) {
    this->inbound_routes_valid_ &=
        this->inbound_routes_.add(id, resource, command);
  }
  void add_inbound_binding(DeclarativeInboundBinding *binding) {
    if (binding == nullptr || inbound_binding_count_ >= 16) {
      inbound_routes_valid_ = false;
      return;
    }
    inbound_bindings_[inbound_binding_count_++] = binding;
  }
#endif

#ifdef USE_MQTT
  MqttWireTransport &mqtt_wire() { return mqtt_wire_; }
#endif
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  void set_mqtt_command_topic(const char *topic) {
    this->mqtt_command_topic_ = topic;
  }
  void set_mqtt_result_topic(const char *topic) { this->mqtt_result_topic_ = topic; }
  void set_mqtt_outgoing_topic(const char *topic) { this->mqtt_outgoing_topic_ = topic; }
  void set_mqtt_outgoing_target(const char *target) { this->mqtt_outgoing_target_ = target; }
  uint32_t observed_commands() const { return this->observed_commands_; }
  uint32_t valid_commands() const { return this->valid_commands_; }
  uint32_t rejected_commands() const { return this->rejected_commands_; }
#endif

 protected:
  const char *device_id_{nullptr};
  TransactionTracker<4> transactions_{};
#ifdef USE_COMMUNICATION_NET_INBOUND
  InboundRouteRegistry<16> inbound_routes_{};
  bool inbound_routes_valid_{true};
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
  RouteAdmission<8, 16> route_admission_{nullptr, inbound_routes_};
#endif
  DeclarativeInboundBinding *inbound_bindings_[16]{};
  size_t inbound_binding_count_{0};
#endif
#ifdef USE_MQTT
  MqttWireTransport mqtt_wire_{};
#endif
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  const char *mqtt_command_topic_{nullptr};
  const char *mqtt_result_topic_{nullptr};
  const char *mqtt_outgoing_topic_{nullptr};
  const char *mqtt_outgoing_target_{nullptr};
  bool mqtt_command_subscription_registered_{false};
  bool mqtt_result_subscription_registered_{false};
  bool mqtt_outgoing_subscription_registered_{false};
  uint32_t observed_commands_{0};
  uint32_t valid_commands_{0};
  uint32_t rejected_commands_{0};
  uint8_t canonical_command_[192]{};
  // Keep the bounded receive copy off the constrained loopTask stack.
  char received_topic_[MqttMailbox::MAX_TOPIC + 1]{};
  uint8_t received_payload_[MqttMailbox::MAX_PAYLOAD]{};
#endif
};

}  // namespace communication_net_protocol
}  // namespace esphome
