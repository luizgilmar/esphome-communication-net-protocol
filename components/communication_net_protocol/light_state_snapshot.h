#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/core/preferences.h"

#if defined(USE_COMMUNICATION_NET_ACTIVE_GATE) || defined(USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH)
#include "esphome/components/espnow_net_protocol/message_model.h"
#endif
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
#include "esphome/components/espnow_net_protocol/espnow_net_protocol.h"
#endif

namespace esphome {
namespace light { class LightState; }
namespace binary_sensor { class BinarySensor; }
namespace communication_net_protocol {

class MqttWireTransport;

// Optional, bounded retained state publication. A failed MQTT publish never
// advances the cache; reconnect always republishes the current observations.
class LightStateSnapshot {
 public:
  bool configure(const char *topic, uint8_t qos, uint32_t interval_ms);
  bool add_light_field(const char *field, bool rgb);
  bool add_light(light::LightState *light);
  bool add_binary_field(const char *field, binary_sensor::BinarySensor *sensor);
  bool configured() const { return topic_ != nullptr && field_count_ != 0; }
  void setup();
  void loop(uint32_t now_ms, MqttWireTransport &mqtt);
#if defined(USE_COMMUNICATION_NET_ACTIVE_GATE) || defined(USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH)
  bool write_remote_state(espnow_net_protocol::NetStateSnapshot &snapshot);
#endif
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
  bool configure_push(
      espnow_net_protocol::EspNowNetProtocolComponent *endpoint,
      const char *peer_id, uint32_t settle_ms, uint32_t min_interval_ms,
      uint32_t startup_quiet_ms, uint32_t startup_spread_ms);
#endif

 private:
  static constexpr size_t MAX_FIELDS = 4;
  static constexpr size_t MAX_LIGHTS = 3;
  static constexpr size_t MAX_WIRE_PAYLOAD = 448;
  struct Value {
    uint8_t known{0};
    uint8_t on{0};
    uint8_t rgb{0};
    uint8_t effect_active{0};
    uint8_t red{0};
    uint8_t green{0};
    uint8_t blue{0};
    uint8_t brightness{0};
  };
  bool capture_(Value *values) const;
  bool update_version_(const Value *values);
  bool encode_payload_(const Value *values, char *payload, size_t capacity,
                       size_t &used) const;
  uint32_t preference_key_() const;
  struct Field {
    const char *name{nullptr};
    light::LightState *lights[MAX_LIGHTS]{};
    binary_sensor::BinarySensor *sensor{nullptr};
    uint8_t light_count{0};
    bool rgb{false};
  };
  const char *topic_{nullptr};
  uint8_t qos_{1};
  uint32_t interval_ms_{500};
  uint32_t last_check_ms_{0};
  Field fields_[MAX_FIELDS]{};
  size_t field_count_{0};
  Value last_values_[MAX_FIELDS]{};
  ESPPreferenceObject generation_preference_{};
  uint32_t generation_{0};
  uint32_t revision_{0};
  bool version_ready_{false};
  bool values_observed_{false};
  bool published_{false};
  uint32_t published_generation_{0};
  uint32_t published_revision_{0};
  bool was_connected_{false};
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
  void process_push_completion_(uint32_t now_ms);
  void try_push_(uint32_t now_ms, bool mqtt_connected);
  uint64_t push_transaction_id_() const;
  espnow_net_protocol::EspNowNetProtocolComponent *push_endpoint_{nullptr};
  const char *push_peer_id_{nullptr};
  uint32_t push_settle_ms_{300};
  uint32_t push_min_interval_ms_{1000};
  uint32_t startup_quiet_ms_{5000};
  uint32_t startup_spread_ms_{30000};
  uint32_t startup_push_due_ms_{0};
  uint32_t push_not_before_ms_{0};
  uint32_t last_push_attempt_ms_{0};
  uint32_t push_generation_{0};
  uint32_t push_revision_{0};
  uint64_t push_transaction_id_inflight_{0};
  bool push_dirty_{false};
  bool push_inflight_{false};
#endif
};

}  // namespace communication_net_protocol
}  // namespace esphome
