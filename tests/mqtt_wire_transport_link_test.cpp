#include <cassert>
#include <cstring>

#define USE_MQTT
#include "../components/communication_net_protocol/mqtt_wire_transport.h"

int main() {
  esphome::mqtt::MQTTClientComponent client;
  esphome::mqtt::global_mqtt_client = &client;
  esphome::communication_net_protocol::MqttWireTransport wire;
  assert(wire.subscribe("commands/device/command"));
  assert(client.subscribed_topic == "commands/device/command");
  assert(client.on_message);
  client.on_message("commands/device/command", "{\"command\":1}");
  char topic[193]{};
  uint8_t payload[1280]{};
  size_t length = 0;
  assert(wire.take_received(topic, sizeof(topic), payload, sizeof(payload), length));
  assert(std::strcmp(topic, "commands/device/command") == 0);
  assert(length == 13);
  assert(wire.publish(topic, payload, length));
  client.connected = false;
  assert(!wire.available());
  assert(!wire.publish(topic, payload, length));
  return 0;
}
