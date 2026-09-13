#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace communication_net_protocol {

// Application outcome, independent from MQTT publish and ESP-NOW delivery ACK.
enum class TransactionStage : uint8_t {
  IN_PROGRESS,
  SUCCEEDED,
  REJECTED,
  FAILED,
  EXPIRED,
  INTERRUPTED,
};

enum class TransactionObserveStatus : uint8_t {
  ACCEPTED,
  UNKNOWN_TRANSACTION,
  STALE_SESSION,
  ALREADY_TERMINAL,
};

struct TransactionEvent {
  uint64_t transaction_id{0};
  TransactionStage stage{TransactionStage::FAILED};
  uint32_t elapsed_ms{0};
};

// One in-progress and one terminal event can be retained per transaction.
// Overlapping responses cannot overwrite the terminal outcome or push data
// from an MQTT callback onto the scheduler's stack.
template<size_t Capacity> class TransactionTracker {
 public:
  static_assert(Capacity > 0 && Capacity <= 16, "bounded transaction capacity required");

  bool begin(uint64_t transaction_id, uint64_t source_boot_id,
             uint32_t now_ms, uint32_t timeout_ms) {
    if (transaction_id == 0 || source_boot_id == 0 || timeout_ms == 0 ||
        this->find_(transaction_id) != nullptr) return false;
    for (auto &slot : slots_) {
      if (slot.occupied) continue;
      slot = {};
      slot.occupied = true;
      slot.transaction_id = transaction_id;
      slot.source_boot_id = source_boot_id;
      slot.started_ms = now_ms;
      slot.timeout_ms = timeout_ms;
      return true;
    }
    return false;
  }

  TransactionObserveStatus observe(uint64_t transaction_id,
                                   uint64_t source_boot_id,
                                   TransactionStage stage, uint32_t now_ms) {
    Slot *slot = this->find_(transaction_id);
    if (slot == nullptr) return TransactionObserveStatus::UNKNOWN_TRANSACTION;
    if (slot->source_boot_id != source_boot_id)
      return TransactionObserveStatus::STALE_SESSION;
    if (slot->terminal_pending) return TransactionObserveStatus::ALREADY_TERMINAL;
    TransactionEvent event{transaction_id, stage, now_ms - slot->started_ms};
    if (stage == TransactionStage::IN_PROGRESS) {
      // Multiple progress updates coalesce without losing a final response.
      slot->progress = event;
      slot->progress_pending = true;
    } else {
      slot->terminal = event;
      slot->terminal_pending = true;
    }
    return TransactionObserveStatus::ACCEPTED;
  }

  // Call only after the executor has confirmed a separate stop command.
  // Do not manufacture a successful endpoint result for the original motion.
  // The caller must verify that stop and motion refer to the same resource;
  // this transport-independent tracker has no knowledge of resource IDs.
  TransactionObserveStatus confirm_interruption(uint64_t transaction_id,
                                                uint64_t source_boot_id,
                                                uint32_t now_ms) {
    const TransactionObserveStatus status = this->observe(
        transaction_id, source_boot_id, TransactionStage::INTERRUPTED, now_ms);
    if (status == TransactionObserveStatus::ACCEPTED) {
      // A stale progress indication must not precede the interruption.
      this->find_(transaction_id)->progress_pending = false;
    }
    return status;
  }

  // Deadline of the *overall application transaction*, not of an individual
  // transport attempt. The orchestration layer may fallback before this point.
  void expire(uint32_t now_ms) {
    for (auto &slot : slots_) {
      if (!slot.occupied || slot.terminal_pending ||
          now_ms - slot.started_ms < slot.timeout_ms) continue;
      slot.terminal = {slot.transaction_id, TransactionStage::EXPIRED,
                       now_ms - slot.started_ms};
      slot.terminal_pending = true;
    }
  }

  bool take_event(TransactionEvent &event) {
    for (auto &slot : slots_) {
      if (!slot.occupied) continue;
      if (slot.progress_pending) {
        event = slot.progress;
        slot.progress_pending = false;
        return true;
      }
      if (slot.terminal_pending) {
        event = slot.terminal;
        slot = {};
        return true;
      }
    }
    return false;
  }

  bool contains(uint64_t transaction_id) const {
    return this->find_(transaction_id) != nullptr;
  }

 private:
  struct Slot {
    uint64_t transaction_id{0};
    uint64_t source_boot_id{0};
    uint32_t started_ms{0};
    uint32_t timeout_ms{0};
    TransactionEvent progress{};
    TransactionEvent terminal{};
    bool occupied{false};
    bool progress_pending{false};
    bool terminal_pending{false};
  };

  Slot *find_(uint64_t transaction_id) {
    for (auto &slot : slots_)
      if (slot.occupied && slot.transaction_id == transaction_id) return &slot;
    return nullptr;
  }
  const Slot *find_(uint64_t transaction_id) const {
    for (const auto &slot : slots_)
      if (slot.occupied && slot.transaction_id == transaction_id) return &slot;
    return nullptr;
  }
  Slot slots_[Capacity]{};
};

}  // namespace communication_net_protocol
}  // namespace esphome
