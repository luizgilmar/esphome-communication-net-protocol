#include <cassert>
#include <cstdio>
#include "components/communication_net_protocol/outbound_communication_transaction_runner.h"
#include "components/communication_net_protocol/inbound_replay_guard.h"
using namespace esphome::communication_net_protocol::outbound;

struct Link : CommunicationTransportAdapter {
  CommunicationTransport type;
  bool busy=false, ready=false, available=true;
  unsigned sends=0, cancels=0;
  CommandRequest request{};
  CommandResult response{};
  explicit Link(CommunicationTransport t):type(t){}
  CommunicationTransport transport_type() const override { return type; }
  bool is_available() const override { return available; }
  bool is_busy() const override { return busy; }
  TransportStartStatus start(const CommandRequest &r) override {
    if(busy) return TransportStartStatus::BUSY;
    request=r; busy=true; ++sends; return TransportStartStatus::STARTED;
  }
  void loop(uint32_t) override {}
  bool has_result() const override { return ready; }
  bool take_result(CommandResult &r) override {
    if(!ready) return false;
    r=response; ready=false;
    if(r.result!=CommandResultStatus::IN_PROGRESS) busy=false;
    return true;
  }
  bool cancel(TransactionId id) override {
    if(id!=request.transaction_id) return false;
    busy=ready=false; ++cancels; return true;
  }
  void reply(CommandResultStatus status, uint32_t estimate=5000) {
    response={}; response.transaction_id=request.transaction_id;
    response.transport_used=type; response.transport_status=TransportStatus::SUCCEEDED;
    response.result=status; response.execution.started=true;
    if(status==CommandResultStatus::IN_PROGRESS) {
      response.execution.has_estimated_completion=true;
      response.execution.estimated_completion_ms=estimate;
    }
    ready=true;
  }
  void fail() {
    reply(CommandResultStatus::FAILED);
    response.execution.started=false;
    response.transport_status=TransportStatus::CONNECTION_FAILED;
    response.error.code=CommunicationErrorCode::CONNECTION_FAILED;
    response.error.message.assign("lost link");
  }
};
struct Fixture {
  Link mqtt{CommunicationTransport::MQTT}, radio{CommunicationTransport::ESP_NOW};
  CommunicationTransportRegistry registry;
  CommunicationTransactionRunner runner{registry};
  Fixture(uint32_t start=0) {
    registry.register_transport(&mqtt); registry.register_transport(&radio);
    CommandRequest req; req.transaction_id=123; req.timeout_ms=2000;
    req.target.device_id.assign("hub"); req.target.resource.assign("light/spot");
    req.command.name.assign("toggle");
    CommunicationPolicy policy; policy.add_transport(CommunicationTransport::MQTT);
    policy.add_transport(CommunicationTransport::ESP_NOW);
    assert(runner.start(req,policy,start));
  }
  void successful(CommunicationTransport t) {
    CommandResult result; assert(runner.take_result(result));
    assert(result.result==CommandResultStatus::SUCCEEDED && result.transport_used==t);
    assert(!mqtt.busy && !radio.busy);
    assert(!runner.has_progress_result());
    runner.loop(9000); assert(!runner.has_result());
  }
};
int main() {
  { // MQTT silence falls back before overall deadline, retaining MQTT listener.
    Fixture f; f.runner.loop(499); assert(!f.radio.sends);
    f.runner.loop(500); assert(f.radio.sends==1 && f.mqtt.busy && !f.mqtt.cancels);
    assert(f.radio.request.transaction_id==f.mqtt.request.transaction_id);
    f.radio.reply(CommandResultStatus::SUCCEEDED); f.runner.loop(600);
    f.successful(CommunicationTransport::ESP_NOW);
  }
  { // Lost MQTT ACK: original MQTT final wins while ESP-NOW attempt is pending.
    Fixture f; f.runner.loop(500); f.mqtt.reply(CommandResultStatus::SUCCEEDED);
    f.radio.reply(CommandResultStatus::IN_PROGRESS);
    f.runner.loop(600); f.successful(CommunicationTransport::MQTT);
  }
  { // Lost ACK recovered on radio, but final still arrives on original MQTT.
    Fixture f; f.runner.loop(500); f.radio.reply(CommandResultStatus::IN_PROGRESS,5000);
    f.runner.loop(600); assert(f.runner.has_progress_result());
    f.runner.loop(2100); assert(!f.runner.has_result());
    f.mqtt.reply(CommandResultStatus::SUCCEEDED); f.runner.loop(3000);
    f.successful(CommunicationTransport::MQTT);
  }
  { // ACK stops speculative resend and permits long execution.
    Fixture f; f.mqtt.reply(CommandResultStatus::IN_PROGRESS,10000); f.runner.loop(100);
    f.runner.loop(2500); assert(!f.radio.sends && !f.runner.has_result());
    f.mqtt.reply(CommandResultStatus::SUCCEEDED); f.runner.loop(8000);
    f.successful(CommunicationTransport::MQTT);
  }
  { // Duplicate ACK cannot keep extending deadline.
    Fixture f; f.mqtt.reply(CommandResultStatus::IN_PROGRESS,3000); f.runner.loop(100);
    f.mqtt.reply(CommandResultStatus::IN_PROGRESS,10000); f.runner.loop(3000);
    f.runner.loop(4100); CommandResult r; assert(f.runner.take_result(r));
    assert(r.error.code==CommunicationErrorCode::TIMED_OUT);
  }
  { // Radio transport failure does not discard original MQTT listener.
    Fixture f; f.runner.loop(500); f.radio.fail(); f.runner.loop(600);
    assert(!f.runner.has_result() && f.mqtt.busy);
    f.mqtt.reply(CommandResultStatus::SUCCEEDED); f.runner.loop(700);
    f.successful(CommunicationTransport::MQTT);
  }
  { // Accepted execution survives link failure via same-identity alternate.
    Fixture f; f.mqtt.reply(CommandResultStatus::IN_PROGRESS); f.runner.loop(100);
    f.mqtt.fail(); f.runner.loop(200); assert(f.radio.sends==1);
    f.radio.reply(CommandResultStatus::SUCCEEDED); f.runner.loop(300);
    f.successful(CommunicationTransport::ESP_NOW);
  }
  { // Complete silence expires once, releasing both listeners.
    Fixture f; f.runner.loop(500); f.runner.loop(2000);
    CommandResult r; assert(f.runner.take_result(r)); assert(r.error.code==CommunicationErrorCode::TIMED_OUT);
    assert(!f.mqtt.busy && !f.radio.busy && !f.runner.take_result(r));
  }
  { // Millisecond rollover preserves acceptance deadline.
    Fixture f(UINT32_MAX-200); f.runner.loop(298); assert(!f.radio.sends);
    f.runner.loop(299); assert(f.radio.sends==1);
  }
  { // Same identity across transports is pending/replayed, never re-executed.
    esphome::communication_net_protocol::InboundReplayGuard<4> guard;
    using D=esphome::communication_net_protocol::InboundDecision;
    const uint8_t command[]={1,2,3}, terminal[]={4,5};
    assert(guard.begin("tx",9,123,command,3)==D::NEW_COMMAND);
    assert(guard.begin("tx",9,123,command,3)==D::DUPLICATE_PENDING);
    assert(guard.finish("tx",9,123,command,3,
      {esphome::communication_net_protocol::InboundTerminalStatus::SUCCEEDED,terminal,2}));
    assert(guard.begin("tx",9,123,command,3)==D::DUPLICATE_TERMINAL);
  }
  std::puts("PASS 10 outbound acceptance, late reply and deduplication scenarios");
}
