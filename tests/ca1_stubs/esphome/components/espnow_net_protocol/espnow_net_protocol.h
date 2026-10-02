#pragma once
#include "esphome/components/espnow_net_protocol/command_dispatcher.h"
namespace esphome { namespace espnow_net_protocol {
class NetCommandResultObserver { public: virtual ~NetCommandResultObserver()=default; virtual void on_net_command_result(PeerIndex,const NetResult&)=0; };
class EspNowNetProtocolComponent { public:
struct Radio { bool initialized()const{return true;} bool channel_matches()const{return true;} };
Radio radio_; bool external=false; NetCommand saved{};
bool runtime_enabled()const{return true;}
const Radio& radio()const{return radio_;}
bool can_start_command()const{return true;}
void set_command_result_observer(NetCommandResultObserver*){}
bool start_command(PeerIndex,const NetCommand&c,uint32_t){saved=c;return true;}
bool cancel_command(TransactionId){return true;}
bool use_external_result_deadline(TransactionId){external=true;return true;}
void set_verified_command_observer(NetCommandIdentityObserver*){}
void set_command_handler(NetCommandHandler*){}
void set_interrupt_command_handler(NetCommandHandler*){}
void set_interruptible_inbound(bool){}
};
} }
