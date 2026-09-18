#include <cassert>
#include <cstring>

#include "../components/communication_net_protocol/route_admission.h"

using namespace esphome::communication_net_protocol;

int main() {
  InboundRouteRegistry<2> routes;
  assert(routes.add("configured_one", "light/one", "toggle"));
  RouteAdmission<2, 2> receiver("hub", routes);
  const char *route_id = nullptr;
  InboundCommandView mqtt{"tx", 77, 100,
                          {"hub", "light/one", "toggle", nullptr, 0}};
  assert(receiver.admit(mqtt, route_id) == InboundDecision::NEW_COMMAND);
  assert(std::strcmp(route_id, "configured_one") == 0);
  InboundCommandView radio = mqtt;  // Authenticated adapter supplies same session.
  assert(receiver.admit(radio, route_id) == InboundDecision::DUPLICATE_PENDING);
  assert(route_id == nullptr);
  const uint8_t state[] = {1};
  assert(receiver.complete(radio, {InboundTerminalStatus::SUCCEEDED,
                                   state, sizeof(state)}));
  assert(receiver.admit(mqtt, route_id) == InboundDecision::DUPLICATE_TERMINAL);
  uint8_t replay[4]{};
  size_t written = 0;
  InboundTerminalStatus status = InboundTerminalStatus::FAILED;
  assert(receiver.replay_terminal(mqtt, status, replay, sizeof(replay), written));
  assert(status == InboundTerminalStatus::SUCCEEDED && written == 1 && replay[0] == 1);

  radio.intent.device = "other";
  assert(receiver.admit(radio, route_id) == InboundDecision::INVALID);
  radio.intent.device = "hub";
  radio.intent.resource = "light/unknown";
  assert(receiver.admit(radio, route_id) == InboundDecision::INVALID);
  radio.intent.resource = "light/one";
  radio.intent.action = "turn_off";
  assert(receiver.admit(radio, route_id) == InboundDecision::INVALID);
  radio.intent.action = "toggle";
  radio.intent.arguments = state;
  radio.intent.arguments_length = 1;
  assert(receiver.admit(radio, route_id) == InboundDecision::CONFLICT);
  radio.intent.arguments = nullptr;
  radio.intent.arguments_length = 0;
  radio.source_boot_id = 78;
  assert(receiver.admit(radio, route_id) == InboundDecision::NEW_COMMAND);
  assert(receiver.clear_old_terminal_session("tx", 77) == 1);
  assert(receiver.clear_old_terminal_session("tx", 78) == 0);
}
