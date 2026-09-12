#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace communication_net_protocol {

// Foundation only. No subscriptions, peer ownership, or commands are started.
class CommunicationNetProtocolComponent : public Component {
 public:
  void set_device_id(const char *device_id) { this->device_id_ = device_id; }
  void setup() override {}
  void dump_config() override;

 protected:
  const char *device_id_{nullptr};
};

}  // namespace communication_net_protocol
}  // namespace esphome
