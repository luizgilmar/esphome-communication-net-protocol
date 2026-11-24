#pragma once
#include "esphome/core/defines.h"
#ifdef USE_MOTION_COVER
#include <cmath>
#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/communication_net_protocol/cover_motion_model.h"
namespace esphome::motion_cover {
class MotionCover : public cover::Cover, public Component {
 public:
  void set_backend(cover::Cover *backend,uint32_t open_ms,uint32_t close_ms) {backend_=backend;open_ms_=open_ms;close_ms_=close_ms;}
  float get_setup_priority() const override {return setup_priority::DATA-1;}
  cover::CoverTraits get_traits() override {return backend_->get_traits();}
  void setup() override {
    if(backend_==nullptr || open_ms_==0 || close_ms_==0) {mark_failed();return;}
    position=backend_->position;current_operation=backend_->current_operation;target_=position;planned_=current_operation==cover::COVER_OPERATION_IDLE;
    backend_->add_on_state_callback(StateForwarder{this});
  }
  void loop() override {
    if(is_failed()) return;
    if(current_operation!=backend_->current_operation) synchronize_(false,backend_->position);
    position=backend_->position;
  }
  uint32_t revision() const {return revision_;}
  uint32_t movement() const {return movement_;}
  bool sample(communication_net_protocol::CoverMotionFrame &frame) const {
    const float pos=backend_->position;
    if(is_failed() || !std::isfinite(pos) || pos<0 || pos>1 || revision_==0) return false;
    frame.position=uint16_t(std::round(pos*10000));frame.operation=uint8_t(backend_->current_operation);
    frame.movement=movement_;frame.revision=revision_;frame.planned=planned_;
    if(frame.operation==0 || !planned_) {frame.target=frame.position;frame.remaining_ms=0;return true;}
    frame.target=uint16_t(std::round(target_*10000));
    const float full=frame.operation==1 ? open_ms_ : close_ms_;
    frame.remaining_ms=uint32_t(std::ceil(std::fabs(target_-pos)*full));
    return frame.remaining_ms>0;
  }
 protected:
  void control(const cover::CoverCall &call) override {
    if(is_failed() || (!call.get_stop() && !call.get_position().has_value() && !call.get_toggle().has_value())) return;
    forwarding_=true;
    auto forwarded=backend_->make_call();
    if(call.get_stop()) forwarded.set_command_stop();
    if(call.get_position().has_value()) forwarded.set_position(*call.get_position());
    if(call.get_toggle().has_value()) forwarded.set_command_toggle();
    forwarded.perform();forwarding_=false;
    const auto operation=backend_->current_operation;
    float target=backend_->position;
    if(operation!=cover::COVER_OPERATION_IDLE) target=call.get_position().has_value() ? *call.get_position() : (operation==cover::COVER_OPERATION_OPENING ? 1.0f : 0.0f);
    synchronize_(true,target);
    publish_state(false); // Backend alone owns restore/calibration state.
  }
 private:
  struct StateForwarder {MotionCover *owner;void operator()() const {owner->backend_update_();}};
  void backend_update_() {
    if(forwarding_) return;
    if(backend_->current_operation!=current_operation) synchronize_(false,backend_->position);
    position=backend_->position;publish_state(false);
  }
  void synchronize_(bool controlled,float target) {
    const auto next=backend_->current_operation;
    const bool moving=next!=cover::COVER_OPERATION_IDLE;
    const bool changed=next!=current_operation || (controlled && moving && !planned_) || (controlled && moving && std::fabs(target-target_)>0.00001f);
    if(changed) {
      if(revision_==UINT32_MAX || (moving && movement_==UINT32_MAX)) {mark_failed();return;}
      ++revision_;if(moving)++movement_;
    }
    current_operation=next;position=backend_->position;
    if(!moving) {target_=position;planned_=true;}
    else if(controlled) {target_=target;planned_=true;}
    else {target_=position;planned_=false;} // Never invent an intermediate target after bypass.
  }
  cover::Cover *backend_{nullptr};uint32_t open_ms_{0},close_ms_{0},revision_{1},movement_{0};
  float target_{0};bool forwarding_{false},planned_{false};
};
} // namespace esphome::motion_cover
#endif
