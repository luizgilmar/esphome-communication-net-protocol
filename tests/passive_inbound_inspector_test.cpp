#include <cassert>
#include <cstring>

#include "../components/communication_net_protocol/passive_inbound_inspector.h"
#include "../components/communication_net_protocol/mqtt_envelope_decoder.h"

using namespace esphome::communication_net_protocol;

int main() {
  InboundRouteRegistry<2> routes;
  assert(routes.add("spot", "light/spot_chuveiro", "toggle"));
  const char *route = nullptr;
  uint8_t canonical[192]{}, mqtt_canonical[192]{};
  size_t length = 0, mqtt_length = 0;
  const uint8_t payload[] = {'{', '}'};
  assert(inspect_inbound_command(routes, "quartogian", "quartogian",
                                 "light/spot_chuveiro", "toggle", payload,
                                 sizeof(payload), route, canonical,
                                 sizeof(canonical), length));
  assert(std::strcmp(route, "spot") == 0);
  const char mqtt[] =
      "{\"reply_to\":\"tx/results/tx\",\"source\":{\"device_id\":\"tx\",\"boot_id\":\"7\"},"
      "\"transaction_id\":\"42\",\"target\":{\"device_id\":\"quartogian\","
      "\"resource\":\"light/spot_chuveiro\"},\"command\":{\"name\":\"toggle\","
      "\"payload\":{}},\"timeout_ms\":2000}";
  assert(decode_mqtt_command_envelope(
      reinterpret_cast<const uint8_t *>(mqtt), std::strlen(mqtt),
      "quartogian", mqtt_canonical, sizeof(mqtt_canonical), mqtt_length));
  assert(length == mqtt_length &&
         std::memcmp(canonical, mqtt_canonical, length) == 0);
  assert(!inspect_inbound_command(routes, "quartogian", "quartogian",
                                  "light/spot_chuveiro", "toggle",
                                  reinterpret_cast<const uint8_t *>("{\"x\":1}"),
                                  7, route, canonical, sizeof(canonical), length));
  assert(!inspect_inbound_command(routes, "quartogian", "other",
                                  "light/spot_chuveiro", "toggle", payload,
                                  sizeof(payload), route, canonical,
                                  sizeof(canonical), length));
}
