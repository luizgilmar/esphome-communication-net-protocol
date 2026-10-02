#pragma once
#include "outbound_communication_transaction_runner.h"
#ifdef USE_COMMUNICATION_NET_OUTBOUND
#include "esphome/core/log.h"
#define COMM_DELIVERY_LOG(...) ESP_LOGI("communication.delivery", __VA_ARGS__)
#else
#define COMM_DELIVERY_LOG(...) ((void) 0)
#endif

namespace esphome {
namespace communication_net_protocol { namespace outbound {

inline bool CommunicationTransactionRunner::start(
    const CommandRequest &request,
    const CommunicationPolicy &policy,
    uint32_t now_ms) {
  if (this->state_ != TransactionRunnerState::IDLE || !request.is_valid() ||
      !policy.is_valid()) {
    return false;
  }

  this->request_ = request;
  this->policy_ = policy;
  this->attempts_.clear();
  this->started_ms_ = now_ms;
  this->attempt_started_ms_ = now_ms;
  this->listening_.clear();
  this->accepted_ = false;
  this->acceptance_fallback_ = policy.transport_count() == 2 &&
      policy.contains(CommunicationTransport::MQTT) &&
      policy.contains(CommunicationTransport::ESP_NOW);
  this->progress_result_ = {};
  this->has_progress_result_ = false;
  this->has_last_transport_failure_ = false;
  this->active_transport_ = nullptr;
  this->state_ = TransactionRunnerState::ACTIVE;
  this->attempt_next_(now_ms);
  return true;
}

inline void CommunicationTransactionRunner::loop(uint32_t now_ms) {
  this->registry_.loop(now_ms);
  if (this->state_ != TransactionRunnerState::ACTIVE) return;
  // Drain every participating transport before deadlines. A final reply from
  // an earlier attempt remains authoritative while another send is pending.
  for (size_t i = 0; i < this->policy_.transport_count(); ++i) {
    const auto type = this->policy_.transport_at(i);
    if (!this->listening_.was_attempted(type)) continue;
    auto *completed_transport = this->registry_.find(type);
    if (completed_transport == nullptr || !completed_transport->has_result()) continue;
    CommandResult result{};
    if (!completed_transport->take_result(result)) continue;
    if (result.transaction_id != this->request_.transaction_id ||
        result.transport_used != completed_transport->transport_type() ||
        !result.is_consistent()) {
      // Malformed/stale replies must not terminate a different valid attempt.
      continue;
    }
    if (result.result == CommandResultStatus::IN_PROGRESS) {
      if (!this->accepted_) {
        this->accepted_ = true;
        this->accepted_ms_ = now_ms;
        const uint32_t estimate = result.execution.estimated_completion_ms;
        // Bounded completion budget, independent from the acceptance deadline.
        const uint32_t bounded = estimate > 3600000U ? 3600000U : estimate;
        this->completion_timeout_ms_ = bounded + 1000U;
        if (this->completion_timeout_ms_ < this->request_.timeout_ms)
          this->completion_timeout_ms_ = this->request_.timeout_ms;
        this->progress_result_ = result;
        this->progress_result_.latency_ms = now_ms - this->started_ms_;
        this->has_progress_result_ = true;
        COMM_DELIVERY_LOG("CA1 accepted tx=%llu transport=%u completion_ms=%u",
            static_cast<unsigned long long>(result.transaction_id),
            static_cast<unsigned>(type), static_cast<unsigned>(this->completion_timeout_ms_));
      }
      continue;  // Duplicate ACK never extends the execution deadline.
    }
    if (result.transport_status == TransportStatus::SUCCEEDED ||
        result.result == CommandResultStatus::SUCCEEDED ||
        result.result == CommandResultStatus::REJECTED) {
      this->complete_(result, now_ms);
      return;
    }
    this->last_transport_failure_ = result;
    this->has_last_transport_failure_ = true;
    if (this->active_transport_ == completed_transport) this->active_transport_ = nullptr;
  }
  if (this->timed_out_(now_ms)) {
    const auto type = this->active_transport_ == nullptr ? CommunicationTransport::NONE
        : this->active_transport_->transport_type();
    this->complete_failure_(type, TransportStatus::TIMED_OUT,
        CommunicationErrorCode::TIMED_OUT, "end-to-end transaction timeout", false, now_ms);
    return;
  }
  // Keep listening after an unacknowledged MQTT send. Do not cancel it when
  // launching ESP-NOW. If a listener is lost after ACK, the same identity on
  // the remaining transport queries/replays the receiver's existing execution.
  uint32_t ack_ms = this->request_.timeout_ms / 3U;
  if (ack_ms > 500U) ack_ms = 500U;
  if (ack_ms == 0) ack_ms = 1;
  const bool missing_ack = this->acceptance_fallback_ && !this->accepted_ &&
      now_ms - this->attempt_started_ms_ >= ack_ms;
  if (this->active_transport_ == nullptr || missing_ack) this->attempt_next_(now_ms);
}

inline bool CommunicationTransactionRunner::has_listener_() const {
  for (size_t i = 0; i < this->policy_.transport_count(); ++i) {
    const auto type = this->policy_.transport_at(i);
    auto *adapter = this->registry_.find(type);
    if (this->listening_.was_attempted(type) && adapter != nullptr &&
        (adapter->is_busy() || adapter->has_result())) return true;
  }
  return false;
}

inline void CommunicationTransactionRunner::cancel_listeners_() {
  for (size_t i = 0; i < this->policy_.transport_count(); ++i) {
    const auto type = this->policy_.transport_at(i);
    auto *adapter = this->registry_.find(type);
    if (this->listening_.was_attempted(type) && adapter != nullptr)
      adapter->cancel(this->request_.transaction_id);
  }
}

inline bool CommunicationTransactionRunner::take_progress_result(
    CommandResult &result) {
  if (!this->has_progress_result_) return false;
  result = this->progress_result_;
  this->progress_result_ = {};
  this->has_progress_result_ = false;
  return true;
}

inline bool CommunicationTransactionRunner::take_result(CommandResult &result) {
  if (!this->has_result()) {
    return false;
  }
  result = this->terminal_result_;
  this->reset_();
  return true;
}

inline bool CommunicationTransactionRunner::attempt_next_(uint32_t now_ms) {
  for (size_t attempt = 0; attempt < CommunicationPolicy::MAX_TRANSPORTS;
       attempt++) {
    const PolicyDecision decision = this->policy_engine_.select_next(
        this->policy_, this->registry_.availability(), this->attempts_);
    if (decision.type != PolicyDecisionType::USE_TRANSPORT) {
      if (this->has_listener_() || this->accepted_) return false;
      if (this->has_last_transport_failure_) {
        this->complete_(this->last_transport_failure_, now_ms);
      } else {
        this->complete_failure_(CommunicationTransport::NONE,
                                TransportStatus::UNAVAILABLE,
                                CommunicationErrorCode::TRANSPORT_UNAVAILABLE,
                                "no eligible communication transport", true,
                                now_ms);
      }
      return false;
    }

    CommunicationTransportAdapter *adapter =
        this->registry_.find(decision.transport);
    this->attempts_.mark_attempted(decision.transport);
    if (adapter == nullptr) {
      this->last_transport_failure_ = this->make_failure_(
          decision.transport, TransportStatus::UNAVAILABLE,
          CommunicationErrorCode::TRANSPORT_UNAVAILABLE,
          "selected transport is not registered", true, now_ms);
      this->has_last_transport_failure_ = true;
      continue;
    }

    const TransportStartStatus start_status = adapter->start(this->request_);
    if (start_status == TransportStartStatus::STARTED) {
      adapter->use_external_result_deadline(this->request_.transaction_id);
      this->active_transport_ = adapter;
      this->listening_.mark_attempted(decision.transport);
      this->attempt_started_ms_ = now_ms;
      COMM_DELIVERY_LOG("CA1 send tx=%llu transport=%u previous_reply_paths_retained=1",
          static_cast<unsigned long long>(this->request_.transaction_id),
          static_cast<unsigned>(decision.transport));
      return true;
    }

    if (start_status == TransportStartStatus::INVALID_REQUEST) {
      this->complete_failure_(decision.transport,
                              TransportStatus::NOT_ATTEMPTED,
                              CommunicationErrorCode::INVALID_REQUEST,
                              "transport rejected invalid request", false,
                              now_ms);
      return false;
    }

    if (start_status == TransportStartStatus::REJECTED) {
      this->complete_failure_(decision.transport,
                              TransportStatus::NOT_ATTEMPTED,
                              CommunicationErrorCode::PROTOCOL_ERROR,
                              "transport rejected attempt", false, now_ms);
      return false;
    }

    const bool busy = start_status == TransportStartStatus::BUSY;
    this->last_transport_failure_ = this->make_failure_(
        decision.transport, TransportStatus::UNAVAILABLE,
        CommunicationErrorCode::TRANSPORT_UNAVAILABLE,
        busy ? "transport became busy" : "transport became unavailable", true,
        now_ms);
    this->has_last_transport_failure_ = true;
  }

  this->complete_(this->last_transport_failure_, now_ms);
  return false;
}

inline void CommunicationTransactionRunner::handle_attempt_result_(
    const CommandResult &result, uint32_t now_ms) {
  const PolicyDecision decision = this->policy_engine_.evaluate_result(
      this->policy_, this->registry_.availability(), this->attempts_, result);

  if (decision.type == PolicyDecisionType::COMPLETE) {
    this->complete_(result, now_ms);
    return;
  }

  if (decision.type == PolicyDecisionType::INVALID_RESULT) {
    this->complete_failure_(result.transport_used,
                            TransportStatus::PROTOCOL_ERROR,
                            CommunicationErrorCode::INTERNAL_ERROR,
                            "transport returned inconsistent result", false,
                            now_ms);
    return;
  }

  this->last_transport_failure_ = result;
  this->has_last_transport_failure_ = true;
  if (decision.type == PolicyDecisionType::EXHAUSTED) {
    this->complete_(result, now_ms);
    return;
  }

  this->attempt_next_(now_ms);
}

inline void CommunicationTransactionRunner::complete_(const CommandResult &result,
                                                uint32_t now_ms) {
  this->cancel_listeners_();
  COMM_DELIVERY_LOG("CA1 terminal tx=%llu transport=%u result=%u",
      static_cast<unsigned long long>(result.transaction_id),
      static_cast<unsigned>(result.transport_used), static_cast<unsigned>(result.result));
  this->has_progress_result_ = false;
  this->terminal_result_ = result;
  this->terminal_result_.latency_ms = now_ms - this->started_ms_;
  this->active_transport_ = nullptr;
  this->state_ = TransactionRunnerState::RESULT_READY;
}

inline void CommunicationTransactionRunner::complete_failure_(
    CommunicationTransport transport, TransportStatus transport_status,
    CommunicationErrorCode error_code, const char *message, bool retryable,
    uint32_t now_ms) {
  this->complete_(this->make_failure_(transport, transport_status, error_code,
                                     message, retryable, now_ms),
                  now_ms);
}

inline CommandResult CommunicationTransactionRunner::make_failure_(
    CommunicationTransport transport, TransportStatus transport_status,
    CommunicationErrorCode error_code, const char *message, bool retryable,
    uint32_t now_ms) const {
  CommandResult result{};
  result.transaction_id = this->request_.transaction_id;
  result.transport_used = transport;
  result.transport_status = transport_status;
  result.result = CommandResultStatus::FAILED;
  result.error.code = error_code;
  result.error.retryable = retryable;
  result.error.message.assign(message);
  result.latency_ms = now_ms - this->started_ms_;
  return result;
}

inline bool CommunicationTransactionRunner::timed_out_(uint32_t now_ms) const {
  if (this->accepted_)
    return now_ms - this->accepted_ms_ >= this->completion_timeout_ms_;
  return (now_ms - this->started_ms_) >= this->request_.timeout_ms;
}

inline void CommunicationTransactionRunner::reset_() {
  this->policy_ = {};
  this->attempts_.clear();
  this->request_ = {};
  this->progress_result_ = {};
  this->terminal_result_ = {};
  this->last_transport_failure_ = {};
  this->active_transport_ = nullptr;
  this->started_ms_ = 0;
  this->has_progress_result_ = false;
  this->has_last_transport_failure_ = false;
  this->state_ = TransactionRunnerState::IDLE;
}

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome
#undef COMM_DELIVERY_LOG
