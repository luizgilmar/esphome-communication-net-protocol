#pragma once
#include "esphome/core/defines.h"
#ifdef USE_COMMUNICATION_NET_LIGHT_SUPPLIES
#include <vector>
#include "esphome/core/automation.h"
#include "esphome/components/light/light_state.h"

namespace esphome::communication_net_protocol {
// Membership is declared by the installation; no resource names are embedded.
class LightSupplyGroup {
 public:
  explicit LightSupplyGroup(light::LightState *supply) : supply_(supply) {}
  void add_member(light::LightState *member) { members_.push_back(member); }
  void release_if_unused() {
    if (members_.empty()) return;
    for (auto *member : members_) {
      // Preserve power both during a fade-out and when another light was requested on.
      if (member->current_values.is_on() || member->remote_values.is_on()) return;
    }
    supply_->turn_off().perform();
  }
 private:
  light::LightState *supply_;
  std::vector<light::LightState *> members_;
};
template<typename... Ts> class ReleaseLightSupplyAction : public Action<Ts...> {
 public:
  explicit ReleaseLightSupplyAction(LightSupplyGroup *group) : group_(group) {}
  void play(const Ts &...) override { group_->release_if_unused(); }
 private:
  LightSupplyGroup *group_;
};
}  // namespace esphome::communication_net_protocol
#endif  // USE_COMMUNICATION_NET_LIGHT_SUPPLIES
