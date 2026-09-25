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

static void put_u32(uint8_t *target, uint32_t value) {
  target[0] = static_cast<uint8_t>(value);
  target[1] = static_cast<uint8_t>(value >> 8U);
  target[2] = static_cast<uint8_t>(value >> 16U);
  target[3] = static_cast<uint8_t>(value >> 24U);
}

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

uint32_t LightStateSnapshot::preference_key_() const {
  uint32_t hash = 2166136261UL ^ 0xC9E20002UL;
  if (this->topic_ != nullptr)
    for (const char *cursor = this->topic_; *cursor != '\0'; ++cursor) {
      hash ^= static_cast<uint8_t>(*cursor);
      hash *= 16777619UL;
    }
  return hash == 0 ? 1 : hash;
}

void LightStateSnapshot::setup() {
  if (!this->configured()) return;
  this->generation_preference_ =
      global_preferences->make_preference<uint32_t>(this->preference_key_());
  uint32_t previous = 0;
  this->generation_preference_.load(&previous);
  this->generation_ = previous == UINT32_MAX ? 1 : previous + 1;
  this->version_ready_ =
      this->generation_preference_.save(&this->generation_);
  if (!this->version_ready_) {
    ESP_LOGE(TAG, "Snapshot generation could not be persisted");
    return;
  }
  ESP_LOGI(TAG, "Snapshot generation ready generation=%u",
           static_cast<unsigned>(this->generation_));
}

bool LightStateSnapshot::capture_(Value *values) const {
  if (values == nullptr || this->field_count_ == 0) return false;
  for (size_t i = 0; i < this->field_count_; ++i) {
    const Field &field = this->fields_[i];
    Value &value = values[i];
    value = {};
    value.known = 1;
    value.rgb = field.rgb ? 1 : 0;
    if (field.sensor != nullptr) {
      value.known = field.sensor->has_state() ? 1 : 0;
      value.on = value.known && field.sensor->state ? 1 : 0;
      continue;
    }
    light::LightState *selected = field.lights[0];
    for (uint8_t j = 0; j < field.light_count; ++j) {
      if (field.lights[j]->current_values.is_on()) {
        if (!value.on) selected = field.lights[j];
        value.on = 1;
      }
    }
    if (!field.rgb || selected == nullptr) continue;
    const auto &current = selected->current_values;
    const auto &effect = selected->get_effect_name();
    value.effect_active =
        value.on && !effect.empty() && effect != "None" ? 1 : 0;
    value.red = static_cast<uint8_t>(current.get_red() * 255.0f);
    value.green = static_cast<uint8_t>(current.get_green() * 255.0f);
    value.blue = static_cast<uint8_t>(current.get_blue() * 255.0f);
    value.brightness =
        static_cast<uint8_t>(current.get_brightness() * 255.0f);
  }
  return true;
}

bool LightStateSnapshot::update_version_(const Value *values) {
  if (!this->version_ready_ || values == nullptr) return false;
  const size_t bytes = this->field_count_ * sizeof(Value);
  if (this->values_observed_ &&
      std::memcmp(values, this->last_values_, bytes) == 0)
    return false;
  if (this->revision_ == UINT32_MAX) {
    this->generation_ = this->generation_ == UINT32_MAX
                            ? 1
                            : this->generation_ + 1;
    if (!this->generation_preference_.save(&this->generation_)) {
      this->version_ready_ = false;
      ESP_LOGE(TAG, "Snapshot generation rollover could not be persisted");
      return false;
    }
    this->revision_ = 0;
  }
  this->revision_++;
  std::memcpy(this->last_values_, values, bytes);
  this->values_observed_ = true;
  return true;
}

bool LightStateSnapshot::encode_payload_(const Value *values, char *payload,
                                         size_t capacity,
                                         size_t &used) const {
  used = 0;
  if (values == nullptr || payload == nullptr || capacity < 3 ||
      this->field_count_ == 0 || !this->version_ready_ ||
      this->revision_ == 0)
    return false;
  const int metadata = std::snprintf(
      payload, capacity,
      "{\"_meta\":{\"version\":2,\"generation\":%u,\"revision\":%u}",
      static_cast<unsigned>(this->generation_),
      static_cast<unsigned>(this->revision_));
  if (metadata < 0 || static_cast<size_t>(metadata) >= capacity) return false;
  used = static_cast<size_t>(metadata);
  for (size_t i = 0; i < field_count_; ++i) {
    const Field &field = fields_[i];
    const Value &value = values[i];
    const int written = field.rgb
        ? std::snprintf(payload + used, capacity - used,
                        ",\"%s\":{\"known\":%s,\"on\":%s,\"red\":%u,\"green\":%u,\"blue\":%u,\"brightness\":%u,\"effect_active\":%s}",
                        field.name, value.known ? "true" : "false",
                        value.on ? "true" : "false", value.red, value.green,
                        value.blue, value.brightness,
                        value.effect_active ? "true" : "false")
        : std::snprintf(payload + used, capacity - used,
                        ",\"%s\":{\"known\":%s,\"on\":%s}", field.name,
                        value.known ? "true" : "false",
                        value.on ? "true" : "false");
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
    espnow_net_protocol::NetStateSnapshot &snapshot) {
  Value values[MAX_FIELDS]{};
  if (!this->capture_(values)) return false;
  this->update_version_(values);
  if (!this->version_ready_ || this->revision_ == 0) return false;
  uint8_t payload[espnow_net_protocol::NetStateSnapshot::MAX_DATA_SIZE]{};
  size_t used = 0;
  payload[used++] = 2;  // state-fields/v2 binary encoding version
  put_u32(payload + used, this->generation_);
  used += 4;
  put_u32(payload + used, this->revision_);
  used += 4;
  payload[used++] = static_cast<uint8_t>(field_count_);
  for (size_t index = 0; index < field_count_; ++index) {
    const Field &field = fields_[index];
    const Value &value = values[index];
    const size_t name_size = std::strlen(field.name);
    if (name_size == 0 || name_size > 31 ||
        used + 1 + name_size + 5 > sizeof(payload))
      return false;
    payload[used++] = static_cast<uint8_t>(name_size);
    std::memcpy(payload + used, field.name, name_size);
    used += name_size;
    payload[used++] = (value.known ? 0x01 : 0) | (value.on ? 0x02 : 0) |
                      (field.rgb ? 0x04 : 0) |
                      (value.effect_active ? 0x08 : 0);
    payload[used++] = value.red;
    payload[used++] = value.green;
    payload[used++] = value.blue;
    payload[used++] = value.brightness;
  }
  snapshot = {};
  snapshot.completeness = espnow_net_protocol::NetStateCompleteness::COMPLETE;
  return snapshot.schema.assign("state-fields/v2") &&
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

  Value values[MAX_FIELDS]{};
  if (!this->capture_(values)) return;
  this->update_version_(values);
  if (!this->version_ready_ || this->revision_ == 0 ||
      (this->published_ &&
       this->published_generation_ == this->generation_ &&
       this->published_revision_ == this->revision_))
    return;
  char payload[MAX_WIRE_PAYLOAD]{};
  size_t used = 0;
  if (!this->encode_payload_(values, payload, sizeof(payload), used)) {
    ESP_LOGE(TAG, "Snapshot exceeds bounded payload");
    return;
  }
  if (!mqtt.publish(topic_, reinterpret_cast<const uint8_t *>(payload), used,
                    qos_, true)) return;
  published_ = true;
  this->published_generation_ = this->generation_;
  this->published_revision_ = this->revision_;
  ESP_LOGI(TAG,
           "Retained snapshot published topic=%s generation=%u revision=%u "
           "bytes=%u",
           topic_, static_cast<unsigned>(this->generation_),
           static_cast<unsigned>(this->revision_), static_cast<unsigned>(used));
}

}  // namespace communication_net_protocol
}  // namespace esphome

#endif  // USE_COMMUNICATION_NET_STATE_SNAPSHOT
