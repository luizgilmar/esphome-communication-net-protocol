#pragma once
#include <cstddef>
#include <cstdint>
namespace esphome::communication_net_protocol {
struct ObservedCoverState { uint8_t position{0}, operation{0}; };
constexpr bool decode_cover_state_v1(const uint8_t *data, size_t length, ObservedCoverState &output) {
  if (data == nullptr || length != 2 || data[0] > 100 || data[1] > 2) return false;
  output = {data[0], data[1]};
  return true;
}
}  // namespace esphome::communication_net_protocol
