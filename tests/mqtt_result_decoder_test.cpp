#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "../components/communication_net_protocol/mqtt_result_decoder.h"

using namespace esphome::communication_net_protocol;

static bool decode(const char *json, MqttResultObservation &out) {
  return decode_mqtt_result(reinterpret_cast<const uint8_t *>(json),
                            std::strlen(json), out);
}

int main() {
  TransactionTracker<2> tracker;
  TransactionEvent event{};
  MqttResultObservation result{};
  assert(tracker.begin(13522938521060376637ULL, 55, 100, 30000));
  assert(decode("{\"transaction_id\":\"13522938521060376637\","
                "\"result\":\"in_progress\",\"execution\":{\"started\":true,"
                "\"estimated_completion_ms\":19000},\"remote_state\":{\"complete\":false,"
                "\"value\":{\"position\":100}}}", result));
  assert(result.stage == TransactionStage::IN_PROGRESS);
  assert(correlate_mqtt_result(tracker, result, 55, 200) ==
         TransactionObserveStatus::ACCEPTED);
  assert(decode("{\"remote_state\":{\"complete\":true,\"value\":{\"position\":100}},"
                "\"execution\":{\"started\":true},\"result\":\"succeeded\","
                "\"transaction_id\":\"13522938521060376637\"}", result));
  assert(correlate_mqtt_result(tracker, result, 56, 300) ==
         TransactionObserveStatus::STALE_SESSION);
  assert(correlate_mqtt_result(tracker, result, 55, 300) ==
         TransactionObserveStatus::ACCEPTED);
  assert(tracker.take_event(event) && event.stage == TransactionStage::IN_PROGRESS);
  assert(tracker.take_event(event) && event.stage == TransactionStage::SUCCEEDED);
  assert(!tracker.take_event(event));
  assert(correlate_mqtt_result(tracker, result, 55, 400) ==
         TransactionObserveStatus::UNKNOWN_TRANSACTION);

  assert(tracker.begin(91, 55, 1000, 30000));
  assert(decode("{\"transaction_id\":\"91\",\"result\":\"failed\","
                "\"execution\":{\"started\":true},\"error\":{\"code\":\"interrupted\","
                "\"retryable\":false,\"message\":\"Cover movement interrupted by stop\"}}",
                result));
  assert(result.stage == TransactionStage::INTERRUPTED);
  assert(correlate_mqtt_result(tracker, result, 55, 1100) ==
         TransactionObserveStatus::ACCEPTED);
  assert(tracker.take_event(event) && event.stage == TransactionStage::INTERRUPTED);

  for (const char *invalid : {
           "{\"transaction_id\":\"91\",\"result\":\"succeeded\",\"result\":\"failed\"}",
           "{\"transaction_id\":\"18446744073709551616\",\"result\":\"succeeded\"}",
           "{\"transaction_id\":\"0\",\"result\":\"succeeded\"}",
           "{\"transaction_id\":\"91\",\"result\":\"unknown\"}",
           "{\"transaction_id\":\"91\",\"result\":\"succeeded\",\"execution\":{\"started\":tru}}",
       }) {
    result.transaction_id = 999;
    assert(!decode(invalid, result));
    assert(result.transaction_id == 0);
  }
}
