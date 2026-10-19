#include "outbound_mqtt_json_codec.h"

#include <cstdio>
#include <cstring>

namespace esphome {
namespace communication_net_protocol { namespace outbound {
namespace {

class MqttWriter {
 public:
  MqttWriter(char *data, size_t capacity) : data_(data), capacity_(capacity) {}

  bool append(char value) {
    if (size_ == capacity_) return false;
    data_[size_++] = value;
    return true;
  }

  bool append(const char *value, size_t length) {
    if (value == nullptr || length > capacity_ - size_) return false;
    std::memcpy(data_ + size_, value, length);
    size_ += length;
    return true;
  }

  bool append(const char *value) {
    return value != nullptr && append(value, std::strlen(value));
  }

  bool json_string(const char *value, size_t length) {
    if (!append('"')) return false;
    for (size_t index = 0; index < length; index++) {
      const unsigned char byte = static_cast<unsigned char>(value[index]);
      if (byte == '"') {
        if (!append("\\\"")) return false;
      } else if (byte == '\\') {
        if (!append("\\\\")) return false;
      } else if (byte < 0x20U) {
        char escaped[7]{};
        std::snprintf(escaped, sizeof(escaped), "\\u%04x", byte);
        if (!append(escaped, 6)) return false;
      } else if (!append(static_cast<char>(byte))) {
        return false;
      }
    }
    return append('"');
  }

  size_t size() const { return size_; }

 private:
  char *data_;
  size_t capacity_;
  size_t size_{0};
};

}  // namespace

bool CommunicationMqttJsonCodec::valid_topic_(const char *topic,
                                               size_t &size) {
  size = 0;
  if (topic == nullptr || topic[0] == '\0') return false;
  while (size <= MqttTopic::MAX_SIZE && topic[size] != '\0') {
    const unsigned char byte = static_cast<unsigned char>(topic[size]);
    if (byte < 0x20U || byte == 0x7FU || topic[size] == '+' ||
        topic[size] == '#') return false;
    size++;
  }
  return size != 0 && size <= MqttTopic::MAX_SIZE;
}

bool CommunicationMqttJsonCodec::build_command_topic(
    const char *command_prefix, const char *target_device_id,
    MqttTopic &topic) {
  this->set_error_(MqttJsonCodecError::NONE);
  topic.value.clear();
  size_t prefix_size = 0;
  size_t device_size = 0;
  if (!valid_topic_(command_prefix, prefix_size) ||
      !valid_topic_(target_device_id, device_size) ||
      command_prefix[prefix_size - 1] == '/' ||
      target_device_id[0] == '/' || target_device_id[device_size - 1] == '/') {
    this->set_error_(MqttJsonCodecError::INVALID_TOPIC);
    return false;
  }
  char output[MqttTopic::MAX_SIZE + 1]{};
  const int written = std::snprintf(output, sizeof(output), "%s/%s/command",
                                    command_prefix, target_device_id);
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(output) ||
      !topic.value.assign(output)) {
    this->set_error_(MqttJsonCodecError::INVALID_TOPIC);
    return false;
  }
  return true;
}

bool CommunicationMqttJsonCodec::encode_request(
    const CommandRequest &request, const char *reply_topic,
    MqttJsonPayload &payload, const char *source_device_id,
    uint64_t source_boot_id) {
  this->set_error_(MqttJsonCodecError::NONE);
  payload.data.clear();
  size_t reply_topic_size = 0;
  if (!valid_topic_(reply_topic, reply_topic_size)) {
    this->set_error_(MqttJsonCodecError::INVALID_TOPIC);
    return false;
  }

  HttpJsonRequestBody shared_body{};
  if (!this->shared_json_codec_.encode_request(request, shared_body)) {
    const HttpJsonCodecError error = this->shared_json_codec_.last_error();
    this->set_error_(error == HttpJsonCodecError::INVALID_REQUEST
                         ? MqttJsonCodecError::INVALID_REQUEST
                     : error == HttpJsonCodecError::INVALID_JSON_PAYLOAD
                         ? MqttJsonCodecError::INVALID_JSON_PAYLOAD
                         : MqttJsonCodecError::PAYLOAD_TOO_LARGE);
    return false;
  }

  const uint8_t *shared = shared_body.data.data();
  const size_t shared_size = shared_body.data.size();
  if (shared_size < 2 || shared[0] != '{') {
    this->set_error_(MqttJsonCodecError::MALFORMED_RESPONSE);
    return false;
  }

  char output[MqttJsonPayload::MAX_SIZE]{};
  MqttWriter writer{output, sizeof(output)};
  bool ok = writer.append("{\"reply_to\":") &&
            writer.json_string(reply_topic, reply_topic_size);
  if (ok && source_device_id != nullptr && source_device_id[0] != '\0' &&
      source_boot_id != 0) {
    const size_t source_size = std::strlen(source_device_id);
    char boot_id[24]{};
    const int boot_size = std::snprintf(
        boot_id, sizeof(boot_id), "%llu",
        static_cast<unsigned long long>(source_boot_id));
    ok = source_size <= CommandTarget::MAX_DEVICE_ID_LENGTH && boot_size > 0 &&
         static_cast<size_t>(boot_size) < sizeof(boot_id) &&
         writer.append(",\"source\":{\"device_id\":") &&
         writer.json_string(source_device_id, source_size) &&
         writer.append(",\"boot_id\":") &&
         writer.json_string(boot_id, static_cast<size_t>(boot_size)) &&
         writer.append('}');
  }
  ok = ok && writer.append(',') &&
       writer.append(reinterpret_cast<const char *>(shared + 1),
                     shared_size - 1);
  if (!ok || !payload.data.assign(
                 reinterpret_cast<const uint8_t *>(output), writer.size())) {
    this->set_error_(MqttJsonCodecError::PAYLOAD_TOO_LARGE);
    return false;
  }
  return true;
}

bool CommunicationMqttJsonCodec::decode_response(
    TransactionId expected_transaction_id, const uint8_t *payload,
    size_t payload_size, uint32_t latency_ms, CommandResult &result) {
  this->set_error_(MqttJsonCodecError::NONE);
  if (!this->shared_json_codec_.decode_response(
          expected_transaction_id, payload, payload_size, latency_ms, result)) {
    this->map_http_error_();
    return false;
  }
  result.transport_used = CommunicationTransport::MQTT;
  return true;
}

void CommunicationMqttJsonCodec::map_http_error_() {
  switch (this->shared_json_codec_.last_error()) {
    case HttpJsonCodecError::EMPTY_RESPONSE:
      this->set_error_(MqttJsonCodecError::EMPTY_RESPONSE);
      break;
    case HttpJsonCodecError::CORRELATION_MISMATCH:
      this->set_error_(MqttJsonCodecError::CORRELATION_MISMATCH);
      break;
    case HttpJsonCodecError::UNSUPPORTED_RESULT:
      this->set_error_(MqttJsonCodecError::UNSUPPORTED_RESULT);
      break;
    case HttpJsonCodecError::INCONSISTENT_RESULT:
      this->set_error_(MqttJsonCodecError::INCONSISTENT_RESULT);
      break;
    default:
      this->set_error_(MqttJsonCodecError::MALFORMED_RESPONSE);
      break;
  }
}

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome
