#pragma once

#include <cstdint>

#include "outbound_communication_policy.h"
#include "outbound_communication_transport.h"
#include "outbound_communication_transaction.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

enum class TransactionRunnerState : uint8_t {
  IDLE,
  ACTIVE,
  RESULT_READY,
};

class CommunicationTransactionRunner {
 public:
  explicit CommunicationTransactionRunner(
      CommunicationTransportRegistry &registry)
      : registry_(registry) {}

  bool start(const CommandRequest &request,
             const CommunicationPolicy &policy,
             uint32_t now_ms);
  void loop(uint32_t now_ms);

  bool has_result() const {
    return this->state_ == TransactionRunnerState::RESULT_READY;
  }
  bool take_result(CommandResult &result);
  bool has_progress_result() const { return this->has_progress_result_; }
  bool take_progress_result(CommandResult &result);

  TransactionRunnerState state() const { return this->state_; }
  TransactionId active_transaction_id() const {
    return this->state_ == TransactionRunnerState::IDLE
               ? 0
               : this->request_.transaction_id;
  }

 private:
  bool attempt_next_(uint32_t now_ms);
  void handle_attempt_result_(const CommandResult &result, uint32_t now_ms);
  void complete_(const CommandResult &result, uint32_t now_ms);
  void complete_failure_(CommunicationTransport transport,
                         TransportStatus transport_status,
                         CommunicationErrorCode error_code,
                         const char *message,
                         bool retryable,
                         uint32_t now_ms);
  CommandResult make_failure_(CommunicationTransport transport,
                              TransportStatus transport_status,
                              CommunicationErrorCode error_code,
                              const char *message,
                              bool retryable,
                              uint32_t now_ms) const;
  bool timed_out_(uint32_t now_ms) const;
  void reset_();

  CommunicationTransportRegistry &registry_;
  CommunicationPolicyEngine policy_engine_{};
  CommunicationPolicy policy_{};
  TransportAttemptSet attempts_{};
  CommandRequest request_{};
  CommandResult progress_result_{};
  CommandResult terminal_result_{};
  CommandResult last_transport_failure_{};
  CommunicationTransportAdapter *active_transport_{nullptr};
  TransportAttemptSet listening_{};
  uint32_t started_ms_{0};
  uint32_t attempt_started_ms_{0};
  uint32_t accepted_ms_{0};
  uint32_t completion_timeout_ms_{0};
  bool accepted_{false};
  bool acceptance_fallback_{false};
  bool has_listener_() const;
  void cancel_listeners_();
  bool has_progress_result_{false};
  bool has_last_transport_failure_{false};
  TransactionRunnerState state_{TransactionRunnerState::IDLE};
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome

#include "outbound_runner_impl.h"
