#include "outbound_mqtt_command_transport.h"

#ifdef USE_MQTT

#include <cstring>

#include "esphome/core/hal.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

bool EsphomeMqttCommandTransport::is_available() const {
  return this->configured_.load() && mqtt::global_mqtt_client != nullptr &&
         this->subscription_registered_.load() &&
         mqtt::global_mqtt_client->is_connected() && !this->is_busy();
}

bool EsphomeMqttCommandTransport::configure(const char *command_prefix,
                                            const char *reply_topic,
                                            const char *source_device_id,
                                            uint64_t source_boot_id) {
  if (this->is_busy()) return false;

  MqttTopic validated_prefix{};
  MqttTopic validated_reply{};
  MqttTopic validation_topic{};
  MqttJsonPayload validation_payload{};
  CommandRequest validation_request{};
  validation_request.transaction_id = 1;
  validation_request.target.device_id.assign("validation");
  validation_request.command.name.assign("validation");
  validation_request.timeout_ms = 1;
  if (!validated_prefix.value.assign(command_prefix) ||
      !this->codec_.build_command_topic(command_prefix, "validation",
                                        validation_topic) ||
      !this->codec_.encode_request(validation_request, reply_topic,
                                   validation_payload, source_device_id,
                                   source_boot_id) ||
      (source_device_id != nullptr && source_device_id[0] != '\0' &&
       !this->source_device_id_.assign(source_device_id)) ||
      !validated_reply.value.assign(reply_topic)) {
    return false;
  }

  if (this->subscription_registered_.load() &&
      mqtt::global_mqtt_client != nullptr) {
    mqtt::global_mqtt_client->unsubscribe(this->reply_topic_.value.c_str());
  }
  this->subscription_registered_.store(false);
  this->command_prefix_ = validated_prefix;
  this->reply_topic_ = validated_reply;
  this->source_boot_id_ = source_boot_id;
  this->configured_.store(true);
  return true;
}

void EsphomeMqttCommandTransport::ensure_subscription_() {
  if (!this->configured_.load() || this->subscription_registered_.load() ||
      mqtt::global_mqtt_client == nullptr) {
    return;
  }
  mqtt::global_mqtt_client->subscribe(
      this->reply_topic_.value.c_str(),
      [this](const std::string &topic, const std::string &payload) {
        this->on_message_(topic, payload);
      },
      MQTT_QOS);
  this->subscription_registered_.store(true);
}

TransportStartStatus EsphomeMqttCommandTransport::start(
    const CommandRequest &request) {
  if (!request.is_valid()) return TransportStartStatus::INVALID_REQUEST;
  if (!this->is_available()) return TransportStartStatus::UNAVAILABLE;
  if (!this->codec_.build_command_topic(this->command_prefix_.value.c_str(),
                                        request.target.device_id.c_str(),
                                        this->command_topic_) ||
      !this->codec_.encode_request(request, this->reply_topic_.value.c_str(),
                                   this->request_payload_,
                                   this->source_device_id_.c_str(),
                                   this->source_boot_id_)) {
    return TransportStartStatus::INVALID_REQUEST;
  }

  this->transaction_id_ = request.transaction_id;
  this->connection_failure_reported_ = false;
  this->started_ms_ = millis();
  this->result_ = {};
  this->response_size_.store(0);
  this->response_read_index_.store(0);
  this->response_write_index_.store(0);
  this->response_count_.store(0);
  this->result_ready_.store(false);
  this->active_.store(true);

  const bool accepted = mqtt::global_mqtt_client->publish(
      this->command_topic_.value.c_str(),
      reinterpret_cast<const char *>(this->request_payload_.data.data()),
      this->request_payload_.data.size(), MQTT_QOS, false);
  if (!accepted) {
    this->active_.store(false);
    return TransportStartStatus::UNAVAILABLE;
  }
  return TransportStartStatus::STARTED;
}

void EsphomeMqttCommandTransport::loop(uint32_t now_ms) {
  this->ensure_subscription_();
  if (!this->active_.load()) return;
  if (mqtt::global_mqtt_client == nullptr ||
      !mqtt::global_mqtt_client->is_connected()) {
    if (this->result_ready_.load() || this->connection_failure_reported_) return;
    this->connection_failure_reported_ = true;
    this->publish_failure_(TransportStatus::CONNECTION_FAILED,
                           CommunicationErrorCode::CONNECTION_FAILED,
                           "MQTT broker disconnected", true,
                           now_ms - this->started_ms_);
    // Notify the common coordinator but retain correlation across reconnect.
    this->result_ready_.store(true);
    return;
  }
  this->connection_failure_reported_ = false;
  // A progress result must be consumed by the runner before decoding the
  // terminal response; otherwise the shared result slot is overwritten.
  if (this->result_ready_.load() || this->response_count_.load() == 0) return;
  const size_t index = this->response_read_index_.load();
  const size_t payload_size = this->response_queue_sizes_[index];
  const bool overflow = this->response_queue_overflow_[index];
  this->response_read_index_.store((index + 1) % MAX_PENDING_RESPONSES);
  this->response_count_.fetch_sub(1);
  if (overflow) {
    this->publish_failure_(TransportStatus::PROTOCOL_ERROR,
                           CommunicationErrorCode::PROTOCOL_ERROR,
                           "MQTT response exceeds bounded capacity", false,
                           now_ms - this->started_ms_);
    this->finish_(true);
    return;
  }

  if (!this->codec_.decode_response(
          this->transaction_id_, this->response_queue_[index].data.data(),
          payload_size, now_ms - this->started_ms_,
          this->result_)) {
    if (this->codec_.last_error() ==
        MqttJsonCodecError::CORRELATION_MISMATCH) {
      this->ignored_response_count_.fetch_add(1);
      return;
    }
    this->publish_failure_(TransportStatus::PROTOCOL_ERROR,
                           CommunicationErrorCode::PROTOCOL_ERROR,
                           "invalid normalized MQTT response", false,
                           now_ms - this->started_ms_);
  } else if (this->result_.result == CommandResultStatus::IN_PROGRESS) {
    // The command is still active. Keep the subscription and correlation for
    // the subsequent terminal result on the same transaction ID.
    this->result_ready_.store(true);
    return;
  }
  this->finish_(true);
}

bool EsphomeMqttCommandTransport::take_result(CommandResult &result) {
  if (!this->result_ready_.load()) return false;
  result = this->result_;
  this->result_ready_.store(false);
  if (result.result != CommandResultStatus::IN_PROGRESS && !this->active_.load())
    this->transaction_id_ = 0;
  return true;
}

bool EsphomeMqttCommandTransport::cancel(TransactionId transaction_id) {
  if (!this->active_.load() || transaction_id != this->transaction_id_)
    return false;
  this->finish_(false);
  return true;
}

void EsphomeMqttCommandTransport::on_message_(
    const std::string &topic, const std::string &payload) {
  if (!this->active_.load() || topic != this->reply_topic_.value.c_str()) return;
  if (this->response_count_.load() >= MAX_PENDING_RESPONSES) {
    this->ignored_response_count_.fetch_add(1);
    return;
  }
  const size_t index = this->response_write_index_.load();
  this->response_size_.store(payload.size());
  this->response_queue_sizes_[index] = payload.size();
  this->response_queue_overflow_[index] =
      !this->response_queue_[index].data.assign(
          reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
  this->response_write_index_.store((index + 1) % MAX_PENDING_RESPONSES);
  this->response_count_.fetch_add(1);
}

void EsphomeMqttCommandTransport::publish_failure_(
    TransportStatus transport_status, CommunicationErrorCode error_code,
    const char *message, bool retryable, uint32_t latency_ms) {
  this->result_ = {};
  this->result_.transaction_id = this->transaction_id_;
  this->result_.transport_used = CommunicationTransport::MQTT;
  this->result_.transport_status = transport_status;
  this->result_.result = CommandResultStatus::FAILED;
  this->result_.error.code = error_code;
  this->result_.error.retryable = retryable;
  this->result_.error.message.assign(message);
  this->result_.latency_ms = latency_ms;
}

void EsphomeMqttCommandTransport::finish_(bool publish_result) {
  this->active_.store(false);
  this->response_count_.store(0);
  this->result_ready_.store(publish_result);
}

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome

#endif  // USE_MQTT
