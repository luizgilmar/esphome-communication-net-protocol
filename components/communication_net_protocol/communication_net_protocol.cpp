#include "communication_net_protocol.h"

#include "esphome/core/log.h"

namespace esphome {
namespace communication_net_protocol {

static const char *const TAG = "communication_net_protocol";

void CommunicationNetProtocolComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Communication NetProtocol: FOUNDATION ONLY (routing disabled)");
  ESP_LOGCONFIG(TAG, "  Device ID: %s", this->device_id_ == nullptr ? "" : this->device_id_);
}

}  // namespace communication_net_protocol
}  // namespace esphome
