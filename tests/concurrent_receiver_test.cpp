#include <cassert>
#include <cstdio>
#include "components/communication_net_protocol/communication_net_protocol.h"
using namespace esphome;
using namespace communication_net_protocol;
using namespace espnow_net_protocol;
struct Receiver : CommunicationNetProtocolComponent {
  using CommunicationNetProtocolComponent::start_inbound_;
  using CommunicationNetProtocolComponent::publish_inbound_result_;
};
NetCommand command(uint64_t id, const char *resource, const char *action="toggle") {
  NetCommand c{}; c.transaction_id=id; c.source_boot_id=42; c.timeout_ms=22000;
  c.source_device_id.assign("tx"); c.device_id.assign("hub");
  c.resource.assign(resource); c.name.assign(action); c.payload.assign((const uint8_t*)"{}",2);
  return c;
}
void bind(Receiver &rx, DeclarativeInboundBinding &b, const char *id, const char *resource,
          uint32_t delay, const char *action="toggle") {
  b.set_completion_delay(delay); b.set_completion_timeout(delay+1000);
  rx.add_inbound_route(id,resource,action); rx.add_inbound_binding(&b);
}
int main() {
  mqtt::MQTTClientComponent broker; mqtt::global_mqtt_client=&broker;
  for (bool radio : {false,true}) {
    Receiver rx;rx.set_device_id("hub"); rx.set_mqtt_inbound_execution("tx","results");
    DeclarativeInboundBinding strips,spot,blind,stop;
    bind(rx,strips,"strips","light/strips",1000);
    bind(rx,spot,"spot","light/spot",10);
    bind(rx,blind,"blind","cover/blind",19000,"open"); blind.set_interruptible(true);
    bind(rx,stop,"stop","cover/blind",50,"stop"); stop.set_interrupts_active(true);
    auto a=command(1,"light/strips"), b=command(2,"light/spot"), d=command(3,"cover/blind","open");
    assert(rx.start_inbound_(a,0,radio)==NetCommandHandlerStartStatus::STARTED);
    assert(rx.start_inbound_(b,1,radio)==NetCommandHandlerStartStatus::STARTED);
    assert(rx.start_inbound_(d,2,radio)==NetCommandHandlerStartStatus::STARTED);
    // Cross-transport copies register their reply path but never trigger twice.
    assert(rx.start_inbound_(a,3,!radio)==NetCommandHandlerStartStatus::STARTED);
    assert(rx.start_inbound_(b,4,!radio)==NetCommandHandlerStartStatus::STARTED);
    assert(rx.start_inbound_(d,5,!radio)==NetCommandHandlerStartStatus::STARTED);
    assert(strips.calls==1 && spot.calls==1 && blind.calls==1);
    NetResult result{};
    assert(rx.take_result(2,result) && result.status==NetResultStatus::IN_PROGRESS);
    rx.loop(20);
    assert(rx.take_result(2,result) && result.status==NetResultStatus::SUCCEEDED);
    assert(rx.take_result(1,result) && result.status==NetResultStatus::IN_PROGRESS);
    assert(rx.take_result(3,result) && result.status==NetResultStatus::IN_PROGRESS);
    // A stop affects only its persiana, not another in-flight resource.
    auto interrupt=command(4,"cover/blind","stop");
    assert(rx.start_inbound_(interrupt,30,radio)==NetCommandHandlerStartStatus::STARTED);
    assert(rx.start_inbound_(interrupt,31,!radio)==NetCommandHandlerStartStatus::STARTED);
    rx.loop(90);
    assert(rx.take_result(4,result) && result.status==NetResultStatus::SUCCEEDED);
    assert(rx.take_result(3,result) && result.error.code==NetErrorCode::INTERRUPTED);
    assert(!rx.has_result(1));
    rx.loop(1001);
    assert(rx.take_result(1,result) && result.status==NetResultStatus::SUCCEEDED);
    rx.publish_inbound_result_(); rx.publish_inbound_result_();
    assert(rx.start_inbound_(a,1002,true)==NetCommandHandlerStartStatus::STARTED);
    assert(rx.take_result(1,result) && result.status==NetResultStatus::SUCCEEDED);
    assert(strips.calls==1 && spot.calls==1 && blind.calls==1 && stop.calls==1);
  }
  {
    // Real radio dispatchers share the common executor and retain their own
    // result correlation, including STOP while both normal lanes are active.
    Receiver rx;rx.set_device_id("hub");DeclarativeInboundBinding strip,blind,stop;
    bind(rx,strip,"strip","light/strips",1000);
    bind(rx,blind,"blind","cover/blind",19000,"open");blind.set_interruptible(true);
    bind(rx,stop,"stop","cover/blind",50,"stop");stop.set_interrupts_active(true);
    InboundCommandDispatcher first,second,interrupt;
    for(auto *d : {&first,&second,&interrupt}) {d->set_handler(&rx);d->set_identity_observer(&rx);}
    auto wire=[](const NetCommand &c) {
      EspNowInboundApplicationMessage m{};m.peer_index=0;
      m.message.envelope.kind=EspNowFrameKind::COMMAND;m.message.envelope.transaction_id=c.transaction_id;
      EspNowCommandCodec codec;EspNowCommandPayload payload;
      assert(codec.encode(c,payload));assert(m.message.data.assign(payload.data.data(),payload.data.size()));return m;
    };
    auto a=wire(command(40,"light/strips")), b=wire(command(41,"cover/blind","open"));
    auto halt=wire(command(42,"cover/blind","stop"));
    assert(first.accept(a,0));assert(second.accept(b,1));
    first.loop(2);second.loop(2);PendingNetResult pending{};
    assert(first.take_result(pending) && pending.transaction_id==40 && !pending.terminal);
    assert(second.take_result(pending) && pending.transaction_id==41 && !pending.terminal);
    assert(!interrupt.can_accept_interrupt(a,second));
    assert(!interrupt.can_accept_interrupt(halt,first));
    assert(interrupt.can_accept_interrupt(halt,second));
    assert(interrupt.accept_interrupt(halt,second,3));
    interrupt.loop(60);second.loop(60);first.loop(60);
    assert(interrupt.take_result(pending) && pending.transaction_id==42 && pending.terminal);
    assert(second.take_result(pending) && pending.transaction_id==41 && pending.terminal);
    assert(!first.has_result());first.loop(1001);
    assert(first.take_result(pending) && pending.transaction_id==40 && pending.terminal);
  }
  {
    Receiver rx;rx.set_device_id("hub");DeclarativeInboundBinding binding;
    bind(rx,binding,"spot","light/spot",1000);
    auto first=command(10,"light/spot"), second=command(11,"light/spot");
    assert(rx.start_inbound_(first,0,true)==NetCommandHandlerStartStatus::STARTED);
    assert(rx.start_inbound_(second,1,true)==NetCommandHandlerStartStatus::BUSY);
    assert(binding.calls==1);rx.loop(1000);NetResult result;
    assert(rx.take_result(10,result) && result.status==NetResultStatus::SUCCEEDED);
    assert(rx.start_inbound_(second,1001,true)==NetCommandHandlerStartStatus::STARTED);
    assert(binding.calls==2);
  }
  {
    Receiver rx;rx.set_device_id("hub");DeclarativeInboundBinding bindings[5];
    const char *resources[]={"light/0","light/1","light/2","light/3","light/4"};
    for(int i=0;i<5;++i)bind(rx,bindings[i],resources[i],resources[i],100);
    for(int i=0;i<4;++i)assert(rx.start_inbound_(command(20+i,resources[i]),0,true)==NetCommandHandlerStartStatus::STARTED);
    assert(rx.start_inbound_(command(24,resources[4]),1,true)==NetCommandHandlerStartStatus::BUSY);
    assert(bindings[4].calls==0);rx.loop(100);NetResult result;
    for(int i=0;i<4;++i)assert(rx.take_result(20+i,result) && result.status==NetResultStatus::SUCCEEDED);
    assert(rx.start_inbound_(command(24,resources[4]),101,true)==NetCommandHandlerStartStatus::STARTED);
  }
  std::puts("PASS CC1: parallel resources, cross-transport dedup/replay, isolated stop, same-resource exclusion, bounded capacity/reuse");
}
