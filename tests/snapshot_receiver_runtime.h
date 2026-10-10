#pragma once
#include <cstring>
#include "esphome/components/tx_ultimate/tx_ultimate.h"

namespace snapshot_receiver_test {
using namespace esphome::tx_ultimate;

// Test data, not an alternative production decoder or transport implementation.
inline bool make_frame(StateSnapshot &snapshot, unsigned count, unsigned fault) {
  const char *names[] = {"spot", "strips", "panel1", "panel2", "general",
                         "superior", "other1", "other2", "other3"};
  uint8_t data[256]{};
  size_t used = 10;
  data[0] = 2;
  data[1] = 1;  // generation 1, little endian
  data[5] = 1;  // revision 1
  data[9] = static_cast<uint8_t>(count);
  for (unsigned i = 0; i < count; i++) {
    const char *name = names[i];
    if (fault == 1 && i == 3) name = "unowned";  // missing panel2
    if (fault == 2 && i == 4) name = "spot";     // duplicate mapped field
    const size_t length = std::strlen(name);
    if (used + length + 6 > sizeof(data)) return false;
    data[used++] = static_cast<uint8_t>(length);
    std::memcpy(data + used, name, length);
    used += length;
    data[used++] = i == 1 ? 0x07 : i == 3 ? 0x03 : 0x01;
    data[used++] = 255;
    data[used++] = 0;
    data[used++] = 0;
    data[used++] = 100;
  }
  if (fault == 3) used--;          // truncated final field
  if (fault == 4) data[used++] = 0; // trailing data
  snapshot = {};
  snapshot.completeness = RemoteStateCompleteness::COMPLETE;
  return snapshot.schema.assign("state-fields/v2") && snapshot.data.assign(data, used);
}

inline bool check_case(TXUltimateComponent &component, unsigned count,
                       unsigned fault, bool expected_success) {
  auto &ambient = *component.get_ambient_state_indicator();
  PublicSceneStateRegistry scene_states;
  RetainedStateObserver observer(ambient, scene_states);
  if (!observer.add_source("bench", "bench_producer", "bench_policy",
                           "validation/state", 1, true, 0)) return false;
  const char *fields[] = {"spot", "strips", "panel1", "panel2"};
  for (uint8_t i = 0; i < 4; i++) {
    ambient.set_binary(i + 1, true, true);  // detect partial writes on rejection
    if (!observer.add_mapping(fields[i], i + 1,
           i == 1 ? RetainedStateRendering::RGB : RetainedStateRendering::BINARY,
           false, nullptr, nullptr, nullptr, "bench_producer")) return false;
  }
  CommandRequest request;
  if (!observer.prepare_snapshot_query(100000, 42, request)) return false;
  CommandResult result;
  result.transaction_id = request.transaction_id;
  result.result = CommandResultStatus::SUCCEEDED;
  if (!make_frame(result.remote_state, count, fault) ||
      !observer.handle_snapshot_result(result)) return false;
  if (observer.required_sources_synchronized() != expected_success ||
      observer.mapping_count() != 4) return false;
  const AmbientIndicatorState expected[] = {AmbientIndicatorState::OFF,
      AmbientIndicatorState::RGB, AmbientIndicatorState::OFF, AmbientIndicatorState::ON};
  for (uint8_t i = 0; i < 4; i++) {
    if (ambient.state(i + 1) != (expected_success ? expected[i] : AmbientIndicatorState::ON))
      return false;
  }
  return true;
}

inline void run(TXUltimateComponent &component) {
  const struct { const char *name; unsigned count, fault; bool success; } cases[] = {
      {"legacy_four", 4, 0, true}, {"six_fields", 6, 0, true},
      {"eight_fields", 8, 0, true}, {"missing_owned", 6, 1, false},
      {"duplicate_owned", 6, 2, false}, {"truncated", 6, 3, false},
      {"trailing_data", 6, 4, false}, {"over_count", 9, 0, false}};
  unsigned passed = 0;
  ESP_LOGI("snapshot.test", "START cases=8 transport=injected receiver=real_tx");
  for (const auto &test : cases) {
    const bool ok = check_case(component, test.count, test.fault, test.success);
    if (ok) passed++;
    ESP_LOGI("snapshot.test", "CASE name=%s verdict=%s", test.name, ok ? "PASS" : "FAIL");
  }
  ESP_LOGI("snapshot.test", "END passed=%u total=8", passed);
}
}  // namespace snapshot_receiver_test
