#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace communication_net_protocol {

// Declarative route names. This registry does not authorize or execute an
// inbound message; the shared admission gate must run before dispatch.
template<size_t Capacity = 16> class InboundRouteRegistry {
 public:
  static_assert(Capacity > 0 && Capacity <= 32, "bounded route capacity required");

  bool add(const char *id, const char *resource, const char *command) {
    const size_t id_size = length_(id, 31);
    const size_t resource_size = length_(resource, 63);
    const size_t command_size = length_(command, 63);
    if (!id_size || !resource_size || !command_size || size_ == Capacity ||
        this->find(resource, command) != nullptr) return false;
    for (size_t i = 0; i < size_; ++i)
      if (std::strcmp(entries_[i].id, id) == 0) return false;
    // These values come from generated YAML string literals and therefore
    // remain valid for the component lifetime.  Keep references instead of
    // reserving three maximum-sized text buffers for every declared route.
    entries_[size_++] = {id, resource, command};
    return true;
  }

  const char *find(const char *resource, const char *command) const {
    const size_t index = this->find_index(resource, command);
    return index < size_ ? entries_[index].id : nullptr;
  }

  size_t find_index(const char *resource, const char *command) const {
    if (!resource || !command) return Capacity;
    for (size_t i = 0; i < size_; ++i)
      if (std::strcmp(entries_[i].resource, resource) == 0 &&
          std::strcmp(entries_[i].command, command) == 0) return i;
    return Capacity;
  }

  size_t size() const { return size_; }

 private:
  static size_t length_(const char *value, size_t maximum) {
    if (!value) return 0;
    size_t count = 0;
    while (count <= maximum && value[count] != '\0') ++count;
    return count <= maximum ? count : 0;
  }
  struct Entry {
    const char *id{nullptr};
    const char *resource{nullptr};
    const char *command{nullptr};
  };
  Entry entries_[Capacity]{};
  uint8_t size_{0};
};

}  // namespace communication_net_protocol
}  // namespace esphome
