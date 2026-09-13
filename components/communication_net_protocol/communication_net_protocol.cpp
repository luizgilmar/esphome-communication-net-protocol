#include "communication_net_protocol.h"

#include "esphome/core/log.h"

namespace esphome {
namespace communication_net_protocol {

static const char *const TAG = "communication_net_protocol";

void CommunicationNetProtocolComponent::loop() {
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  if (this->mqtt_command_topic_ == nullptr) return;
  if (!this->mqtt_subscription_registered_) {
    this->mqtt_subscription_registered_ =
        this->mqtt_wire_.subscribe(this->mqtt_command_topic_);
    if (this->mqtt_subscription_registered_)
      ESP_LOGI(TAG, "MQTT command observation subscribed");
    return;
  }
  size_t payload_length = 0;
  if (this->mqtt_wire_.take_received(
          this->received_topic_, sizeof(this->received_topic_),
          this->received_payload_, sizeof(this->received_payload_),
          payload_length)) {
    ++this->observed_commands_;
    ESP_LOGD(TAG, "MQTT command observed bytes=%u count=%u (not executed)",
             static_cast<unsigned>(payload_length),
             static_cast<unsigned>(this->observed_commands_));
    size_t canonical_length = 0;
    if (decode_mqtt_command_probe(this->received_payload_, payload_length,
                                  this->device_id_, this->canonical_command_,
                                  sizeof(this->canonical_command_), canonical_length)) {
      ++this->valid_commands_;
      ESP_LOGI(TAG, "MQTT canonical command validated bytes=%u count=%u (not executed)",
               static_cast<unsigned>(canonical_length),
               static_cast<unsigned>(this->valid_commands_));
    } else {
      ++this->rejected_commands_;
      ESP_LOGD(TAG, "MQTT probe payload rejected count=%u (not executed)",
               static_cast<unsigned>(this->rejected_commands_));
    }
  }
#endif
}

void CommunicationNetProtocolComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Communication NetProtocol: FOUNDATION ONLY (routing disabled)");
  ESP_LOGCONFIG(TAG, "  Device ID: %s", this->device_id_ == nullptr ? "" : this->device_id_);
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  ESP_LOGCONFIG(TAG, "  MQTT command observation: %s",
                this->mqtt_command_topic_ == nullptr ? "DISABLED" : "RECEIVE ONLY");
#endif
}

}  // namespace communication_net_protocol
}  // namespace esphome
