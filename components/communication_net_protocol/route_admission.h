#pragma once

#include <cstddef>
#include <cstring>

#include "inbound_command_gate.h"
#include "inbound_route_registry.h"

namespace esphome {
namespace communication_net_protocol {

// A single instance per receiver mediates both transport adapters. The
// adapter must establish the sender identity and session before calling this.
template<size_t ReplayCapacity = 8, size_t RouteCapacity = 16>
class RouteAdmission {
 public:
  RouteAdmission(const char *local_device,
                 const InboundRouteRegistry<RouteCapacity> &routes)
      : local_device_(local_device), routes_(routes) {}

  void set_local_device(const char *device) { local_device_ = device; }

  InboundDecision admit(const InboundCommandView &view,
                        const char *&route_id) {
    route_id = nullptr;
    if (!this->matches_(view)) return InboundDecision::INVALID;
    const char *candidate = routes_.find(view.intent.resource, view.intent.action);
    if (candidate == nullptr) return InboundDecision::INVALID;
    const InboundDecision decision = gate_.admit(view);
    if (decision == InboundDecision::NEW_COMMAND) route_id = candidate;
    return decision;
  }

  bool complete(const InboundCommandView &view,
                const InboundTerminalView &terminal) {
    return this->matches_(view) &&
           routes_.find(view.intent.resource, view.intent.action) != nullptr &&
           gate_.complete(view, terminal);
  }

  bool replay_terminal(const InboundCommandView &view,
                       InboundTerminalStatus &status, uint8_t *out,
                       size_t capacity, size_t &written) const {
    written = 0;
    return this->matches_(view) &&
           routes_.find(view.intent.resource, view.intent.action) != nullptr &&
           gate_.replay_terminal(view, status, out, capacity, written);
  }

  // Only call after an authenticated session transition was established.
  size_t clear_old_terminal_session(const char *source_id, uint64_t old_boot_id) {
    return gate_.clear_old_terminal_session(source_id, old_boot_id);
  }

 private:
  bool matches_(const InboundCommandView &view) const {
    return local_device_ != nullptr && view.intent.device != nullptr &&
           std::strcmp(local_device_, view.intent.device) == 0;
  }

  const char *local_device_;
  const InboundRouteRegistry<RouteCapacity> &routes_;
  InboundCommandGate<ReplayCapacity> gate_{};
};

}  // namespace communication_net_protocol
}  // namespace esphome
