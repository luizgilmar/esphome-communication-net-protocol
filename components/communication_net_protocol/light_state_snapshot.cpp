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

#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
bool LightStateSnapshot::configure_push(
    espnow_net_protocol::EspNowNetProtocolComponent *endpoint,
    const char *peer_id, uint32_t settle_ms, uint32_t min_interval_ms,
    uint32_t startup_quiet_ms, uint32_t startup_spread_ms) {
  if (endpoint == nullptr || peer_id == nullptr || !*peer_id ||
      min_interval_ms == 0)
    return false;
  this->push_endpoint_ = endpoint;
  this->push_peer_id_ = peer_id;
  this->push_settle_ms_ = settle_ms;
  this->push_min_interval_ms_ = min_interval_ms;
  this->startup_quiet_ms_ = startup_quiet_ms;
  this->startup_spread_ms_ = startup_spread_ms;
  return true;
}
#endif

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
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
  uint32_t startup_hash = this->preference_key_() ^ this->generation_;
  startup_hash ^= startup_hash >> 16U;
  startup_hash *= 0x7FEB352DUL;
  startup_hash ^= startup_hash >> 15U;
  const uint32_t offset = this->startup_spread_ms_ == 0
                              ? 0
                              : startup_hash % this->startup_spread_ms_;
  this->startup_push_due_ms_ = this->startup_quiet_ms_ + offset;
  this->push_not_before_ms_ = this->startup_push_due_ms_;
#endif
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

#if defined(USE_COMMUNICATION_NET_ACTIVE_GATE) || defined(USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH)
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

#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
uint64_t LightStateSnapshot::push_transaction_id_() const {
  return 0xD000000000000000ULL |
         (static_cast<uint64_t>(this->generation_ & 0x0FFFFFFFUL) << 32U) |
         this->revision_;
}

void LightStateSnapshot::process_push_completion_(uint32_t now_ms) {
  if (!this->push_inflight_ || this->push_endpoint_ == nullptr) return;
  espnow_net_protocol::TransactionId transaction_id = 0;
  bool succeeded = false;
  if (!this->push_endpoint_->take_background_result_completion(transaction_id,
                                                                succeeded)) {
    if (!this->push_endpoint_->runtime_enabled() ||
        !this->push_endpoint_->background_result_active()) {
      this->push_inflight_ = false;
      this->push_transaction_id_inflight_ = 0;
      this->push_not_before_ms_ = now_ms + this->push_min_interval_ms_;
    }
    return;
  }
  if (transaction_id != this->push_transaction_id_inflight_) return;
  this->push_inflight_ = false;
  this->push_transaction_id_inflight_ = 0;
  if (succeeded && this->generation_ == this->push_generation_ &&
      this->revision_ == this->push_revision_)
    this->push_dirty_ = false;
  if (!succeeded) this->push_not_before_ms_ = now_ms + this->push_min_interval_ms_;
  ESP_LOGI(TAG,
           "ESP-NOW snapshot push completed generation=%u revision=%u "
           "succeeded=%s dirty=%s",
           static_cast<unsigned>(this->push_generation_),
           static_cast<unsigned>(this->push_revision_), YESNO(succeeded),
           YESNO(this->push_dirty_));
}

void LightStateSnapshot::try_push_(uint32_t now_ms, bool mqtt_connected) {
  if (mqtt_connected || !this->push_dirty_ || this->push_inflight_ ||
      this->push_endpoint_ == nullptr || this->push_peer_id_ == nullptr ||
      static_cast<int32_t>(now_ms - this->push_not_before_ms_) < 0 ||
      (this->last_push_attempt_ms_ != 0 &&
       now_ms - this->last_push_attempt_ms_ < this->push_min_interval_ms_))
    return;
  this->last_push_attempt_ms_ = now_ms;
  const auto peer = this->push_endpoint_->peer_index(this->push_peer_id_);
  if (peer == espnow_net_protocol::INVALID_PEER_INDEX) return;
  espnow_net_protocol::NetResult result{};
  result.transaction_id = this->push_transaction_id_();
  result.status = espnow_net_protocol::NetResultStatus::SUCCEEDED;
  result.execution.started = true;
  if (!this->write_remote_state(result.remote_state) ||
      !this->push_endpoint_->start_background_result(peer, result))
    return;
  this->push_generation_ = this->generation_;
  this->push_revision_ = this->revision_;
  this->push_transaction_id_inflight_ = result.transaction_id;
  this->push_inflight_ = true;
  ESP_LOGI(TAG,
           "ESP-NOW snapshot push started peer=%s generation=%u revision=%u "
           "tx=%llu",
           this->push_peer_id_, static_cast<unsigned>(this->push_generation_),
           static_cast<unsigned>(this->push_revision_),
           static_cast<unsigned long long>(result.transaction_id));
}
#endif

void LightStateSnapshot::loop(uint32_t now_ms, MqttWireTransport &mqtt) {
  if (topic_ == nullptr || field_count_ == 0) return;
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
  this->process_push_completion_(now_ms);
#endif
  const bool connected = mqtt.available();
  if (last_check_ms_ != 0 && now_ms - last_check_ms_ < interval_ms_) {
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
    this->try_push_(now_ms, connected);
#endif
    return;
  }
  last_check_ms_ = now_ms;
  const bool had_values = this->values_observed_;
  Value values[MAX_FIELDS]{};
  if (!this->capture_(values)) return;
  const bool changed = this->update_version_(values);
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
  if (changed) {
    this->push_dirty_ = true;
    const uint32_t settled = now_ms + this->push_settle_ms_;
    this->push_not_before_ms_ = !had_values
                                    ? this->startup_push_due_ms_
                                    : settled;
    if (static_cast<int32_t>(this->startup_push_due_ms_ -
                             this->push_not_before_ms_) > 0)
      this->push_not_before_ms_ = this->startup_push_due_ms_;
  }
#endif
  if (!connected) {
    was_connected_ = false;
    published_ = false;
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
    this->try_push_(now_ms, false);
#endif
    return;
  }
  was_connected_ = true;
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
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT_PUSH
  // A successful retained MQTT publication makes the current revision
  // available through the primary transport.  Clear the pending background
  // push even if an older ESP-NOW revision is still in flight; otherwise that
  // stale dirty bit can trigger an unnecessary push when MQTT disconnects.
  this->push_dirty_ = false;
#endif
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
