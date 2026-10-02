#pragma once

#include <cstddef>
#include <cstdint>

#include "outbound_communication_transaction.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

constexpr size_t COMMUNICATION_TRANSPORT_COUNT =
    static_cast<size_t>(CommunicationTransport::MATTER) + 1U;

class CommunicationPolicy {
 public:
  static constexpr size_t MAX_TRANSPORTS = 4;

  bool add_transport(CommunicationTransport transport) {
    if (transport == CommunicationTransport::NONE ||
        this->transport_count_ >= MAX_TRANSPORTS ||
        this->contains(transport)) {
      return false;
    }

    this->transports_[this->transport_count_++] = transport;
    return true;
  }

  bool contains(CommunicationTransport transport) const {
    for (size_t index = 0; index < this->transport_count_; index++) {
      if (this->transports_[index] == transport) {
        return true;
      }
    }
    return false;
  }

  CommunicationTransport transport_at(size_t index) const {
    if (index >= this->transport_count_) {
      return CommunicationTransport::NONE;
    }
    return this->transports_[index];
  }

  size_t transport_count() const { return this->transport_count_; }
  bool is_valid() const { return this->transport_count_ != 0; }

 private:
  CommunicationTransport transports_[MAX_TRANSPORTS]{};
  size_t transport_count_{0};
};

class TransportAvailability {
 public:
  bool set_available(CommunicationTransport transport, bool available) {
    const size_t index = static_cast<size_t>(transport);
    if (transport == CommunicationTransport::NONE ||
        index >= COMMUNICATION_TRANSPORT_COUNT) {
      return false;
    }
    this->available_[index] = available;
    return true;
  }

  bool is_available(CommunicationTransport transport) const {
    const size_t index = static_cast<size_t>(transport);
    return transport != CommunicationTransport::NONE &&
           index < COMMUNICATION_TRANSPORT_COUNT && this->available_[index];
  }

 private:
  bool available_[COMMUNICATION_TRANSPORT_COUNT]{};
};

class TransportAttemptSet {
 public:
  bool mark_attempted(CommunicationTransport transport) {
    const uint8_t index = static_cast<uint8_t>(transport);
    if (transport == CommunicationTransport::NONE || index >= 8U) {
      return false;
    }
    this->attempted_mask_ |= static_cast<uint8_t>(1U << index);
    return true;
  }

  bool was_attempted(CommunicationTransport transport) const {
    const uint8_t index = static_cast<uint8_t>(transport);
    return transport != CommunicationTransport::NONE && index < 8U &&
           (this->attempted_mask_ & static_cast<uint8_t>(1U << index)) != 0;
  }

  void clear() { this->attempted_mask_ = 0; }

 private:
  uint8_t attempted_mask_{0};
};

enum class PolicyDecisionType : uint8_t {
  USE_TRANSPORT,
  COMPLETE,
  EXHAUSTED,
  INVALID_RESULT,
};

struct PolicyDecision {
  PolicyDecisionType type{PolicyDecisionType::EXHAUSTED};
  CommunicationTransport transport{CommunicationTransport::NONE};
};

class CommunicationPolicyEngine {
 public:
  PolicyDecision select_next(const CommunicationPolicy &policy,
                             const TransportAvailability &availability,
                             const TransportAttemptSet &attempts) const {
    for (size_t index = 0; index < policy.transport_count(); index++) {
      const CommunicationTransport transport = policy.transport_at(index);
      if (availability.is_available(transport) &&
          !attempts.was_attempted(transport)) {
        return {PolicyDecisionType::USE_TRANSPORT, transport};
      }
    }
    return {PolicyDecisionType::EXHAUSTED, CommunicationTransport::NONE};
  }

  PolicyDecision evaluate_result(const CommunicationPolicy &policy,
                                 const TransportAvailability &availability,
                                 const TransportAttemptSet &attempts,
                                 const CommandResult &result) const {
    if (!result.is_consistent()) {
      return {PolicyDecisionType::INVALID_RESULT,
              CommunicationTransport::NONE};
    }

    if (result.result == CommandResultStatus::SUCCEEDED ||
        result.result == CommandResultStatus::IN_PROGRESS ||
        result.result == CommandResultStatus::REJECTED ||
        result.transport_status == TransportStatus::SUCCEEDED) {
      return {PolicyDecisionType::COMPLETE, CommunicationTransport::NONE};
    }

    TransportAttemptSet completed_attempts = attempts;
    completed_attempts.mark_attempted(result.transport_used);
    return this->select_next(policy, availability, completed_attempts);
  }
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome
