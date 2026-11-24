#pragma once
#include <cstddef>
#include <cstdint>
namespace esphome::communication_net_protocol {
constexpr uint32_t COVER_MOTION_MAX_MS=86400000;
struct CoverMotionFrame {
  uint32_t generation{0},revision{0},movement{0},remaining_ms{0};
  uint16_t position{0},target{0}; // basis points, 0..10000
  uint8_t operation{0}; // idle/opening/closing
  bool planned{true};
  char resource[64]{};
};
constexpr bool valid_cover_motion(const CoverMotionFrame &f) {
  if(f.generation==0 || f.revision==0 || f.position>10000 || f.target>10000 || f.operation>2 || f.remaining_ms>COVER_MOTION_MAX_MS) return false;
  size_t n=0;
  for(;n<64 && f.resource[n];++n) {
    const char c=f.resource[n];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='_'||c=='-'||c=='.')) return false;
  }
  if(n==0 || n>63) return false;
  if(f.operation==0) return f.remaining_ms==0 && f.target==f.position;
  if(f.movement==0) return false;
  if(!f.planned) return f.remaining_ms==0 && f.target==f.position;
  return f.remaining_ms>0 && (f.operation==1 ? f.target>f.position : f.target<f.position);
}
constexpr void motion_put32(uint8_t *p,uint32_t v) {for(size_t i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
constexpr uint32_t motion_get32(const uint8_t *p) {return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
constexpr bool encode_cover_motion(const CoverMotionFrame &f,uint8_t *out,size_t capacity,size_t &used) {
  if(!valid_cover_motion(f) || out==nullptr) return false;
  size_t n=0;while(f.resource[n])++n;
  if(capacity<24+n) return false;
  out[0]=1;motion_put32(out+1,f.generation);motion_put32(out+5,f.revision);motion_put32(out+9,f.movement);
  out[13]=uint8_t(f.position);out[14]=uint8_t(f.position>>8);out[15]=uint8_t(f.target);out[16]=uint8_t(f.target>>8);
  motion_put32(out+17,f.remaining_ms);out[21]=f.operation;out[22]=f.planned?1:0;out[23]=uint8_t(n);
  for(size_t i=0;i<n;++i)out[24+i]=uint8_t(f.resource[i]);used=24+n;return true;
}
constexpr bool decode_cover_motion(const uint8_t *data,size_t size,CoverMotionFrame &output) {
  if(data==nullptr || size<25 || data[0]!=1 || data[22]>1 || data[23]==0 || data[23]>63 || size!=size_t(24+data[23])) return false;
  CoverMotionFrame f;f.generation=motion_get32(data+1);f.revision=motion_get32(data+5);f.movement=motion_get32(data+9);
  f.position=uint16_t(data[13])|(uint16_t(data[14])<<8);f.target=uint16_t(data[15])|(uint16_t(data[16])<<8);
  f.remaining_ms=motion_get32(data+17);f.operation=data[21];f.planned=data[22]!=0;
  for(size_t i=0;i<data[23];++i)f.resource[i]=char(data[24+i]);
  if(!valid_cover_motion(f)) return false;output=f;return true;
}
// Visual progression never changes the authoritative sample or confirms completion.
class CoverMotionProjection {
 public:
  constexpr bool accept(const CoverMotionFrame &frame,uint32_t now,bool correlated=false) {
    if(!valid_cover_motion(frame)) return false;
    if(seen_ && (frame.generation<frame_.generation ||
      (frame.generation==frame_.generation && (frame.revision<frame_.revision || (!correlated && frame.revision==frame_.revision))))) return false;
    frame_=frame;received_=now;known_=true;seen_=true;return true;
  }
  constexpr bool known() const {return known_;}
  constexpr const CoverMotionFrame &confirmed_sample() const {return frame_;}
  constexpr bool visual_known() const {return known_ && (frame_.operation==0 || frame_.planned);}
  constexpr uint16_t visual(uint32_t now) const {
    if(!visual_known() || frame_.operation==0) return frame_.position;
    const uint32_t elapsed=uint32_t(now-received_);
    if(elapsed>=frame_.remaining_ms) return frame_.target;
    const uint32_t distance=frame_.target>=frame_.position ? frame_.target-frame_.position : frame_.position-frame_.target;
    const uint16_t travelled=uint16_t(uint64_t(distance)*elapsed/frame_.remaining_ms);
    return frame_.operation==1 ? frame_.position+travelled : frame_.position-travelled;
  }
  constexpr bool awaiting_final(uint32_t now) const {
    return known_ && frame_.operation!=0 && frame_.planned && uint32_t(now-received_)>=frame_.remaining_ms;
  }
  constexpr bool recovery_due(uint32_t now,uint32_t grace) const {
    return awaiting_final(now) && uint64_t(uint32_t(now-received_))>=uint64_t(frame_.remaining_ms)+grace;
  }
  constexpr void invalidate() {known_=false;}
 private:
  CoverMotionFrame frame_{};uint32_t received_{0};bool known_{false},seen_{false};
};
} // namespace esphome::communication_net_protocol
