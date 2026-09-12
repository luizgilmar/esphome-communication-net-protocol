#include "mqtt_wire_transport.h"

#ifdef USE_MQTT
#include <cstring>

#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome {
namespace communication_net_protocol {

bool MqttWireTransport::available() const {
  return mqtt::global_mqtt_client != nullptr &&
         mqtt::global_mqtt_client->is_connected();
}

bool MqttWireTransport::subscribe(const char *topic, uint8_t qos) {
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
}

bool MqttWireTransport::publish(const char *topic, const uint8_t *payload,
                                size_t length, uint8_t qos, bool retain) {
  if (!available() || topic == nullptr || topic[0] == '\0' ||
      std::strlen(topic) > MqttMailbox::MAX_TOPIC ||
      (payload == nullptr && length != 0) ||
      length > MqttMailbox::MAX_PAYLOAD || qos > 2) return false;
  return mqtt::global_mqtt_client->publish(
      topic, reinterpret_cast<const char *>(payload), length, qos, retain);
}

}  // namespace communication_net_protocol
}  // namespace esphome
#endif  // USE_MQTT
