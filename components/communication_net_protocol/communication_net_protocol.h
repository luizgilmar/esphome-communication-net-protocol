#pragma once

#include "esphome/core/component.h"
#include "transaction_tracker.h"

#ifdef USE_MQTT
#include "mqtt_wire_transport.h"
#include "mqtt_mailbox.h"
#endif

namespace esphome {
namespace communication_net_protocol {

// Optional receive-only MQTT probe. No peer ownership or commands are started.
class CommunicationNetProtocolComponent : public Component {
 public:
  void set_device_id(const char *device_id) { this->device_id_ = device_id; }
  void setup() override {}
  void loop() override;
  void dump_config() override;

  TransactionTracker<4> &transactions() { return this->transactions_; }

#ifdef USE_MQTT
  MqttWireTransport &mqtt_wire() { return mqtt_wire_; }
#endif
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  void set_mqtt_command_topic(const char *topic) {
    this->mqtt_command_topic_ = topic;
  }
  uint32_t observed_commands() const { return this->observed_commands_; }
#endif

 protected:
  const char *device_id_{nullptr};
  TransactionTracker<4> transactions_{};
#ifdef USE_MQTT
  MqttWireTransport mqtt_wire_{};
#endif
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  const char *mqtt_command_topic_{nullptr};
  bool mqtt_subscription_registered_{false};
  uint32_t observed_commands_{0};
  // Keep the bounded receive copy off the constrained loopTask stack.
  char received_topic_[MqttMailbox::MAX_TOPIC + 1]{};
  uint8_t received_payload_[MqttMailbox::MAX_PAYLOAD]{};
#endif
};

}  // namespace communication_net_protocol
}  // namespace esphome
