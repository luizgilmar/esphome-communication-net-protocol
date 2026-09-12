#include <cassert>
#include <cstring>

#include "../components/communication_net_protocol/canonical_command.h"
#include "../components/communication_net_protocol/inbound_replay_guard.h"

using namespace esphome::communication_net_protocol;

int main() {
  uint8_t from_mqtt[192]{};
  uint8_t from_espnow[192]{};
  size_t mqtt_length = 0;
  size_t espnow_length = 0;
  CanonicalCommandView command{"quartogian", "light/spot_chuveiro", "toggle", nullptr, 0};
  assert(encode_canonical_command(command, from_mqtt, sizeof(from_mqtt), mqtt_length));
  assert(encode_canonical_command(command, from_espnow, sizeof(from_espnow), espnow_length));
  assert(mqtt_length == espnow_length);
  assert(std::memcmp(from_mqtt, from_espnow, mqtt_length) == 0);
  InboundReplayGuard<1> guard;
  assert(guard.begin("tx", 3, 100, from_mqtt, mqtt_length) == InboundDecision::NEW_COMMAND);
  assert(guard.begin("tx", 3, 100, from_espnow, espnow_length) == InboundDecision::DUPLICATE_PENDING);

  command.action = "toggle/other";
  assert(encode_canonical_command(command, from_espnow, sizeof(from_espnow), espnow_length));
  assert(guard.begin("tx", 3, 100, from_espnow, espnow_length) == InboundDecision::CONFLICT);
  assert(!encode_canonical_command(command, from_espnow, 2, espnow_length));
  assert(espnow_length == 0);
  command.action = "";
  assert(!encode_canonical_command(command, from_espnow, sizeof(from_espnow), espnow_length));
  return 0;
}
