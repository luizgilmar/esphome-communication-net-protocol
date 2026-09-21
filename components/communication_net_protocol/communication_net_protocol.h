#pragma once

#include "esphome/core/component.h"
#include "transaction_tracker.h"
#ifdef USE_COMMUNICATION_NET_INBOUND
#ifndef COMMUNICATION_NET_INBOUND_CAPACITY
#define COMMUNICATION_NET_INBOUND_CAPACITY 16
#endif
#include "inbound_route_registry.h"
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
#include "route_admission.h"
#include "esphome/components/espnow_net_protocol/result_codec.h"
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
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT
#include "light_state_snapshot.h"
#endif

namespace esphome {
namespace communication_net_protocol {

// Optional receive-only MQTT probe. No peer ownership or commands are started.
class CommunicationNetProtocolComponent : public Component
#ifdef USE_COMMUNICATION_NET_ESPNOW_OBSERVER
    , public espnow_net_protocol::NetCommandIdentityObserver
#endif
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
    , public espnow_net_protocol::NetCommandHandler
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
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
  void activate_inbound_executor(
      espnow_net_protocol::EspNowNetProtocolComponent *endpoint) {
    if (endpoint != nullptr) endpoint->set_command_handler(this);
  }
  espnow_net_protocol::NetCommandHandlerStartStatus start(
      const espnow_net_protocol::NetCommand &command, uint32_t now_ms) override;
  void loop(uint32_t now_ms) override;
  bool has_result() const override { return radio_result_ready_; }
  bool take_result(espnow_net_protocol::NetResult &result) override;
  bool cancel(uint64_t transaction_id) override;
  void set_mqtt_inbound_execution(const char *source, const char *reply_topic) {
    mqtt_execution_source_ = source;
    mqtt_execution_reply_ = reply_topic;
  }
#endif
  void setup() override {}
  void loop() override;
  void dump_config() override;
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT
  bool configure_state_snapshot(const char *topic, uint8_t qos, uint32_t interval_ms) {
    return state_snapshot_.configure(topic, qos, interval_ms);
  }
  bool add_snapshot_light_field(const char *field, bool rgb) {
    return state_snapshot_.add_light_field(field, rgb);
  }
  bool add_snapshot_light(light::LightState *state) {
    return state_snapshot_.add_light(state);
  }
  bool add_snapshot_binary_field(const char *field, binary_sensor::BinarySensor *sensor) {
    return state_snapshot_.add_binary_field(field, sensor);
  }
#endif

  TransactionTracker<4> &transactions() { return this->transactions_; }
#ifdef USE_COMMUNICATION_NET_INBOUND
  void add_inbound_route(const char *id, const char *resource,
                         const char *command) {
    this->inbound_routes_valid_ &=
        this->inbound_routes_.add(id, resource, command);
  }
  void add_inbound_binding(DeclarativeInboundBinding *binding) {
    if (binding == nullptr || inbound_binding_count_ >= COMMUNICATION_NET_INBOUND_CAPACITY) {
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
  InboundRouteRegistry<COMMUNICATION_NET_INBOUND_CAPACITY> inbound_routes_{};
  bool inbound_routes_valid_{true};
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
  RouteAdmission<8, COMMUNICATION_NET_INBOUND_CAPACITY> route_admission_{nullptr, inbound_routes_};
  const espnow_net_protocol::NetCommand *verified_command_{nullptr};
  espnow_net_protocol::NetCommand active_command_{};
  espnow_net_protocol::NetResult inbound_result_{};
  espnow_net_protocol::EspNowResultCodec result_codec_{};
  DeclarativeInboundBinding *active_binding_{nullptr};
  const char *mqtt_execution_source_{nullptr};
  const char *mqtt_execution_reply_{nullptr};
  uint32_t inbound_started_ms_{0};
  uint32_t inbound_timeout_ms_{0};
  bool inbound_expected_on_{false};
  bool inbound_active_{false};
  bool radio_waiting_{false};
  bool mqtt_waiting_{false};
  bool radio_result_ready_{false};
  bool mqtt_result_ready_{false};
  espnow_net_protocol::NetCommandHandlerStartStatus start_inbound_(
      const espnow_net_protocol::NetCommand &command, uint32_t now_ms,
      bool radio);
  void finish_inbound_(espnow_net_protocol::NetResult result);
  void receive_mqtt_inbound_(const uint8_t *payload, size_t length);
  void publish_inbound_result_();
#endif
  DeclarativeInboundBinding *inbound_bindings_[COMMUNICATION_NET_INBOUND_CAPACITY]{};
  size_t inbound_binding_count_{0};
#endif
#ifdef USE_MQTT
  MqttWireTransport mqtt_wire_{};
#endif
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT
  LightStateSnapshot state_snapshot_{};
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
