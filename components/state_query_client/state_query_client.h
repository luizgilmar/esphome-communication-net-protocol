#pragma once
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/espnow_net_protocol/espnow_net_protocol.h"
#include "esphome/components/communication_net_protocol/state_traffic_scheduler.h"
#include <esp_random.h>
#include <cstring>
#include <cstdio>
namespace esphome::state_query_client {
namespace net = esphome::espnow_net_protocol;
class StateQueryClient : public Component, public net::NetCommandResultObserver, public net::NetUnsolicitedResultObserver {
 public:
  void configure(net::EspNowNetProtocolComponent *radio, const char *peer,
      const char *source, const char *target, uint32_t interval, uint32_t timeout) {
    radio_=radio; peer_=peer; source_=source; target_=target; interval_=interval; timeout_=timeout;
  }
  void set_subscribe(bool enabled) {subscribe_=enabled;}
  void set_radio_required(bool required) {
    if(radio_required_==required) return;
    radio_required_=required;since_=millis();wait_=500+esp_random()%1001;
    retry_.recovered();
    ESP_LOGI("state.query","ROUTE radio_required=%u",unsigned(required));
  }
  uint32_t state_generation() const {return generation_;}
  uint32_t state_revision() const {return revision_;}
  bool ingest_snapshot(const net::NetStateSnapshot &snapshot,const char *origin) {
    return accept_snapshot_(snapshot,origin);
  }
  float get_setup_priority() const override { return -20.0f; }
  void setup() override {
    peer_index_=radio_->peer_index(peer_);
    if (peer_index_==net::INVALID_PEER_INDEX) { mark_failed(); return; }
    session_=(uint64_t(esp_random())<<32)|esp_random();
    if (!session_) session_=1;
    radio_->set_command_result_observer(this);
    if(subscribe_) radio_->set_unsolicited_result_observer(this);
    since_=millis(); wait_=5000+esp_random()%10001;
    ESP_LOGI("state.query","READY startup_wait_ms=%u",unsigned(wait_));
  }
  void loop() override {
    const uint32_t now=millis();
    if (pending_) {
      if (uint32_t(now-started_)>=timeout_) {
        radio_->cancel_command(transaction_);
        pending_=false; challenge_=0; retry_.defer(now,esp_random());
        ESP_LOGW("state.query","TIMEOUT tx=%llu",(unsigned long long)transaction_);
      }
      return;
    }
    if(subscribe_ && !radio_required_ && (!lease_possible_ || challenge_==0)) return;
    if (uint32_t(now-since_)<wait_ || !retry_.ready(now)) return;
    // A busy endpoint must not trigger repeated admission attempts each loop.
    since_=now; wait_=1000+esp_random()%1001;
    if (!radio_->runtime_enabled() || !radio_->can_start_command()) return;
    if (sequence_==0x7fffffffffffffffULL) {mark_failed();return;}
    net::NetCommand request{};
    request.transaction_id=++sequence_;
    request.source_boot_id=session_;
    request.timeout_ms=timeout_;
    char command_name[32]{};
    if(subscribe_ && challenge_!=0)
      std::snprintf(command_name,sizeof(command_name),radio_required_ ? "lease_%016llx" : "stop_%016llx",(unsigned long long)challenge_);
    else std::strcpy(command_name,subscribe_ ? "hello" : "get");
    const uint8_t payload[]{'{','}'};
    if (!request.source_device_id.assign(source_) || !request.device_id.assign(target_) ||
        !request.resource.assign(subscribe_ ? "state/observation/v1" : "state/snapshot") || !request.name.assign(command_name) ||
        !request.payload.assign(payload,sizeof(payload))) {mark_failed();return;}
    releasing_=subscribe_ && !radio_required_ && challenge_!=0;
    transaction_=request.transaction_id;
    if (!radio_->start_command(peer_index_,request,now)) {retry_.defer(now,esp_random());return;}
    if(subscribe_ && challenge_!=0 && !releasing_) lease_possible_=true;
    pending_=true; started_=now;
    ESP_LOGI("state.query","SEND tx=%llu",(unsigned long long)transaction_);
  }
  void on_net_command_result(net::PeerIndex peer,const net::NetResult &result) override {
    if (!pending_ || peer!=peer_index_ || result.transaction_id!=transaction_) return;
    if (result.status==net::NetResultStatus::IN_PROGRESS) return;
    pending_=false;
    const uint32_t now=millis(); since_=now;
    const auto &snapshot=result.remote_state;
    if(subscribe_ && challenge_==0 && result.status==net::NetResultStatus::SUCCEEDED && result.consistent() &&
       snapshot.completeness==net::NetStateCompleteness::COMPLETE &&
       std::strcmp(snapshot.schema.c_str(),"state-lease/v1")==0 && snapshot.data.size()==12) {
      uint64_t nonce=0; for(size_t i=0;i<8;++i) nonce|=uint64_t(snapshot.data.data()[i])<<(8*i);
      uint32_t validity=read32_(snapshot.data.data()+8);
      if(nonce!=0 && validity>=30000 && validity<=300000) {
        challenge_=nonce;renew_ms_=validity/3;retry_.recovered();wait_=0;
        ESP_LOGI("state.query","CHALLENGE validity_ms=%u",unsigned(validity));return;
      }
    }
    if (result.status!=net::NetResultStatus::SUCCEEDED || !result.consistent() ||
        snapshot.completeness!=net::NetStateCompleteness::COMPLETE ||
        std::strcmp(snapshot.schema.c_str(),"state-fields/v2")!=0 ||
        snapshot.data.size()<10 || snapshot.data.data()[0]!=2) {
      challenge_=0;wait_=0;retry_.defer(now,esp_random());
      ESP_LOGW("state.query","REJECTED tx=%llu status=%u",(unsigned long long)transaction_,unsigned(result.status));
      return;
    }
    if(releasing_) {
      challenge_=0;lease_possible_=false;
      ESP_LOGI("state.query","RELEASED tx=%llu",(unsigned long long)transaction_);
    }
    retry_.recovered();wait_=(subscribe_ ? renew_ms_ : interval_)+esp_random()%1001;
    if(!accept_snapshot_(snapshot,"reply")) return;
    const auto *bytes=snapshot.data.data();
    char hex[513]{};const char *digits="0123456789abcdef";
    for(size_t i=0;i<snapshot.data.size();++i){hex[2*i]=digits[bytes[i]>>4];hex[2*i+1]=digits[bytes[i]&15];}
    ESP_LOGI("state.query","SNAPSHOT tx=%llu rtt_ms=%u bytes=%u hex=%s",(unsigned long long)transaction_,unsigned(now-started_),unsigned(snapshot.data.size()),hex);
  }
  void on_unsolicited_net_result(net::PeerIndex peer,const net::NetResult &result) override {
    if(!subscribe_ || peer!=peer_index_ || result.status!=net::NetResultStatus::SUCCEEDED ||
       !result.consistent() || (result.transaction_id & (uint64_t(1)<<63))==0) return;
    accept_snapshot_(result.remote_state,"push");
  }
 private:
  static uint32_t read32_(const uint8_t *p) {
    return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
  }
  bool accept_snapshot_(const net::NetStateSnapshot &snapshot,const char *origin) {
    if(snapshot.completeness!=net::NetStateCompleteness::COMPLETE ||
       std::strcmp(snapshot.schema.c_str(),"state-fields/v2")!=0 || snapshot.data.size()<10) return false;
    const auto *p=snapshot.data.data(); const size_t length=snapshot.data.size();
    if(p[0]!=2 || p[9]==0 || p[9]>4) return false;
    size_t offset=10;
    for(size_t i=0;i<p[9];++i) {
      if(offset>=length) return false;
      const size_t name=p[offset++];
      if(name==0 || name>31 || offset+name+5>length) return false;
      offset+=name;
      if(p[offset]&0xf0) return false;
      offset+=5;
    }
    if(offset!=length) return false;
    const uint32_t generation=read32_(p+1),revision=read32_(p+5);
    if(generation==0 || revision==0 || generation<generation_ ||
       (generation==generation_ && revision<revision_)) return false;
    if(generation==generation_ && revision==revision_) return true;
    generation_=generation;revision_=revision;
    ESP_LOGI("state.query","STATE origin=%s generation=%u revision=%u",origin,unsigned(generation),unsigned(revision));
    return true;
  }
  net::EspNowNetProtocolComponent *radio_{nullptr};
  const char *peer_{nullptr},*source_{nullptr},*target_{nullptr};
  net::PeerIndex peer_index_{net::INVALID_PEER_INDEX};
  uint32_t interval_{15000},timeout_{4000},since_{0},wait_{0},started_{0};
  bool radio_required_{true},lease_possible_{false},releasing_{false};
  bool subscribe_{false}; uint64_t challenge_{0};
  uint32_t renew_ms_{20000},generation_{0},revision_{0};
  uint64_t session_{0},sequence_{0},transaction_{0};bool pending_{false};
  esphome::communication_net_protocol::StateRetryWindow retry_{{1000,1000,2000,30000,1000}};
};
}
