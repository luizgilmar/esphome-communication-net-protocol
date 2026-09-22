#include "communication_net_protocol.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "mqtt_envelope_decoder.h"
#include "mqtt_result_decoder.h"
#include <cstring>
#include <cstdio>
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
#include "esphome/components/light/light_state.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome {
namespace communication_net_protocol {

static const char *const TAG = "communication_net_protocol";

#ifdef USE_COMMUNICATION_NET_ESPNOW_OBSERVER
void CommunicationNetProtocolComponent::on_command_identity(
    espnow_net_protocol::PeerIndex peer,
    const espnow_net_protocol::NetCommand &command) {
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
  this->verified_command_ = &command;
#else
  uint8_t canonical[192]{};
  size_t length = 0;
  const char *route = nullptr;
  const bool matched = inspect_inbound_command(
      this->inbound_routes_, this->device_id_, command.device_id.c_str(),
      command.resource.c_str(), command.name.c_str(), command.payload.data(),
      command.payload.size(), route, canonical, sizeof(canonical), length);
  ESP_LOGI(TAG,
           "ESP-NOW verified source observed peer=%u tx=%llu route=%s canonical_bytes=%u (no admission or execution)",
           static_cast<unsigned>(peer),
           static_cast<unsigned long long>(command.transaction_id),
           matched ? route : "<unmatched>",
           static_cast<unsigned>(length));
#endif
}
#endif

#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
using NetCommand = espnow_net_protocol::NetCommand;
using NetResult = espnow_net_protocol::NetResult;
using NetStatus = espnow_net_protocol::NetResultStatus;
using NetStart = espnow_net_protocol::NetCommandHandlerStartStatus;

NetStart CommunicationNetProtocolComponent::start(const NetCommand &command,
                                                   uint32_t now_ms) {
  const bool verified = this->verified_command_ == &command;
  this->verified_command_ = nullptr;
  if (!verified) return NetStart::REJECTED;
  return this->start_inbound_(command, now_ms, true);
}

NetStart CommunicationNetProtocolComponent::start_inbound_(
    const NetCommand &command, uint32_t now_ms, bool radio) {
  if (!this->inbound_routes_valid_) return NetStart::BUSY;
  if (!command.valid() || (command.payload.size() != 0 &&
      (command.payload.size() != 2 || command.payload.data()[0] != '{' ||
       command.payload.data()[1] != '}'))) return NetStart::INVALID_COMMAND;
  const InboundCommandView view{
      command.source_device_id.c_str(), command.source_boot_id,
      command.transaction_id,
      {command.device_id.c_str(), command.resource.c_str(),
       command.name.c_str(), nullptr, 0}};
  const char *declared_route = this->inbound_routes_.find(
      command.resource.c_str(), command.name.c_str());
  DeclarativeInboundBinding *declared_binding = nullptr;
  for (size_t i = 0; declared_route != nullptr &&
                     i < this->inbound_binding_count_; ++i)
    if (std::strcmp(this->inbound_bindings_[i]->route_id(),
                    declared_route) == 0) {
      declared_binding = this->inbound_bindings_[i];
      break;
    }
  bool interrupt = false;
  if (this->inbound_active_ &&
      command.transaction_id != this->active_command_.transaction_id) {
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
    interrupt = this->active_binding_ != nullptr &&
                this->active_binding_->interruptible() &&
                declared_binding != nullptr &&
                declared_binding->interrupts_active() &&
                this->command_matches_active_resource_(command);
    const bool same_interrupt = this->interrupt_active_ &&
        command.transaction_id == this->interrupt_command_.transaction_id;
    if (!interrupt || (this->interrupt_active_ && !same_interrupt) ||
        this->interrupt_radio_result_ready_ ||
        this->interrupt_mqtt_result_ready_)
      return NetStart::BUSY;
#else
    return NetStart::BUSY;
#endif
  } else if (this->radio_result_ready_ || this->mqtt_result_ready_) {
    return NetStart::BUSY;
  }
  const char *route = nullptr;
  const InboundDecision decision = this->route_admission_.admit(view, route);
  if (decision == InboundDecision::DUPLICATE_TERMINAL) {
    uint8_t encoded[256]{};
    size_t size = 0;
    InboundTerminalStatus status{};
    NetResult replay{};
    if (!this->route_admission_.replay_terminal(
            view, status, encoded, sizeof(encoded), size) ||
        !this->result_codec_.decode(command.transaction_id, encoded, size,
                                    0, replay)) return NetStart::REJECTED;
    if (radio) {
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
      if (interrupt) {
        if (this->interrupt_radio_result_ready_) return NetStart::BUSY;
        this->interrupt_result_ = replay;
        this->interrupt_radio_result_ready_ = true;
        return NetStart::STARTED;
      }
#endif
      if (this->radio_result_ready_) return NetStart::BUSY;
      this->inbound_result_ = replay;
      this->radio_result_ready_ = true;
    } else {
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
      if (interrupt) {
        if (this->interrupt_mqtt_result_ready_) return NetStart::BUSY;
        this->interrupt_result_ = replay;
        this->interrupt_mqtt_result_ready_ = true;
        return NetStart::STARTED;
      }
#endif
      this->inbound_result_ = replay;
      this->mqtt_result_ready_ = true;
    }
    return NetStart::STARTED;
  }
  if (decision == InboundDecision::DUPLICATE_PENDING &&
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
      interrupt && this->interrupt_active_ &&
      command.transaction_id == this->interrupt_command_.transaction_id) {
    if (radio != this->interrupt_radio_waiting_) return NetStart::BUSY;
    this->interrupt_result_ = {};
    this->interrupt_result_.transaction_id = command.transaction_id;
    this->interrupt_result_.status = NetStatus::IN_PROGRESS;
    this->interrupt_result_.execution.started = true;
    this->interrupt_result_.execution.has_estimated_completion = true;
    this->interrupt_result_.execution.estimated_completion_ms =
        this->interrupt_timeout_ms_;
    if (radio) this->interrupt_radio_result_ready_ = true;
    else this->interrupt_mqtt_result_ready_ = true;
    return NetStart::STARTED;
  }
  if (decision == InboundDecision::DUPLICATE_PENDING &&
#endif
      this->inbound_active_ &&
      command.transaction_id == this->active_command_.transaction_id) {
    if (radio != this->radio_waiting_) return NetStart::BUSY;
    this->inbound_result_ = {};
    this->inbound_result_.transaction_id = command.transaction_id;
    this->inbound_result_.status = NetStatus::IN_PROGRESS;
    this->inbound_result_.execution.started = true;
    this->inbound_result_.execution.has_estimated_completion = true;
    this->inbound_result_.execution.estimated_completion_ms =
        this->inbound_timeout_ms_;
    if (radio) this->radio_result_ready_ = true;
    else this->mqtt_result_ready_ = true;
    return NetStart::STARTED;
  }
  if (decision != InboundDecision::NEW_COMMAND) return NetStart::REJECTED;
  DeclarativeInboundBinding *binding = declared_binding;
  if (binding == nullptr) {
    (void) this->route_admission_.complete(
        view, {InboundTerminalStatus::REJECTED, nullptr, 0});
    return NetStart::REJECTED;
  }
  if (interrupt) {
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
    this->interrupt_command_ = command;
    this->interrupt_binding_ = binding;
    this->interrupt_started_ms_ = now_ms;
    this->interrupt_timeout_ms_ = binding->completion_timeout();
    this->interrupt_radio_waiting_ = radio;
    this->interrupt_mqtt_waiting_ = !radio;
    this->interrupt_active_ = true;
    ESP_LOGI(TAG, "Inbound interrupt route=%s transport=%s tx=%llu active_tx=%llu",
             route, radio ? "esp_now" : "mqtt",
             static_cast<unsigned long long>(command.transaction_id),
             static_cast<unsigned long long>(this->active_command_.transaction_id));
    binding->trigger();
    this->interrupt_result_ = {};
    this->interrupt_result_.transaction_id = command.transaction_id;
    this->interrupt_result_.status = NetStatus::IN_PROGRESS;
    this->interrupt_result_.execution.started = true;
    this->interrupt_result_.execution.has_estimated_completion = true;
    this->interrupt_result_.execution.estimated_completion_ms =
        binding->timer_completion() ? binding->completion_delay()
                                    : this->interrupt_timeout_ms_;
    if (radio) this->interrupt_radio_result_ready_ = true;
    else this->interrupt_mqtt_result_ready_ = true;
    return NetStart::STARTED;
#endif
  }
  this->active_command_ = command;
  this->active_binding_ = binding;
  this->inbound_started_ms_ = now_ms;
  this->inbound_timeout_ms_ = binding->completion_timeout();
  this->radio_waiting_ = radio;
  this->mqtt_waiting_ = !radio;
  this->inbound_active_ = true;
  ESP_LOGI(TAG, "Inbound route=%s transport=%s tx=%llu",
           route, radio ? "esp_now" : "mqtt",
           static_cast<unsigned long long>(command.transaction_id));
  if (binding->light() != nullptr) {
    bool on = binding->light()->current_values.is_on();
    for (size_t i = 0; i < binding->toggle_reference_count(); ++i)
      on |= binding->toggle_reference_at(i)->current_values.is_on();
    this->inbound_expected_on_ =
        binding->expected() == LightExpectedState::ON ||
        (binding->expected() == LightExpectedState::TOGGLED && !on);
  } else if (binding->binary_sensor() != nullptr) {
    auto *sensor = binding->binary_sensor();
    if (!sensor->has_state()) {
      NetResult unavailable{};
      unavailable.transaction_id = command.transaction_id;
      unavailable.status = NetStatus::FAILED;
      unavailable.error.code = espnow_net_protocol::NetErrorCode::TARGET_UNAVAILABLE;
      unavailable.error.message.assign("binary sensor state unavailable");
      this->finish_inbound_(unavailable);
      return NetStart::STARTED;
    }
    this->inbound_expected_on_ =
        binding->expected() == LightExpectedState::ON ||
        (binding->expected() == LightExpectedState::TOGGLED && !sensor->state);
  }
  binding->trigger();
  if (binding->light() == nullptr && binding->binary_sensor() == nullptr &&
      !binding->timer_completion()) {
    NetResult immediate{};
    immediate.transaction_id = command.transaction_id;
    immediate.status = NetStatus::SUCCEEDED;
    immediate.execution.started = true;
    this->finish_inbound_(immediate);
  } else {
    this->inbound_result_ = {};
    this->inbound_result_.transaction_id = command.transaction_id;
    this->inbound_result_.status = NetStatus::IN_PROGRESS;
    this->inbound_result_.execution.started = true;
    this->inbound_result_.execution.has_estimated_completion = true;
    this->inbound_result_.execution.estimated_completion_ms =
        this->inbound_timeout_ms_;
    if (radio) this->radio_result_ready_ = true;
    else this->mqtt_result_ready_ = true;
  }
  return NetStart::STARTED;
}

void CommunicationNetProtocolComponent::loop(uint32_t now_ms) {
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
  if (this->interrupt_active_ && !this->interrupt_radio_result_ready_ &&
      !this->interrupt_mqtt_result_ready_ &&
      this->interrupt_binding_ != nullptr) {
    const bool completed = this->interrupt_binding_->timer_completion() &&
        now_ms - this->interrupt_started_ms_ >=
            this->interrupt_binding_->completion_delay();
    const bool timed_out = now_ms - this->interrupt_started_ms_ >=
                           this->interrupt_timeout_ms_;
    if (completed || timed_out) {
      NetResult result{};
      result.transaction_id = this->interrupt_command_.transaction_id;
      result.latency_ms = now_ms - this->interrupt_started_ms_;
      result.execution.started = true;
      if (completed) {
        result.status = NetStatus::SUCCEEDED;
      } else {
        result.status = NetStatus::FAILED;
        result.error.code = espnow_net_protocol::NetErrorCode::TIMED_OUT;
        result.error.message.assign("interrupt completion timed out");
      }
      this->finish_interrupt_(result);
      if (completed && this->inbound_active_) {
        NetResult interrupted{};
        interrupted.transaction_id = this->active_command_.transaction_id;
        interrupted.latency_ms = now_ms - this->inbound_started_ms_;
        interrupted.status = NetStatus::FAILED;
        interrupted.execution.started = true;
        interrupted.error.code =
            espnow_net_protocol::NetErrorCode::INTERRUPTED;
        interrupted.error.message.assign("operation interrupted by command");
        this->finish_inbound_(interrupted);
      }
    }
  }
#endif
  if (!this->inbound_active_ || this->radio_result_ready_ ||
      this->mqtt_result_ready_ || this->active_binding_ == nullptr ||
      (this->active_binding_->light() == nullptr &&
       this->active_binding_->binary_sensor() == nullptr &&
       !this->active_binding_->timer_completion())) return;
  bool completed = false;
  if (this->active_binding_->timer_completion()) {
    completed = now_ms - this->inbound_started_ms_ >=
                this->active_binding_->completion_delay();
  } else {
    completed = this->active_binding_->binary_sensor() == nullptr ||
                (this->active_binding_->binary_sensor()->has_state() &&
                 this->active_binding_->binary_sensor()->state ==
                     this->inbound_expected_on_);
    for (size_t i = 0; i < this->active_binding_->light_count(); ++i)
      completed &= this->active_binding_->light_at(i)->current_values.is_on() ==
                   this->inbound_expected_on_;
  }
  if (completed && this->active_binding_->check_rgb()) {
    for (size_t i = 0; i < this->active_binding_->rgb_light_count(); ++i) {
      const auto &values = this->active_binding_->rgb_light_at(i)->current_values;
      const float observed[3]{values.get_red(), values.get_green(), values.get_blue()};
      for (size_t channel = 0; channel < 3; ++channel) {
        const int actual = static_cast<int>(observed[channel] * 255.0f + 0.5f);
        const int expected = this->active_binding_->expected_rgb(channel);
        // One unit accounts for float-to-byte rounding in light color values.
        completed &= actual >= expected - 1 && actual <= expected + 1;
      }
    }
  }
  if (completed && this->active_binding_->check_brightness()) {
    const int expected = (this->active_binding_->expected_brightness() * 255 + 50) / 100;
    for (size_t i = 0; i < this->active_binding_->brightness_light_count(); ++i) {
      const auto &values = this->active_binding_->brightness_light_at(i)->current_values;
      const int actual = static_cast<int>(values.get_brightness() * 255.0f + 0.5f);
      completed &= actual >= expected - 1 && actual <= expected + 1;
    }
  }
  if (completed && this->active_binding_->expected_effect() != nullptr) {
    for (size_t i = 0; i < this->active_binding_->effect_light_count(); ++i)
      completed &= std::strcmp(
          this->active_binding_->effect_light_at(i)->get_effect_name().c_str(),
          this->active_binding_->expected_effect()) == 0;
  }
  const bool timed_out = now_ms - this->inbound_started_ms_ >=
                         this->inbound_timeout_ms_;
  if (!completed && !timed_out) return;
  NetResult result{};
  result.transaction_id = this->active_command_.transaction_id;
  result.latency_ms = now_ms - this->inbound_started_ms_;
  result.execution.started = true;
  if (completed) {
    result.status = NetStatus::SUCCEEDED;
    result.remote_state.completeness =
        espnow_net_protocol::NetStateCompleteness::COMPLETE;
    const uint8_t state = this->active_binding_->timer_completion() ? 0 :
                          this->active_binding_->binary_sensor() != nullptr
                              ? (this->active_binding_->binary_sensor()->state ? 1 : 0)
                              : (this->active_binding_->light()->current_values.is_on() ? 1 : 0);
    if (this->active_binding_->timer_completion()) {
      result.remote_state.completeness =
          espnow_net_protocol::NetStateCompleteness::NOT_PROVIDED;
    } else if (this->active_binding_->result_rgb()) {
      const auto &values = this->active_binding_->light()->current_values;
      const uint8_t rgb[5]{state,
                           static_cast<uint8_t>(values.get_red() * 255.0f + 0.5f),
                           static_cast<uint8_t>(values.get_green() * 255.0f + 0.5f),
                           static_cast<uint8_t>(values.get_blue() * 255.0f + 0.5f),
                           static_cast<uint8_t>(values.get_brightness() * 255.0f + 0.5f)};
      result.remote_state.schema.assign("rgb-state/v1");
      result.remote_state.data.assign(rgb, sizeof(rgb));
    } else {
      result.remote_state.schema.assign("binary-state/v1");
      result.remote_state.data.assign(&state, 1);
    }
  } else {
    result.status = NetStatus::FAILED;
    result.error.code = espnow_net_protocol::NetErrorCode::TIMED_OUT;
    result.error.message.assign("declared completion state not observed");
  }
  this->finish_inbound_(result);
}

void CommunicationNetProtocolComponent::finish_inbound_(NetResult result) {
  espnow_net_protocol::EspNowResultPayload encoded{};
  if (this->result_codec_.encode(result, encoded) &&
      encoded.data.size() <= 256) {
    const auto status = result.status == NetStatus::SUCCEEDED
                            ? InboundTerminalStatus::SUCCEEDED
                            : InboundTerminalStatus::FAILED;
    const InboundCommandView view{
        this->active_command_.source_device_id.c_str(),
        this->active_command_.source_boot_id,
        this->active_command_.transaction_id,
        {this->active_command_.device_id.c_str(),
         this->active_command_.resource.c_str(),
         this->active_command_.name.c_str(), nullptr, 0}};
    (void) this->route_admission_.complete(
        view, {status, encoded.data.data(), encoded.data.size()});
  }
  this->inbound_active_ = false;
  this->active_binding_ = nullptr;
  ESP_LOGI(TAG, "Inbound completed tx=%llu result=%u",
           static_cast<unsigned long long>(result.transaction_id),
           static_cast<unsigned>(result.status));
  this->inbound_result_ = result;
  this->radio_result_ready_ = this->radio_waiting_;
  this->mqtt_result_ready_ = this->mqtt_waiting_;
}

#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
bool CommunicationNetProtocolComponent::command_matches_active_resource_(
    const NetCommand &command) const {
  return this->inbound_active_ &&
      std::strcmp(command.source_device_id.c_str(),
                  this->active_command_.source_device_id.c_str()) == 0 &&
      command.source_boot_id == this->active_command_.source_boot_id &&
      std::strcmp(command.device_id.c_str(),
                  this->active_command_.device_id.c_str()) == 0 &&
      std::strcmp(command.resource.c_str(),
                  this->active_command_.resource.c_str()) == 0;
}

void CommunicationNetProtocolComponent::finish_interrupt_(NetResult result) {
  espnow_net_protocol::EspNowResultPayload encoded{};
  if (this->result_codec_.encode(result, encoded) && encoded.data.size() <= 256) {
    const auto status = result.status == NetStatus::SUCCEEDED
                            ? InboundTerminalStatus::SUCCEEDED
                            : InboundTerminalStatus::FAILED;
    const InboundCommandView view{
        this->interrupt_command_.source_device_id.c_str(),
        this->interrupt_command_.source_boot_id,
        this->interrupt_command_.transaction_id,
        {this->interrupt_command_.device_id.c_str(),
         this->interrupt_command_.resource.c_str(),
         this->interrupt_command_.name.c_str(), nullptr, 0}};
    (void) this->route_admission_.complete(
        view, {status, encoded.data.data(), encoded.data.size()});
  }
  this->interrupt_active_ = false;
  this->interrupt_binding_ = nullptr;
  ESP_LOGI(TAG, "Inbound interrupt completed tx=%llu result=%u",
           static_cast<unsigned long long>(result.transaction_id),
           static_cast<unsigned>(result.status));
  this->interrupt_result_ = result;
  this->interrupt_radio_result_ready_ = this->interrupt_radio_waiting_;
  this->interrupt_mqtt_result_ready_ = this->interrupt_mqtt_waiting_;
}
#endif

bool CommunicationNetProtocolComponent::take_result(NetResult &result) {
  if (!this->radio_result_ready_) return false;
  result = this->inbound_result_;
  this->radio_result_ready_ = false;
  if (result.status != NetStatus::IN_PROGRESS) this->radio_waiting_ = false;
  return true;
}

bool CommunicationNetProtocolComponent::has_result(
    uint64_t transaction_id) const {
  if (this->radio_result_ready_ &&
      this->inbound_result_.transaction_id == transaction_id) return true;
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
  return this->interrupt_radio_result_ready_ &&
         this->interrupt_result_.transaction_id == transaction_id;
#else
  return false;
#endif
}

bool CommunicationNetProtocolComponent::take_result(
    uint64_t transaction_id, NetResult &result) {
  if (this->radio_result_ready_ &&
      this->inbound_result_.transaction_id == transaction_id)
    return this->take_result(result);
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
  if (!this->interrupt_radio_result_ready_ ||
      this->interrupt_result_.transaction_id != transaction_id) return false;
  result = this->interrupt_result_;
  this->interrupt_radio_result_ready_ = false;
  if (result.status != NetStatus::IN_PROGRESS)
    this->interrupt_radio_waiting_ = false;
  return true;
#else
  return false;
#endif
}

bool CommunicationNetProtocolComponent::cancel(uint64_t transaction_id) {
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
  if (this->interrupt_active_ &&
      this->interrupt_command_.transaction_id == transaction_id) {
    this->interrupt_active_ = false;
    this->interrupt_binding_ = nullptr;
    return true;
  }
#endif
  if (!this->inbound_active_ ||
      this->active_command_.transaction_id != transaction_id) return false;
  // Execution might already have happened; leave this transaction reserved.
  this->inbound_active_ = false;
  this->active_binding_ = nullptr;
  return true;
}

void CommunicationNetProtocolComponent::receive_mqtt_inbound_(
    const uint8_t *payload, size_t length) {
  uint8_t canonical[192]{};
  size_t canonical_size = 0;
  MqttCommandFields fields{};
  if (!decode_mqtt_command_envelope(
          payload, length, this->device_id_, canonical, sizeof(canonical),
          canonical_size, nullptr, this->mqtt_execution_source_,
          this->mqtt_execution_reply_, &fields) ||
      fields.source_boot_id == 0 || fields.timeout_ms == 0) {
    ESP_LOGW(TAG, "Inbound MQTT envelope rejected");
    return;
  }
  NetCommand command{};
  command.transaction_id = fields.transaction_id;
  command.source_boot_id = fields.source_boot_id;
  command.timeout_ms = fields.timeout_ms;
  if (!command.source_device_id.assign(fields.source) ||
      !command.device_id.assign(fields.device) ||
      !command.resource.assign(fields.resource) ||
      !command.name.assign(fields.action) ||
      !command.payload.assign(reinterpret_cast<const uint8_t *>("{}"), 2))
    return;
  const NetStart status = this->start_inbound_(command, millis(), false);
  if (status != NetStart::STARTED) {
    char result[256]{};
    const int size = std::snprintf(
        result, sizeof(result),
        "{\"transaction_id\":\"%llu\",\"result\":\"rejected\","
        "\"execution\":{\"started\":false},\"error\":{"
        "\"code\":\"remote_rejected\",\"retryable\":false}}",
        static_cast<unsigned long long>(fields.transaction_id));
    if (size > 0 && static_cast<size_t>(size) < sizeof(result))
      this->mqtt_wire_.publish(this->mqtt_execution_reply_,
                               reinterpret_cast<const uint8_t *>(result),
                               static_cast<size_t>(size));
  }
}

void CommunicationNetProtocolComponent::publish_inbound_result_() {
  if (this->mqtt_execution_reply_ == nullptr) return;
  NetResult *selected = nullptr;
  bool *ready = nullptr;
  bool *waiting = nullptr;
#ifdef USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND
  // Publish the stop response first; the interrupted operation follows on the
  // next cooperative loop iteration.
  if (this->interrupt_mqtt_result_ready_) {
    selected = &this->interrupt_result_;
    ready = &this->interrupt_mqtt_result_ready_;
    waiting = &this->interrupt_mqtt_waiting_;
  } else
#endif
  if (this->mqtt_result_ready_) {
    selected = &this->inbound_result_;
    ready = &this->mqtt_result_ready_;
    waiting = &this->mqtt_waiting_;
  }
  if (selected == nullptr) return;
  const NetResult &result = *selected;
  const char *status = result.status == NetStatus::SUCCEEDED ? "succeeded" :
                       result.status == NetStatus::IN_PROGRESS ? "in_progress" :
                       result.status == NetStatus::REJECTED ? "rejected" : "failed";
  char payload[384]{};
  int size = 0;
  if (result.status == NetStatus::IN_PROGRESS) {
    size = std::snprintf(payload, sizeof(payload),
                         "{\"transaction_id\":\"%llu\",\"result\":\"in_progress\","
                         "\"execution\":{\"started\":true,\"estimated_completion_ms\":%u}}",
                         static_cast<unsigned long long>(result.transaction_id),
                         static_cast<unsigned>(result.execution.estimated_completion_ms));
  } else if (result.status == NetStatus::SUCCEEDED) {
    const auto &remote = result.remote_state;
    const bool on = remote.data.size() >= 1 && remote.data.data()[0] == 1;
    if (std::strcmp(remote.schema.c_str(), "rgb-state/v1") == 0 &&
        remote.data.size() == 5) {
      const uint8_t *rgb = remote.data.data();
      size = std::snprintf(payload, sizeof(payload),
                           "{\"transaction_id\":\"%llu\",\"result\":\"succeeded\","
                           "\"execution\":{\"started\":true},\"remote_state\":{"
                           "\"complete\":true,\"value\":{\"on\":%s,\"red\":%u,"
                           "\"green\":%u,\"blue\":%u,\"brightness\":%u}}}",
                           static_cast<unsigned long long>(result.transaction_id),
                           on ? "true" : "false", rgb[1], rgb[2], rgb[3], rgb[4]);
    } else {
      size = std::snprintf(payload, sizeof(payload),
                           "{\"transaction_id\":\"%llu\",\"result\":\"succeeded\","
                           "\"execution\":{\"started\":true},\"remote_state\":{"
                           "\"complete\":true,\"value\":{\"on\":%s}}}",
                           static_cast<unsigned long long>(result.transaction_id),
                           on ? "true" : "false");
    }
  } else {
    const char *error = result.error.code ==
                                espnow_net_protocol::NetErrorCode::INTERRUPTED
                            ? "interrupted" : result.error.code ==
                                espnow_net_protocol::NetErrorCode::TARGET_UNAVAILABLE
                            ? "target_unavailable" : "timed_out";
    size = std::snprintf(payload, sizeof(payload),
                         "{\"transaction_id\":\"%llu\",\"result\":\"%s\","
                         "\"execution\":{\"started\":%s},\"error\":{"
                         "\"code\":\"%s\",\"retryable\":false}}",
                         static_cast<unsigned long long>(result.transaction_id), status,
                         result.execution.started ? "true" : "false", error);
  }
  if (size > 0 && static_cast<size_t>(size) < sizeof(payload) &&
      this->mqtt_wire_.publish(this->mqtt_execution_reply_,
                               reinterpret_cast<const uint8_t *>(payload),
                               static_cast<size_t>(size))) {
    *ready = false;
    if (result.status != NetStatus::IN_PROGRESS) *waiting = false;
  }
}
#endif

void CommunicationNetProtocolComponent::loop() {
#ifdef USE_COMMUNICATION_NET_STATE_SNAPSHOT
  this->state_snapshot_.loop(millis(), this->mqtt_wire_);
#endif
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
  this->loop(millis());
  this->publish_inbound_result_();
#endif
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  if (this->mqtt_command_topic_ && !this->mqtt_command_subscription_registered_) {
    this->mqtt_command_subscription_registered_ =
        this->mqtt_wire_.subscribe(this->mqtt_command_topic_);
    if (this->mqtt_command_subscription_registered_)
      ESP_LOGI(TAG, "MQTT command observation subscribed");
    return;
  }
  if (this->mqtt_result_topic_ && !this->mqtt_result_subscription_registered_) {
    this->mqtt_result_subscription_registered_ =
        this->mqtt_wire_.subscribe(this->mqtt_result_topic_);
    if (this->mqtt_result_subscription_registered_)
      ESP_LOGI(TAG, "MQTT result observation subscribed");
    return;
  }
  if (this->mqtt_outgoing_topic_ && !this->mqtt_outgoing_subscription_registered_) {
    this->mqtt_outgoing_subscription_registered_ =
        this->mqtt_wire_.subscribe(this->mqtt_outgoing_topic_);
    if (this->mqtt_outgoing_subscription_registered_)
      ESP_LOGI(TAG, "MQTT outgoing command observation subscribed");
    return;
  }
  if (this->mqtt_outgoing_topic_) {
    this->transactions_.expire(millis());
    TransactionEvent event{};
    (void) this->transactions_.take_event(event);
  }
  size_t payload_length = 0;
  if (this->mqtt_wire_.take_received(
          this->received_topic_, sizeof(this->received_topic_),
          this->received_payload_, sizeof(this->received_payload_),
          payload_length)) {
    if (this->mqtt_result_topic_ &&
        std::strcmp(this->received_topic_, this->mqtt_result_topic_) == 0) {
      MqttResultObservation result{};
      if (!decode_mqtt_result(this->received_payload_, payload_length, result)) {
        ESP_LOGD(TAG, "MQTT result rejected bytes=%u (observation only)",
                 static_cast<unsigned>(payload_length));
      } else {
        const auto correlation = this->transactions_.observe_result(
            result.transaction_id, result.stage, millis());
        ESP_LOGI(TAG, "MQTT result observed id=%llu stage=%u correlation=%u (observation only)",
                 static_cast<unsigned long long>(result.transaction_id),
                 static_cast<unsigned>(result.stage), static_cast<unsigned>(correlation));
      }
      return;
    }
    if (this->mqtt_outgoing_topic_ &&
        std::strcmp(this->received_topic_, this->mqtt_outgoing_topic_) == 0) {
      MqttCommandIdentity identity{};
      size_t canonical_length = 0;
      if (decode_mqtt_command_envelope(
              this->received_payload_, payload_length, this->mqtt_outgoing_target_,
              this->canonical_command_, sizeof(this->canonical_command_),
              canonical_length, &identity, this->device_id_, this->mqtt_result_topic_)) {
        const bool tracked = this->transactions_.begin(
            identity.transaction_id, identity.source_boot_id, millis(), 30000);
        ESP_LOGI(TAG, "MQTT outgoing command observed id=%llu tracked=%s (not executed)",
                 static_cast<unsigned long long>(identity.transaction_id),
                 tracked ? "YES" : "NO");
      } else {
        ESP_LOGD(TAG, "MQTT outgoing command rejected bytes=%u (observation only)",
                 static_cast<unsigned>(payload_length));
      }
      return;
    }
    if (!this->mqtt_command_topic_ ||
        std::strcmp(this->received_topic_, this->mqtt_command_topic_) != 0) return;
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
    if (this->mqtt_execution_source_ != nullptr) {
      this->receive_mqtt_inbound_(this->received_payload_, payload_length);
      return;
    }
#endif
    ++this->observed_commands_;
    ESP_LOGD(TAG, "MQTT command observed bytes=%u count=%u (not executed)",
             static_cast<unsigned>(payload_length),
             static_cast<unsigned>(this->observed_commands_));
    size_t canonical_length = 0;
    if (decode_mqtt_command_envelope(this->received_payload_, payload_length,
                                     this->device_id_, this->canonical_command_,
                                     sizeof(this->canonical_command_), canonical_length) ||
        decode_mqtt_command_probe(this->received_payload_, payload_length,
                                  this->device_id_, this->canonical_command_,
                                  sizeof(this->canonical_command_), canonical_length)) {
      ++this->valid_commands_;
      ESP_LOGI(TAG, "MQTT canonical command validated bytes=%u count=%u (not executed)",
               static_cast<unsigned>(canonical_length),
               static_cast<unsigned>(this->valid_commands_));
    } else {
      ++this->rejected_commands_;
      ESP_LOGD(TAG, "MQTT probe payload rejected count=%u (not executed)",
               static_cast<unsigned>(this->rejected_commands_));
    }
  }
#endif
}

void CommunicationNetProtocolComponent::dump_config() {
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
  ESP_LOGCONFIG(TAG, "Communication NetProtocol: inbound executor enabled");
#else
  ESP_LOGCONFIG(TAG, "Communication NetProtocol: FOUNDATION ONLY (routing disabled)");
#endif
  ESP_LOGCONFIG(TAG, "  Device ID: %s", this->device_id_ == nullptr ? "" : this->device_id_);
#ifdef USE_COMMUNICATION_NET_INBOUND
#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE
  ESP_LOGCONFIG(TAG, "  Inbound route declarations: %u (executor enabled)",
#else
  ESP_LOGCONFIG(TAG, "  Inbound route declarations: %u (dispatch disabled)",
#endif
                static_cast<unsigned>(this->inbound_routes_.size()));
  if (!this->inbound_routes_valid_)
    ESP_LOGE(TAG, "Inbound route registration failed");
#else
  ESP_LOGCONFIG(TAG, "  Inbound route declarations: 0 (dispatch disabled)");
#endif
#if defined(USE_MQTT) && defined(USE_COMMUNICATION_NET_MQTT_LISTENER)
  ESP_LOGCONFIG(TAG, "  MQTT command observation: %s",
                this->mqtt_command_topic_ == nullptr ? "DISABLED" : "RECEIVE ONLY");
  ESP_LOGCONFIG(TAG, "  MQTT result observation: %s",
                this->mqtt_result_topic_ == nullptr ? "DISABLED" : "RECEIVE ONLY");
  ESP_LOGCONFIG(TAG, "  MQTT outgoing command observation: %s",
                this->mqtt_outgoing_topic_ == nullptr ? "DISABLED" : "RECEIVE ONLY");
#endif
}

}  // namespace communication_net_protocol
}  // namespace esphome
