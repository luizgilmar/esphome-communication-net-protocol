#include <cassert>
#include "state_traffic_scheduler.h"
using namespace esphome::communication_net_protocol;
int main() {
  StateTrafficLimits limits{10, 20, 100, 800, 10};
  StateTrafficScheduler<2, 2> s(limits);
  StateTrafficScheduler<2, 2>::Ticket a, b;
  assert(!s.mark_latest(0, 0));
  assert(s.set_enabled(0, true) && s.set_enabled(1, true));
  assert(s.mark_latest(0, 0) && s.mark_latest(1, 0));
  assert(!s.take(0, false, a));
  assert(s.take(0, true, a) && a.peer == 0);
  assert(!s.take(100, true, b)); // one operation in flight
  assert(s.mark_latest(0, 0)); // update while older snapshot is in flight
  assert(s.complete(a, true, 0, 0));
  assert(!s.complete(a, true, 0, 0));
  assert(!s.take(9, true, b));
  assert(s.take(10, true, b) && b.peer == 1);
  assert(s.complete(b, false, 10, 0));
  assert(s.take(20, true, a) && a.peer == 0); // newer update was preserved
  assert(s.complete(a, true, 20, 0));
  assert(!s.take(109, true, a));
  assert(s.take(110, true, a) && a.peer == 1);
  assert(s.complete(a, true, 110, 0));
  assert(!s.take(200, true, a));
  StateRetryWindow retry(limits);
  retry.defer(0xfffffff0U, 0);
  retry.defer(0xfffffff1U, 0); // repeated error does not postpone deadline
  assert(!retry.ready(83));
  assert(retry.ready(84)); // millis wrap
  retry.defer(84, 0);
  assert(!retry.ready(283) && retry.ready(284));
  retry.recovered();
  assert(retry.ready(284));
  StateTrafficLimits invalid{0, 20, 100, 800, 10};
  assert(!invalid.valid());
}
