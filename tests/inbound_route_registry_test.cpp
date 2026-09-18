#include <cassert>
#include <cstring>

#include "../components/communication_net_protocol/inbound_route_registry.h"

using esphome::communication_net_protocol::InboundRouteRegistry;

int main() {
  InboundRouteRegistry<2> routes;
  assert(routes.add("first", "light/one", "toggle"));
  assert(routes.add("second", "cover/two", "open"));
  assert(routes.size() == 2);
  assert(std::strcmp(routes.find("light/one", "toggle"), "first") == 0);
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
}
