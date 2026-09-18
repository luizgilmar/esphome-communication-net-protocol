#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace communication_net_protocol {

enum class InboundDecision : uint8_t {
  NEW_COMMAND,
  DUPLICATE_PENDING,
  DUPLICATE_TERMINAL,
  CONFLICT,
  INVALID,
  FULL,
  RETIRED,
};

enum class InboundTerminalStatus : uint8_t { SUCCEEDED, REJECTED, FAILED };

// Transport-neutral verified result. `data` is a bounded application result,
// not a serialized MQTT response or an ESP-NOW frame.
struct InboundTerminalView {
  InboundTerminalStatus status{InboundTerminalStatus::FAILED};
  const uint8_t *data{nullptr};
  size_t length{0};
};

// One device-wide table for inbound commands, regardless of wire transport.
// Old terminal entries may be evicted after recording a session watermark;
// pending entries are retained so uncertain actions cannot run again.
template<size_t Capacity, size_t MaxSource = 63, size_t MaxCommand = 192,
         size_t MaxTerminal = 256>
class InboundReplayGuard {
 public:
  static_assert(Capacity > 0 && Capacity <= 16, "bounded inbound capacity required");
  static_assert(MaxSource > 0 && MaxSource <= 63, "bounded source id required");
  static_assert(MaxCommand > 0 && MaxCommand <= 192, "bounded command required");
  static_assert(MaxTerminal > 0 && MaxTerminal <= 256, "bounded terminal result required");

  InboundDecision begin(const char *source_id, uint64_t source_boot_id,
                        uint64_t transaction_id, const uint8_t *command,
                        size_t command_length) {
    if (source_id == nullptr || source_boot_id == 0 || transaction_id == 0 ||
        command == nullptr || command_length == 0 || command_length > MaxCommand)
      return InboundDecision::INVALID;
    const size_t source_length = bounded_length_(source_id);
    if (source_length == 0 || source_length > MaxSource) return InboundDecision::INVALID;
    for (const auto &entry : entries_) {
      if (!entry.occupied || entry.source_boot_id != source_boot_id ||
          entry.transaction_id != transaction_id ||
          std::strcmp(entry.source_id, source_id) != 0) continue;
      if (entry.command_length != command_length ||
          std::memcmp(entry.command, command, command_length) != 0)
        return InboundDecision::CONFLICT;
      return entry.terminal ? InboundDecision::DUPLICATE_TERMINAL
                            : InboundDecision::DUPLICATE_PENDING;
    }
    Session *session = this->find_session_(source_id, source_boot_id);
    if (session != nullptr && transaction_id <= session->retired_through)
      return InboundDecision::RETIRED;
    if (session == nullptr) {
      for (auto &candidate : sessions_) {
        if (candidate.occupied) continue;
        session = &candidate;
        break;
      }
      if (session == nullptr) return InboundDecision::FULL;
    }
    Entry *free_entry = nullptr;
    for (auto &entry : entries_) {
      if (!entry.occupied) { free_entry = &entry; break; }
    }
    if (free_entry == nullptr) {
      // Reuse the oldest terminal entry. Pending operations are never evicted.
      for (auto &entry : entries_) {
        if (!entry.terminal || !entry.occupied) continue;
        if (free_entry == nullptr || entry.sequence < free_entry->sequence)
          free_entry = &entry;
      }
      if (free_entry == nullptr) return InboundDecision::FULL;
      Session *retired = find_session_(free_entry->source_id,
                                       free_entry->source_boot_id);
      if (retired == nullptr) return InboundDecision::FULL;
      if (retired->retired_through < free_entry->transaction_id)
        retired->retired_through = free_entry->transaction_id;
      if (retired == session && transaction_id <= session->retired_through)
        return InboundDecision::RETIRED;
    }
    if (!session->occupied) {
      std::memcpy(session->source_id, source_id, source_length + 1);
      session->source_boot_id = source_boot_id;
      session->occupied = true;
    }
    *free_entry = {};
    std::memcpy(free_entry->source_id, source_id, source_length + 1);
    free_entry->source_boot_id = source_boot_id;
    free_entry->transaction_id = transaction_id;
    std::memcpy(free_entry->command, command, command_length);
    free_entry->command_length = command_length;
    free_entry->sequence = ++sequence_;
    free_entry->occupied = true;
    return InboundDecision::NEW_COMMAND;
  }

  // Mark terminal only after the device adapter confirms its final outcome.
  // An unsuccessful transport ACK alone must not mark a command terminal.
  bool finish(const char *source_id, uint64_t source_boot_id,
              uint64_t transaction_id, const uint8_t *command,
              size_t command_length, const InboundTerminalView &result) {
    if (source_id == nullptr || command == nullptr || command_length == 0 ||
        command_length > MaxCommand || result.length > MaxTerminal ||
        (result.data == nullptr && result.length != 0)) return false;
    for (auto &entry : entries_) {
      if (!entry.occupied || entry.source_boot_id != source_boot_id ||
          entry.transaction_id != transaction_id ||
          std::strcmp(entry.source_id, source_id) != 0 ||
          entry.command_length != command_length ||
          std::memcmp(entry.command, command, command_length) != 0) continue;
      if (entry.terminal) return false;
      entry.terminal_status = result.status;
      if (result.length != 0)
        std::memcpy(entry.terminal_data, result.data, result.length);
      entry.terminal_length = result.length;
      entry.terminal = true;
      return true;
    }
    return false;
  }

  // Read-only replay: the adapter may serialize the same outcome for either
  // transport, without re-running the local action. Caller owns `out`.
  bool get_terminal(const char *source_id, uint64_t source_boot_id,
                    uint64_t transaction_id, const uint8_t *command,
                    size_t command_length, InboundTerminalStatus &status,
                    uint8_t *out, size_t capacity, size_t &written) const {
    written = 0;
    if (source_id == nullptr || command == nullptr || command_length == 0 ||
        command_length > MaxCommand) return false;
    for (const auto &entry : entries_) {
      if (!entry.occupied || !entry.terminal ||
          entry.source_boot_id != source_boot_id ||
          entry.transaction_id != transaction_id ||
          std::strcmp(entry.source_id, source_id) != 0 ||
          entry.command_length != command_length ||
          std::memcmp(entry.command, command, command_length) != 0 ||
          capacity < entry.terminal_length ||
          (out == nullptr && entry.terminal_length != 0)) continue;
      status = entry.terminal_status;
      if (entry.terminal_length != 0)
        std::memcpy(out, entry.terminal_data, entry.terminal_length);
      written = entry.terminal_length;
      return true;
    }
    return false;
  }

  // The application must establish a new sender session before clearing its
  // old terminal entries. Keep uncertain pending commands reserved.
  size_t clear_source_session(const char *source_id, uint64_t old_boot_id) {
    if (source_id == nullptr || old_boot_id == 0) return 0;
    size_t removed = 0;
    for (auto &entry : entries_) {
      if (!entry.occupied || !entry.terminal ||
          entry.source_boot_id != old_boot_id ||
          std::strcmp(entry.source_id, source_id) != 0) continue;
      Session *session = find_session_(entry.source_id, entry.source_boot_id);
      if (session != nullptr && session->retired_through < entry.transaction_id)
        session->retired_through = entry.transaction_id;
      entry = {};
      ++removed;
    }
    return removed;
  }

 private:
  struct Entry {
    char source_id[MaxSource + 1]{};
    uint64_t source_boot_id{0};
    uint64_t transaction_id{0};
    uint8_t command[MaxCommand]{};
    size_t command_length{0};
    uint8_t terminal_data[MaxTerminal]{};
    size_t terminal_length{0};
    InboundTerminalStatus terminal_status{InboundTerminalStatus::FAILED};
    bool occupied{false};
    bool terminal{false};
    uint64_t sequence{0};
  };

  struct Session {
    char source_id[MaxSource + 1]{};
    uint64_t source_boot_id{0};
    uint64_t retired_through{0};
    bool occupied{false};
  };

  Session *find_session_(const char *source_id, uint64_t source_boot_id) {
    for (auto &session : sessions_)
      if (session.occupied && session.source_boot_id == source_boot_id &&
          std::strcmp(session.source_id, source_id) == 0) return &session;
    return nullptr;
  }

  static size_t bounded_length_(const char *value) {
    size_t length = 0;
    while (length <= MaxSource && value[length] != '\0') ++length;
    return length;
  }

  Entry entries_[Capacity]{};
  // One compact watermark per sender session. Do not erase a watermark while
  // its boot ID can still be accepted by a transport adapter.
  Session sessions_[Capacity]{};
  uint64_t sequence_{0};
};

}  // namespace communication_net_protocol
}  // namespace esphome
