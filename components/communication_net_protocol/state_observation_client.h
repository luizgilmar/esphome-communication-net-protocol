#pragma once
#include <cstdint>

namespace esphome::communication_net_protocol {
// Transport-free control state. The owner submits through its existing command
// scheduler and completes only correlated, validated responses. Retained or
// unsolicited snapshots must never call complete(MQTT_QUERY).
class StateObservationClient {
 public:
  enum class Operation : uint8_t { NONE, MQTT_QUERY, HELLO, LEASE, STOP };
  bool configure(uint32_t query_interval, uint32_t freshness) {
    if (query_interval < 5000 || query_interval > 60000 || freshness < 2 * query_interval || freshness > 300000) return false;
    interval_ = query_interval; freshness_ = freshness; enabled_ = true; return true;
  }
  bool enabled() const { return enabled_; }
  uint32_t freshness() const { return freshness_; }
  bool mqtt_healthy(uint32_t now) const {
    return connected_ && proof_ && uint32_t(now - proof_at_) < freshness_;
  }
  void connection(bool connected) {
    if (!connected) {
      proof_ = false;
      if (pending_ == Operation::MQTT_QUERY) pending_mqtt_valid_ = false;
    }
    if (connected && !connected_) { mqtt_due_ = 0; mqtt_failures_ = 0; }
    connected_ = connected;
  }
  Operation due(uint32_t now) const {
    if (!enabled_ || pending_ != Operation::NONE) return Operation::NONE;
    if (connected_ && mqtt_due_ == 0) return Operation::MQTT_QUERY;
    // Keep the radio lease current before probing a silent MQTT endpoint again.
    if (!mqtt_healthy(now) && ready(now, radio_due_)) {
      if (nonce_ == 0 || (!active_ && uint32_t(now - hello_at_) >= 10000)) return Operation::HELLO;
      if (!active_ || uint32_t(now - lease_at_) >= validity_ / 3) return Operation::LEASE;
    }
    if (connected_ && ready(now, mqtt_due_)) return Operation::MQTT_QUERY;
    if (mqtt_healthy(now) && active_ && ready(now, radio_due_)) return Operation::STOP;
    return Operation::NONE;
  }
  bool begin(Operation operation, uint32_t now) {
    if (operation == Operation::NONE || operation != due(now)) return false;
    pending_ = operation; pending_mqtt_valid_ = connected_; return true;
  }
  Operation pending() const { return pending_; }
  uint64_t nonce() const { return nonce_; }
  bool active() const { return active_; }
  void complete(bool success, uint32_t now, uint32_t jitter,
                uint64_t nonce = 0, uint32_t validity = 0) {
    const Operation operation = pending_; pending_ = Operation::NONE;
    if (operation == Operation::NONE) return;
    if (operation == Operation::MQTT_QUERY) {
      success = success && pending_mqtt_valid_ && connected_;
      if (success) { proof_ = true; proof_at_ = now; mqtt_failures_ = 0; mqtt_due_ = now + interval_; }
      else mqtt_due_ = now + retry(mqtt_failures_, jitter);
      return;
    }
    if (operation == Operation::HELLO) {
      success = success && nonce != 0 && validity >= 30000 && validity <= 300000;
      if (success) { nonce_ = nonce; validity_ = validity; active_ = false; hello_at_ = now; }
    } else if (operation == Operation::LEASE && success) {
      active_ = true; lease_at_ = now;
    } else if (operation == Operation::STOP) {
      // Lost stop acknowledgement is resolved by server expiry; never renew
      // a lease while a fresh MQTT proof is available.
      active_ = false; nonce_ = 0;
    }
    if (success) { radio_failures_ = 0; radio_due_ = now + (operation == Operation::HELLO ? 1 : 1000); }
    else { nonce_ = 0; active_ = false; radio_due_ = now + retry(radio_failures_, jitter); }
  }
 private:
  static bool ready(uint32_t now, uint32_t deadline) {
    return deadline == 0 || int32_t(now - deadline) >= 0;
  }
  static uint32_t retry(uint8_t &failures, uint32_t jitter) {
    uint32_t delay = 2000U << (failures < 4 ? failures : 4);
    if (failures < 5) ++failures;
    if (delay > 30000) delay = 30000;
    return delay + jitter % 1001;
  }
  uint64_t nonce_{0};
  uint32_t interval_{15000}, freshness_{45000}, proof_at_{0};
  uint32_t mqtt_due_{0}, radio_due_{0}, validity_{60000}, hello_at_{0}, lease_at_{0};
  Operation pending_{Operation::NONE};
  uint8_t mqtt_failures_{0}, radio_failures_{0};
  bool enabled_{false}, connected_{false}, proof_{false}, active_{false}, pending_mqtt_valid_{false};
};
static_assert(sizeof(StateObservationClient) <= 64, "State observation control must stay bounded");
}
