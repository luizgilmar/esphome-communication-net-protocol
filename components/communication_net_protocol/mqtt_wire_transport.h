#pragma once

#include <cstddef>
#include <cstdint>

#include "mqtt_mailbox.h"

namespace esphome {
namespace communication_net_protocol {

// Byte transport only: the shared application protocol owns correlation,
// deduplication and fallback. No automatic subscription/command dispatch.
class MqttWireTransport {
 public:
  bool subscribe(const char *topic, uint8_t qos = 1);
  bool publish(const char *topic, const uint8_t *payload, size_t length,
               uint8_t qos = 1, bool retain = false);
  bool take_received(char *topic, size_t topic_capacity, uint8_t *payload,
                     size_t payload_capacity, size_t &payload_length) {
    return mailbox_.take(topic, topic_capacity, payload, payload_capacity,
                         payload_length);
  }
  bool available() const;
  uint32_t dropped() const { return mailbox_.dropped(); }

 private:
  MqttMailbox mailbox_{};
};

}  // namespace communication_net_protocol
}  // namespace esphome
