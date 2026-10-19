#pragma once

#include <cstddef>
#include <cstdint>

#include "outbound_communication_transaction.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

enum class HttpJsonCodecError : uint8_t {
  NONE,
  INVALID_REQUEST,
  INVALID_JSON_PAYLOAD,
  REQUEST_TOO_LARGE,
  EMPTY_RESPONSE,
  MALFORMED_RESPONSE,
  CORRELATION_MISMATCH,
  UNSUPPORTED_RESULT,
  INCONSISTENT_RESULT,
};

struct HttpJsonRequestBody {
  static constexpr size_t MAX_SIZE = 1024;
  BoundedBytes<MAX_SIZE> data{};
};

class CommunicationHttpJsonCodec {
 public:
  bool encode_request(const CommandRequest &request, HttpJsonRequestBody &body);
  bool decode_response(TransactionId expected_transaction_id,
                       const uint8_t *body, size_t body_size,
                       uint32_t latency_ms, CommandResult &result);
  HttpJsonCodecError last_error() const { return this->last_error_; }

 private:
  void set_error_(HttpJsonCodecError error) { this->last_error_ = error; }
  HttpJsonCodecError last_error_{HttpJsonCodecError::NONE};
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome
