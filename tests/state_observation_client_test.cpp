#include "../components/communication_net_protocol/state_observation_client.h"
#include <cassert>
using Client = esphome::communication_net_protocol::StateObservationClient;
using Op = Client::Operation;
int main() {
  Client c;
  assert(!c.configure(1000,45000));
  assert(!c.configure(15000,20000));
  assert(c.configure(15000,45000));
  c.connection(true);
  assert(c.due(0)==Op::MQTT_QUERY);
  assert(c.begin(Op::MQTT_QUERY,0));
  assert(c.due(1)==Op::NONE); // one pending transaction
  c.complete(true,1,0);
  assert(c.mqtt_healthy(45000));
  assert(!c.mqtt_healthy(45001)); // connected silence must fall back
  assert(c.due(45001)==Op::HELLO);
  assert(c.begin(Op::HELLO,45001));
  c.complete(true,45002,0,123,60000);
  assert(c.due(45003)==Op::LEASE);
  assert(c.begin(Op::LEASE,45003)); c.complete(true,45004,0);
  assert(c.active());
  assert(c.due(45005)==Op::MQTT_QUERY);
  assert(c.begin(Op::MQTT_QUERY,45005)); c.complete(true,45006,0);
  assert(c.mqtt_healthy(45006));
  assert(c.due(46004)==Op::STOP);
  assert(c.begin(Op::STOP,46004)); c.complete(false,46005,0);
  assert(!c.active()); // lost stop reply: expiry, no renewal while fresh
  assert(c.due(49000)==Op::NONE);

  Client disconnect;
  disconnect.configure(15000,45000); disconnect.connection(true);
  assert(disconnect.begin(Op::MQTT_QUERY,1));
  disconnect.connection(false); disconnect.connection(true);
  disconnect.complete(true,2,0);
  assert(!disconnect.mqtt_healthy(2)); // old session cannot prove recovery

  Client retry;
  retry.configure(15000,45000); retry.connection(false);
  for (unsigned i=0,now=1,delay=2000; i<6; ++i) {
    assert(retry.begin(Op::HELLO,now)); retry.complete(false,now,0);
    assert(retry.due(now+delay-1)==Op::NONE);
    assert(retry.due(now+delay)==Op::HELLO);
    now+=delay;
    delay=delay<16000 ? delay*2 : 30000;
  }

  Client nonce;
  nonce.configure(15000,45000); nonce.connection(false);
  assert(nonce.begin(Op::HELLO,1)); nonce.complete(true,2,0,456,60000);
  assert(nonce.due(10002)==Op::HELLO); // expired activation challenge

  Client renewal;
  renewal.configure(15000,45000); renewal.connection(false);
  assert(renewal.begin(Op::HELLO,1)); renewal.complete(true,2,0,789,60000);
  assert(renewal.begin(Op::LEASE,3)); renewal.complete(true,4,0);
  assert(renewal.due(20003)==Op::NONE);
  assert(renewal.begin(Op::LEASE,20004)); renewal.complete(false,20005,0);
  assert(renewal.nonce()==0); // publisher restart must get a new challenge
  assert(renewal.due(22005)==Op::HELLO);

  Client wrap;
  wrap.configure(15000,45000); wrap.connection(true);
  assert(wrap.begin(Op::MQTT_QUERY,0xfffffff0U)); wrap.complete(true,0xfffffff1U,0);
  assert(wrap.mqtt_healthy(1000));
  assert(!wrap.mqtt_healthy(45000));
  return 0;
}
