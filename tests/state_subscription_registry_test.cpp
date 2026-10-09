#include <cassert>
#include "state_subscription_registry.h"
using namespace esphome::communication_net_protocol;
int main() {
  using R = StateSubscriptionResult;
  StateSubscriptionRegistry<2> s(100);
  assert(s.configure_peer(0, 3));
  assert(!s.configure_peer(0, 7));
  assert(s.establish_session(0, 42));
  assert(s.apply(0, 42, 1, 4, true, 0) == R::UNAUTHORIZED);
  assert(s.apply(0, 41, 1, 1, true, 0) == R::UNAUTHORIZED);
  assert(s.apply(0, 42, 1, 3, true, 0xfffffff0U) == R::APPLIED);
  assert(s.active_resources(0, 83) == 3);
  assert(s.apply(0, 42, 1, 3, true, 83) == R::DUPLICATE);
  assert(s.active_resources(0, 84) == 0); // duplicate did not renew
  s.expire(84);
  assert(s.establish_session(0, 42)); // same session preserves ordering
  assert(s.apply(0, 42, 2, 3, true, 90) == R::APPLIED);
  assert(s.apply(0, 42, 1, 0, false, 91) == R::STALE);
  assert(s.active_resources(0, 91) == 3);
  assert(s.apply(0, 42, 2, 0, false, 91) == R::INVALID);
  assert(s.apply(0, 42, 3, 0, false, 92) == R::APPLIED);
  assert(s.active_resources(0, 92) == 0);
  assert(s.establish_session(0, 43));
  assert(s.apply(0, 42, 4, 3, true, 93) == R::UNAUTHORIZED);
  assert(s.apply(0, 43, 1, 1, true, 93) == R::APPLIED);
  assert(s.active_resources(1, 93) == 0);
}
