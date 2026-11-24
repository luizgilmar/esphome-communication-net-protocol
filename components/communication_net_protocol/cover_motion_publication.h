#pragma once
#include "esphome/core/defines.h"
#ifdef USE_COVER_MOTION_PUBLISHER
#include "esphome/components/espnow_net_protocol/message_model.h"
namespace esphome::cover {class Cover;}
namespace esphome::communication_net_protocol {
class CoverMotionPublications {
 public:
  virtual ~CoverMotionPublications()=default;
  virtual size_t count() const=0;
  virtual int query_index(const char *resource) const=0;
  virtual uint64_t version(size_t index)=0;
  virtual uint32_t movement_for(const cover::Cover *cover) const=0;
  virtual bool write_motion(size_t index,espnow_net_protocol::NetStateSnapshot &snapshot)=0;
};
}
#endif
