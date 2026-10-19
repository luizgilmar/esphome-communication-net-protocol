#include "outbound_http_json_codec.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace communication_net_protocol { namespace outbound {
namespace {

constexpr size_t MAX_JSON_DEPTH = 8;

struct JsonView {
  const char *data{nullptr};
  size_t size{0};
};

void skip_ws(const char *data, size_t size, size_t &pos) {
  while (pos < size && (data[pos] == ' ' || data[pos] == '\t' ||
                        data[pos] == '\r' || data[pos] == '\n')) pos++;
}

bool scan_string(const char *data, size_t size, size_t &pos) {
  if (pos >= size || data[pos++] != '"') return false;
  while (pos < size) {
    const unsigned char byte = static_cast<unsigned char>(data[pos++]);
    if (byte == '"') return true;
    if (byte < 0x20U) return false;
    if (byte != '\\') continue;
    if (pos >= size) return false;
    const char escape = data[pos++];
    if (std::strchr("\"\\/bfnrt", escape) != nullptr) continue;
    if (escape != 'u' || pos + 4 > size) return false;
    for (size_t i = 0; i < 4; i++) {
      const char hex = data[pos++];
      if (!((hex >= '0' && hex <= '9') || (hex >= 'a' && hex <= 'f') ||
            (hex >= 'A' && hex <= 'F'))) return false;
    }
  }
  return false;
}

bool scan_value(const char *data, size_t size, size_t &pos, size_t depth);

bool scan_container(const char *data, size_t size, size_t &pos, size_t depth,
                    bool object) {
  if (depth >= MAX_JSON_DEPTH) return false;
  const char close = object ? '}' : ']';
  pos++;
  skip_ws(data, size, pos);
  if (pos < size && data[pos] == close) {
    pos++;
    return true;
  }
  while (pos < size) {
    if (object) {
      if (!scan_string(data, size, pos)) return false;
      skip_ws(data, size, pos);
      if (pos >= size || data[pos++] != ':') return false;
      skip_ws(data, size, pos);
    }
    if (!scan_value(data, size, pos, depth + 1)) return false;
    skip_ws(data, size, pos);
    if (pos < size && data[pos] == close) {
      pos++;
      return true;
    }
    if (pos >= size || data[pos++] != ',') return false;
    skip_ws(data, size, pos);
  }
  return false;
}

bool scan_literal(const char *data, size_t size, size_t &pos,
                  const char *literal) {
  const size_t length = std::strlen(literal);
  if (pos + length > size ||
      std::memcmp(data + pos, literal, length) != 0) return false;
  pos += length;
  return true;
}

bool scan_number(const char *data, size_t size, size_t &pos) {
  if (pos < size && data[pos] == '-') pos++;
  if (pos >= size) return false;
  if (data[pos] == '0') pos++;
  else {
    if (data[pos] < '1' || data[pos] > '9') return false;
    while (pos < size && data[pos] >= '0' && data[pos] <= '9') pos++;
  }
  if (pos < size && data[pos] == '.') {
    pos++;
    const size_t start = pos;
    while (pos < size && data[pos] >= '0' && data[pos] <= '9') pos++;
    if (pos == start) return false;
  }
  if (pos < size && (data[pos] == 'e' || data[pos] == 'E')) {
    pos++;
    if (pos < size && (data[pos] == '+' || data[pos] == '-')) pos++;
    const size_t start = pos;
    while (pos < size && data[pos] >= '0' && data[pos] <= '9') pos++;
    if (pos == start) return false;
  }
  return true;
}

bool scan_value(const char *data, size_t size, size_t &pos, size_t depth) {
  skip_ws(data, size, pos);
  if (pos >= size) return false;
  if (data[pos] == '{') return scan_container(data, size, pos, depth, true);
  if (data[pos] == '[') return scan_container(data, size, pos, depth, false);
  if (data[pos] == '"') return scan_string(data, size, pos);
  if (data[pos] == 't') return scan_literal(data, size, pos, "true");
  if (data[pos] == 'f') return scan_literal(data, size, pos, "false");
  if (data[pos] == 'n') return scan_literal(data, size, pos, "null");
  return scan_number(data, size, pos);
}

bool single_value(const char *data, size_t size) {
  size_t pos = 0;
  if (!scan_value(data, size, pos, 0)) return false;
  skip_ws(data, size, pos);
  return pos == size;
}

class Writer {
 public:
  Writer(char *data, size_t capacity) : data_(data), capacity_(capacity) {}
  bool append(char value) {
    if (size_ == capacity_) return false;
    data_[size_++] = value;
    return true;
  }
  bool append(const char *value) {
    return value != nullptr && append(value, std::strlen(value));
  }
  bool append(const char *value, size_t length) {
    if (value == nullptr || length > capacity_ - size_) return false;
    std::memcpy(data_ + size_, value, length);
    size_ += length;
    return true;
  }
  bool string(const char *value, size_t length) {
    if (!append('"')) return false;
    for (size_t i = 0; i < length; i++) {
      const unsigned char byte = static_cast<unsigned char>(value[i]);
      const char *escape = nullptr;
      if (byte == '"') escape = "\\\"";
      else if (byte == '\\') escape = "\\\\";
      else if (byte == '\b') escape = "\\b";
      else if (byte == '\f') escape = "\\f";
      else if (byte == '\n') escape = "\\n";
      else if (byte == '\r') escape = "\\r";
      else if (byte == '\t') escape = "\\t";
      if (escape != nullptr) {
        if (!append(escape)) return false;
      } else if (byte < 0x20U) {
        char encoded[7]{};
        std::snprintf(encoded, sizeof(encoded), "\\u%04x", byte);
        if (!append(encoded, 6)) return false;
      } else if (!append(static_cast<char>(byte))) return false;
    }
    return append('"');
  }
  size_t size() const { return size_; }
 private:
  char *data_;
  size_t capacity_;
  size_t size_{0};
};

bool member(JsonView object, const char *name, JsonView &value) {
  size_t pos = 0;
  skip_ws(object.data, object.size, pos);
  if (pos >= object.size || object.data[pos++] != '{') return false;
  skip_ws(object.data, object.size, pos);
  while (pos < object.size && object.data[pos] != '}') {
    const size_t key_start = pos;
    if (!scan_string(object.data, object.size, pos)) return false;
    const size_t key_end = pos;
    skip_ws(object.data, object.size, pos);
    if (pos >= object.size || object.data[pos++] != ':') return false;
    skip_ws(object.data, object.size, pos);
    const size_t value_start = pos;
    if (!scan_value(object.data, object.size, pos, 0)) return false;
    const size_t name_size = std::strlen(name);
    if (key_end == key_start + name_size + 2 &&
        std::memcmp(object.data + key_start + 1, name, name_size) == 0) {
      value = {object.data + value_start, pos - value_start};
      return true;
    }
    skip_ws(object.data, object.size, pos);
    if (pos < object.size && object.data[pos] == ',') {
      pos++;
      skip_ws(object.data, object.size, pos);
    } else break;
  }
  return false;
}

bool string_value(JsonView view, char *output, size_t capacity) {
  if (capacity == 0 || view.size < 2 || view.data[0] != '"' ||
      view.data[view.size - 1] != '"') return false;
  size_t written = 0;
  for (size_t pos = 1; pos + 1 < view.size; pos++) {
    char value = view.data[pos];
    if (value == '\\') {
      if (++pos + 1 >= view.size) return false;
      const char escape = view.data[pos];
      if (std::strchr("\"\\/", escape) != nullptr) value = escape;
      else if (escape == 'b') value = '\b';
      else if (escape == 'f') value = '\f';
      else if (escape == 'n') value = '\n';
      else if (escape == 'r') value = '\r';
      else if (escape == 't') value = '\t';
      else if (escape == 'u') {
        if (pos + 4 >= view.size - 1) return false;
        pos += 4;
        value = '?';
      } else return false;
    }
    if (written + 1 >= capacity) return false;
    output[written++] = value;
  }
  output[written] = '\0';
  return true;
}

bool bool_value(JsonView view, bool &value) {
  if (view.size == 4 && std::memcmp(view.data, "true", 4) == 0) {
    value = true;
    return true;
  }
  if (view.size == 5 && std::memcmp(view.data, "false", 5) == 0) {
    value = false;
    return true;
  }
  return false;
}

bool uint64_value(JsonView view, uint64_t &value) {
  char text[32]{};
  if (view.size >= sizeof(text)) return false;
  if (view.size >= 2 && view.data[0] == '"') {
    if (!string_value(view, text, sizeof(text))) return false;
  } else {
    std::memcpy(text, view.data, view.size);
    text[view.size] = '\0';
  }
  if (text[0] == '\0') return false;
  uint64_t parsed = 0;
  for (size_t i = 0; text[i] != '\0'; i++) {
    if (text[i] < '0' || text[i] > '9') return false;
    const uint8_t digit = static_cast<uint8_t>(text[i] - '0');
    if (parsed > (UINT64_MAX - digit) / 10U) return false;
    parsed = parsed * 10U + digit;
  }
  value = parsed;
  return true;
}

bool uint32_value(JsonView view, uint32_t &value) {
  uint64_t parsed = 0;
  if (!uint64_value(view, parsed) || parsed > UINT32_MAX) return false;
  value = static_cast<uint32_t>(parsed);
  return true;
}

bool state_value(JsonView view, StateSnapshot &state) {
  if (view.size > StateSnapshot::MAX_DATA_SIZE ||
      !state.schema.assign("application/json")) return false;
  state.completeness = RemoteStateCompleteness::COMPLETE;
  return state.data.assign(reinterpret_cast<const uint8_t *>(view.data),
                           view.size);
}

bool hex_state_value(JsonView remote, StateSnapshot &state) {
  JsonView field{};
  char schema[StateSnapshot::MAX_SCHEMA_LENGTH + 1]{};
  char hex[StateSnapshot::MAX_DATA_SIZE * 2 + 1]{};
  if (!member(remote, "schema", field) ||
      !string_value(field, schema, sizeof(schema)) ||
      !member(remote, "data_hex", field) ||
      !string_value(field, hex, sizeof(hex)) ||
      !state.schema.assign(schema))
    return false;
  const size_t hex_size = std::strlen(hex);
  if (hex_size == 0 || (hex_size & 1U) != 0 ||
      hex_size / 2 > StateSnapshot::MAX_DATA_SIZE)
    return false;
  uint8_t data[StateSnapshot::MAX_DATA_SIZE]{};
  for (size_t index = 0; index < hex_size; index += 2) {
    const auto nibble = [](char value) -> int {
      if (value >= '0' && value <= '9') return value - '0';
      if (value >= 'a' && value <= 'f') return value - 'a' + 10;
      if (value >= 'A' && value <= 'F') return value - 'A' + 10;
      return -1;
    };
    const int high = nibble(hex[index]);
    const int low = nibble(hex[index + 1]);
    if (high < 0 || low < 0) return false;
    data[index / 2] = static_cast<uint8_t>((high << 4U) | low);
  }
  return state.data.assign(data, hex_size / 2);
}

CommunicationErrorCode error_code(const char *value) {
  if (std::strcmp(value, "invalid_request") == 0)
    return CommunicationErrorCode::INVALID_REQUEST;
  if (std::strcmp(value, "target_unavailable") == 0)
    return CommunicationErrorCode::TARGET_UNAVAILABLE;
  if (std::strcmp(value, "transport_unavailable") == 0)
    return CommunicationErrorCode::TRANSPORT_UNAVAILABLE;
  if (std::strcmp(value, "connection_failed") == 0)
    return CommunicationErrorCode::CONNECTION_FAILED;
  if (std::strcmp(value, "authentication_failed") == 0)
    return CommunicationErrorCode::AUTHENTICATION_FAILED;
  if (std::strcmp(value, "timed_out") == 0)
    return CommunicationErrorCode::TIMED_OUT;
  if (std::strcmp(value, "protocol_error") == 0)
    return CommunicationErrorCode::PROTOCOL_ERROR;
  if (std::strcmp(value, "remote_rejected") == 0)
    return CommunicationErrorCode::REMOTE_REJECTED;
  if (std::strcmp(value, "interrupted") == 0)
    return CommunicationErrorCode::INTERRUPTED;
  return CommunicationErrorCode::INTERNAL_ERROR;
}

}  // namespace

bool CommunicationHttpJsonCodec::encode_request(const CommandRequest &request,
                                                HttpJsonRequestBody &body) {
  set_error_(HttpJsonCodecError::NONE);
  body.data.clear();
  if (!request.is_valid()) {
    set_error_(HttpJsonCodecError::INVALID_REQUEST);
    return false;
  }
  const char *payload =
      reinterpret_cast<const char *>(request.command.payload.data());
  const size_t payload_size = request.command.payload.size();
  if (payload_size != 0 && !single_value(payload, payload_size)) {
    set_error_(HttpJsonCodecError::INVALID_JSON_PAYLOAD);
    return false;
  }

  char output[HttpJsonRequestBody::MAX_SIZE]{};
  char transaction_id[24]{};
  char timeout_ms[16]{};
  std::snprintf(transaction_id, sizeof(transaction_id), "%" PRIu64,
                request.transaction_id);
  std::snprintf(timeout_ms, sizeof(timeout_ms), "%" PRIu32,
                request.timeout_ms);
  Writer writer{output, sizeof(output)};
  const bool ok =
      writer.append("{\"transaction_id\":") &&
      writer.string(transaction_id, std::strlen(transaction_id)) &&
      writer.append(",\"target\":{\"device_id\":") &&
      writer.string(request.target.device_id.c_str(),
                    request.target.device_id.size()) &&
      writer.append(",\"resource\":") &&
      writer.string(request.target.resource.c_str(),
                    request.target.resource.size()) &&
      writer.append("},\"command\":{\"name\":") &&
      writer.string(request.command.name.c_str(), request.command.name.size()) &&
      writer.append(",\"payload\":") &&
      (payload_size == 0 ? writer.append("{}")
                         : writer.append(payload, payload_size)) &&
      writer.append("},\"timeout_ms\":") && writer.append(timeout_ms) &&
      writer.append('}');
  if (!ok || !body.data.assign(reinterpret_cast<const uint8_t *>(output),
                               writer.size())) {
    set_error_(HttpJsonCodecError::REQUEST_TOO_LARGE);
    return false;
  }
  return true;
}

bool CommunicationHttpJsonCodec::decode_response(
    TransactionId expected_transaction_id, const uint8_t *body,
    size_t body_size, uint32_t latency_ms, CommandResult &result) {
  set_error_(HttpJsonCodecError::NONE);
  result = {};
  if (body == nullptr || body_size == 0) {
    set_error_(HttpJsonCodecError::EMPTY_RESPONSE);
    return false;
  }
  const char *json = reinterpret_cast<const char *>(body);
  if (!single_value(json, body_size)) {
    set_error_(HttpJsonCodecError::MALFORMED_RESPONSE);
    return false;
  }
  const JsonView root{json, body_size};
  JsonView field{};
  uint64_t transaction_id = 0;
  if (!member(root, "transaction_id", field) ||
      !uint64_value(field, transaction_id)) {
    set_error_(HttpJsonCodecError::MALFORMED_RESPONSE);
    return false;
  }
  if (transaction_id != expected_transaction_id) {
    set_error_(HttpJsonCodecError::CORRELATION_MISMATCH);
    return false;
  }
  char result_name[24]{};
  if (!member(root, "result", field) ||
      !string_value(field, result_name, sizeof(result_name))) {
    set_error_(HttpJsonCodecError::MALFORMED_RESPONSE);
    return false;
  }

  result.transaction_id = transaction_id;
  result.transport_used = CommunicationTransport::HTTP;
  result.transport_status = TransportStatus::SUCCEEDED;
  result.latency_ms = latency_ms;
  if (std::strcmp(result_name, "succeeded") == 0)
    result.result = CommandResultStatus::SUCCEEDED;
  else if (std::strcmp(result_name, "in_progress") == 0)
    result.result = CommandResultStatus::IN_PROGRESS;
  else if (std::strcmp(result_name, "rejected") == 0)
    result.result = CommandResultStatus::REJECTED;
  else if (std::strcmp(result_name, "failed") == 0)
    result.result = CommandResultStatus::FAILED;
  else {
    set_error_(HttpJsonCodecError::UNSUPPORTED_RESULT);
    return false;
  }

  JsonView execution{};
  JsonView remote_state{};
  JsonView error{};
  if (member(root, "execution", execution)) {
    if (member(execution, "started", field) &&
        !bool_value(field, result.execution.started)) goto malformed;
    if (member(execution, "estimated_completion_ms", field)) {
      result.execution.has_estimated_completion = true;
      if (!uint32_value(field, result.execution.estimated_completion_ms))
        goto malformed;
    }
    if (member(execution, "progress", field)) {
      uint32_t progress = 0;
      if (!uint32_value(field, progress) || progress > 100) goto malformed;
      result.execution.has_progress = true;
      result.execution.progress_percent = static_cast<uint8_t>(progress);
    }
    if (member(execution, "expected_final_state", field)) {
      result.execution.has_expected_final_state = true;
      if (!state_value(field, result.execution.expected_final_state))
        goto malformed;
    }
  }

  if (member(root, "remote_state", remote_state)) {
    bool complete = false;
    if (member(remote_state, "complete", field) &&
        !bool_value(field, complete)) goto malformed;
    JsonView schema_field{};
    JsonView data_field{};
    if (member(remote_state, "schema", schema_field) &&
        member(remote_state, "data_hex", data_field)) {
      if (!hex_state_value(remote_state, result.remote_state)) goto malformed;
      result.remote_state.completeness =
          complete ? RemoteStateCompleteness::COMPLETE
                   : RemoteStateCompleteness::PARTIAL;
    } else if (member(remote_state, "value", field)) {
      if (!state_value(field, result.remote_state)) goto malformed;
      result.remote_state.completeness =
          complete ? RemoteStateCompleteness::COMPLETE
                   : RemoteStateCompleteness::PARTIAL;
    }
  }

  if (member(root, "error", error)) {
    char code[32]{};
    char message[CommunicationError::MAX_MESSAGE_LENGTH + 1]{};
    if (!member(error, "code", field) ||
        !string_value(field, code, sizeof(code)) ||
        !member(error, "message", field) ||
        !string_value(field, message, sizeof(message)) ||
        !result.error.message.assign(message)) goto malformed;
    result.error.code = error_code(code);
    if (member(error, "retryable", field) &&
        !bool_value(field, result.error.retryable)) goto malformed;
  }

  if (!result.is_consistent()) {
    set_error_(HttpJsonCodecError::INCONSISTENT_RESULT);
    return false;
  }
  return true;

malformed:
  set_error_(HttpJsonCodecError::MALFORMED_RESPONSE);
  return false;
}

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome
