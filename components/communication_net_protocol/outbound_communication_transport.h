#pragma once

#include <cstddef>
#include <cstdint>

#include "outbound_communication_policy.h"
#include "outbound_communication_transaction.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

enum class TransportStartStatus : uint8_t {
  STARTED,
  BUSY,
  INVALID_REQUEST,
  UNAVAILABLE,
  REJECTED,
};

class CommunicationTransportAdapter {
 public:
  virtual ~CommunicationTransportAdapter() = default;

  virtual CommunicationTransport transport_type() const = 0;
  virtual bool is_available() const = 0;
  virtual bool is_busy() const = 0;

  // Starts one asynchronous attempt. Implementations must return promptly and
  // must not retain references to request storage owned by the caller.
  virtual TransportStartStatus start(const CommandRequest &request) = 0;
  // Called only by a coordinator that will cancel at its application deadline.
  virtual void use_external_result_deadline(TransactionId) {}

  // Advances transport work cooperatively. It must not block waiting for I/O.
  virtual void loop(uint32_t now_ms) = 0;

  virtual bool has_result() const = 0;

  // Moves the completed normalized result to the caller and releases the
  // adapter for another attempt. Returns false when no result is ready.
  virtual bool take_result(CommandResult &result) = 0;

  // Best-effort cancellation of the matching active attempt.
  virtual bool cancel(TransactionId transaction_id) = 0;

  // The shared submission service requires all participating adapters to use
  // the same source/session identity. Legacy adapters remain usable by their
  // existing callers, but are not admitted by that service until upgraded.
  virtual bool application_identity_matches(const char *, uint64_t) const { return false; }
};

class CommunicationTransportRegistry {
 public:
  static constexpr size_t MAX_TRANSPORTS = CommunicationPolicy::MAX_TRANSPORTS;

  bool register_transport(CommunicationTransportAdapter *adapter) {
    if (adapter == nullptr ||
        adapter->transport_type() == CommunicationTransport::NONE ||
        this->transport_count_ >= MAX_TRANSPORTS ||
        this->find(adapter->transport_type()) != nullptr) {
      return false;
    }

    this->transports_[this->transport_count_++] = adapter;
    return true;
  }

  CommunicationTransportAdapter *find(CommunicationTransport transport) const {
    if (transport == CommunicationTransport::NONE) {
      return nullptr;
    }

    for (size_t index = 0; index < this->transport_count_; index++) {
      if (this->transports_[index]->transport_type() == transport) {
        return this->transports_[index];
      }
    }
    return nullptr;
  }

  TransportAvailability availability() const {
    TransportAvailability snapshot{};
    for (size_t index = 0; index < this->transport_count_; index++) {
      CommunicationTransportAdapter *adapter = this->transports_[index];
      snapshot.set_available(adapter->transport_type(),
                             adapter->is_available() && !adapter->is_busy());
    }
    return snapshot;
  }

  void loop(uint32_t now_ms) {
    for (size_t index = 0; index < this->transport_count_; index++) {
      this->transports_[index]->loop(now_ms);
    }
  }

  size_t transport_count() const { return this->transport_count_; }

 private:
  CommunicationTransportAdapter *transports_[MAX_TRANSPORTS]{};
  size_t transport_count_{0};
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome
