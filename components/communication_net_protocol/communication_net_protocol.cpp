#include "communication_net_protocol.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "mqtt_envelope_decoder.h"
#include "mqtt_result_decoder.h"
#include <cstring>

namespace esphome {
namespace communication_net_protocol {

static const char *const TAG = "communication_net_protocol";

#ifdef USE_COMMUNICATION_NET_ESPNOW_OBSERVER
void CommunicationNetProtocolComponent::on_command_identity(
    espnow_net_protocol::PeerIndex peer,
    const espnow_net_protocol::NetCommand &command) {
  uint8_t canonical[192]{};
  size_t length = 0;
  const char *route = nullptr;
  const bool matched = inspect_inbound_command(
      this->inbound_routes_, this->device_id_, command.device_id.c_str(),
      command.resource.c_str(), command.name.c_str(), command.payload.data(),
      command.payload.size(), route, canonical, sizeof(canonical), length);
  ESP_LOGI(TAG,
           "ESP-NOW verified source observed peer=%u tx=%llu route=%s canonical_bytes=%u (no admission or execution)",
           static_cast<unsigned>(peer),
           static_cast<unsigned long long>(command.transaction_id),
           matched ? route : "<unmatched>",
           static_cast<unsigned>(length));
}
#endif

void CommunicationNetProtocolComponent::loop() {
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  if (this->mqtt_command_topic_ && !this->mqtt_command_subscription_registered_) {
    this->mqtt_command_subscription_registered_ =
        this->mqtt_wire_.subscribe(this->mqtt_command_topic_);
    if (this->mqtt_command_subscription_registered_)
      ESP_LOGI(TAG, "MQTT command observation subscribed");
    return;
  }
  if (this->mqtt_result_topic_ && !this->mqtt_result_subscription_registered_) {
    this->mqtt_result_subscription_registered_ =
        this->mqtt_wire_.subscribe(this->mqtt_result_topic_);
    if (this->mqtt_result_subscription_registered_)
      ESP_LOGI(TAG, "MQTT result observation subscribed");
    return;
  }
  if (this->mqtt_outgoing_topic_ && !this->mqtt_outgoing_subscription_registered_) {
    this->mqtt_outgoing_subscription_registered_ =
        this->mqtt_wire_.subscribe(this->mqtt_outgoing_topic_);
    if (this->mqtt_outgoing_subscription_registered_)
      ESP_LOGI(TAG, "MQTT outgoing command observation subscribed");
    return;
  }
  if (this->mqtt_outgoing_topic_) {
    this->transactions_.expire(millis());
    TransactionEvent event{};
    (void) this->transactions_.take_event(event);
  }
  size_t payload_length = 0;
  if (this->mqtt_wire_.take_received(
          this->received_topic_, sizeof(this->received_topic_),
          this->received_payload_, sizeof(this->received_payload_),
          payload_length)) {
    if (this->mqtt_result_topic_ &&
        std::strcmp(this->received_topic_, this->mqtt_result_topic_) == 0) {
      MqttResultObservation result{};
      if (!decode_mqtt_result(this->received_payload_, payload_length, result)) {
        ESP_LOGD(TAG, "MQTT result rejected bytes=%u (observation only)",
                 static_cast<unsigned>(payload_length));
      } else {
        const auto correlation = this->transactions_.observe_result(
            result.transaction_id, result.stage, millis());
        ESP_LOGI(TAG, "MQTT result observed id=%llu stage=%u correlation=%u (observation only)",
                 static_cast<unsigned long long>(result.transaction_id),
                 static_cast<unsigned>(result.stage), static_cast<unsigned>(correlation));
      }
      return;
    }
    if (this->mqtt_outgoing_topic_ &&
        std::strcmp(this->received_topic_, this->mqtt_outgoing_topic_) == 0) {
      MqttCommandIdentity identity{};
      size_t canonical_length = 0;
      if (decode_mqtt_command_envelope(
              this->received_payload_, payload_length, this->mqtt_outgoing_target_,
              this->canonical_command_, sizeof(this->canonical_command_),
              canonical_length, &identity, this->device_id_, this->mqtt_result_topic_)) {
        const bool tracked = this->transactions_.begin(
            identity.transaction_id, identity.source_boot_id, millis(), 30000);
        ESP_LOGI(TAG, "MQTT outgoing command observed id=%llu tracked=%s (not executed)",
                 static_cast<unsigned long long>(identity.transaction_id),
                 tracked ? "YES" : "NO");
      } else {
        ESP_LOGD(TAG, "MQTT outgoing command rejected bytes=%u (observation only)",
                 static_cast<unsigned>(payload_length));
      }
      return;
    }
    if (!this->mqtt_command_topic_ ||
        std::strcmp(this->received_topic_, this->mqtt_command_topic_) != 0) return;
    ++this->observed_commands_;
    ESP_LOGD(TAG, "MQTT command observed bytes=%u count=%u (not executed)",
             static_cast<unsigned>(payload_length),
             static_cast<unsigned>(this->observed_commands_));
    size_t canonical_length = 0;
    if (decode_mqtt_command_envelope(this->received_payload_, payload_length,
                                     this->device_id_, this->canonical_command_,
                                     sizeof(this->canonical_command_), canonical_length) ||
        decode_mqtt_command_probe(this->received_payload_, payload_length,
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
#ifdef USE_COMMUNICATION_NET_INBOUND
  ESP_LOGCONFIG(TAG, "  Inbound route declarations: %u (dispatch disabled)",
                static_cast<unsigned>(this->inbound_routes_.size()));
  if (!this->inbound_routes_valid_)
    ESP_LOGE(TAG, "Inbound route registration failed");
#else
  ESP_LOGCONFIG(TAG, "  Inbound route declarations: 0 (dispatch disabled)");
#endif
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  ESP_LOGCONFIG(TAG, "  MQTT command observation: %s",
                this->mqtt_command_topic_ == nullptr ? "DISABLED" : "RECEIVE ONLY");
  ESP_LOGCONFIG(TAG, "  MQTT result observation: %s",
                this->mqtt_result_topic_ == nullptr ? "DISABLED" : "RECEIVE ONLY");
  ESP_LOGCONFIG(TAG, "  MQTT outgoing command observation: %s",
                this->mqtt_outgoing_topic_ == nullptr ? "DISABLED" : "RECEIVE ONLY");
#endif
}

}  // namespace communication_net_protocol
}  // namespace esphome
