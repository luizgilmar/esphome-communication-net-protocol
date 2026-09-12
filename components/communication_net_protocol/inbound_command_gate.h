#pragma once

#include <cstddef>
#include <cstdint>

#include "canonical_command.h"
#include "inbound_replay_guard.h"

namespace esphome {
namespace communication_net_protocol {

// Borrowed inputs supplied by an authenticated transport adapter. Neither a
// broker payload's claimed source nor an ESP-NOW payload field establishes
// identity on its own: the adapter must validate the sender first.
struct InboundCommandView {
  const char *source_id{nullptr};
  uint64_t source_boot_id{0};
  uint64_t transaction_id{0};
  CanonicalCommandView intent{};
};

// The same gate instance must serve all configured transports on a device.
// Call from the cooperative application loop, never from a radio/MQTT callback.
// Only NEW_COMMAND grants permission to invoke the device's local executor.
template<size_t Capacity, size_t MaxSource = 63> class InboundCommandGate {
 public:
  InboundDecision admit(const InboundCommandView &command) {
    uint8_t canonical[192]{};
    size_t size = 0;
    if (!encode_canonical_command(command.intent, canonical,
                                  sizeof(canonical), size))
      return InboundDecision::INVALID;
    return guard_.begin(command.source_id, command.source_boot_id,
                        command.transaction_id, canonical, size);
  }

  bool complete(const InboundCommandView &command,
                const InboundTerminalView &result) {
    uint8_t canonical[192]{};
    size_t size = 0;
    if (!encode_canonical_command(command.intent, canonical,
                                  sizeof(canonical), size)) return false;
    return guard_.finish(command.source_id, command.source_boot_id,
                         command.transaction_id, canonical, size, result);
  }

  bool replay_terminal(const InboundCommandView &command,
                       InboundTerminalStatus &status, uint8_t *out,
                       size_t capacity, size_t &written) const {
    uint8_t canonical[192]{};
    size_t size = 0;
    if (!encode_canonical_command(command.intent, canonical,
                                  sizeof(canonical), size)) {
      written = 0;
      return false;
    }
    return guard_.get_terminal(command.source_id, command.source_boot_id,
                               command.transaction_id, canonical, size,
                               status, out, capacity, written);
  }

  // Session changes must be validated by the adapter; this only releases
  // terminal entries and deliberately retains uncertain pending operations.
  size_t clear_old_terminal_session(const char *source_id, uint64_t old_boot_id) {
    return guard_.clear_source_session(source_id, old_boot_id);
  }

 private:
  InboundReplayGuard<Capacity, MaxSource> guard_{};
};

}  // namespace communication_net_protocol
}  // namespace esphome
