#include <cassert>

#include "../components/communication_net_protocol/inbound_replay_guard.h"

using esphome::communication_net_protocol::InboundDecision;
using esphome::communication_net_protocol::InboundReplayGuard;
using esphome::communication_net_protocol::InboundTerminalStatus;

int main() {
  InboundReplayGuard<2, 16, 8> guard;
  static_assert(sizeof(InboundReplayGuard<2, 16, 8>) <= 720,
                "entries must reference the bounded session table");
  const uint8_t toggle_on[] = {1, 0, 2};
  const uint8_t toggle_off[] = {1, 0, 3};
  assert(guard.begin(nullptr, 1, 10, toggle_on, sizeof(toggle_on)) == InboundDecision::INVALID);
  assert(guard.begin("sender", 0, 10, toggle_on, sizeof(toggle_on)) == InboundDecision::INVALID);
  assert(guard.begin("sender", 1, 0, toggle_on, sizeof(toggle_on)) == InboundDecision::INVALID);
  assert(guard.begin("sender", 1, 10, nullptr, sizeof(toggle_on)) == InboundDecision::INVALID);
  assert(guard.begin("sender", 1, 10, toggle_on, 0) == InboundDecision::INVALID);
  assert(guard.begin("sender", 1, 10, toggle_on, 9) == InboundDecision::INVALID);
  assert(guard.begin("sender-id-too-long", 1, 10, toggle_on, sizeof(toggle_on)) == InboundDecision::INVALID);

  // Simulate the same toggle received by MQTT then by ESP-NOW fallback.
  assert(guard.begin("sender", 1, 10, toggle_on, sizeof(toggle_on)) == InboundDecision::NEW_COMMAND);
  assert(guard.begin("sender", 1, 10, toggle_on, sizeof(toggle_on)) == InboundDecision::DUPLICATE_PENDING);
  assert(guard.begin("sender", 1, 10, toggle_off, sizeof(toggle_off)) == InboundDecision::CONFLICT);
  assert(guard.begin("sender", 1, 10, toggle_on, 2) == InboundDecision::CONFLICT);
  const uint8_t confirmed[] = {0, 19, 255};
  assert(!guard.finish("sender", 1, 10, toggle_off, sizeof(toggle_off),
                       {InboundTerminalStatus::SUCCEEDED, nullptr, 0}));
  assert(guard.finish("sender", 1, 10, toggle_on, sizeof(toggle_on), {InboundTerminalStatus::SUCCEEDED,
                                       confirmed, sizeof(confirmed)}));
  assert(!guard.finish("sender", 1, 10, toggle_on, sizeof(toggle_on),
                       {InboundTerminalStatus::FAILED, nullptr, 0}));
  assert(guard.begin("sender", 1, 10, toggle_on, sizeof(toggle_on)) == InboundDecision::DUPLICATE_TERMINAL);
  InboundTerminalStatus status = InboundTerminalStatus::FAILED;
  uint8_t replay[3]{};
  size_t replay_length = 0;
  assert(!guard.get_terminal("sender", 1, 10, toggle_off, sizeof(toggle_off),
                             status, replay, sizeof(replay), replay_length));
  assert(!guard.get_terminal("sender", 1, 10, toggle_on, sizeof(toggle_on),
                             status, replay, 2, replay_length));
  assert(guard.get_terminal("sender", 1, 10, toggle_on, sizeof(toggle_on),
                            status, replay, sizeof(replay), replay_length));
  assert(status == InboundTerminalStatus::SUCCEEDED &&
         replay_length == sizeof(confirmed) && replay[2] == 255);
  assert(!guard.finish("other", 1, 10, toggle_on, sizeof(toggle_on),
                       {InboundTerminalStatus::FAILED, nullptr, 0}));

  assert(guard.begin("sender", 2, 10, toggle_on, sizeof(toggle_on)) == InboundDecision::NEW_COMMAND);
  assert(guard.begin("other", 1, 11, toggle_on, sizeof(toggle_on)) == InboundDecision::FULL);
  assert(guard.clear_source_session("sender", 1) == 1);
  assert(guard.clear_source_session("sender", 2) == 0);
  // Old session remains a tombstone; accepting it again is the adapter's job
  // only after validating a genuinely new boot session.
  assert(guard.begin("other", 1, 11, toggle_on, sizeof(toggle_on)) == InboundDecision::FULL);
  assert(guard.begin("sender", 1, 10, toggle_on, sizeof(toggle_on)) == InboundDecision::RETIRED);

  InboundReplayGuard<2, 16, 8> rotating;
  for (uint64_t transaction = 1; transaction <= 25; ++transaction) {
    assert(rotating.begin("sender", 17, transaction, toggle_on,
                          sizeof(toggle_on)) == InboundDecision::NEW_COMMAND);
    assert(rotating.finish("sender", 17, transaction, toggle_on,
                           sizeof(toggle_on),
                           {InboundTerminalStatus::SUCCEEDED, confirmed,
                            sizeof(confirmed)}));
  }
  assert(rotating.begin("sender", 17, 1, toggle_on,
                        sizeof(toggle_on)) == InboundDecision::RETIRED);
  assert(rotating.begin("sender", 17, 25, toggle_on,
                        sizeof(toggle_on)) == InboundDecision::DUPLICATE_TERMINAL);
  assert(rotating.begin("sender", 17, 26, toggle_on,
                        sizeof(toggle_on)) == InboundDecision::NEW_COMMAND);
  assert(rotating.begin("sender", 17, 20, toggle_on,
                        sizeof(toggle_on)) == InboundDecision::RETIRED);

  InboundReplayGuard<2, 16, 8> pending;
  assert(pending.begin("sender", 17, 1, toggle_on,
                       sizeof(toggle_on)) == InboundDecision::NEW_COMMAND);
  assert(pending.begin("sender", 17, 2, toggle_on,
                       sizeof(toggle_on)) == InboundDecision::NEW_COMMAND);
  assert(pending.begin("sender", 17, 3, toggle_on,
                       sizeof(toggle_on)) == InboundDecision::FULL);
  assert(pending.begin("sender", 17, 1, toggle_on,
                       sizeof(toggle_on)) == InboundDecision::DUPLICATE_PENDING);
  return 0;
}
