#include <cassert>

#include "../components/communication_net_protocol/inbound_command_gate.h"

using namespace esphome::communication_net_protocol;

int main() {
  InboundCommandGate<2> device_gate;
  InboundCommandView from_mqtt{};
  from_mqtt.source_id = "controller-one";
  from_mqtt.source_boot_id = 91;
  from_mqtt.transaction_id = 12;
  from_mqtt.intent = {"device-a", "relay/1", "toggle", nullptr, 0};
  assert(device_gate.admit(from_mqtt) == InboundDecision::NEW_COMMAND);

  // The radio adapter can build the same semantic view from the frame
  // envelope and its authenticated peer, without sharing any TX/HUB types.
  InboundCommandView from_radio = from_mqtt;
  assert(device_gate.admit(from_radio) == InboundDecision::DUPLICATE_PENDING);
  const uint8_t state[] = {1, 0, 4};
  assert(device_gate.complete(from_radio, {InboundTerminalStatus::SUCCEEDED,
                                           state, sizeof(state)}));
  assert(device_gate.admit(from_mqtt) == InboundDecision::DUPLICATE_TERMINAL);
  uint8_t replay[8]{};
  size_t length = 0;
  InboundTerminalStatus status = InboundTerminalStatus::FAILED;
  assert(device_gate.replay_terminal(from_mqtt, status, replay,
                                     sizeof(replay), length));
  assert(status == InboundTerminalStatus::SUCCEEDED &&
         length == sizeof(state) && replay[2] == 4);
  from_radio.intent.action = "turn_off";
  assert(device_gate.admit(from_radio) == InboundDecision::CONFLICT);
  assert(!device_gate.complete(from_radio, {InboundTerminalStatus::FAILED,
                                            nullptr, 0}));
  assert(!device_gate.replay_terminal(from_radio, status, replay,
                                      sizeof(replay), length));
  from_radio.source_boot_id = 92;
  assert(device_gate.admit(from_radio) == InboundDecision::NEW_COMMAND);
  assert(device_gate.clear_old_terminal_session("controller-one", 91) == 1);
  assert(device_gate.clear_old_terminal_session("controller-one", 92) == 0);
  return 0;
}
