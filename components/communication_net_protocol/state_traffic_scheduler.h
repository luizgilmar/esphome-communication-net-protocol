#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::communication_net_protocol {

// Main-loop only. Peer/resource indices are validated configuration slots.
// Transport/authentication, lease control and payload storage remain external.
struct StateTrafficLimits {
  uint32_t node_interval_ms;
  uint32_t peer_interval_ms;
  uint32_t retry_initial_ms;
  uint32_t retry_max_ms;
  uint32_t jitter_ms;
  bool valid() const {
    return node_interval_ms > 0 && peer_interval_ms > 0 &&
           retry_initial_ms > 0 && retry_max_ms >= retry_initial_ms &&
           uint64_t(retry_max_ms) + jitter_ms < 0x80000000ULL &&
           node_interval_ms < 0x80000000UL && peer_interval_ms < 0x80000000UL;
  }
};

class StateRetryWindow {
 public:
  explicit StateRetryWindow(StateTrafficLimits limits) : limits_(limits) {}
  bool ready(uint32_t now) const {
    return limits_.valid() && (!waiting_ || uint32_t(now - since_) >= delay_);
  }
  void defer(uint32_t now, uint32_t entropy) {
    if (!limits_.valid()) return;
    // Duplicate failure notifications must not restart or escalate this window.
    if (waiting_ && !ready(now)) return;
    backoff_ = backoff_ == 0 ? limits_.retry_initial_ms
        : uint32_t(uint64_t(backoff_) * 2 > limits_.retry_max_ms
                       ? limits_.retry_max_ms : uint64_t(backoff_) * 2);
    delay_ = backoff_ + entropy % (limits_.jitter_ms + 1);
    since_ = now;
    waiting_ = true;
  }
  void recovered() { waiting_ = false; backoff_ = 0; }
 private:
  StateTrafficLimits limits_;
  uint32_t since_{0}, delay_{0}, backoff_{0};
  bool waiting_{false};
};

template<size_t Peers, size_t Resources> class StateTrafficScheduler {
  static_assert(Peers > 0 && Resources > 0, "nonempty configured capacity required");
 public:
  struct Ticket { size_t peer{0}, resource{0}; uint64_t token{0}; };
  explicit StateTrafficScheduler(StateTrafficLimits limits) : limits_(limits) {}
  bool set_enabled(size_t peer, bool enabled) {
    if (peer >= Peers) return false;
    peers_[peer].enabled = enabled;
    if (!enabled) for (auto &slot : peers_[peer].slots) slot.pending = false;
    return true;
  }
  bool mark_latest(size_t peer, size_t resource) {
    if (peer >= Peers || resource >= Resources || !peers_[peer].enabled ||
        serial_ == UINT64_MAX) return false;
    auto &slot = peers_[peer].slots[resource];
    slot.token = ++serial_;
    slot.pending = true;
    return true;
  }
  // Taking a ticket consumes budget even when the transport subsequently fails.
  // Caller passes false while commands/STOP need reserved transport capacity.
  bool take(uint32_t now, bool background_allowed, Ticket &ticket) {
    if (!limits_.valid() || !background_allowed || inflight_ ||
        (node_sent_ && uint32_t(now - node_last_) < limits_.node_interval_ms)) return false;
    for (size_t n = 0; n < Peers; ++n) {
      size_t index = (next_peer_ + n) % Peers;
      auto &peer = peers_[index];
      if (!peer.enabled || (peer.sent && uint32_t(now - peer.last) < limits_.peer_interval_ms) ||
          (peer.waiting && uint32_t(now - peer.failed_at) < peer.delay)) continue;
      for (size_t r = 0; r < Resources; ++r) {
        size_t resource = (peer.next_resource + r) % Resources;
        auto &slot = peer.slots[resource];
        if (!slot.pending) continue;
        ticket = {index, resource, slot.token};
        active_ = ticket;
        inflight_ = node_sent_ = peer.sent = true;
        node_last_ = peer.last = now;
        next_peer_ = (index + 1) % Peers;
        peer.next_resource = (resource + 1) % Resources;
        return true;
      }
    }
    return false;
  }
  bool complete(const Ticket &ticket, bool success, uint32_t now, uint32_t entropy) {
    if (!inflight_ || ticket.peer != active_.peer || ticket.resource != active_.resource ||
        ticket.token != active_.token) return false;
    inflight_ = false;
    auto &peer = peers_[ticket.peer];
    auto &slot = peer.slots[ticket.resource];
    if (success) {
      if (slot.token == ticket.token) slot.pending = false;
      peer.waiting = false;
      peer.backoff = 0;
    } else {
      peer.backoff = peer.backoff == 0 ? limits_.retry_initial_ms
          : uint32_t(uint64_t(peer.backoff) * 2 > limits_.retry_max_ms
                         ? limits_.retry_max_ms : uint64_t(peer.backoff) * 2);
      peer.delay = peer.backoff + entropy % (limits_.jitter_ms + 1);
      peer.failed_at = now;
      peer.waiting = true;
    }
    return true;
  }
 private:
  struct Slot { bool pending{false}; uint64_t token{0}; };
  struct Peer {
    bool enabled{false}, sent{false}, waiting{false};
    uint32_t last{0}, failed_at{0}, delay{0}, backoff{0};
    size_t next_resource{0};
    std::array<Slot, Resources> slots{};
  };
  StateTrafficLimits limits_;
  std::array<Peer, Peers> peers_{};
  Ticket active_{};
  uint64_t serial_{0};
  uint32_t node_last_{0};
  size_t next_peer_{0};
  bool node_sent_{false}, inflight_{false};
};
}  // namespace esphome::communication_net_protocol
