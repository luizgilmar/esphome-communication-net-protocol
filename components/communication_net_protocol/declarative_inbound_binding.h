#pragma once

#include <cstdint>
#include <cstddef>

#include "esphome/core/automation.h"

namespace esphome {
namespace light {
class LightState;
}  // namespace light

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
  void set_completion_timeout(uint32_t timeout_ms) { timeout_ms_ = timeout_ms; }

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
  uint32_t completion_timeout() const { return timeout_ms_; }

 private:
  const char *route_id_{nullptr};
  light::LightState *lights_[4]{};
  size_t light_count_{0};
  light::LightState *toggle_references_[3]{};
  size_t toggle_reference_count_{0};
  LightExpectedState expected_{LightExpectedState::TOGGLED};
  uint32_t timeout_ms_{2000};
};

}  // namespace communication_net_protocol
}  // namespace esphome
