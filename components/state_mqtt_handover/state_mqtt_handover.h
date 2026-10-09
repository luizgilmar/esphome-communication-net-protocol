#pragma once
#include "esphome/core/defines.h"
#ifdef USE_STATE_MQTT_HANDOVER
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/state_query_client/state_query_client.h"
#include "esphome/components/communication_net_protocol/mqtt_wire_transport.h"
#include "esphome/components/json/json_util.h"
#include <esp_random.h>
#include <cstdio>
#include <cstring>

namespace esphome::state_mqtt_handover {
namespace net=esphome::espnow_net_protocol;
class StateMqttHandover : public Component {
 public:
  void configure(state_query_client::StateQueryClient *client,const char *state,const char *command,
      const char *reply,const char *source,const char *target,uint32_t interval,uint32_t freshness) {
    client_=client;state_topic_=state;command_topic_=command;reply_topic_=reply;
    source_=source;target_=target;interval_=interval;freshness_=freshness;
  }
  float get_setup_priority() const override {return -30.0f;}
  void setup() override {
    if(!wire_.subscribe(state_topic_,1) || !wire_.subscribe(reply_topic_,1)) {mark_failed();return;}
    // Replies lack a boot ID, so correlation uses a random per-boot sequence namespace.
    sequence_=((uint64_t(esp_random())<<32)|esp_random()) & 0x3fffffffffffffffULL;
    session_=(uint64_t(esp_random())<<32)|esp_random();if(!session_) session_=1;
    since_=millis();wait_=2000+esp_random()%1001;
  }
  void loop() override {
    const uint32_t now=millis();const bool connected=wire_.available();
    if(connected!=connected_) {
      connected_=connected;synced_=false;pending_=false;retry_.recovered();
      since_=now;wait_=1000+esp_random()%1001;
      // A message queued before a disconnect cannot prove the next connection fresh.
      wire_.release_received();
      ESP_LOGI("state.handover","MQTT connected=%u",unsigned(connected));
    }
    const char *topic=nullptr;const uint8_t *payload=nullptr;size_t length=0;
    if(wire_.peek_received(topic,payload,length)) {
      if(connected) receive_(topic,payload,length,now);
      wire_.release_received();
    }
    if(pending_ && uint32_t(now-started_)>=4000) {
      pending_=false;retry_.defer(now,esp_random());since_=now;wait_=0;
      ESP_LOGW("state.handover","SYNC timeout tx=%llu",(unsigned long long)transaction_);
    }
    const bool healthy=connected && synced_ && uint32_t(now-proof_at_)<freshness_;
    if(healthy!=healthy_) {
      healthy_=healthy;
      ESP_LOGI("state.handover","ROUTE preferred=%s",healthy ? "mqtt" : "esp_now");
    }
    client_->set_radio_required(!healthy);
    if(!connected || pending_ || uint32_t(now-since_)<wait_ || !retry_.ready(now)) return;
    since_=now;wait_=1000+esp_random()%1001;
    if(sequence_==0x7fffffffffffffffULL) {mark_failed();return;}
    transaction_=++sequence_;
    const int size=std::snprintf(request_,sizeof(request_),
      "{\"transaction_id\":\"%llu\",\"reply_to\":\"%s\",\"source\":{\"device_id\":\"%s\",\"boot_id\":\"%llu\"},"
      "\"target\":{\"device_id\":\"%s\",\"resource\":\"state/snapshot\"},\"command\":{\"name\":\"get\",\"payload\":{}},\"timeout_ms\":4000}",
      (unsigned long long)transaction_,reply_topic_,source_,(unsigned long long)session_,target_);
    if(size<=0 || size>=int(sizeof(request_))) {mark_failed();return;}
    if(!wire_.publish(command_topic_,reinterpret_cast<const uint8_t *>(request_),size,1,false)) {
      retry_.defer(now,esp_random());return;
    }
    pending_=true;started_=now;
    ESP_LOGI("state.handover","SYNC send tx=%llu",(unsigned long long)transaction_);
  }
 private:
  static void put32_(uint8_t *p,uint32_t n) {for(size_t i=0;i<4;++i)p[i]=uint8_t(n>>(8*i));}
  static int hex_(char c) {
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
  }
  bool live_snapshot_(JsonObject root) {
    const JsonObject meta=root["_meta"].as<JsonObject>();
    if(meta.isNull() || !meta["version"].is<uint32_t>() || meta["version"].as<uint32_t>()!=2 ||
       !meta["generation"].is<uint32_t>() || !meta["revision"].is<uint32_t>() ||
       root.size()<2 || root.size()>5) return false;
    bytes_[0]=2;put32_(bytes_+1,meta["generation"].as<uint32_t>());
    put32_(bytes_+5,meta["revision"].as<uint32_t>());bytes_[9]=uint8_t(root.size()-1);
    size_t used=10;
    for(JsonPair pair:root) {
      const char *name=pair.key().c_str();if(std::strcmp(name,"_meta")==0) continue;
      const size_t count=std::strlen(name);
      if(count==0 || count>31 || used+1+count+5>sizeof(bytes_) || name[0]<'a' || name[0]>'z') return false;
      for(size_t i=0;i<count;++i)
        if(!((name[i]>='a'&&name[i]<='z') || (name[i]>='0'&&name[i]<='9') || name[i]=='_')) return false;
      const JsonObject value=pair.value().as<JsonObject>();
      if(value.isNull() || !value["known"].is<bool>() || !value["on"].is<bool>()) return false;
      bytes_[used++]=uint8_t(count);std::memcpy(bytes_+used,name,count);used+=count;
      bool rgb=!value["red"].isNull();uint8_t colors[4]{};
      if(rgb) {
        const char *keys[]{"red","green","blue","brightness"};
        for(size_t i=0;i<4;++i) {
          if(!value[keys[i]].is<unsigned>() || value[keys[i]].as<unsigned>()>255) return false;
          colors[i]=uint8_t(value[keys[i]].as<unsigned>());
        }
        if(!value["effect_active"].is<bool>()) return false;
      }
      bytes_[used++]=(value["known"].as<bool>()?1:0)|(value["on"].as<bool>()?2:0)|
          (rgb?4:0)|((rgb&&value["effect_active"].as<bool>())?8:0);
      std::memcpy(bytes_+used,colors,4);used+=4;
    }
    snapshot_={};snapshot_.completeness=net::NetStateCompleteness::COMPLETE;
    return snapshot_.schema.assign("state-fields/v2") && snapshot_.data.assign(bytes_,used);
  }
  void receive_(const char *topic,const uint8_t *payload,size_t length,uint32_t now) {
    // One bounded mailbox; parse only from the cooperative component loop.
    if(length==0 || length>1280) return;
    json::parse_json(payload,length,[this,topic,now](JsonObject root)->bool {
      if(std::strcmp(topic,state_topic_)==0) {
        if(!live_snapshot_(root)) return false;
        // Retained/live publications update the cache but never establish freshness.
        return client_->ingest_snapshot(snapshot_,"mqtt");
      }
      if(std::strcmp(topic,reply_topic_)!=0 || !pending_) return false;
      char expected[21]{};std::snprintf(expected,sizeof(expected),"%llu",(unsigned long long)transaction_);
      if(!root["transaction_id"].is<const char *>() || std::strcmp(root["transaction_id"].as<const char *>(),expected)!=0 ||
         !root["result"].is<const char *>() || std::strcmp(root["result"].as<const char *>(),"succeeded")!=0 ||
         !root["execution"]["started"].is<bool>() || !root["execution"]["started"].as<bool>()) return false;
      const JsonObject remote=root["remote_state"].as<JsonObject>();
      if(remote.isNull() || !remote["complete"].is<bool>() || !remote["complete"].as<bool>() ||
         !remote["schema"].is<const char *>() || std::strcmp(remote["schema"].as<const char *>(),"state-fields/v2")!=0 ||
         !remote["data_hex"].is<const char *>()) return false;
      const char *hex=remote["data_hex"].as<const char *>();size_t count=std::strlen(hex);
      if(count<20 || count>512 || count%2) return false;
      for(size_t i=0;i<count/2;++i) {
        const int high=hex_(hex[2*i]),low=hex_(hex[2*i+1]);if(high<0 || low<0) return false;
        bytes_[i]=uint8_t((high<<4)|low);
      }
      snapshot_={};snapshot_.completeness=net::NetStateCompleteness::COMPLETE;
      if(!snapshot_.schema.assign("state-fields/v2") || !snapshot_.data.assign(bytes_,count/2) ||
         !client_->ingest_snapshot(snapshot_,"mqtt_sync")) return false;
      synced_=true;proof_at_=now;pending_=false;retry_.recovered();since_=now;wait_=interval_+esp_random()%1001;
      ESP_LOGI("state.handover","SYNC fresh tx=%llu generation=%u revision=%u rtt_ms=%u",
          (unsigned long long)transaction_,unsigned(client_->state_generation()),unsigned(client_->state_revision()),unsigned(now-started_));
      return true;
    });
  }
  state_query_client::StateQueryClient *client_{nullptr};
  communication_net_protocol::MqttWireTransport wire_{};
  communication_net_protocol::StateRetryWindow retry_{{1000,1000,2000,30000,1000}};
  const char *state_topic_{nullptr},*command_topic_{nullptr},*reply_topic_{nullptr},*source_{nullptr},*target_{nullptr};
  net::NetStateSnapshot snapshot_{};uint8_t bytes_[256]{};char request_[704]{};
  uint32_t interval_{15000},freshness_{45000},since_{0},wait_{0},started_{0},proof_at_{0};
  uint64_t sequence_{0},transaction_{0},session_{0};
  bool connected_{false},pending_{false},synced_{false},healthy_{false};
};
}
#endif
