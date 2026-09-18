#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "canonical_command.h"

namespace esphome {
namespace communication_net_protocol {

struct MqttCommandIdentity {
  uint64_t transaction_id{0};
  uint64_t source_boot_id{0};
};

struct MqttCommandFields {
  char source[64]{};
  char device[64]{};
  char resource[64]{};
  char action[64]{};
  char reply_topic[193]{};
  uint64_t transaction_id{0};
  uint64_t source_boot_id{0};
  uint32_t timeout_ms{0};
};

// Receive-only compatibility decoder for the current cross-device MQTT wire
// envelope. No source field here authenticates a sender, and this decoder must
// not grant execution rights. Commands with arguments need a separate codec.
class MqttEnvelopeReader {
 public:
  MqttEnvelopeReader(const uint8_t *payload, size_t length)
      : payload_(payload), length_(length) {}

  bool decode(const char *expected_device, uint8_t *canonical, size_t capacity,
              size_t &written, MqttCommandIdentity *identity = nullptr,
              const char *expected_source = nullptr,
              const char *expected_reply = nullptr,
              MqttCommandFields *fields = nullptr) {
    written = 0;
    if (identity) *identity = {};
    if (fields) *fields = {};
    timeout_ms_ = 0;
    if (!payload_ || !expected_device || !canonical || length_ == 0 ||
        length_ > 1280 || !object_open_()) return false;
    unsigned seen = 0;
    char device[64]{}, resource[64]{}, action[64]{};
    char source_device[64]{}, reply_topic[193]{};
    uint64_t transaction_id = 0, source_boot_id = 0;
    do {
      char key[32]{};
      if (!string_(key, sizeof(key)) || !colon_()) return false;
      unsigned bit = 0;
      if (std::strcmp(key, "transaction_id") == 0) {
        bit = 1;
        char transaction[21]{};
        if (!string_(transaction, sizeof(transaction)) ||
            !decimal_(transaction, &transaction_id)) return false;
      } else if (std::strcmp(key, "reply_to") == 0) {
        bit = 2;
        char reply[193]{};
        if (!string_(reply, sizeof(reply)) || std::strchr(reply, '+') ||
            std::strchr(reply, '#')) return false;
        std::strcpy(reply_topic, reply);
      } else if (std::strcmp(key, "source") == 0) {
        bit = 4;
        if (!source_(source_device, source_boot_id)) return false;
      } else if (std::strcmp(key, "target") == 0) {
        bit = 8;
        if (!target_(device, resource)) return false;
      } else if (std::strcmp(key, "command") == 0) {
        bit = 16;
        if (!command_(action)) return false;
      } else if (std::strcmp(key, "timeout_ms") == 0) {
        bit = 32;
        if (!timeout_()) return false;
      } else {
        return false;
      }
      if (seen & bit) return false;
      seen |= bit;
      space_();
      if (consume_('}')) break;
      if (!consume_(',')) return false;
    } while (true);
    space_();
    if (pos_ != length_ || (seen & 31) != 31 ||
        std::strcmp(device, expected_device) != 0 ||
        (expected_source && std::strcmp(source_device, expected_source) != 0) ||
        (expected_reply && std::strcmp(reply_topic, expected_reply) != 0)) return false;
    if (!encode_canonical_command({device, resource, action, nullptr, 0},
                                  canonical, capacity, written)) return false;
    if (identity) *identity = {transaction_id, source_boot_id};
    if (fields) {
      std::strcpy(fields->source, source_device);
      std::strcpy(fields->device, device);
      std::strcpy(fields->resource, resource);
      std::strcpy(fields->action, action);
      std::strcpy(fields->reply_topic, reply_topic);
      fields->transaction_id = transaction_id;
      fields->source_boot_id = source_boot_id;
      fields->timeout_ms = timeout_ms_;
    }
    return true;
  }

 private:
  bool source_(char *device, uint64_t &boot_id) {
    if (!object_open_()) return false;
    unsigned seen = 0;
    do {
      char key[32]{}, value[64]{};
      if (!string_(key, sizeof(key)) || !colon_() ||
          !string_(value, sizeof(value))) return false;
      const unsigned bit = std::strcmp(key, "device_id") == 0 ? 1 :
                           std::strcmp(key, "boot_id") == 0 ? 2 : 0;
      if (!bit || (seen & bit) || (bit == 2 && !decimal_(value, &boot_id))) return false;
      if (bit == 1) std::strcpy(device, value);
      seen |= bit;
      space_();
      if (consume_('}')) return seen == 3;
      if (!consume_(',')) return false;
    } while (true);
  }

  bool target_(char *device, char *resource) {
    if (!object_open_()) return false;
    unsigned seen = 0;
    do {
      char key[32]{};
      if (!string_(key, sizeof(key)) || !colon_()) return false;
      const unsigned bit = std::strcmp(key, "device_id") == 0 ? 1 :
                           std::strcmp(key, "resource") == 0 ? 2 : 0;
      if (!bit || (seen & bit) ||
          !string_(bit == 1 ? device : resource, 64)) return false;
      seen |= bit;
      space_();
      if (consume_('}')) return seen == 3;
      if (!consume_(',')) return false;
    } while (true);
  }

  bool command_(char *action) {
    if (!object_open_()) return false;
    unsigned seen = 0;
    do {
      char key[32]{};
      if (!string_(key, sizeof(key)) || !colon_()) return false;
      unsigned bit = 0;
      if (std::strcmp(key, "name") == 0) {
        bit = 1;
        if (!string_(action, 64)) return false;
      } else if (std::strcmp(key, "payload") == 0) {
        bit = 2;
        // An empty object has no arguments to omit from canonical identity.
        // Nonempty payloads need normalization before they can be tracked.
        if (!consume_('{') || !consume_('}')) return false;
      } else return false;
      if (seen & bit) return false;
      seen |= bit;
      if (consume_('}')) return (seen & 1) != 0;
      if (!consume_(',')) return false;
    } while (true);
  }

  bool timeout_() {
    space_();
    uint32_t value = 0;
    size_t digits = 0;
    const bool leading_zero = pos_ < length_ && payload_[pos_] == '0';
    while (pos_ < length_ && payload_[pos_] >= '0' && payload_[pos_] <= '9') {
      const uint8_t digit = payload_[pos_++] - '0';
      if (value > (UINT32_MAX - digit) / 10) return false;
      value = value * 10 + digit;
      ++digits;
    }
    if (digits == 0 || value == 0 || (leading_zero && digits > 1)) return false;
    timeout_ms_ = value;
    return true;
  }

  bool decimal_(const char *value, uint64_t *out = nullptr) const {
    if (!value || !*value) return false;
    uint64_t number = 0;
    for (const char *p = value; *p; ++p) {
      if (*p < '0' || *p > '9') return false;
      const uint8_t digit = static_cast<uint8_t>(*p - '0');
      if (number > (UINT64_MAX - digit) / 10) return false;
      number = number * 10 + digit;
    }
    if (out) *out = number;
    return number != 0;
  }
  void space_() {
    while (pos_ < length_ && (payload_[pos_] == ' ' || payload_[pos_] == '\n' ||
                              payload_[pos_] == '\r' || payload_[pos_] == '\t')) ++pos_;
  }
  bool consume_(char c) {
    space_();
    if (pos_ == length_ || payload_[pos_] != static_cast<uint8_t>(c)) return false;
    ++pos_;
    return true;
  }
  bool object_open_() {
    return consume_('{') && (space_(), pos_ < length_ && payload_[pos_] != '}');
  }
  bool colon_() { return consume_(':'); }
  bool string_(char *out, size_t capacity) {
    if (!consume_('"')) return false;
    size_t written = 0;
    while (pos_ < length_ && payload_[pos_] != '"') {
      const uint8_t c = payload_[pos_++];
      if (c < 0x21 || c > 0x7e || c == '\\' || written + 1 >= capacity)
        return false;
      out[written++] = static_cast<char>(c);
    }
    if (!written || pos_ == length_) return false;
    ++pos_;
    out[written] = '\0';
    return true;
  }

  const uint8_t *payload_;
  size_t length_;
  size_t pos_{0};
  uint32_t timeout_ms_{0};
};

inline bool decode_mqtt_command_envelope(const uint8_t *payload, size_t length,
                                         const char *expected_device,
                                         uint8_t *canonical, size_t capacity,
                                         size_t &written,
                                         MqttCommandIdentity *identity = nullptr,
                                         const char *expected_source = nullptr,
                                         const char *expected_reply = nullptr,
                                         MqttCommandFields *fields = nullptr) {
  return MqttEnvelopeReader(payload, length).decode(expected_device, canonical,
                                                     capacity, written, identity,
                                                     expected_source, expected_reply,
                                                     fields);
}

}  // namespace communication_net_protocol
}  // namespace esphome
