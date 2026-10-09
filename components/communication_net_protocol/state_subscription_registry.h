#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::communication_net_protocol {

enum class StateSubscriptionResult : uint8_t {
  APPLIED, DUPLICATE, STALE, UNAUTHORIZED, INVALID
};

// Bounded main-loop registry. Indices/masks refer to declarative configuration.
// Only an authenticated adapter may establish a session or submit control.
// Session freshness must be established externally; random IDs are not clocks.
template<size_t Peers> class StateSubscriptionRegistry {
  static_assert(Peers > 0, "nonempty peer capacity required");
 public:
  explicit StateSubscriptionRegistry(uint32_t validity_ms) : validity_ms_(validity_ms) {}
  bool configure_peer(size_t peer, uint64_t allowed_resources) {
    if (peer >= Peers || peers_[peer].configured) return false;
    peers_[peer].configured = true;
    peers_[peer].allowed = allowed_resources;
    return true;
  }
  // New authenticated session invalidates old lease and control sequence.
  // Repeated establishment of the same session must not erase ordering.
  bool establish_session(size_t peer, uint64_t session) {
    if (peer >= Peers || !peers_[peer].configured || session == 0) return false;
    auto &entry = peers_[peer];
    if (entry.session == session) return true;
    entry.session = session;
    entry.sequence = 0;
    entry.resources = 0;
    entry.active = false;
    return true;
  }
  StateSubscriptionResult apply(size_t peer, uint64_t session, uint64_t sequence,
                                uint64_t resources, bool enable, uint32_t now) {
    if (peer >= Peers || !peers_[peer].configured || session == 0 ||
        session != peers_[peer].session) return StateSubscriptionResult::UNAUTHORIZED;
    if (validity_ms_ == 0 || validity_ms_ >= 0x80000000UL || sequence == 0 ||
        (enable && resources == 0) || (!enable && resources != 0))
      return StateSubscriptionResult::INVALID;
    auto &entry = peers_[peer];
    if ((resources & ~entry.allowed) != 0) return StateSubscriptionResult::UNAUTHORIZED;
    if (sequence < entry.sequence) return StateSubscriptionResult::STALE;
    if (sequence == entry.sequence) {
      // Duplicate control never extends lease; conflicting reuse is invalid.
      return entry.resources == resources && entry.last_enable == enable
          ? StateSubscriptionResult::DUPLICATE : StateSubscriptionResult::INVALID;
    }
    entry.sequence = sequence;
    entry.resources = resources;
    entry.last_enable = enable;
    entry.active = enable;
    entry.renewed_at = now;
    return StateSubscriptionResult::APPLIED;
  }
  uint64_t active_resources(size_t peer, uint32_t now) const {
    if (peer >= Peers || validity_ms_ == 0 || validity_ms_ >= 0x80000000UL) return 0;
    const auto &entry = peers_[peer];
    return entry.active && uint32_t(now - entry.renewed_at) < validity_ms_
        ? entry.resources : 0;
  }
  // Call periodically to latch expiry; keep sequence tombstones against replay.
  void expire(uint32_t now) {
    for (auto &entry : peers_)
      if (entry.active && uint32_t(now - entry.renewed_at) >= validity_ms_)
        entry.active = false;
  }
 private:
  struct Entry {
    bool configured{false}, active{false}, last_enable{false};
    uint64_t allowed{0}, session{0}, sequence{0}, resources{0};
    uint32_t renewed_at{0};
  };
  uint32_t validity_ms_;
  std::array<Entry, Peers> peers_{};
};
}  // namespace esphome::communication_net_protocol
