#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "canonical_command.h"
#include "inbound_route_registry.h"

namespace esphome {
namespace communication_net_protocol {

// Read-only route check. Never reserves an entry in the replay guard.
template<size_t Capacity> bool inspect_inbound_command(
    const InboundRouteRegistry<Capacity> &routes, const char *local_device,
    const char *device, const char *resource, const char *action,
    const uint8_t *payload, size_t payload_size, const char *&route_id,
    uint8_t *canonical, size_t canonical_capacity, size_t &canonical_size) {
  route_id = nullptr;
  canonical_size = 0;
  if (local_device == nullptr || device == nullptr || resource == nullptr ||
      action == nullptr || std::strcmp(local_device, device) != 0 ||
      (payload == nullptr && payload_size != 0)) return false;
  // MQTT's current envelope accepts only {} for an empty payload. The radio
  // adapter may emit zero bytes or exactly {} for the same intent.
  if (payload_size != 0 &&
      (payload_size != 2 || payload[0] != '{' || payload[1] != '}'))
    return false;
  const char *matched = routes.find(resource, action);
  if (matched == nullptr ||
      !encode_canonical_command({device, resource, action, nullptr, 0},
                                canonical, canonical_capacity, canonical_size))
    return false;
  route_id = matched;
  return true;
}

}  // namespace communication_net_protocol
}  // namespace esphome
