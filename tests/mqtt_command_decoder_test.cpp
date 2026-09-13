#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "../components/communication_net_protocol/mqtt_command_decoder.h"

using esphome::communication_net_protocol::decode_mqtt_command_probe;

int main() {
  uint8_t encoded[192]{};
  size_t length = 0;
  const char *good = "{\"device\":\"quartogian_probe\",\"resource\":\"light/spot_chuveiro\",\"action\":\"toggle\"}";
  assert(decode_mqtt_command_probe(reinterpret_cast<const uint8_t *>(good),
                                   std::strlen(good), "quartogian_probe",
                                   encoded, sizeof(encoded), length));
  assert(length > 3 && encoded[0] == 1);
  const char *reordered = " { \"action\": \"toggle\", \"resource\": \"light/spot_chuveiro\", \"device\": \"quartogian_probe\" } ";
  uint8_t again[192]{};
  size_t same_length = 0;
  assert(decode_mqtt_command_probe(reinterpret_cast<const uint8_t *>(reordered),
                                   std::strlen(reordered), "quartogian_probe",
                                   again, sizeof(again), same_length));
  assert(length == same_length && std::memcmp(encoded, again, length) == 0);
  for (const char *bad : {
           "{\"probe\":true}",
           "{\"device\":\"quartogian\",\"resource\":\"light/spot_chuveiro\",\"action\":\"toggle\"}",
           "{\"device\":\"quartogian_probe\",\"device\":\"quartogian_probe\",\"action\":\"toggle\"}",
           "{\"device\":\"quartogian_probe\",\"resource\":\"x\",\"action\":\"toggle\",\"extra\":\"x\"}",
           "{\"device\":\"quartogian_probe\",\"resource\":\"x\",\"action\":\"toggle\"}garbage",
           "{\"device\":\"quartogian_probe\",\"resource\":\"x\",\"action\":\"to\\u0067gle\"}",
       }) {
    length = 123;
    assert(!decode_mqtt_command_probe(reinterpret_cast<const uint8_t *>(bad),
                                      std::strlen(bad), "quartogian_probe",
                                      encoded, sizeof(encoded), length));
    assert(length == 0);
  }
}
