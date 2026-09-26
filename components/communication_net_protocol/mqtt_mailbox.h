#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace communication_net_protocol {

// One cooperative receive slot; MQTT callbacks never parse application data.
class MqttMailbox {
 public:
  static constexpr size_t MAX_TOPIC = 192;
  static constexpr size_t MAX_PAYLOAD = 1280;

  bool put(const char *topic, size_t topic_length, const uint8_t *payload,
           size_t payload_length) {
    if (occupied_ || topic == nullptr || topic_length == 0 ||
        topic_length > MAX_TOPIC || payload_length > MAX_PAYLOAD ||
        (payload == nullptr && payload_length != 0)) {
      ++dropped_;
      return false;
    }
    std::memcpy(topic_, topic, topic_length);
    topic_[topic_length] = '\0';
    if (payload_length != 0) std::memcpy(payload_, payload, payload_length);
    topic_length_ = topic_length;
    payload_length_ = payload_length;
    occupied_ = true;
    return true;
  }

  bool take(char *topic, size_t topic_capacity, uint8_t *payload,
            size_t payload_capacity, size_t &payload_length) {
    if (!occupied_ || topic == nullptr || topic_capacity <= topic_length_ ||
        payload_capacity < payload_length_ ||
        (payload == nullptr && payload_length_ != 0)) return false;
    std::memcpy(topic, topic_, topic_length_ + 1);
    if (payload_length_ != 0) std::memcpy(payload, payload_, payload_length_);
    payload_length = payload_length_;
    occupied_ = false;
    return true;
  }

  bool peek(const char *&topic, const uint8_t *&payload,
            size_t &payload_length) const {
    if (!occupied_) return false;
    topic = topic_;
    payload = payload_;
    payload_length = payload_length_;
    return true;
  }

  void release() { occupied_ = false; }

  bool occupied() const { return occupied_; }
  uint32_t dropped() const { return dropped_; }

 private:
  char topic_[MAX_TOPIC + 1]{};
  uint8_t payload_[MAX_PAYLOAD]{};
  size_t topic_length_{0};
  size_t payload_length_{0};
  uint32_t dropped_{0};
  bool occupied_{false};
};

}  // namespace communication_net_protocol
}  // namespace esphome
