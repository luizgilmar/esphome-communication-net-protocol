#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace communication_net_protocol { namespace outbound {

using TransactionId = uint64_t;

template<size_t Capacity> class BoundedText {
 public:
  static_assert(Capacity > 0, "BoundedText capacity must be greater than zero");

  bool assign(const char *value) {
    this->clear();
    if (value == nullptr) {
      return false;
    }

    size_t length = 0;
    while (length <= Capacity && value[length] != '\0') {
      length++;
    }
    if (length > Capacity) {
      return false;
    }

    std::memcpy(this->data_, value, length);
    this->data_[length] = '\0';
    this->size_ = length;
    return true;
  }

  void clear() {
    this->data_[0] = '\0';
    this->size_ = 0;
  }

  const char *c_str() const { return this->data_; }
  size_t size() const { return this->size_; }
  bool empty() const { return this->size_ == 0; }

 private:
  char data_[Capacity + 1]{};
  size_t size_{0};
};

template<size_t Capacity> class BoundedBytes {
 public:
  static_assert(Capacity > 0, "BoundedBytes capacity must be greater than zero");

  bool assign(const uint8_t *value, size_t length) {
    this->clear();
    if ((value == nullptr && length != 0) || length > Capacity) {
      return false;
    }

    if (length != 0) {
      std::memcpy(this->data_, value, length);
    }
    this->size_ = length;
    return true;
  }

  void clear() { this->size_ = 0; }

  const uint8_t *data() const { return this->data_; }
  size_t size() const { return this->size_; }
  bool empty() const { return this->size_ == 0; }

 private:
  uint8_t data_[Capacity]{};
  size_t size_{0};
};

enum class CommunicationTransport : uint8_t {
  NONE,
  HTTP,
  MQTT,
  ESP_NOW,
  MATTER,
};

enum class TransportStatus : uint8_t {
  NOT_ATTEMPTED,
  SUCCEEDED,
  UNAVAILABLE,
  CONNECTION_FAILED,
  AUTHENTICATION_FAILED,
  TIMED_OUT,
  PROTOCOL_ERROR,
};

enum class CommandResultStatus : uint8_t {
  SUCCEEDED,
  IN_PROGRESS,
  REJECTED,
  FAILED,
};

enum class RemoteStateCompleteness : uint8_t {
  NOT_PROVIDED,
  PARTIAL,
  COMPLETE,
};

enum class CommunicationErrorCode : uint8_t {
  NONE,
  INVALID_REQUEST,
  TARGET_UNAVAILABLE,
  TRANSPORT_UNAVAILABLE,
  CONNECTION_FAILED,
  AUTHENTICATION_FAILED,
  TIMED_OUT,
  PROTOCOL_ERROR,
  REMOTE_REJECTED,
  INTERNAL_ERROR,
  INTERRUPTED,
};

struct CommandTarget {
  static constexpr size_t MAX_DEVICE_ID_LENGTH = 63;
  static constexpr size_t MAX_RESOURCE_LENGTH = 63;

  BoundedText<MAX_DEVICE_ID_LENGTH> device_id{};
  BoundedText<MAX_RESOURCE_LENGTH> resource{};

  bool is_valid() const { return !this->device_id.empty(); }
};

struct CommandIntent {
  static constexpr size_t MAX_NAME_LENGTH = 47;
  static constexpr size_t MAX_PAYLOAD_SIZE = 256;

  BoundedText<MAX_NAME_LENGTH> name{};
  BoundedBytes<MAX_PAYLOAD_SIZE> payload{};

  bool is_valid() const { return !this->name.empty(); }
};

struct CommandRequest {
  static constexpr size_t MAX_POLICY_ID_LENGTH = 31;

  TransactionId transaction_id{0};
  CommandTarget target{};
  CommandIntent command{};
  uint32_t timeout_ms{0};
  BoundedText<MAX_POLICY_ID_LENGTH> policy_id{};

  bool is_valid() const {
    return this->transaction_id != 0 && this->target.is_valid() &&
           this->command.is_valid() && this->timeout_ms != 0;
  }
};

struct StateSnapshot {
  static constexpr size_t MAX_SCHEMA_LENGTH = 31;
  static constexpr size_t MAX_DATA_SIZE = 256;

  RemoteStateCompleteness completeness{RemoteStateCompleteness::NOT_PROVIDED};
  BoundedText<MAX_SCHEMA_LENGTH> schema{};
  BoundedBytes<MAX_DATA_SIZE> data{};

  bool is_consistent() const {
    if (this->completeness == RemoteStateCompleteness::NOT_PROVIDED) {
      return this->schema.empty() && this->data.empty();
    }
    return !this->schema.empty() && !this->data.empty();
  }
};

struct ExecutionContext {
  bool started{false};
  bool has_estimated_completion{false};
  uint32_t estimated_completion_ms{0};
  bool has_progress{false};
  uint8_t progress_percent{0};
  bool has_expected_final_state{false};
  StateSnapshot expected_final_state{};

  bool is_consistent() const {
    if (this->has_estimated_completion &&
        (!this->started || this->estimated_completion_ms == 0)) {
      return false;
    }
    if (this->has_progress && (!this->started || this->progress_percent > 100)) {
      return false;
    }
    if (this->has_expected_final_state) {
      return this->started && this->expected_final_state.is_consistent() &&
             this->expected_final_state.completeness !=
                 RemoteStateCompleteness::NOT_PROVIDED;
    }
    return this->expected_final_state.completeness ==
               RemoteStateCompleteness::NOT_PROVIDED &&
           this->expected_final_state.is_consistent();
  }
};

struct CommunicationError {
  static constexpr size_t MAX_MESSAGE_LENGTH = 95;

  CommunicationErrorCode code{CommunicationErrorCode::NONE};
  bool retryable{false};
  BoundedText<MAX_MESSAGE_LENGTH> message{};

  bool is_consistent() const {
    if (this->code == CommunicationErrorCode::NONE) {
      return !this->retryable && this->message.empty();
    }
    return !this->message.empty();
  }
};

struct CommandResult {
  TransactionId transaction_id{0};
  CommunicationTransport transport_used{CommunicationTransport::NONE};
  TransportStatus transport_status{TransportStatus::NOT_ATTEMPTED};
  CommandResultStatus result{CommandResultStatus::FAILED};
  ExecutionContext execution{};
  StateSnapshot remote_state{};
  CommunicationError error{};
  uint32_t latency_ms{0};

  bool is_consistent() const {
    if (this->transaction_id == 0 || !this->execution.is_consistent() ||
        !this->remote_state.is_consistent() || !this->error.is_consistent()) {
      return false;
    }

    if (this->result == CommandResultStatus::IN_PROGRESS) {
      return this->transport_used != CommunicationTransport::NONE &&
             this->transport_status == TransportStatus::SUCCEEDED &&
             this->execution.started &&
             this->execution.has_estimated_completion &&
             this->execution.estimated_completion_ms != 0 &&
             this->error.code == CommunicationErrorCode::NONE;
    }

    if (this->result == CommandResultStatus::SUCCEEDED) {
      return this->transport_used != CommunicationTransport::NONE &&
             this->transport_status == TransportStatus::SUCCEEDED &&
             this->execution.started &&
             this->error.code == CommunicationErrorCode::NONE;
    }

    return this->error.code != CommunicationErrorCode::NONE;
  }
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome
