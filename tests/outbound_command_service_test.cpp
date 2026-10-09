#define INTERRUPT 226  // Xtensa register macro must remain compatible.
// Native behavioral fixture. Compile/run before publishing; not executed here.
#include <cassert>
#include <cstring>
#include "../components/communication_net_protocol/outbound_command_service.h"
using namespace esphome::communication_net_protocol::outbound;

struct FakeTransport : CommunicationTransportAdapter {
  const char *source{"controller"};
  uint64_t boot{1000};
  bool active{false}, ready{false};
  CommandRequest sent;
  CommandResult result;
  CommunicationTransport transport_type() const override { return CommunicationTransport::MQTT; }
  bool is_available() const override { return !active && !ready; }
  bool is_busy() const override { return active || ready; }
  bool application_identity_matches(const char *id, uint64_t session) const override {
    return std::strcmp(id, source) == 0 && session == boot;
  }
  TransportStartStatus start(const CommandRequest &request) override {
    if (is_busy()) return TransportStartStatus::BUSY;
    sent = request;
    active = true;
    return TransportStartStatus::STARTED;
  }
  void loop(uint32_t) override {}
  bool has_result() const override { return ready; }
  bool take_result(CommandResult &out) override {
    if (!ready) return false;
    out = result;
    active = ready = false;
    return true;
  }
  bool cancel(TransactionId id) override {
    if (!active || sent.transaction_id != id) return false;
    active = ready = false;
    return true;
  }
  void complete() {
    result = {};
    result.transaction_id = sent.transaction_id;
    result.transport_used = CommunicationTransport::MQTT;
    result.transport_status = TransportStatus::SUCCEEDED;
    result.result = CommandResultStatus::SUCCEEDED;
    result.execution.started = true;
    ready = true;
  }
};
struct Observer : SubmissionObserver {
  size_t completions{0};
  SubmissionContext last;
  void on_submission_result(const SubmissionContext &context, const CommandResult &result, bool terminal) override {
    assert(context.transaction_id == result.transaction_id);
    if (terminal) { ++completions; last = context; }
  }
};
struct Fixture {
  FakeTransport first, second, interrupt;
  CommunicationTransportRegistry a, b, c;
  CommandSubmissionService service;
  Observer observer;
  Fixture() : service(a, b, c) {
    assert(a.register_transport(&first));
    assert(b.register_transport(&second));
    assert(c.register_transport(&interrupt));
    CommunicationPolicy policy;
    assert(policy.add_transport(CommunicationTransport::MQTT));
    assert(service.add_policy("default", policy));
    assert(service.add_destination("route_a", "hub_a", "default"));
    assert(service.add_destination("route_b", "hub_b", "default"));
    assert(service.configure_session("controller", 1000));
    assert(service.add_observer(&observer));
  }
};
CommandDraft draft(const char *destination, const char *device, const char *resource, const char *command = "toggle") {
  CommandDraft result;
  assert(result.destination.assign(destination));
  assert(result.target.device_id.assign(device));
  assert(result.target.resource.assign(resource));
  assert(result.command.name.assign(command));
  const uint8_t payload[]{'{', '}'};
  assert(result.command.payload.assign(payload, sizeof(payload)));
  result.timeout_ms = 1000;
  return result;
}

int main() {
  {
    Fixture f;
    const auto a = f.service.submit(draft("route_a", "hub_a", "relay/1"), 0);
    const auto b = f.service.submit(draft("route_b", "hub_b", "relay/1"), 0);
    assert(a.status == SubmissionStatus::ADMITTED && b.status == SubmissionStatus::ADMITTED);
    assert(a.transaction_id != b.transaction_id);
    assert(f.service.submit(draft("route_a", "hub_a", "relay/2"), 0).status == SubmissionStatus::BUSY);
    f.second.complete();  // Replies in reverse order must retain their targets.
    f.service.loop(1);
    assert(f.observer.completions == 1 && std::strcmp(f.observer.last.target.device_id.c_str(), "hub_b") == 0);
    f.first.complete();
    f.service.loop(2);
    assert(f.observer.completions == 2 && std::strcmp(f.observer.last.target.device_id.c_str(), "hub_a") == 0);
  }
  {
    Fixture f;
    auto moving = draft("route_a", "hub_a", "cover/blind", "open");
    assert(f.service.submit(moving, 0).status == SubmissionStatus::ADMITTED);
    assert(f.service.submit(moving, 0).status == SubmissionStatus::BUSY);
    assert(f.service.submit(draft("route_b", "hub_b", "relay/1"), 0).status == SubmissionStatus::ADMITTED);
    auto stop = moving;
    assert(stop.command.name.assign("stop"));
    stop.kind = SubmissionKind::PRIORITY_INTERRUPT;
    assert(f.service.submit(stop, 1).status == SubmissionStatus::ADMITTED);
    assert(std::strcmp(f.interrupt.sent.command.name.c_str(), "stop") == 0);
    assert(f.service.submit(stop, 1).status == SubmissionStatus::BUSY);
  }
  {
    Fixture f;
    assert(f.service.submit(draft("route_a", "hub_b", "relay/1"), 0).status == SubmissionStatus::INVALID);
    auto invalid = draft("route_a", "hub_a", "relay/1");
    const uint8_t json[]{'{', 'x', '}'};
    assert(invalid.command.payload.assign(json, sizeof(json)));
    assert(f.service.submit(invalid, 0).status == SubmissionStatus::INVALID);
    assert(!f.service.configure_session("another", 2000));
  }
  {
    Fixture f;
    f.first.boot = f.second.boot = 2000;
    assert(f.service.submit(draft("route_a", "hub_a", "relay/1"), 0).status == SubmissionStatus::UNAVAILABLE);
  }
  {
    FakeTransport adapter;
    CommunicationTransportRegistry shared, interrupt;
    assert(shared.register_transport(&adapter));
    CommandSubmissionService service(shared, shared, interrupt);
    CommunicationPolicy policy;
    assert(policy.add_transport(CommunicationTransport::MQTT));
    assert(service.configure_session("controller", 1000));
    assert(service.add_policy("default", policy));
    assert(service.add_destination("route_a", "hub_a", "default"));
    assert(service.submit(draft("route_a", "hub_a", "relay/1"), 0).status == SubmissionStatus::UNAVAILABLE);
  }
}

static_assert(INTERRUPT == 226, "Do not undefine platform register macros");
