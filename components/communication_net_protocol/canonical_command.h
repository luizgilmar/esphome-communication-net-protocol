#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace communication_net_protocol {

// Transport-independent application intent; source/transaction IDs belong
// to the replay guard, not to these bytes.
struct CanonicalCommandView {
  const char *device{nullptr};
  const char *resource{nullptr};
  const char *action{nullptr};
  const uint8_t *arguments{nullptr};
  size_t arguments_length{0};
};

// Both adapters must normalize arguments identically before invoking this.
// The byte format uses length-prefixed fields to avoid ambiguous boundaries.
inline bool encode_canonical_command(const CanonicalCommandView &view,
                                     uint8_t *out, size_t capacity,
                                     size_t &written) {
  written = 0;
  if (out == nullptr || (view.arguments == nullptr && view.arguments_length != 0) ||
      view.arguments_length > 128) return false;
  const char *parts[] = {view.device, view.resource, view.action};
  size_t lengths[3]{};
  size_t required = 1 + 3 + view.arguments_length + 1;
  for (size_t i = 0; i < 3; ++i) {
    if (parts[i] == nullptr) return false;
    while (lengths[i] <= 63 && parts[i][lengths[i]] != '\0') ++lengths[i];
    if (lengths[i] == 0 || lengths[i] > 63) return false;
    required += lengths[i];
  }
  if (required > capacity || required > 192) return false;
  out[written++] = 1;  // identity format version
  for (size_t i = 0; i < 3; ++i) {
    out[written++] = static_cast<uint8_t>(lengths[i]);
    std::memcpy(out + written, parts[i], lengths[i]);
    written += lengths[i];
  }
  out[written++] = static_cast<uint8_t>(view.arguments_length);
  if (view.arguments_length != 0) {
    std::memcpy(out + written, view.arguments, view.arguments_length);
    written += view.arguments_length;
  }
  return true;
}

}  // namespace communication_net_protocol
}  // namespace esphome
