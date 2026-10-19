#include "esphome/core/defines.h"

#if defined(USE_TX_ULTIMATE_ESPNOW) || defined(USE_COMMUNICATION_NET_OUTBOUND_ESPNOW)

#include "outbound_espnow_command_transport.h"

#include "esphome/core/hal.h"

namespace esphome {
namespace communication_net_protocol { namespace outbound {

namespace {
// Older ESP-NOW components have no application identity fields. Keep their
// legacy command path available while the receiver is being upgraded.
template<typename Command>
auto populate_net_command_identity(Command &command, const char *source_id,
                              uint64_t boot_id, int)
    -> decltype(command.source_device_id.assign(source_id),
                command.source_boot_id = boot_id, bool()) {
  if (!command.source_device_id.assign(source_id)) return false;
  command.source_boot_id = boot_id;
  return command.valid();
}

template<typename Command>
bool populate_net_command_identity(Command &, const char *, uint64_t, ...) {
  return false;
}
}  // namespace

bool EspNowCommandTransport::is_available() const {
  if (this->protocol_ == nullptr || !this->protocol_->runtime_enabled() ||
      this->destinations_ == nullptr ||
      this->state_ != EspNowCommandTransportState::IDLE)
    return false;
  const auto &radio = this->protocol_->radio();
  return radio.initialized() && radio.channel_matches() &&
         this->protocol_->can_start_command();
}

TransportStartStatus EspNowCommandTransport::start(
    const CommandRequest &request) {
  if (!request.is_valid()) return TransportStartStatus::INVALID_REQUEST;
  if (this->is_busy()) return TransportStartStatus::BUSY;
  if (!this->is_available()) return TransportStartStatus::UNAVAILABLE;

  const uint8_t peer = this->destinations_->espnow_peer_for(
      request.target.device_id.c_str());
  if (peer == CommunicationDestinationRegistry::INVALID_ESPNOW_PEER)
    return TransportStartStatus::UNAVAILABLE;

  espnow_net_protocol::NetCommand command{};
  if (!this->model_adapter_.to_net(request, command))
    return TransportStartStatus::INVALID_REQUEST;
  if (this->application_boot_id_ != nullptr) {
    if (*this->application_boot_id_ == 0 ||
        !populate_net_command_identity(command, this->source_device_id_.c_str(),
                                       *this->application_boot_id_, 0))
      return TransportStartStatus::UNAVAILABLE;
  }
  if (!this->protocol_->start_command(peer, command, millis()))
    return TransportStartStatus::BUSY;

  this->transaction_id_ = request.transaction_id;
  this->peer_index_ = peer;
  this->result_ = {};
  this->state_ = EspNowCommandTransportState::WAITING_FOR_RESULT;
  return TransportStartStatus::STARTED;
}

void EspNowCommandTransport::loop(uint32_t now_ms) {
  (void) now_ms;
}

void EspNowCommandTransport::on_net_command_result(
    espnow_net_protocol::PeerIndex peer,
    const espnow_net_protocol::NetResult &result) {
  const bool pending_progress = this->state_ == EspNowCommandTransportState::RESULT_READY &&
      this->result_.result == CommandResultStatus::IN_PROGRESS;
  if ((this->state_ != EspNowCommandTransportState::WAITING_FOR_RESULT && !pending_progress) ||
      peer != this->peer_index_ || result.transaction_id != this->transaction_id_) {
    this->ignored_message_count_++;
    return;
  }
  this->result_ = {};
  if (!this->model_adapter_.from_net(result, this->result_)) {
    this->ignored_message_count_++;
    return;
  }
  if (this->result_.result == CommandResultStatus::FAILED &&
      (result.transport_failure || !result.execution.started))
    this->result_.transport_status =
        pre_execution_transport_status_(result.error.code);
  this->state_ = EspNowCommandTransportState::RESULT_READY;
}

bool EspNowCommandTransport::take_result(CommandResult &result) {
  if (!this->has_result()) return false;
  result = this->result_;
  if (result.result == CommandResultStatus::IN_PROGRESS) {
    // Acknowledging delivery and accepting the remote command does not end
    // the transaction. Keep its correlation and peer reservation until the
    // remote executor reports a terminal result.
    this->result_ = {};
    this->state_ = EspNowCommandTransportState::WAITING_FOR_RESULT;
  } else {
    this->reset_();
  }
  return true;
}

bool EspNowCommandTransport::cancel(TransactionId transaction_id) {
  if (this->state_ == EspNowCommandTransportState::IDLE ||
      transaction_id == 0 || transaction_id != this->transaction_id_)
    return false;
  if (this->protocol_ != nullptr)
    (void) this->protocol_->cancel_command(transaction_id);
  this->reset_();
  return true;
}

TransportStatus EspNowCommandTransport::pre_execution_transport_status_(
    espnow_net_protocol::NetErrorCode error) {
  switch (error) {
    case espnow_net_protocol::NetErrorCode::TRANSPORT_UNAVAILABLE:
      return TransportStatus::UNAVAILABLE;
    case espnow_net_protocol::NetErrorCode::CONNECTION_FAILED:
      return TransportStatus::CONNECTION_FAILED;
    case espnow_net_protocol::NetErrorCode::AUTHENTICATION_FAILED:
      return TransportStatus::AUTHENTICATION_FAILED;
    case espnow_net_protocol::NetErrorCode::TIMED_OUT:
      return TransportStatus::TIMED_OUT;
    default:
      return TransportStatus::PROTOCOL_ERROR;
  }
}

void EspNowCommandTransport::reset_() {
  this->result_ = {};
  this->transaction_id_ = 0;
  this->peer_index_ = espnow_net_protocol::INVALID_PEER_INDEX;
  this->state_ = EspNowCommandTransportState::IDLE;
}

} }  // namespace communication_net_protocol::outbound
}  // namespace esphome

#endif  // shared outbound ESP-NOW
