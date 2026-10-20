#pragma once
#include <cstddef>
#include <cstdint>

namespace esphome::communication_net_protocol {
struct ObservedLightField {
  char name[32]{};
  uint8_t flags{0}, red{0}, green{0}, blue{0}, brightness{0};
  constexpr bool known() const { return flags & 1; }
  constexpr bool on() const { return flags & 2; }
  constexpr bool rgb() const { return flags & 4; }
};
struct ObservedLightSnapshot {
  uint32_t generation{0}, revision{0};
  size_t count{0};
  ObservedLightField fields[8]{};
};
// Equal versions refresh only after a new correlated query, never a repeated push.
constexpr bool state_version_allowed(uint32_t generation, uint32_t revision,
                                     uint32_t accepted_generation, uint32_t accepted_revision, bool allow_equal) {
  return generation != 0 && revision != 0 && (generation > accepted_generation ||
      (generation == accepted_generation && (revision > accepted_revision || (allow_equal && revision == accepted_revision))));
}
constexpr uint32_t state_read32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
constexpr bool state_name_equal(const char *a, const char *b) {
  for (size_t i = 0; i < 32; ++i) {
    if (a[i] != b[i]) return false;
    if (a[i] == 0) return true;
  }
  return false;
}
// Validate the complete binary frame before exposing any state to a consumer.
// Transport authentication, correlation and ordering belong to the caller.
constexpr bool decode_state_fields_v2(const uint8_t *data, size_t length, ObservedLightSnapshot &output) {
  if (data == nullptr || length < 10 || length > 256 || data[0] != 2 || data[9] == 0 || data[9] > 8) return false;
  ObservedLightSnapshot parsed{};
  parsed.generation = state_read32(data + 1);
  parsed.revision = state_read32(data + 5);
  parsed.count = data[9];
  if (parsed.generation == 0 || parsed.revision == 0) return false;
  size_t offset = 10;
  for (size_t i = 0; i < parsed.count; ++i) {
    if (offset >= length) return false;
    const size_t size = data[offset++];
    if (size == 0 || size > 31 || offset + size + 5 > length) return false;
    auto &field = parsed.fields[i];
    for (size_t j = 0; j < size; ++j) {
      const uint8_t c = data[offset++];
      if (!((c >= 'a' && c <= 'z') || (j > 0 && ((c >= '0' && c <= '9') || c == '_')))) return false;
      field.name[j] = static_cast<char>(c);
    }
    for (size_t j = 0; j < i; ++j) if (state_name_equal(field.name, parsed.fields[j].name)) return false;
    field.flags = data[offset++];
    if ((field.flags & 0xf0) != 0) return false;
    field.red = data[offset++]; field.green = data[offset++]; field.blue = data[offset++]; field.brightness = data[offset++];
  }
  if (offset != length) return false;
  output = parsed;
  return true;
}
}  // namespace esphome::communication_net_protocol
