#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "esphome/components/mqtt/mqtt_client.h"

#include "outbound_mqtt_json_codec.h"
#include "outbound_communication_transport.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

class EsphomeMqttCommandTransport : public CommunicationTransportAdapter {
 public:
  CommunicationTransport transport_type() const override {
    return CommunicationTransport::MQTT;
  }
  bool is_available() const override;
  bool is_busy() const override {
    return this->active_.load() || this->result_ready_.load();
  }

  bool configure(const char *command_prefix, const char *reply_topic,
                 const char *source_device_id = nullptr,
                 uint64_t source_boot_id = 0);
  bool configured() const { return this->configured_.load(); }
  bool application_identity_matches(const char *source, uint64_t boot_id) const override {
    return configured_.load() && source != nullptr && boot_id != 0 &&
           source_boot_id_ == boot_id && std::strcmp(source_device_id_.c_str(), source) == 0;
  }

  TransportStartStatus start(const CommandRequest &request) override;
  void loop(uint32_t now_ms) override;
  bool has_result() const override { return this->result_ready_.load(); }
  bool take_result(CommandResult &result) override;
  bool cancel(TransactionId transaction_id) override;

  size_t ignored_response_count() const {
    return this->ignored_response_count_.load();
  }
  size_t response_size() const { return this->response_size_.load(); }

 private:
  static constexpr uint8_t MQTT_QOS = 1;
  static constexpr size_t MAX_PENDING_RESPONSES = 4;

  void on_message_(const std::string &topic, const std::string &payload);
  void ensure_subscription_();
  void publish_failure_(TransportStatus transport_status,
                        CommunicationErrorCode error_code,
                        const char *message, bool retryable,
                        uint32_t latency_ms);
  void finish_(bool publish_result);

  CommunicationMqttJsonCodec codec_{};
  MqttTopic command_topic_{};
  MqttTopic command_prefix_{};
  MqttTopic reply_topic_{};
  BoundedText<CommandTarget::MAX_DEVICE_ID_LENGTH> source_device_id_{};
  uint64_t source_boot_id_{0};
  MqttJsonPayload request_payload_{};
  MqttJsonPayload response_queue_[MAX_PENDING_RESPONSES]{};
  size_t response_queue_sizes_[MAX_PENDING_RESPONSES]{};
  bool response_queue_overflow_[MAX_PENDING_RESPONSES]{};
  CommandResult result_{};
  TransactionId transaction_id_{0};
  uint32_t started_ms_{0};

  std::atomic<bool> configured_{false};
  std::atomic<bool> subscription_registered_{false};
  std::atomic<bool> active_{false};
  bool connection_failure_reported_{false};
  std::atomic<bool> result_ready_{false};
  std::atomic<size_t> response_read_index_{0};
  std::atomic<size_t> response_write_index_{0};
  std::atomic<size_t> response_count_{0};
  std::atomic<size_t> response_size_{0};
  std::atomic<size_t> ignored_response_count_{0};
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome

#endif  // USE_MQTT
