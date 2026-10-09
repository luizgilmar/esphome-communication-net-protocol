#pragma once

#include <cstddef>
#include <cstring>

namespace esphome {
namespace communication_net_protocol {

// Source and response-topic pairs are generated from configuration, never from a command.
// Broker access control remains responsible for authenticating MQTT publishers.
template<size_t Capacity = 8> class MqttSourceRegistry {
 public:
  bool add(const char *source, const char *reply) {
    if (!valid_(source, 63) || !valid_(reply, 192) || size_ == Capacity ||
        std::strchr(reply, '+') || std::strchr(reply, '#')) return false;
    for (size_t i = 0; i < size_; ++i)
      if (std::strcmp(entries_[i].source, source) == 0 ||
          std::strcmp(entries_[i].reply, reply) == 0) return false;
    entries_[size_++] = {source, reply};
    return true;
  }

  const char *reply_for(const char *source) const {
    if (source == nullptr) return nullptr;
    for (size_t i = 0; i < size_; ++i)
      if (std::strcmp(entries_[i].source, source) == 0) return entries_[i].reply;
    return nullptr;
  }

  bool accepts(const char *source, const char *reply) const {
    const char *expected = reply_for(source);
    return expected != nullptr && reply != nullptr && std::strcmp(expected, reply) == 0;
  }

  bool empty() const { return size_ == 0; }

 private:
  static bool valid_(const char *value, size_t limit) {
    if (value == nullptr || *value == '\0') return false;
    size_t length = 0;
    while (length <= limit && value[length] != '\0') ++length;
    return length <= limit;
  }
  struct Entry { const char *source; const char *reply; };
  Entry entries_[Capacity]{};
  size_t size_{0};
};

}  // namespace communication_net_protocol
}  // namespace esphome
