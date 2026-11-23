#pragma once
#include <esp_random.h>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "../communication_net_protocol/outbound_command_service.h"
#include "../communication_net_protocol/outbound_destination_registry.h"
#ifdef USE_COMMUNICATION_NET_OUTBOUND_MQTT
#include "../communication_net_protocol/outbound_mqtt_command_transport.h"
#endif
#ifdef USE_COMMUNICATION_NET_OUTBOUND_ESPNOW
#include "../communication_net_protocol/outbound_espnow_command_transport.h"
#endif

namespace esphome::communication_net_protocol::outbound {
class PushSnapshotObserver {
 public:
  virtual ~PushSnapshotObserver() = default;
  virtual void on_pushed_snapshot(const char *destination, const char *device, const CommandResult &result) = 0;
};
class OutboundRuntime : public Component, public CommandSubmissionPort, public SubmissionObserver
#ifdef USE_COMMUNICATION_NET_OUTBOUND_ESPNOW
    , public espnow_net_protocol::NetCommandResultObserver, public espnow_net_protocol::NetUnsolicitedResultObserver
#endif
{
 public:
  float get_setup_priority() const override { return -10.0f; }
  void set_source(const char *source) { config_ok_ = source_.assign(source) && !source_.empty() && config_ok_; }
#ifdef USE_COMMUNICATION_NET_OUTBOUND_MQTT
  void set_mqtt(const char *prefix, const char *reply) { prefix_ = prefix; reply_ = reply; }
#endif
#ifdef USE_COMMUNICATION_NET_OUTBOUND_ESPNOW
  void set_radio(espnow_net_protocol::EspNowNetProtocolComponent *endpoint) { endpoint_ = endpoint; }
#endif
  void add_policy(const char *id, uint8_t first, uint8_t second) {
    CommunicationPolicy policy;
    bool ok = policy.add_transport(static_cast<CommunicationTransport>(first));
    if (second != 0) ok = policy.add_transport(static_cast<CommunicationTransport>(second)) && ok;
    config_ok_ = ok && service_.add_policy(id, policy) && config_ok_;
  }
  void add_destination(const char *id, const char *device, const char *policy, const char *peer) {
    if (destination_count_ >= 16) { config_ok_ = false; return; }
    const bool ok = service_.add_destination(id, device, policy) && destinations_.add_destination(device);
    config_ok_ = ok && config_ok_;
    metadata_[destination_count_++] = {id, device, peer};
  }
  void add_observer(SubmissionObserver *observer) { config_ok_ = service_.add_observer(observer) && config_ok_; }
  void add_push_observer(PushSnapshotObserver *observer) {
#ifdef USE_COMMUNICATION_NET_OUTBOUND_ESPNOW
    if (observer == nullptr || push_count_ == 4) { config_ok_ = false; return; }
    for (size_t i = 0; i < push_count_; ++i) if (push_observers_[i] == observer) return;
    push_observers_[push_count_++] = observer;
#else
    config_ok_ = false;
#endif
  }
  void setup() override {
    boot_ = ((uint64_t(esp_random()) << 32) | esp_random()) & 0x7fffffffffffffffULL;
    if (boot_ == 0) boot_ = 1;
    bool ok = config_ok_ && service_.configure_session(source_.c_str(), boot_);
#ifdef USE_COMMUNICATION_NET_OUTBOUND_MQTT
    if (prefix_ == nullptr || reply_ == nullptr) ok = false;
    else for (size_t i = 0; i < 3; i++) {
      ok = mqtt_[i].configure(prefix_, reply_, source_.c_str(), boot_) && ok;
      ok = registries_[i].register_transport(&mqtt_[i]) && ok;
    }
#endif
#ifdef USE_COMMUNICATION_NET_OUTBOUND_ESPNOW
    if (endpoint_ == nullptr) ok = false;
    else {
      for (size_t i = 0; i < destination_count_; i++) {
        if (metadata_[i].peer == nullptr || metadata_[i].peer[0] == '\0') continue;
        const auto peer = endpoint_->peer_index(metadata_[i].peer);
        ok = destinations_.set_espnow_peer(metadata_[i].device, peer) && ok;
      }
      for (auto &adapter : radio_) {
        adapter.set_protocol(endpoint_, false);  // One owner routes both adapters.
        adapter.set_destination_registry(&destinations_);
        ok = adapter.set_application_identity(source_.c_str(), &boot_) && ok;
      }
      // Two endpoint slots: one normal command and one reserved for interruption.
      ok = registries_[0].register_transport(&radio_[0]) && ok;
      ok = registries_[2].register_transport(&radio_[1]) && ok;
      endpoint_->set_command_result_observer(this);
      if (push_count_ != 0) endpoint_->set_unsolicited_result_observer(this);
    }
#endif
    ok = service_.add_observer(this) && ok;
    if (!ok) { mark_failed(); ESP_LOGE("communication.outbound", "Runtime configuration failed"); return; }
    ready_ = true;
    ESP_LOGI("communication.outbound", "READY source=%s session=%llu", source_.c_str(), static_cast<unsigned long long>(boot_));
  }
  Submission submit(const CommandDraft &draft, uint32_t now_ms) override {
    if (!ready_) return {SubmissionStatus::UNAVAILABLE, 0};
    const auto submitted = service_.submit(draft, now_ms);
    ESP_LOGI("communication.outbound", "SUBMIT destination=%s resource=%s command=%s status=%u tx=%llu",
             draft.destination.c_str(), draft.target.resource.c_str(), draft.command.name.c_str(),
             unsigned(submitted.status), static_cast<unsigned long long>(submitted.transaction_id));
    return submitted;
  }
  void loop() override { if (ready_) service_.loop(millis()); }
  void on_submission_result(const SubmissionContext &context, const CommandResult &result, bool terminal) override {
    ESP_LOGI("communication.outbound", "RESULT destination=%s resource=%s tx=%llu terminal=%s result=%u error=%u",
             context.destination.c_str(), context.target.resource.c_str(), static_cast<unsigned long long>(result.transaction_id),
             terminal ? "YES" : "NO", static_cast<unsigned>(result.result), static_cast<unsigned>(result.error.code));
  }
#ifdef USE_COMMUNICATION_NET_OUTBOUND_ESPNOW
  void on_unsolicited_net_result(espnow_net_protocol::PeerIndex peer, const espnow_net_protocol::NetResult &result) override {
    // Encrypted endpoint binds the configured peer; ordinary replies never enter this path.
    if (push_count_ == 0 || (result.transaction_id & (uint64_t(1) << 63)) == 0 ||
        result.status != espnow_net_protocol::NetResultStatus::SUCCEEDED || !result.consistent()) return;
    for (size_t i = 0; i < destination_count_; ++i) {
      const auto &metadata = metadata_[i];
      if (peer == CommunicationDestinationRegistry::INVALID_ESPNOW_PEER ||
          destinations_.espnow_peer_for(metadata.device) != peer) continue;
      CommandResult converted;
      EspNowNetModelAdapter model;
      if (!model.from_net(result, converted)) return;
      for (size_t j = 0; j < push_count_; ++j)
        push_observers_[j]->on_pushed_snapshot(metadata.destination, metadata.device, converted);
      return;
    }
  }
  void on_net_command_result(espnow_net_protocol::PeerIndex peer, const espnow_net_protocol::NetResult &result) override {
    for (auto &adapter : radio_)
      if (adapter.state() != EspNowCommandTransportState::IDLE) adapter.on_net_command_result(peer, result);
  }
#endif
 protected:
  struct Metadata { const char *destination{nullptr}; const char *device{nullptr}; const char *peer{nullptr}; };
  CommunicationTransportRegistry registries_[3];
  CommandSubmissionService service_{registries_[0], registries_[1], registries_[2]};
  CommunicationDestinationRegistry destinations_;
  Metadata metadata_[16]{};
  size_t destination_count_{0};
  BoundedText<63> source_;
  uint64_t boot_{0};
  bool config_ok_{true}, ready_{false};
#ifdef USE_COMMUNICATION_NET_OUTBOUND_MQTT
  const char *prefix_{nullptr}, *reply_{nullptr};
  EsphomeMqttCommandTransport mqtt_[3];
#endif
#ifdef USE_COMMUNICATION_NET_OUTBOUND_ESPNOW
  espnow_net_protocol::EspNowNetProtocolComponent *endpoint_{nullptr};
  EspNowCommandTransport radio_[2];
  PushSnapshotObserver *push_observers_[4]{};
  size_t push_count_{0};
#endif
};
}  // namespace esphome::communication_net_protocol::outbound
