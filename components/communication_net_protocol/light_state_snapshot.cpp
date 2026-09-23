#include "esphome/core/defines.h"

#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT

#include "light_state_snapshot.h"

#include <cstdio>
#include <cstring>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/log.h"
#include "mqtt_wire_transport.h"

namespace esphome {
namespace communication_net_protocol {

static const char *const TAG = "communication_net_protocol.snapshot";

bool LightStateSnapshot::configure(const char *topic, uint8_t qos,
                                   uint32_t interval_ms) {
  if (topic == nullptr || !*topic || qos > 2 || interval_ms == 0) return false;
  topic_ = topic;
  qos_ = qos;
  interval_ms_ = interval_ms;
  return true;
}

bool LightStateSnapshot::add_light_field(const char *field, bool rgb) {
  if (field == nullptr || !*field || field_count_ >= MAX_FIELDS) return false;
  Field &item = fields_[field_count_++];
  item.name = field;
  item.rgb = rgb;
  return true;
}

bool LightStateSnapshot::add_light(light::LightState *state) {
  if (state == nullptr || field_count_ == 0) return false;
  Field &item = fields_[field_count_ - 1];
  if (item.sensor != nullptr || item.light_count >= MAX_LIGHTS) return false;
  item.lights[item.light_count++] = state;
  return true;
}

bool LightStateSnapshot::add_binary_field(const char *field,
                                           binary_sensor::BinarySensor *sensor) {
  if (field == nullptr || !*field || sensor == nullptr ||
      field_count_ >= MAX_FIELDS) return false;
  Field &item = fields_[field_count_++];
  item.name = field;
  item.sensor = sensor;
  return true;
}

bool LightStateSnapshot::encode_payload_(char *payload, size_t capacity,
                                         size_t &used) const {
  used = 0;
  if (payload == nullptr || capacity < 3 || field_count_ == 0) return false;
  payload[used++] = '{';
  for (size_t i = 0; i < field_count_; ++i) {
    const Field &field = fields_[i];
    bool known = true;
    bool on = false;
    uint8_t red = 0, green = 0, blue = 0, brightness = 0;
    bool effect_active = false;
    if (field.sensor != nullptr) {
      known = field.sensor->has_state();
      on = known && field.sensor->state;
    } else {
      light::LightState *selected = field.lights[0];
      for (uint8_t j = 0; j < field.light_count; ++j) {
        if (field.lights[j]->current_values.is_on()) {
          if (!on) selected = field.lights[j];
          on = true;
        }
      }
      if (field.rgb && selected != nullptr) {
        const auto &values = selected->current_values;
        const auto &effect = selected->get_effect_name();
        effect_active = on && !effect.empty() && effect != "None";
        red = static_cast<uint8_t>(values.get_red() * 255.0f);
        green = static_cast<uint8_t>(values.get_green() * 255.0f);
        blue = static_cast<uint8_t>(values.get_blue() * 255.0f);
        brightness = static_cast<uint8_t>(values.get_brightness() * 255.0f);
      }
    }
    const int written = field.rgb
        ? std::snprintf(payload + used, capacity - used,
                        "%s\"%s\":{\"known\":%s,\"on\":%s,\"red\":%u,\"green\":%u,\"blue\":%u,\"brightness\":%u,\"effect_active\":%s}",
                        i ? "," : "", field.name, known ? "true" : "false",
                        on ? "true" : "false", red, green, blue, brightness,
                        effect_active ? "true" : "false")
        : std::snprintf(payload + used, capacity - used,
                        "%s\"%s\":{\"known\":%s,\"on\":%s}",
                        i ? "," : "", field.name, known ? "true" : "false",
                        on ? "true" : "false");
    if (written < 0 || static_cast<size_t>(written) >= capacity - used)
      return false;
    used += static_cast<size_t>(written);
  }
  if (used + 1 >= capacity) return false;
  payload[used++] = '}';
  payload[used] = '\0';
  return true;
}

#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
bool LightStateSnapshot::write_remote_state(
    espnow_net_protocol::NetStateSnapshot &snapshot) const {
  uint8_t payload[espnow_net_protocol::NetStateSnapshot::MAX_DATA_SIZE]{};
  size_t used = 0;
  payload[used++] = 1;  // state-fields/v1 binary encoding version
  payload[used++] = static_cast<uint8_t>(field_count_);
  for (size_t index = 0; index < field_count_; ++index) {
    const Field &field = fields_[index];
    const size_t name_size = std::strlen(field.name);
    if (name_size == 0 || name_size > 31 ||
        used + 1 + name_size + 5 > sizeof(payload))
      return false;
    bool known = true;
    bool on = false;
    uint8_t red = 0, green = 0, blue = 0, brightness = 0;
    bool effect_active = false;
    if (field.sensor != nullptr) {
      known = field.sensor->has_state();
      on = known && field.sensor->state;
    } else {
      light::LightState *selected = field.lights[0];
      for (uint8_t light_index = 0; light_index < field.light_count;
           ++light_index) {
        if (field.lights[light_index]->current_values.is_on()) {
          if (!on) selected = field.lights[light_index];
          on = true;
        }
      }
      if (field.rgb && selected != nullptr) {
        const auto &values = selected->current_values;
        const auto &effect = selected->get_effect_name();
        effect_active = on && !effect.empty() && effect != "None";
        red = static_cast<uint8_t>(values.get_red() * 255.0f);
        green = static_cast<uint8_t>(values.get_green() * 255.0f);
        blue = static_cast<uint8_t>(values.get_blue() * 255.0f);
        brightness = static_cast<uint8_t>(values.get_brightness() * 255.0f);
      }
    }
    payload[used++] = static_cast<uint8_t>(name_size);
    std::memcpy(payload + used, field.name, name_size);
    used += name_size;
    payload[used++] = (known ? 0x01 : 0) | (on ? 0x02 : 0) |
                      (field.rgb ? 0x04 : 0) |
                      (effect_active ? 0x08 : 0);
    payload[used++] = red;
    payload[used++] = green;
    payload[used++] = blue;
    payload[used++] = brightness;
  }
  snapshot = {};
  snapshot.completeness = espnow_net_protocol::NetStateCompleteness::COMPLETE;
  return snapshot.schema.assign("state-fields/v1") &&
         snapshot.data.assign(payload, used);
}
#endif

void LightStateSnapshot::loop(uint32_t now_ms, MqttWireTransport &mqtt) {
  if (topic_ == nullptr || field_count_ == 0 ||
      (was_connected_ && now_ms - last_check_ms_ < interval_ms_)) return;
  last_check_ms_ = now_ms;
  const bool connected = mqtt.available();
  if (!connected) {
    was_connected_ = false;
    published_ = false;
    return;
  }
  was_connected_ = true;

  char payload[MAX_PAYLOAD]{};
  size_t used = 0;
  if (!this->encode_payload_(payload, sizeof(payload), used)) {
    ESP_LOGE(TAG, "Snapshot exceeds bounded payload");
    return;
  }
  if (published_ && std::strcmp(payload, last_payload_) == 0) return;
  if (!mqtt.publish(topic_, reinterpret_cast<const uint8_t *>(payload), used,
                    qos_, true)) return;
  std::memcpy(last_payload_, payload, used + 1);
  published_ = true;
  ESP_LOGI(TAG, "Retained snapshot published topic=%s bytes=%u",
           topic_, static_cast<unsigned>(used));
}

}  // namespace communication_net_protocol
}  // namespace esphome

#endif  // USE_COMMUNICATION_NET_STATE_SNAPSHOT
