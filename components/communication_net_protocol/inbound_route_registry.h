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
  static_assert(Capacity > 0 && Capacity <= 16, "bounded route capacity required");

  bool add(const char *id, const char *resource, const char *command) {
    const size_t id_size = length_(id, 31);
    const size_t resource_size = length_(resource, 63);
    const size_t command_size = length_(command, 63);
    if (!id_size || !resource_size || !command_size || size_ == Capacity ||
        this->find(resource, command) != nullptr) return false;
    for (size_t i = 0; i < size_; ++i)
      if (std::strcmp(entries_[i].id, id) == 0) return false;
    Entry &entry = entries_[size_++];
    std::memcpy(entry.id, id, id_size + 1);
    std::memcpy(entry.resource, resource, resource_size + 1);
    std::memcpy(entry.command, command, command_size + 1);
    return true;
  }

  const char *find(const char *resource, const char *command) const {
    if (!resource || !command) return nullptr;
    for (size_t i = 0; i < size_; ++i)
      if (std::strcmp(entries_[i].resource, resource) == 0 &&
          std::strcmp(entries_[i].command, command) == 0) return entries_[i].id;
    return nullptr;
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
    char id[32]{};
    char resource[64]{};
    char command[64]{};
  };
  Entry entries_[Capacity]{};
  size_t size_{0};
};

}  // namespace communication_net_protocol
}  // namespace esphome
