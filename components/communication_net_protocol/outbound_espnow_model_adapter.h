#pragma once

#include "esphome/core/defines.h"

#if defined(USE_TX_ULTIMATE_ESPNOW) || defined(USE_COMMUNICATION_NET_OUTBOUND_ESPNOW)

#include "esphome/components/espnow_net_protocol/message_model.h"
#include "outbound_communication_transaction.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

class EspNowNetModelAdapter {
 public:
  using NetCommand = espnow_net_protocol::NetCommand;
  using NetResult = espnow_net_protocol::NetResult;

  bool to_net(const CommandRequest &source, NetCommand &target) const {
    target = {};
    if (!source.is_valid()) return false;
    target.transaction_id = source.transaction_id;
    target.timeout_ms = source.timeout_ms;
    return target.device_id.assign(source.target.device_id.c_str()) &&
           target.resource.assign(source.target.resource.c_str()) &&
           target.name.assign(source.command.name.c_str()) &&
           target.payload.assign(source.command.payload.data(),
                                 source.command.payload.size()) &&
           target.valid();
  }

  bool from_net(const NetCommand &source, CommandRequest &target) const {
    target = {};
    if (!source.valid()) return false;
    target.transaction_id = source.transaction_id;
    target.timeout_ms = source.timeout_ms;
    return target.target.device_id.assign(source.device_id.c_str()) &&
           target.target.resource.assign(source.resource.c_str()) &&
           target.command.name.assign(source.name.c_str()) &&
           target.command.payload.assign(source.payload.data(),
                                         source.payload.size()) &&
           target.is_valid();
  }

  bool to_net(const CommandResult &source, NetResult &target) const {
    target = {};
    if (!source.is_consistent() || !map_result_(source.result, target.status) ||
        !map_execution_(source.execution, target.execution) ||
        !map_state_(source.remote_state, target.remote_state) ||
        !map_error_(source.error, target.error)) return false;
    target.transaction_id = source.transaction_id;
    target.latency_ms = source.latency_ms;
    return target.consistent();
  }

  bool from_net(const NetResult &source, CommandResult &target) const {
    target = {};
    if (!source.consistent() || !map_result_(source.status, target.result) ||
        !map_execution_(source.execution, target.execution) ||
        !map_state_(source.remote_state, target.remote_state) ||
        !map_error_(source.error, target.error)) return false;
    target.transaction_id = source.transaction_id;
    target.transport_used = CommunicationTransport::ESP_NOW;
    target.transport_status = TransportStatus::SUCCEEDED;
    target.latency_ms = source.latency_ms;
    return target.is_consistent();
  }

 private:
  using NetCompleteness = espnow_net_protocol::NetStateCompleteness;
  using NetErrorCode = espnow_net_protocol::NetErrorCode;
  using NetResultStatus = espnow_net_protocol::NetResultStatus;

  static bool map_result_(CommandResultStatus in, NetResultStatus &out) {
    switch (in) {
      case CommandResultStatus::SUCCEEDED: out = NetResultStatus::SUCCEEDED; return true;
      case CommandResultStatus::IN_PROGRESS: out = NetResultStatus::IN_PROGRESS; return true;
      case CommandResultStatus::REJECTED: out = NetResultStatus::REJECTED; return true;
      case CommandResultStatus::FAILED: out = NetResultStatus::FAILED; return true;
    }
    return false;
  }
  static bool map_result_(NetResultStatus in, CommandResultStatus &out) {
    switch (in) {
      case NetResultStatus::SUCCEEDED: out = CommandResultStatus::SUCCEEDED; return true;
      case NetResultStatus::IN_PROGRESS: out = CommandResultStatus::IN_PROGRESS; return true;
      case NetResultStatus::REJECTED: out = CommandResultStatus::REJECTED; return true;
      case NetResultStatus::FAILED: out = CommandResultStatus::FAILED; return true;
    }
    return false;
  }
  static bool map_state_(const StateSnapshot &in,
                         espnow_net_protocol::NetStateSnapshot &out) {
    out = {};
    out.completeness = static_cast<NetCompleteness>(in.completeness);
    return out.schema.assign(in.schema.c_str()) &&
           out.data.assign(in.data.data(), in.data.size()) && out.consistent();
  }
  static bool map_state_(const espnow_net_protocol::NetStateSnapshot &in,
                         StateSnapshot &out) {
    out = {};
    out.completeness = static_cast<RemoteStateCompleteness>(in.completeness);
    return out.schema.assign(in.schema.c_str()) &&
           out.data.assign(in.data.data(), in.data.size()) && out.is_consistent();
  }
  static bool map_execution_(const ExecutionContext &in,
                             espnow_net_protocol::NetExecutionContext &out) {
    out.started = in.started;
    out.has_estimated_completion = in.has_estimated_completion;
    out.estimated_completion_ms = in.estimated_completion_ms;
    out.has_progress = in.has_progress;
    out.progress_percent = in.progress_percent;
    out.has_expected_final_state = in.has_expected_final_state;
    return map_state_(in.expected_final_state, out.expected_final_state) &&
           out.consistent();
  }
  static bool map_execution_(const espnow_net_protocol::NetExecutionContext &in,
                             ExecutionContext &out) {
    out.started = in.started;
    out.has_estimated_completion = in.has_estimated_completion;
    out.estimated_completion_ms = in.estimated_completion_ms;
    out.has_progress = in.has_progress;
    out.progress_percent = in.progress_percent;
    out.has_expected_final_state = in.has_expected_final_state;
    return map_state_(in.expected_final_state, out.expected_final_state) &&
           out.is_consistent();
  }
  static bool map_error_(const CommunicationError &in,
                         espnow_net_protocol::NetError &out) {
    out.code = static_cast<NetErrorCode>(in.code);
    out.retryable = in.retryable;
    return out.message.assign(in.message.c_str()) && out.consistent();
  }
  static bool map_error_(const espnow_net_protocol::NetError &in,
                         CommunicationError &out) {
    out.code = static_cast<CommunicationErrorCode>(in.code);
    out.retryable = in.retryable;
    return out.message.assign(in.message.c_str()) && out.is_consistent();
  }
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome

#endif  // outbound ESP-NOW model adapter
