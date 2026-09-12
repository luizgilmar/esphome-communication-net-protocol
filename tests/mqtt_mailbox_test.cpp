#include <cassert>
#include <cstdint>
#include <cstring>

#include "../components/communication_net_protocol/mqtt_mailbox.h"

using esphome::communication_net_protocol::MqttMailbox;

int main() {
  MqttMailbox mailbox;
  const uint8_t first[] = {0, 0xFF, 42};
  assert(mailbox.put("tx/results/node", 15, first, sizeof(first)));
  assert(mailbox.occupied());
  assert(!mailbox.put("other", 5, first, sizeof(first)));
  assert(mailbox.dropped() == 1);
  char topic[MqttMailbox::MAX_TOPIC + 1]{};
  uint8_t payload[MqttMailbox::MAX_PAYLOAD]{};
  size_t length = 0;
  assert(!mailbox.take(topic, sizeof(topic), payload, 1, length));
  assert(mailbox.occupied());
  assert(mailbox.take(topic, sizeof(topic), payload, sizeof(payload), length));
  assert(std::strcmp(topic, "tx/results/node") == 0);
  assert(length == sizeof(first) && std::memcmp(payload, first, length) == 0);
  assert(!mailbox.occupied());
  assert(!mailbox.put("too big", 7, first, MqttMailbox::MAX_PAYLOAD + 1));
  assert(mailbox.dropped() == 2);
  return 0;
}
