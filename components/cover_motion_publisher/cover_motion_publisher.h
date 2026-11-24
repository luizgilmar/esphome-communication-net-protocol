#pragma once
#include "esphome/core/defines.h"
#ifdef USE_COVER_MOTION_PUBLISHER
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/motion_cover/motion_cover.h"
#include "esphome/components/communication_net_protocol/cover_motion_publication.h"
#include "esphome/components/communication_net_protocol/state_traffic_scheduler.h"
namespace esphome::cover_motion_publisher {
namespace net=espnow_net_protocol;
namespace protocol=communication_net_protocol;
class CoverMotionPublisher : public Component, public protocol::CoverMotionPublications {
 public:
  explicit CoverMotionPublisher(uint32_t key):key_(key){}
  float get_setup_priority() const override {return setup_priority::DATA-2;}
  void add_source(motion_cover::MotionCover *cover,const char *resource,const char *query,const char *topic,uint32_t heartbeat) {
    Source s;s.cover=cover;s.resource=resource;s.query=query;s.topic=topic;s.heartbeat=heartbeat;sources_.push_back(s);
  }
  size_t count() const override {return sources_.size();}
  uint32_t movement_for(const cover::Cover *cover) const override {
    for(const auto &s:sources_)if(s.cover==cover)return s.cover->movement();return 0;
  }
  int query_index(const char *resource) const override {
    for(size_t i=0;i<sources_.size();++i)if(sources_[i].query==resource)return int(i);return -1;
  }
  void setup() override {
    auto pref=global_preferences->make_preference<uint32_t>(key_);uint32_t old=0;pref.load(&old);const uint32_t next=old+1;
    if(old==UINT32_MAX || !pref.save(&next) || !global_preferences->sync() || sources_.empty() || sources_.size()>8) {mark_failed();return;}
    generation_=old+1;for(auto &s:sources_)s.last_heartbeat=millis();
  }
  uint64_t version(size_t i) override {
    if(is_failed() || generation_==0 || i>=sources_.size())return 0;
    refresh_(sources_[i],millis());return (uint64_t(generation_)<<32)|sources_[i].revision;
  }
  bool write_motion(size_t i,net::NetStateSnapshot &snapshot) override {
    if(is_failed() || generation_==0 || i>=sources_.size())return false;
    auto &s=sources_[i];refresh_(s,millis());protocol::CoverMotionFrame frame;
    if(!s.cover->sample(frame) || s.resource.size()>63)return false;
    frame.generation=generation_;frame.revision=s.revision;std::memcpy(frame.resource,s.resource.c_str(),s.resource.size()+1);
    uint8_t data[96];size_t used=0;
    if(!protocol::encode_cover_motion(frame,data,sizeof(data),used))return false;
    snapshot={};snapshot.completeness=net::NetStateCompleteness::COMPLETE;
    return snapshot.schema.assign("cover-motion/v1") && snapshot.data.assign(data,used);
  }
  void loop() override {
    if(is_failed() || generation_==0)return;
    const uint32_t now=millis();auto *client=mqtt::global_mqtt_client;const bool connected=client!=nullptr && client->is_connected();
    if(connected && !connected_)for(auto &s:sources_)s.pending=true;connected_=connected;
    for(auto &s:sources_)refresh_(s,now);
    if(!connected || (sent_ && uint32_t(now-last_send_)<500) || !retry_.ready(now))return;
    for(size_t n=0;n<sources_.size();++n) {
      size_t i=(next_+n)%sources_.size();auto &s=sources_[i];if(!s.pending)continue;
      net::NetStateSnapshot frame;if(!write_motion(i,frame))return;
      char hex[193]{};constexpr char digits[]="0123456789abcdef";
      for(size_t j=0;j<frame.data.size();++j){hex[2*j]=digits[frame.data.data()[j]>>4];hex[2*j+1]=digits[frame.data.data()[j]&15];}
      char payload[256];const int length=std::snprintf(payload,sizeof(payload),"{\"schema\":\"cover-motion/v1\",\"data_hex\":\"%s\"}",hex);
      sent_=true;last_send_=now;next_=(i+1)%sources_.size();
      if(length>0 && length<int(sizeof(payload)) && client->publish(s.topic,payload,size_t(length),1,true)) {
        s.pending=false;retry_.recovered();
        ESP_LOGI("cover.motion","PUBLISHED resource=%s generation=%u revision=%u movement=%u",s.resource.c_str(),unsigned(generation_),unsigned(s.revision),unsigned(s.cover->movement()));
      } else retry_.defer(now,0);
      return;
    }
  }
 private:
  struct Source {motion_cover::MotionCover *cover{nullptr};std::string resource,query,topic;
    uint32_t revision{1},model_revision{0},heartbeat{60000},last_heartbeat{0};bool pending{true};};
  void refresh_(Source &s,uint32_t now) {
    if(s.model_revision==s.cover->revision() && (s.heartbeat==0 || uint32_t(now-s.last_heartbeat)<s.heartbeat))return;
    if(s.revision==UINT32_MAX){mark_failed();return;}
    s.model_revision=s.cover->revision();++s.revision;s.last_heartbeat=now;s.pending=true;
  }
  std::vector<Source> sources_;uint32_t key_,generation_{0},last_send_{0};size_t next_{0};bool connected_{false},sent_{false};
  protocol::StateRetryWindow retry_{{500,1000,1000,30000,1000}};
};
}
#endif
