#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "transaction_tracker.h"

namespace esphome {
namespace communication_net_protocol {

struct MqttResultObservation {
  uint64_t transaction_id{0};
  TransactionStage stage{TransactionStage::FAILED};
};

// Decode an application result from the current MQTT wire format. A response
// has no boot ID: the caller must obtain that from its own outstanding request
// and check the subscribed reply topic before calling the transaction tracker.
class MqttResultReader {
 public:
  MqttResultReader(const uint8_t *payload, size_t length)
      : payload_(payload), length_(length) {}

  bool decode(MqttResultObservation &out) {
    out = {};
    if (!payload_ || !length_ || length_ > 1280 || !open_()) return false;
    unsigned seen = 0;
    char status[20]{}, error[32]{};
    uint64_t id = 0;
    do {
      char key[32]{};
      if (!string_(key, sizeof(key)) || !consume_(':')) return false;
      unsigned bit = 0;
      if (std::strcmp(key, "transaction_id") == 0) {
        bit = 1;
        char value[21]{};
        if (!string_(value, sizeof(value)) || !decimal_(value, id)) return false;
      } else if (std::strcmp(key, "result") == 0) {
        bit = 2;
        if (!string_(status, sizeof(status))) return false;
      } else if (std::strcmp(key, "execution") == 0) {
        bit = 4;
        if (!skip_(0)) return false;
      } else if (std::strcmp(key, "remote_state") == 0) {
        bit = 8;
        if (!skip_(0)) return false;
      } else if (std::strcmp(key, "error") == 0) {
        bit = 16;
        if (!error_(error)) return false;
      } else {
        return false;
      }
      if (seen & bit) return false;
      seen |= bit;
      if (consume_('}')) break;
      if (!consume_(',')) return false;
    } while (true);
    space_();
    if (pos_ != length_ || !(seen & 1) || !(seen & 2)) return false;
    TransactionStage stage;
    if (std::strcmp(status, "in_progress") == 0) stage = TransactionStage::IN_PROGRESS;
    else if (std::strcmp(status, "succeeded") == 0) stage = TransactionStage::SUCCEEDED;
    else if (std::strcmp(status, "rejected") == 0) stage = TransactionStage::REJECTED;
    else if (std::strcmp(status, "failed") == 0)
      stage = std::strcmp(error, "interrupted") == 0 ?
          TransactionStage::INTERRUPTED : TransactionStage::FAILED;
    else if (std::strcmp(status, "expired") == 0) stage = TransactionStage::EXPIRED;
    else return false;
    out = {id, stage};
    return true;
  }

 private:
  void space_() {
    while (pos_ < length_ && (payload_[pos_] == ' ' || payload_[pos_] == '\r' ||
                              payload_[pos_] == '\n' || payload_[pos_] == '\t')) ++pos_;
  }
  bool consume_(char c) {
    space_();
    if (pos_ == length_ || payload_[pos_] != static_cast<uint8_t>(c)) return false;
    ++pos_;
    return true;
  }
  bool open_() {
    if (!consume_('{')) return false;
    space_();
    return pos_ < length_ && payload_[pos_] != '}';
  }
  bool string_(char *out, size_t capacity) {
    if (!consume_('"')) return false;
    size_t count = 0;
    while (pos_ < length_ && payload_[pos_] != '"') {
      uint8_t c = payload_[pos_++];
      if (c == '\\' || c < 0x20 || count + 1 >= capacity) return false;
      out[count++] = static_cast<char>(c);
    }
    if (!count || pos_ == length_) return false;
    ++pos_;
    out[count] = '\0';
    return true;
  }
  bool decimal_(const char *value, uint64_t &number) {
    number = 0;
    if (!value || !*value) return false;
    for (const char *p = value; *p; ++p) {
      if (*p < '0' || *p > '9') return false;
      const uint8_t digit = static_cast<uint8_t>(*p - '0');
      if (number > (UINT64_MAX - digit) / 10) return false;
      number = number * 10 + digit;
    }
    return number != 0;
  }
  bool error_(char *code) {
    if (!open_()) return false;
    bool has_code = false;
    do {
      char key[32]{};
      if (!string_(key, sizeof(key)) || !consume_(':')) return false;
      if (std::strcmp(key, "code") == 0) {
        if (has_code || !string_(code, 32)) return false;
        has_code = true;
      } else if (!skip_(0)) return false;
      if (consume_('}')) return has_code;
      if (!consume_(',')) return false;
    } while (true);
  }
  // Skip data whose meaning belongs to the executor. Limit nesting and reject
  // invalid JSON so a malformed remote_state cannot authorize a terminal.
  bool skip_(unsigned depth) {
    if (depth > 8) return false;
    space_();
    if (pos_ == length_) return false;
    const char first = static_cast<char>(payload_[pos_]);
    if (first == '"') {
      ++pos_;
      while (pos_ < length_) {
        const uint8_t c = payload_[pos_++];
        if (c == '"') return true;
        if (c == '\\') {
          if (pos_ == length_) return false;
          const uint8_t escaped = payload_[pos_++];
          if (std::strchr("\"\\/bfnrt", escaped) == nullptr) return false;
        } else if (c < 0x20) return false;
      }
      return false;
    }
    if (first == '{' || first == '[') {
      const char close = first == '{' ? '}' : ']';
      ++pos_;
      if (consume_(close)) return true;
      do {
        if (first == '{' && (!skip_(depth + 1) || !consume_(':'))) return false;
        if (!skip_(depth + 1)) return false;
        if (consume_(close)) return true;
        if (!consume_(',')) return false;
      } while (true);
    }
    const char *literal = first == 't' ? "true" : first == 'f' ? "false" :
                          first == 'n' ? "null" : nullptr;
    if (literal) {
      const size_t n = std::strlen(literal);
      if (n > length_ - pos_ || std::memcmp(payload_ + pos_, literal, n)) return false;
      pos_ += n;
      return true;
    }
    // Bounded JSON number, sufficient for existing position/estimate fields.
    if (first != '-' && (first < '0' || first > '9')) return false;
    if (first == '-') ++pos_;
    if (pos_ == length_ || payload_[pos_] < '0' || payload_[pos_] > '9') return false;
    if (payload_[pos_] == '0') ++pos_;
    else while (pos_ < length_ && payload_[pos_] >= '0' && payload_[pos_] <= '9') ++pos_;
    if (pos_ < length_ && payload_[pos_] == '.') {
      ++pos_;
      if (pos_ == length_ || payload_[pos_] < '0' || payload_[pos_] > '9') return false;
      while (pos_ < length_ && payload_[pos_] >= '0' && payload_[pos_] <= '9') ++pos_;
    }
    return true;
  }

  const uint8_t *payload_;
  size_t length_;
  size_t pos_{0};
};

inline bool decode_mqtt_result(const uint8_t *payload, size_t length,
                               MqttResultObservation &out) {
  return MqttResultReader(payload, length).decode(out);
}

// The caller supplies the boot ID kept with its own outstanding request.
// Unsolicited responses and responses from a different local session cannot
// create transactions. The MQTT connection/topic trust boundary is external.
template<size_t Capacity> TransactionObserveStatus correlate_mqtt_result(
    TransactionTracker<Capacity> &tracker, const MqttResultObservation &result,
    uint64_t expected_boot_id, uint32_t now_ms) {
  return tracker.observe(result.transaction_id, expected_boot_id,
                         result.stage, now_ms);
}

}  // namespace communication_net_protocol
}  // namespace esphome
