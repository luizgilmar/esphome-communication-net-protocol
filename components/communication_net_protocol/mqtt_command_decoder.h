#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "canonical_command.h"

namespace esphome {
namespace communication_net_protocol {

// Deliberately narrow, receive-only probe format. No sender identity is
// inferred from these untrusted MQTT bytes. Unsupported JSON is rejected.
inline bool decode_mqtt_command_probe(const uint8_t *payload, size_t length,
                                      const char *expected_device,
                                      uint8_t *canonical, size_t capacity,
                                      size_t &written) {
  written = 0;
  if (!payload || !expected_device || !canonical || length == 0 || length > 256)
    return false;
  char device[64]{}, resource[64]{}, action[64]{};
  char *fields[] = {device, resource, action};
  unsigned seen = 0;
  size_t pos = 0;
  auto space = [&]() {
    while (pos < length && (payload[pos] == ' ' || payload[pos] == '\n' ||
                            payload[pos] == '\r' || payload[pos] == '\t')) ++pos;
  };
  auto string = [&](char *out, size_t limit) {
    if (pos >= length || payload[pos++] != '"') return false;
    size_t size = 0;
    while (pos < length && payload[pos] != '"') {
      const uint8_t c = payload[pos++];
      // Refuse escapes, controls and non-ASCII until a complete JSON codec
      // and normalized argument representation are specified.
      if (c < 0x21 || c > 0x7e || c == '\\' || size + 1 >= limit) return false;
      out[size++] = static_cast<char>(c);
    }
    if (!size || pos >= length) return false;
    ++pos;
    out[size] = '\0';
    return true;
  };
  space();
  if (pos >= length || payload[pos++] != '{') return false;
  for (unsigned count = 0; count < 3; ++count) {
    space();
    char key[16]{};
    if (!string(key, sizeof(key))) return false;
    int index = std::strcmp(key, "device") == 0 ? 0 :
                std::strcmp(key, "resource") == 0 ? 1 :
                std::strcmp(key, "action") == 0 ? 2 : -1;
    if (index < 0 || (seen & (1U << index))) return false;
    seen |= 1U << index;
    space();
    if (pos >= length || payload[pos++] != ':') return false;
    space();
    if (!string(fields[index], 64)) return false;
    space();
    if (count < 2) {
      if (pos >= length || payload[pos++] != ',') return false;
    }
  }
  space();
  if (pos >= length || payload[pos++] != '}') return false;
  space();
  if (pos != length || seen != 7 || std::strcmp(device, expected_device) != 0)
    return false;
  return encode_canonical_command({device, resource, action, nullptr, 0},
                                  canonical, capacity, written);
}

}  // namespace communication_net_protocol
}  // namespace esphome
