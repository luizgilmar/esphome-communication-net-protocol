#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NSPANEL_SNAPSHOT_MQTT
#include <cstring>
#include <string>
#include "esphome/components/json/json_util.h"
#include "state_fields_decoder.h"
#include "snapshot_limits.h"
#include "outbound_communication_transaction.h"

namespace esphome::communication_net_protocol {
// MQTT field payload published by LightStateSnapshot; bounded before JSON allocation.
inline bool decode_state_fields_mqtt(const uint8_t *payload, size_t size, outbound::StateSnapshot &output) {
  if (payload == nullptr || size == 0 || size > MAX_SNAPSHOT_JSON_BYTES) return false;
  return json::parse_json(payload, size, [&output](JsonObject root) -> bool {
    JsonObjectConst metadata = root["_meta"].as<JsonObjectConst>();
    if (metadata.isNull() || metadata.size() != 3 || !metadata["version"].is<uint32_t>() || metadata["version"].as<uint32_t>() != 2 ||
        !metadata["generation"].is<uint32_t>() || !metadata["revision"].is<uint32_t>() || root.size() < 2 || root.size() > 9) return false;
    const uint32_t generation = metadata["generation"].as<uint32_t>(), revision = metadata["revision"].as<uint32_t>();
    if (generation == 0 || revision == 0) return false;
    uint8_t frame[256]{}; frame[0] = 2;
    for (size_t i = 0; i < 4; ++i) { frame[1+i] = uint8_t(generation >> (8*i)); frame[5+i] = uint8_t(revision >> (8*i)); }
    size_t used = 10;
    JsonObjectConst object = root;
    for (JsonPairConst entry : object) {
      const char *name = entry.key().c_str();
      const size_t length = entry.key().size();
      if (length == 5 && std::memcmp(name, "_meta", 5) == 0) continue;
      JsonObjectConst value = entry.value().as<JsonObjectConst>();
      if (length == 0 || length > 31 || used + length + 6 > sizeof(frame) || value.isNull() ||
          !value["known"].is<bool>() || !value["on"].is<bool>()) return false;
      const bool rgb = value.containsKey("red") || value.containsKey("green") || value.containsKey("blue") || value.containsKey("brightness");
      if (value.size() != (rgb ? 7U : 2U)) return false;
      if (rgb && (!value["red"].is<uint8_t>() || !value["green"].is<uint8_t>() || !value["blue"].is<uint8_t>() ||
                  !value["brightness"].is<uint8_t>() || !value["effect_active"].is<bool>())) return false;
      frame[used++] = length;
      std::memcpy(frame + used, name, length); used += length;
      frame[used++] = (value["known"].as<bool>() ? 1 : 0) | (value["on"].as<bool>() ? 2 : 0) | (rgb ? 4 : 0) | (rgb && value["effect_active"].as<bool>() ? 8 : 0);
      frame[used++] = rgb ? value["red"].as<uint8_t>() : 0; frame[used++] = rgb ? value["green"].as<uint8_t>() : 0;
      frame[used++] = rgb ? value["blue"].as<uint8_t>() : 0; frame[used++] = rgb ? value["brightness"].as<uint8_t>() : 0;
      ++frame[9];
    }
    ObservedLightSnapshot validated;
    if (!decode_state_fields_v2(frame, used, validated)) return false;
    outbound::StateSnapshot parsed;
    parsed.completeness = outbound::RemoteStateCompleteness::COMPLETE;
    if (!parsed.schema.assign("state-fields/v2") || !parsed.data.assign(frame, used)) return false;
    output = parsed; return true;
  });
}
}  // namespace esphome::communication_net_protocol
#endif
