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
  static constexpr size_t MAX_ARGUMENTS = 128;
  static constexpr size_t ROUTED_COMMAND_MAX = 2 + MAX_ARGUMENTS;

  RouteAdmission(const char *local_device,
                 const InboundRouteRegistry<RouteCapacity> &routes)
      : local_device_(local_device), routes_(routes) {}

  void set_local_device(const char *device) { local_device_ = device; }

  InboundDecision admit(const InboundCommandView &view,
                        const char *&route_id) {
    route_id = nullptr;
    if (!this->matches_(view)) return InboundDecision::INVALID;
    const size_t route_index =
        routes_.find_index(view.intent.resource, view.intent.action);
    const char *candidate = routes_.id_at(route_index);
    if (candidate == nullptr) return InboundDecision::INVALID;
    uint8_t command[ROUTED_COMMAND_MAX]{};
    size_t command_length = 0;
    if (!this->encode_routed_command_(view, route_index, command,
                                      command_length))
      return InboundDecision::INVALID;
    const InboundDecision decision = gate_.begin(
        view.source_id, view.source_boot_id, view.transaction_id, command,
        command_length);
    if (decision == InboundDecision::NEW_COMMAND) route_id = candidate;
    return decision;
  }

  bool complete(const InboundCommandView &view,
                const InboundTerminalView &terminal) {
    if (!this->matches_(view)) return false;
    const size_t route_index =
        routes_.find_index(view.intent.resource, view.intent.action);
    uint8_t command[ROUTED_COMMAND_MAX]{};
    size_t command_length = 0;
    return routes_.id_at(route_index) != nullptr &&
           this->encode_routed_command_(view, route_index, command,
                                        command_length) &&
           gate_.finish(view.source_id, view.source_boot_id,
                        view.transaction_id, command, command_length, terminal);
  }

  bool replay_terminal(const InboundCommandView &view,
                       InboundTerminalStatus &status, uint8_t *out,
                       size_t capacity, size_t &written) const {
    written = 0;
    if (!this->matches_(view)) return false;
    const size_t route_index =
        routes_.find_index(view.intent.resource, view.intent.action);
    uint8_t command[ROUTED_COMMAND_MAX]{};
    size_t command_length = 0;
    return routes_.id_at(route_index) != nullptr &&
           this->encode_routed_command_(view, route_index, command,
                                        command_length) &&
           gate_.get_terminal(view.source_id, view.source_boot_id,
                              view.transaction_id, command, command_length,
                              status, out, capacity, written);
  }

  // Only call after an authenticated session transition was established.
  size_t clear_old_terminal_session(const char *source_id, uint64_t old_boot_id) {
    return gate_.clear_source_session(source_id, old_boot_id);
  }

 private:
  bool matches_(const InboundCommandView &view) const {
    return local_device_ != nullptr && view.intent.device != nullptr &&
           std::strcmp(local_device_, view.intent.device) == 0;
  }

  bool encode_routed_command_(const InboundCommandView &view,
                              size_t route_index, uint8_t *out,
                              size_t &written) const {
    written = 0;
    if (out == nullptr || route_index >= routes_.size() ||
        route_index > UINT8_MAX ||
        view.intent.arguments_length > MAX_ARGUMENTS ||
        (view.intent.arguments == nullptr &&
         view.intent.arguments_length != 0))
      return false;
    // Version plus route index replaces the repeated device/resource/action
    // texts only after the declarative route has been validated. Arguments
    // remain part of the identity, so conflicting retries are still rejected.
    out[0] = 1;
    out[1] = static_cast<uint8_t>(route_index);
    if (view.intent.arguments_length != 0)
      std::memcpy(out + 2, view.intent.arguments, view.intent.arguments_length);
    written = 2 + view.intent.arguments_length;
    return true;
  }

  const char *local_device_;
  const InboundRouteRegistry<RouteCapacity> &routes_;
  InboundReplayGuard<ReplayCapacity, 63, ROUTED_COMMAND_MAX> gate_{};
};

}  // namespace communication_net_protocol
}  // namespace esphome
