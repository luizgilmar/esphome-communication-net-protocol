#pragma once
#include "esphome/core/automation.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/cover/cover.h"

namespace esphome::communication_net_protocol {
// These actions consume the validated inbound trigger tuple, never global state.
class ArgumentRgbAction : public Action<uint16_t, uint8_t, uint8_t, uint8_t> {
 public:
  explicit ArgumentRgbAction(light::LightState *target) : target_(target) {}
  void play(const uint16_t &, const uint8_t &red, const uint8_t &green, const uint8_t &blue) override {
    auto call = target_->turn_on();
    call.set_effect("None");
    call.set_rgb(red / 255.0f, green / 255.0f, blue / 255.0f);
    call.perform();
  }
 private:
  light::LightState *target_;
};
class ArgumentBrightnessAction : public Action<uint16_t, uint8_t, uint8_t, uint8_t> {
 public:
  explicit ArgumentBrightnessAction(light::LightState *target) : target_(target) {}
  void play(const uint16_t &value, const uint8_t &, const uint8_t &, const uint8_t &) override {
    if (value < 1 || value > 100) return;
    auto call = target_->turn_on();
    call.set_brightness(value / 100.0f);
    call.perform();
  }
 private:
  light::LightState *target_;
};
class ArgumentPositionAction : public Action<uint16_t, uint8_t, uint8_t, uint8_t> {
 public:
  explicit ArgumentPositionAction(cover::Cover *target) : target_(target) {}
  void play(const uint16_t &value, const uint8_t &, const uint8_t &, const uint8_t &) override {
    if (value > 100) return;
    auto call = target_->make_call();
    call.set_position(value / 100.0f);
    call.perform();
  }
 private:
  cover::Cover *target_;
};
}  // namespace esphome::communication_net_protocol
