#include <cassert>
#include <cstdio>
#include "components/communication_net_protocol/communication_net_protocol.h"
#include "esphome/components/cover/cover.h"
using namespace esphome;using namespace communication_net_protocol;using namespace espnow_net_protocol;
struct Receiver:CommunicationNetProtocolComponent{using CommunicationNetProtocolComponent::start_inbound_;};
NetCommand command(uint64_t id,const char *name){NetCommand c{};c.transaction_id=id;c.source_boot_id=42;c.source_device_id.assign("tx");c.device_id.assign("hub");c.resource.assign("cover/persiana");c.name.assign(name);c.payload.assign((const uint8_t*)"{}",2);c.timeout_ms=22000;return c;}
int main(){
 Receiver receiver;receiver.set_device_id("hub");cover::Cover state;
 DeclarativeInboundBinding open,close,stop;
 for(auto *binding:{&open,&close,&stop}){binding->set_cover(&state);binding->set_completion_timeout(20000);receiver.add_inbound_binding(binding);}
 open.set_cover_expected(CoverExpectedState::OPEN);open.set_interruptible(true);
 close.set_cover_expected(CoverExpectedState::CLOSED);close.set_interruptible(true);
 stop.set_cover_expected(CoverExpectedState::IDLE);stop.set_interrupts_active(true);
 receiver.add_inbound_route("open","cover/persiana","open");receiver.add_inbound_route("close","cover/persiana","close");receiver.add_inbound_route("stop","cover/persiana","stop");
 NetResult result;
 state.position=.8f;state.current_operation=1;
 assert(receiver.start_inbound_(command(1,"open"),0,true)==NetCommandHandlerStartStatus::STARTED);
 assert(receiver.take_result(1,result)&&result.status==NetResultStatus::IN_PROGRESS);
 state.position=1.f;receiver.loop(3900);assert(!receiver.has_result(1));
 state.current_operation=cover::COVER_OPERATION_IDLE;receiver.loop(4000);
 assert(receiver.take_result(1,result)&&result.status==NetResultStatus::SUCCEEDED);
 // Immediate reverse after a partial-duration opening must not wait for 19 s.
 state.current_operation=2;
 assert(receiver.start_inbound_(command(2,"close"),4001,true)==NetCommandHandlerStartStatus::STARTED);
 assert(receiver.take_result(2,result)&&result.status==NetResultStatus::IN_PROGRESS);
 state.position=0.f;receiver.loop(5000);assert(!receiver.has_result(2));
 state.current_operation=cover::COVER_OPERATION_IDLE;receiver.loop(5001);
 assert(receiver.take_result(2,result)&&result.status==NetResultStatus::SUCCEEDED);
 // Already at endpoint: conclude immediately and free the resource again.
 assert(receiver.start_inbound_(command(3,"close"),5002,true)==NetCommandHandlerStartStatus::STARTED);
 assert(receiver.take_result(3,result));receiver.loop(5003);
 assert(receiver.take_result(3,result)&&result.status==NetResultStatus::SUCCEEDED);
 state.position=.5f;state.current_operation=1;
 assert(receiver.start_inbound_(command(4,"open"),5004,true)==NetCommandHandlerStartStatus::STARTED);assert(receiver.take_result(4,result));
 assert(receiver.start_inbound_(command(5,"stop"),5100,true)==NetCommandHandlerStartStatus::STARTED);assert(receiver.take_result(5,result));
 receiver.loop(5101);assert(!receiver.has_result(5));
 state.current_operation=cover::COVER_OPERATION_IDLE;receiver.loop(5102);
 assert(receiver.take_result(5,result)&&result.status==NetResultStatus::SUCCEEDED);
 assert(receiver.take_result(4,result)&&result.error.code==NetErrorCode::INTERRUPTED);
 puts("PASS cover endpoint: partial stroke, immediate reverse, already at endpoint and STOP");
}
