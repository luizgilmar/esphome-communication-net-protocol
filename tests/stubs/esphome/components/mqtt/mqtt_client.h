#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace esphome {
namespace mqtt {
class MQTTClientComponent {
 public:
  using Callback = std::function<void(const std::string &, const std::string &)>;
  bool is_connected() const { return connected; }
  void subscribe(const std::string &topic, Callback callback, uint8_t) {
    subscribed_topic = topic;
    on_message = callback;
  }
  bool publish(const std::string &, const char *, size_t, uint8_t, bool) {
    return connected;
  }
  bool connected{true};
  std::string subscribed_topic;
  Callback on_message;
};
inline MQTTClientComponent *global_mqtt_client = nullptr;
}  // namespace mqtt
}  // namespace esphome
