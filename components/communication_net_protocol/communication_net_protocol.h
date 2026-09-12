#pragma once

#include "esphome/core/component.h"
#include "transaction_tracker.h"

#ifdef USE_MQTT
#include "mqtt_wire_transport.h"
#endif

namespace esphome {
namespace communication_net_protocol {

// Foundation only. No subscriptions, peer ownership, or commands are started.
class CommunicationNetProtocolComponent : public Component {
 public:
  void set_device_id(const char *device_id) { this->device_id_ = device_id; }
  void setup() override {}
  void dump_config() override;

  TransactionTracker<4> &transactions() { return this->transactions_; }

#ifdef USE_MQTT
  MqttWireTransport &mqtt_wire() { return mqtt_wire_; }
#endif

 protected:
  const char *device_id_{nullptr};
  TransactionTracker<4> transactions_{};
#ifdef USE_MQTT
  MqttWireTransport mqtt_wire_{};
#endif
};

}  // namespace communication_net_protocol
}  // namespace esphome
