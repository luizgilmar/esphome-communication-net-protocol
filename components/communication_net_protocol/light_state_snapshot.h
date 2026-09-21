#pragma once

#include <cstddef>
#include <cstdint>

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
  void loop(uint32_t now_ms, MqttWireTransport &mqtt);

 private:
  static constexpr size_t MAX_FIELDS = 4;
  static constexpr size_t MAX_LIGHTS = 3;
  static constexpr size_t MAX_PAYLOAD = 384;
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
  char last_payload_[MAX_PAYLOAD]{};
  bool published_{false};
  bool was_connected_{false};
};

}  // namespace communication_net_protocol
}  // namespace esphome
