#pragma once
#include <cstddef>
namespace esphome::communication_net_protocol {
// MQTT JSON capacity is independent of the unchanged 256-byte binary radio frame.
constexpr size_t MAX_SNAPSHOT_JSON_BYTES = 767;
}  // namespace esphome::communication_net_protocol
