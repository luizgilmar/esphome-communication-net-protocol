#pragma once

#include <cstddef>
#include <cstring>

#include "outbound_communication_transaction.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

class CommunicationDestinationRegistry {
 public:
  static constexpr size_t MAX_DESTINATIONS = 16;
  static constexpr size_t MAX_ENDPOINT_ID_LENGTH = 31;
  static constexpr uint8_t INVALID_ESPNOW_PEER = UINT8_MAX;

  bool add_destination(const char *destination_id) {
    if (this->destination_count_ >= MAX_DESTINATIONS ||
        destination_id == nullptr || destination_id[0] == '\0' ||
        this->find_entry_(destination_id) != nullptr) {
      return false;
    }
    DestinationEntry &entry = this->entries_[this->destination_count_];
    if (!entry.id.assign(destination_id)) return false;
    this->destination_count_++;
    return true;
  }

  bool set_policy(const char *destination_id, const char *policy_id) {
    DestinationEntry *entry = this->find_entry_(destination_id);
    return entry != nullptr && policy_id != nullptr && policy_id[0] != '\0' &&
           entry->policy_id.assign(policy_id);
  }

  bool set_http_endpoint(const char *destination_id, const char *endpoint_id) {
    DestinationEntry *entry = this->find_entry_(destination_id);
    return entry != nullptr && endpoint_id != nullptr && endpoint_id[0] != '\0' &&
           entry->http_endpoint_id.assign(endpoint_id);
  }

  bool set_espnow_peer(const char *destination_id, uint8_t peer_index) {
    DestinationEntry *entry = this->find_entry_(destination_id);
    if (entry == nullptr || peer_index == INVALID_ESPNOW_PEER) return false;
    entry->espnow_peer_index = peer_index;
    return true;
  }

  const char *policy_for(const char *destination_id) const {
    const DestinationEntry *entry = this->find_entry_(destination_id);
    return entry == nullptr || entry->policy_id.empty()
               ? nullptr
               : entry->policy_id.c_str();
  }

  const char *http_endpoint_for(const char *destination_id) const {
    const DestinationEntry *entry = this->find_entry_(destination_id);
    return entry == nullptr || entry->http_endpoint_id.empty()
               ? nullptr
               : entry->http_endpoint_id.c_str();
  }

  uint8_t espnow_peer_for(const char *destination_id) const {
    const DestinationEntry *entry = this->find_entry_(destination_id);
    return entry == nullptr ? INVALID_ESPNOW_PEER : entry->espnow_peer_index;
  }

  bool contains(const char *destination_id) const {
    return this->find_entry_(destination_id) != nullptr;
  }
  size_t destination_count() const { return this->destination_count_; }

 private:
  struct DestinationEntry {
    BoundedText<CommandTarget::MAX_DEVICE_ID_LENGTH> id{};
    BoundedText<CommandRequest::MAX_POLICY_ID_LENGTH> policy_id{};
    BoundedText<MAX_ENDPOINT_ID_LENGTH> http_endpoint_id{};
    uint8_t espnow_peer_index{INVALID_ESPNOW_PEER};
  };

  DestinationEntry *find_entry_(const char *destination_id) {
    return const_cast<DestinationEntry *>(
        static_cast<const CommunicationDestinationRegistry *>(this)
            ->find_entry_(destination_id));
  }

  const DestinationEntry *find_entry_(const char *destination_id) const {
    if (destination_id == nullptr) return nullptr;
    for (size_t index = 0; index < this->destination_count_; index++) {
      if (std::strcmp(this->entries_[index].id.c_str(), destination_id) == 0)
        return &this->entries_[index];
    }
    return nullptr;
  }

  DestinationEntry entries_[MAX_DESTINATIONS]{};
  size_t destination_count_{0};
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome
