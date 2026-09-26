#include <cassert>
#include <cstring>

#include "../components/communication_net_protocol/inbound_route_registry.h"

using esphome::communication_net_protocol::InboundRouteRegistry;

int main() {
  InboundRouteRegistry<2> routes;
  static const char first_id[] = "first";
  assert(routes.add(first_id, "light/one", "toggle"));
  assert(routes.add("second", "cover/two", "open"));
  assert(routes.size() == 2);
  assert(std::strcmp(routes.find("light/one", "toggle"), "first") == 0);
  assert(routes.find("light/one", "toggle") == first_id);
  assert(routes.find("light/one", "open") == nullptr);
  assert(routes.find(nullptr, "toggle") == nullptr);
  assert(!routes.add("third", "light/three", "toggle"));

  InboundRouteRegistry<3> distinct;
  assert(distinct.add("one", "light/one", "toggle"));
  assert(!distinct.add("two", "light/one", "toggle"));
  assert(!distinct.add("one", "light/two", "toggle"));
  assert(!distinct.add("", "light/two", "toggle"));
  assert(!distinct.add("two", "", "toggle"));
  assert(!distinct.add("two", "light/two", nullptr));
  assert(!distinct.add("two", "light/two", "a-command-name-that-exceeds-the-sixty-three-character-boundary-by-a-lot"));
  assert(distinct.size() == 1);

  // The live HUB has 18 bindings after the six brightness routes are added.
  InboundRouteRegistry<20> expanded;
  const char *commands[] = {"toggle", "off", "activate", "deactivate",
                            "warm", "red", "green", "blue", "yellow", "cyan",
                            "magenta", "br12", "br18", "br27", "br40", "br65",
                            "br100", "rainbow", "effect", "reset"};
  for (const char *command : commands)
    assert(expanded.add(command, "light/group", command));
  assert(expanded.size() == 20);
  assert(std::strcmp(expanded.find("light/group", "br100"), "br100") == 0);
  assert(!expanded.add("overflow", "light/group", "overflow"));

  static_assert(sizeof(InboundRouteRegistry<1>) <= 32,
                "route declarations must store references, not text copies");
}
