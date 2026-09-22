#pragma once

#include <cstdint>
#include <cstddef>

#include "esphome/core/automation.h"

namespace esphome {
namespace light {
class LightState;
}  // namespace light
namespace binary_sensor {
class BinarySensor;
}  // namespace binary_sensor

namespace communication_net_protocol {

enum class LightExpectedState : uint8_t { ON, OFF, TOGGLED };

// A route and its local action, with optional light state completion.
class DeclarativeInboundBinding : public Trigger<> {
 public:
  void set_route_id(const char *route_id) { route_id_ = route_id; }
  const char *route_id() const { return route_id_; }
  void set_light(light::LightState *state) {
    lights_[0] = state;
    light_count_ = state == nullptr ? 0 : 1;
  }
  void add_completion_light(light::LightState *state) {
    if (state != nullptr && light_count_ < 4) lights_[light_count_++] = state;
  }
  void add_toggle_reference_light(light::LightState *state) {
    if (state != nullptr && toggle_reference_count_ < 3)
      toggle_references_[toggle_reference_count_++] = state;
  }
  void set_expected(LightExpectedState expected) { expected_ = expected; }
  void set_binary_sensor(binary_sensor::BinarySensor *sensor) { sensor_ = sensor; }
  binary_sensor::BinarySensor *binary_sensor() const { return sensor_; }
  void set_result_rgb(bool enabled) { result_rgb_ = enabled; }
  bool result_rgb() const { return result_rgb_; }
  void set_expected_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    expected_rgb_[0] = red;
    expected_rgb_[1] = green;
    expected_rgb_[2] = blue;
    check_rgb_ = true;
  }
  void add_rgb_light(light::LightState *state) {
    if (state != nullptr && rgb_light_count_ < 3)
      rgb_lights_[rgb_light_count_++] = state;
  }
  void set_expected_brightness(uint8_t percent) {
    expected_brightness_ = percent;
    check_brightness_ = true;
  }
  void add_brightness_light(light::LightState *state) {
    if (state != nullptr && brightness_light_count_ < 3)
      brightness_lights_[brightness_light_count_++] = state;
  }
  void set_expected_effect(const char *name) { expected_effect_ = name; }
  void add_effect_light(light::LightState *state) {
    if (state != nullptr && effect_light_count_ < 3)
      effect_lights_[effect_light_count_++] = state;
  }
  void set_completion_timeout(uint32_t timeout_ms) { timeout_ms_ = timeout_ms; }
  void set_completion_delay(uint32_t delay_ms) {
    completion_delay_ms_ = delay_ms;
    timer_completion_ = true;
  }
  void set_interruptible(bool enabled) { interruptible_ = enabled; }
  void set_interrupts_active(bool enabled) { interrupts_active_ = enabled; }

  light::LightState *light() const { return lights_[0]; }
  size_t light_count() const { return light_count_; }
  light::LightState *light_at(size_t index) const {
    return index < light_count_ ? lights_[index] : nullptr;
  }
  size_t toggle_reference_count() const { return toggle_reference_count_; }
  light::LightState *toggle_reference_at(size_t index) const {
    return index < toggle_reference_count_ ? toggle_references_[index] : nullptr;
  }
  LightExpectedState expected() const { return expected_; }
  bool check_rgb() const { return check_rgb_; }
  uint8_t expected_rgb(size_t channel) const { return expected_rgb_[channel]; }
  size_t rgb_light_count() const { return rgb_light_count_; }
  light::LightState *rgb_light_at(size_t index) const {
    return index < rgb_light_count_ ? rgb_lights_[index] : nullptr;
  }
  bool check_brightness() const { return check_brightness_; }
  uint8_t expected_brightness() const { return expected_brightness_; }
  size_t brightness_light_count() const { return brightness_light_count_; }
  light::LightState *brightness_light_at(size_t index) const {
    return index < brightness_light_count_ ? brightness_lights_[index] : nullptr;
  }
  const char *expected_effect() const { return expected_effect_; }
  size_t effect_light_count() const { return effect_light_count_; }
  light::LightState *effect_light_at(size_t index) const {
    return index < effect_light_count_ ? effect_lights_[index] : nullptr;
  }
  uint32_t completion_timeout() const { return timeout_ms_; }
  bool timer_completion() const { return timer_completion_; }
  uint32_t completion_delay() const { return completion_delay_ms_; }
  bool interruptible() const { return interruptible_; }
  bool interrupts_active() const { return interrupts_active_; }

 private:
  const char *route_id_{nullptr};
  light::LightState *lights_[4]{};
  size_t light_count_{0};
  light::LightState *toggle_references_[3]{};
  size_t toggle_reference_count_{0};
  LightExpectedState expected_{LightExpectedState::TOGGLED};
  binary_sensor::BinarySensor *sensor_{nullptr};
  bool result_rgb_{false};
  uint8_t expected_rgb_[3]{};
  bool check_rgb_{false};
  light::LightState *rgb_lights_[3]{};
  size_t rgb_light_count_{0};
  uint8_t expected_brightness_{0};
  bool check_brightness_{false};
  light::LightState *brightness_lights_[3]{};
  size_t brightness_light_count_{0};
  const char *expected_effect_{nullptr};
  light::LightState *effect_lights_[3]{};
  size_t effect_light_count_{0};
  uint32_t timeout_ms_{2000};
  uint32_t completion_delay_ms_{0};
  bool timer_completion_{false};
  bool interruptible_{false};
  bool interrupts_active_{false};
};

}  // namespace communication_net_protocol
}  // namespace esphome
