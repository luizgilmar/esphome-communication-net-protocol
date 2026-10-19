#pragma once

#include <cstddef>
#include <cstdint>

#include "outbound_http_json_codec.h"
#include "outbound_communication_transaction.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

enum class MqttJsonCodecError : uint8_t {
  NONE,
  INVALID_REQUEST,
  INVALID_TOPIC,
  INVALID_JSON_PAYLOAD,
  PAYLOAD_TOO_LARGE,
  EMPTY_RESPONSE,
  MALFORMED_RESPONSE,
  CORRELATION_MISMATCH,
  UNSUPPORTED_RESULT,
  INCONSISTENT_RESULT,
};

struct MqttTopic {
  static constexpr size_t MAX_SIZE = 192;
  BoundedText<MAX_SIZE> value{};
};

struct MqttJsonPayload {
  static constexpr size_t MAX_SIZE = 1280;
  BoundedBytes<MAX_SIZE> data{};
};

class CommunicationMqttJsonCodec {
 public:
  bool build_command_topic(const char *command_prefix,
                           const char *target_device_id, MqttTopic &topic);
  bool encode_request(const CommandRequest &request, const char *reply_topic,
                      MqttJsonPayload &payload,
                      const char *source_device_id = nullptr,
                      uint64_t source_boot_id = 0);
  bool decode_response(TransactionId expected_transaction_id,
                       const uint8_t *payload, size_t payload_size,
                       uint32_t latency_ms, CommandResult &result);
  MqttJsonCodecError last_error() const { return this->last_error_; }

 private:
  static bool valid_topic_(const char *topic, size_t &size);
  void map_http_error_();
  void set_error_(MqttJsonCodecError error) { this->last_error_ = error; }

  CommunicationHttpJsonCodec shared_json_codec_{};
  MqttJsonCodecError last_error_{MqttJsonCodecError::NONE};
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome
