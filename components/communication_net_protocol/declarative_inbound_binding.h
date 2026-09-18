#pragma once

#include <cstdint>

#include "esphome/core/automation.h"

namespace esphome {
namespace light {
class LightState;
}  // namespace light

namespace communication_net_protocol {

enum class LightExpectedState : uint8_t { ON, OFF, TOGGLED };

// Configuration only. No network callback or component loop invokes trigger()
// until the shared inbound dispatcher is explicitly bound in a later stage.
class DeclarativeInboundBinding : public Trigger<> {
 public:
  void set_light(light::LightState *state) { light_ = state; }
  void set_expected(LightExpectedState expected) { expected_ = expected; }
  void set_completion_timeout(uint32_t timeout_ms) { timeout_ms_ = timeout_ms; }

  light::LightState *light() const { return light_; }
  LightExpectedState expected() const { return expected_; }
  uint32_t completion_timeout() const { return timeout_ms_; }

 private:
  light::LightState *light_{nullptr};
  LightExpectedState expected_{LightExpectedState::TOGGLED};
  uint32_t timeout_ms_{2000};
};

}  // namespace communication_net_protocol
}  // namespace esphome
