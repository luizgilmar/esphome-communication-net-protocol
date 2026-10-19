#pragma once

#include "esphome/core/defines.h"

#if defined(USE_TX_ULTIMATE_ESPNOW) || defined(USE_COMMUNICATION_NET_OUTBOUND_ESPNOW)

#include <cstdint>
#include <cstring>

#include "esphome/components/espnow_net_protocol/espnow_net_protocol.h"

#include "outbound_destination_registry.h"
#include "outbound_espnow_model_adapter.h"
#include "outbound_communication_transport.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

enum class EspNowCommandTransportState : uint8_t {
  IDLE,
  WAITING_FOR_RESULT,
  RESULT_READY,
};

// TX-domain adapter over the hardware-neutral ESP-NOW protocol endpoint.
// Each scheduler lane owns one adapter. The protocol endpoint correlates two
// functional commands while keeping physical radio transmission serialized.
class EspNowCommandTransport
    : public CommunicationTransportAdapter,
      public espnow_net_protocol::NetCommandResultObserver {
 public:
  void set_protocol(
      espnow_net_protocol::EspNowNetProtocolComponent *protocol,
      bool register_observer = true) {
    this->protocol_ = protocol;
    if (this->protocol_ != nullptr && register_observer)
      this->protocol_->set_command_result_observer(this);
  }
  void set_destination_registry(
      const CommunicationDestinationRegistry *destinations) {
    this->destinations_ = destinations;
  }
  bool set_application_identity(const char *source_id,
                                const uint64_t *boot_id) {
    if (boot_id == nullptr || source_id == nullptr || source_id[0] == '\0' ||
        !this->source_device_id_.assign(source_id)) return false;
    this->application_boot_id_ = boot_id;
    return true;
  }

  CommunicationTransport transport_type() const override {
    return CommunicationTransport::ESP_NOW;
  }
  bool application_identity_matches(const char *source, uint64_t boot_id) const override {
    return source != nullptr && boot_id != 0 && application_boot_id_ != nullptr &&
           *application_boot_id_ == boot_id && std::strcmp(source_device_id_.c_str(), source) == 0;
  }
  bool is_available() const override;
  bool is_busy() const override {
    return this->state_ != EspNowCommandTransportState::IDLE;
  }
  TransportStartStatus start(const CommandRequest &request) override;
  void use_external_result_deadline(TransactionId transaction_id) override {
    if (this->protocol_ != nullptr && transaction_id == this->transaction_id_)
      this->protocol_->use_external_result_deadline(transaction_id);
  }
  void loop(uint32_t now_ms) override;
  bool has_result() const override {
    return this->state_ == EspNowCommandTransportState::RESULT_READY;
  }
  bool take_result(CommandResult &result) override;
  bool cancel(TransactionId transaction_id) override;
  void on_net_command_result(
      espnow_net_protocol::PeerIndex peer,
      const espnow_net_protocol::NetResult &result) override;

  EspNowCommandTransportState state() const { return this->state_; }
  uint32_t ignored_message_count() const {
    return this->ignored_message_count_;
  }

 private:
  static TransportStatus pre_execution_transport_status_(
      espnow_net_protocol::NetErrorCode error);
  void reset_();

  espnow_net_protocol::EspNowNetProtocolComponent *protocol_{nullptr};
  const CommunicationDestinationRegistry *destinations_{nullptr};
  espnow_net_protocol::BoundedText<espnow_net_protocol::NetCommand::MAX_DEVICE_ID_LENGTH>
      source_device_id_{};
  const uint64_t *application_boot_id_{nullptr};
  EspNowNetModelAdapter model_adapter_{};
  CommandResult result_{};
  TransactionId transaction_id_{0};
  espnow_net_protocol::PeerIndex peer_index_{
      espnow_net_protocol::INVALID_PEER_INDEX};
  uint32_t ignored_message_count_{0};
  EspNowCommandTransportState state_{EspNowCommandTransportState::IDLE};
};

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome

#endif  // shared outbound ESP-NOW
