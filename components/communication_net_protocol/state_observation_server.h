#pragma once
#include "esphome/core/defines.h"
#ifdef USE_COMMUNICATION_NET_STATE_OBSERVERS
#include "light_state_snapshot.h"
#include "state_subscription_registry.h"
#include "state_traffic_scheduler.h"
#include "esphome/components/espnow_net_protocol/espnow_net_protocol.h"
#include "esphome/core/log.h"
#include <esp_random.h>
#include <cstdio>
#include <cstring>

namespace esphome::communication_net_protocol {
namespace observation_net = esphome::espnow_net_protocol;
// One configured snapshot group, up to eight explicitly authorized peers.
// A fresh challenge binds session changes; random boot IDs are never ordered.
class StateObservationServer {
 public:
  void configure(observation_net::EspNowNetProtocolComponent *radio, uint32_t validity,
                 uint32_t node_interval, uint32_t peer_interval) {
    radio_=radio; validity_=validity;
    leases_=StateSubscriptionRegistry<8>(validity);
    scheduler_=Scheduler({node_interval,peer_interval,2000,30000,1000});
  }
  void add_peer(const char *peer, const char *source) {
    if(count_>=8) return;
    peers_[count_].name=peer; peers_[count_].source=source;
    leases_.configure_peer(count_,1); ++count_;
  }
  bool configured() const { return radio_!=nullptr && count_!=0; }
  // Caller has already checked the radio's configured peer/source binding.
  bool control(observation_net::PeerIndex peer, const observation_net::NetCommand &command,
               uint32_t now, LightStateSnapshot &snapshot, observation_net::NetResult &result) {
    size_t index=0;
    for(;index<count_;++index)
      if(radio_->peer_index(peers_[index].name)==peer &&
         std::strcmp(peers_[index].source,command.source_device_id.c_str())==0) break;
    if(index==count_ || command.source_boot_id==0) return false;
    auto &entry=peers_[index];
    if(std::strcmp(command.name.c_str(),"hello")==0) {
      if(entry.hello_tx!=command.transaction_id || entry.challenge_session!=command.source_boot_id) {
        if(entry.challenge!=0 && uint32_t(now-entry.challenged_at)<2000) return false;
        entry.challenge=(uint64_t(esp_random())<<32)|esp_random();
        if(entry.challenge==0) entry.challenge=1;
        entry.challenge_session=command.source_boot_id; entry.hello_tx=command.transaction_id;
        entry.challenged_at=now;
      }
      uint8_t data[12]{};
      for(size_t i=0;i<8;++i) data[i]=uint8_t(entry.challenge>>(8*i));
      for(size_t i=0;i<4;++i) data[8+i]=uint8_t(validity_>>(8*i));
      result.remote_state.completeness=observation_net::NetStateCompleteness::COMPLETE;
      return result.remote_state.schema.assign("state-lease/v1") && result.remote_state.data.assign(data,sizeof(data));
    }
    char expected[32]{};
    std::snprintf(expected,sizeof(expected),"lease_%016llx",(unsigned long long)entry.challenge);
    const bool enable=std::strcmp(command.name.c_str(),expected)==0;
    if(!enable) std::snprintf(expected,sizeof(expected),"stop_%016llx",(unsigned long long)entry.challenge);
    if(entry.challenge==0 || entry.challenge_session!=command.source_boot_id ||
       std::strcmp(command.name.c_str(),expected)!=0) return false;
    // A challenge expires before activation; active renewals remain sequenced.
    if(entry.active_challenge!=entry.challenge) {
      if(uint32_t(now-entry.challenged_at)>=10000) return false;
      leases_.establish_session(index,command.source_boot_id);
    }
    auto applied=leases_.apply(index,command.source_boot_id,command.transaction_id,enable ? 1 : 0,enable,now);
    if(applied!=StateSubscriptionResult::APPLIED && applied!=StateSubscriptionResult::DUPLICATE) return false;
    entry.active_challenge=entry.challenge;
    scheduler_.set_enabled(index,leases_.active_resources(index,now)!=0);
    if(enable && applied==StateSubscriptionResult::APPLIED) scheduler_.mark_latest(index,0);
    ESP_LOGI("state.observe","LEASE peer=%u tx=%llu result=%u validity_ms=%u",unsigned(peer),
        (unsigned long long)command.transaction_id,unsigned(applied),unsigned(validity_));
    return snapshot.write_remote_state(result.remote_state);
  }
  void loop(uint32_t now, LightStateSnapshot &snapshot, bool command_idle) {
    if(!configured()) return;
    uint64_t completed=0; bool success=false;
    if(inflight_ && radio_->take_background_result_completion(completed,success)) {
      scheduler_.complete(ticket_,success && completed==transaction_,now,esp_random());
      ESP_LOGI("state.observe","DELIVERY tx=%llu success=%u",(unsigned long long)transaction_,unsigned(success && completed==transaction_));
      inflight_=false;
    }
    if(inflight_ && (!radio_->runtime_enabled() || !radio_->background_result_active())) {
      scheduler_.complete(ticket_,false,now,esp_random()); inflight_=false;
    }
    leases_.expire(now);
    const bool changed=generation_!=snapshot.generation() || revision_!=snapshot.revision();
    generation_=snapshot.generation(); revision_=snapshot.revision();
    for(size_t i=0;i<count_;++i) {
      const bool active=leases_.active_resources(i,now)!=0;
      if(peers_[i].was_active && !active) ESP_LOGI("state.observe","EXPIRED_OR_RELEASED peer=%u",unsigned(i));
      peers_[i].was_active=active;
      scheduler_.set_enabled(i,active);
      if(active && changed) scheduler_.mark_latest(i,0);
    }
    if(inflight_ || !radio_->runtime_enabled() || !radio_->can_start_command() ||
       !scheduler_.take(now,command_idle,ticket_)) return;
    // Separate transaction namespace from request sequences (bit 63).
    if(serial_==0x7fffffffffffffffULL) {scheduler_.complete(ticket_,false,now,esp_random());return;}
    result_={}; result_.transaction_id=(uint64_t(1)<<63)|++serial_;
    result_.status=observation_net::NetResultStatus::SUCCEEDED; result_.execution.started=true;
    transaction_=result_.transaction_id;
    if(!snapshot.write_remote_state(result_.remote_state) ||
       !radio_->start_background_result(radio_->peer_index(peers_[ticket_.peer].name),result_)) {
      scheduler_.complete(ticket_,false,now,esp_random()); return;
    }
    inflight_=true;
    ESP_LOGI("state.observe","PUSH peer=%u generation=%u revision=%u tx=%llu",unsigned(ticket_.peer),
        unsigned(snapshot.generation()),unsigned(snapshot.revision()),(unsigned long long)transaction_);
  }
 private:
  using Scheduler=StateTrafficScheduler<8,1>;
  struct Peer {
    const char *name{nullptr},*source{nullptr};
    uint64_t challenge{0},active_challenge{0},challenge_session{0},hello_tx{0};
    uint32_t challenged_at{0};
    bool was_active{false};
  } peers_[8]{};
  observation_net::EspNowNetProtocolComponent *radio_{nullptr};
  StateSubscriptionRegistry<8> leases_{60000};
  Scheduler scheduler_{{1000,2000,2000,30000,1000}};
  Scheduler::Ticket ticket_{};
  observation_net::NetResult result_{};
  size_t count_{0}; uint32_t validity_{60000},generation_{0},revision_{0};
  uint64_t transaction_{0},serial_{0}; bool inflight_{false};
};
}
#endif  // USE_COMMUNICATION_NET_STATE_OBSERVERS
