#pragma once
#include <cstdint>

namespace esphome::communication_net_protocol {
// ESPHome normalizes RGB by its largest channel. RGB describes hue/saturation;
// brightness has its own completion check. Its all-zero RGB becomes white.
constexpr uint8_t normalized_rgb_byte(uint8_t channel, uint8_t maximum) {
  return maximum == 0 ? 255 : (static_cast<unsigned>(channel) * 255U + maximum / 2U) / maximum;
}
constexpr bool rgb_completion_matches(float red, float green, float blue,
                                      uint8_t expected_red, uint8_t expected_green, uint8_t expected_blue) {
  const float observed[3]{red, green, blue};
  const uint8_t expected[3]{expected_red, expected_green, expected_blue};
  const uint8_t rg_max = expected_red > expected_green ? expected_red : expected_green;
  const uint8_t maximum = rg_max > expected_blue ? rg_max : expected_blue;
  for (unsigned i = 0; i < 3; ++i) {
    // Also reject NaN instead of converting it to an integer.
    if (!(observed[i] >= 0.0f && observed[i] <= 1.0f)) return false;
    const int actual = static_cast<int>(observed[i] * 255.0f + 0.5f);
    const int target = normalized_rgb_byte(expected[i], maximum);
    if (actual < target - 1 || actual > target + 1) return false;
  }
  return true;
}
}  // namespace esphome::communication_net_protocol
