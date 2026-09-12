#include <cassert>

#include "../components/communication_net_protocol/transaction_tracker.h"

using esphome::communication_net_protocol::TransactionEvent;
using esphome::communication_net_protocol::TransactionObserveStatus;
using esphome::communication_net_protocol::TransactionStage;
using esphome::communication_net_protocol::TransactionTracker;

int main() {
  TransactionTracker<1> tracker;
  TransactionEvent event{};
  assert(!tracker.begin(0, 9, 100, 200));
  assert(!tracker.begin(7, 0, 100, 200));
  assert(tracker.begin(7, 9, 100, 200));
  assert(!tracker.begin(7, 9, 100, 200));
  assert(!tracker.begin(8, 9, 100, 200));
  assert(tracker.observe(7, 10, TransactionStage::SUCCEEDED, 120) ==
         TransactionObserveStatus::STALE_SESSION);
  assert(tracker.observe(8, 9, TransactionStage::SUCCEEDED, 120) ==
         TransactionObserveStatus::UNKNOWN_TRANSACTION);
  assert(tracker.observe(7, 9, TransactionStage::IN_PROGRESS, 130) ==
         TransactionObserveStatus::ACCEPTED);
  assert(tracker.observe(7, 9, TransactionStage::IN_PROGRESS, 140) ==
         TransactionObserveStatus::ACCEPTED);
  assert(tracker.observe(7, 9, TransactionStage::SUCCEEDED, 150) ==
         TransactionObserveStatus::ACCEPTED);
  assert(tracker.observe(7, 9, TransactionStage::FAILED, 160) ==
         TransactionObserveStatus::ALREADY_TERMINAL);
  tracker.expire(300);
  assert(tracker.take_event(event) && event.stage == TransactionStage::IN_PROGRESS &&
         event.elapsed_ms == 40);
  assert(tracker.take_event(event) && event.stage == TransactionStage::SUCCEEDED &&
         event.elapsed_ms == 50);
  assert(!tracker.take_event(event) && !tracker.contains(7));

  // Deadline is overall: preserve the transaction while an individual
  // transport attempt fails and a fallback may still execute.
  assert(tracker.begin(8, 9, 0xFFFFFFF0u, 32));
  tracker.expire(0x00000005u);
  assert(!tracker.take_event(event));
  tracker.expire(0x00000020u);
  assert(tracker.take_event(event) && event.stage == TransactionStage::EXPIRED);
  assert(!tracker.contains(8));
  return 0;
}
