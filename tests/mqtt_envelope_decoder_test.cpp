#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "../components/communication_net_protocol/mqtt_envelope_decoder.h"

using esphome::communication_net_protocol::decode_mqtt_command_envelope;

static bool decode(const char *json, uint8_t *bytes, size_t &length) {
  return decode_mqtt_command_envelope(
      reinterpret_cast<const uint8_t *>(json), std::strlen(json),
      "quartogian", bytes, 192, length);
}

int main() {
  const char *open = "{\"transaction_id\":\"13522938521060376637\","
                     "\"reply_to\":\"tx/results/tx-quartogian-integration\","
                     "\"source\":{\"device_id\":\"tx-quartogian-integration\","
                     "\"boot_id\":\"1234\"},"
                     "\"target\":{\"device_id\":\"quartogian\","
                     "\"resource\":\"cover/persiana\"},"
                     "\"command\":{\"name\":\"open\"}}";
  const char *reordered = " { \"command\":{\"name\":\"open\"},"
                          "\"target\":{\"resource\":\"cover/persiana\","
                          "\"device_id\":\"quartogian\"},"
                          "\"source\":{\"boot_id\":\"1234\","
                          "\"device_id\":\"tx-quartogian-integration\"},"
                          "\"reply_to\":\"tx/results/tx-quartogian-integration\","
                          "\"transaction_id\":\"13522938521060376637\" } ";
  uint8_t first[192]{}, second[192]{};
  size_t length = 0, second_length = 0;
  assert(decode(open, first, length));
  assert(decode(reordered, second, second_length));
  assert(length == second_length && std::memcmp(first, second, length) == 0);

  const char *wrong_target = "{\"transaction_id\":\"1\",\"reply_to\":\"tx/results/tx\","
                             "\"source\":{\"device_id\":\"tx\",\"boot_id\":\"2\"},"
                             "\"target\":{\"device_id\":\"another\",\"resource\":\"x\"},"
                             "\"command\":{\"name\":\"toggle\"}}";
  const char *with_arguments = "{\"transaction_id\":\"1\",\"reply_to\":\"tx/results/tx\","
                               "\"source\":{\"device_id\":\"tx\",\"boot_id\":\"2\"},"
                               "\"target\":{\"device_id\":\"quartogian\",\"resource\":\"x\"},"
                               "\"command\":{\"name\":\"toggle\",\"args\":{}}}";
  const char *duplicate_id = "{\"transaction_id\":\"1\",\"transaction_id\":\"2\","
                             "\"reply_to\":\"tx/results/tx\","
                             "\"source\":{\"device_id\":\"tx\",\"boot_id\":\"2\"},"
                             "\"target\":{\"device_id\":\"quartogian\",\"resource\":\"x\"},"
                             "\"command\":{\"name\":\"toggle\"}}";
  const char *bad_topic = "{\"transaction_id\":\"1\",\"reply_to\":\"tx/#\","
                          "\"source\":{\"device_id\":\"tx\",\"boot_id\":\"2\"},"
                          "\"target\":{\"device_id\":\"quartogian\",\"resource\":\"x\"},"
                          "\"command\":{\"name\":\"toggle\"}}";
  const char *overflow_id = "{\"transaction_id\":\"18446744073709551616\","
                            "\"reply_to\":\"tx/results/tx\","
                            "\"source\":{\"device_id\":\"tx\",\"boot_id\":\"2\"},"
                            "\"target\":{\"device_id\":\"quartogian\",\"resource\":\"x\"},"
                            "\"command\":{\"name\":\"toggle\"}}";
  for (const char *invalid : {wrong_target, with_arguments, duplicate_id,
                              bad_topic, overflow_id}) {
    length = 42;
    assert(!decode(invalid, first, length));
    assert(length == 0);
  }
  length = 42;
  assert(!decode_mqtt_command_envelope(
      reinterpret_cast<const uint8_t *>(open), std::strlen(open) - 1,
      "quartogian", first, sizeof(first), length));
  assert(length == 0);
}
