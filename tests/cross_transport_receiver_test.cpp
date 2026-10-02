#include <cassert>
#include <cstdio>
#include "components/communication_net_protocol/communication_net_protocol.h"
using namespace esphome;
using namespace communication_net_protocol;
using namespace espnow_net_protocol;
struct Receiver: CommunicationNetProtocolComponent {
 using CommunicationNetProtocolComponent::start_inbound_;
 using CommunicationNetProtocolComponent::publish_inbound_result_;
 bool mqtt_ready()const{for(const auto &slot:executions_) if(slot.mqtt_result_ready_) return true; return false;}
};
NetCommand command(){NetCommand c;c.transaction_id=88;c.source_boot_id=42;c.timeout_ms=2000;
c.source_device_id.assign("tx");c.device_id.assign("hub");c.resource.assign("light/spot");c.name.assign("toggle");c.payload.assign((const uint8_t*)"{}",2);return c;}
int main(){
 mqtt::MQTTClientComponent broker;mqtt::global_mqtt_client=&broker;
 for(bool first_radio:{false,true}){
  Receiver rx;rx.set_device_id("hub");rx.set_mqtt_inbound_execution("tx","replies/tx");
  DeclarativeInboundBinding binding;binding.set_completion_delay(1000);binding.set_completion_timeout(5000);
  rx.add_inbound_route("toggle","light/spot","toggle");rx.add_inbound_binding(&binding);
  auto cmd=command();broker.connected=false;
  assert(rx.start_inbound_(cmd,0,first_radio)==NetCommandHandlerStartStatus::STARTED);
  assert(rx.start_inbound_(cmd,500,!first_radio)==NetCommandHandlerStartStatus::STARTED);
  assert(binding.calls==1);
  NetResult result;assert(rx.take_result(result));assert(result.status==NetResultStatus::IN_PROGRESS);
  rx.loop(1000);assert(rx.take_result(result));assert(result.status==NetResultStatus::SUCCEEDED);
  assert(binding.calls==1 && rx.mqtt_ready());
  rx.publish_inbound_result_();assert(rx.mqtt_ready());
  assert(rx.start_inbound_(cmd,1100,true)==NetCommandHandlerStartStatus::STARTED);
  assert(rx.take_result(result));assert(result.status==NetResultStatus::SUCCEEDED);
  assert(binding.calls==1);
  broker.connected=true;rx.publish_inbound_result_();assert(!rx.mqtt_ready());
  assert(broker.published.back().find("succeeded")!=std::string::npos);
 }
 std::puts("PASS actual receiver: pending cross-transport duplicate, offline ACK, terminal replay, no second execution");
}
