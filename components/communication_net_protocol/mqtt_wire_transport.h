#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "mqtt_mailbox.h"

#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif

namespace esphome {
namespace communication_net_protocol {

// Byte transport only: the shared application protocol owns correlation,
// deduplication and fallback. No automatic subscription/command dispatch.
class MqttWireTransport {
 public:
  bool subscribe(const char *topic, uint8_t qos = 1) {
#ifdef USE_MQTT
    if (mqtt::global_mqtt_client == nullptr || topic == nullptr ||
        topic[0] == '\0' || std::strlen(topic) > MqttMailbox::MAX_TOPIC || qos > 2)
      return false;
    mqtt::global_mqtt_client->subscribe(
        topic, [this](const std::string &received_topic,
                      const std::string &received_payload) {
          (void) this->mailbox_.put(
              received_topic.data(), received_topic.size(),
              reinterpret_cast<const uint8_t *>(received_payload.data()),
              received_payload.size());
        }, qos);
    return true;
#else
    (void) topic;
    (void) qos;
    return false;
#endif
  }
  bool publish(const char *topic, const uint8_t *payload, size_t length,
               uint8_t qos = 1, bool retain = false) {
#ifdef USE_MQTT
    if (!available() || topic == nullptr || topic[0] == '\0' ||
        std::strlen(topic) > MqttMailbox::MAX_TOPIC ||
        (payload == nullptr && length != 0) ||
        length > MqttMailbox::MAX_PAYLOAD || qos > 2) return false;
    return mqtt::global_mqtt_client->publish(
        topic, reinterpret_cast<const char *>(payload), length, qos, retain);
#else
    (void) topic;
    (void) payload;
    (void) length;
    (void) qos;
    (void) retain;
    return false;
#endif
  }
  bool take_received(char *topic, size_t topic_capacity, uint8_t *payload,
                     size_t payload_capacity, size_t &payload_length) {
    return mailbox_.take(topic, topic_capacity, payload, payload_capacity,
                         payload_length);
  }
  bool peek_received(const char *&topic, const uint8_t *&payload,
                     size_t &payload_length) const {
    return this->mailbox_.peek(topic, payload, payload_length);
  }
  void release_received() { this->mailbox_.release(); }
  bool available() const {
#ifdef USE_MQTT
    return mqtt::global_mqtt_client != nullptr &&
           mqtt::global_mqtt_client->is_connected();
#else
    return false;
#endif
  }
  uint32_t dropped() const { return mailbox_.dropped(); }

 private:
  MqttMailbox mailbox_{};
};

}  // namespace communication_net_protocol
}  // namespace esphome
