#pragma once
#include "esphome/components/communication_net_protocol/cover_state_decoder.h"
namespace cover_state_decoder_checks {
constexpr bool cases() {
  using namespace esphome::communication_net_protocol;
  ObservedCoverState out{77,2};
  const uint8_t closed[]{0,0}, open[]{100,0}, opening[]{25,1}, closing[]{75,2};
  const uint8_t bad_position[]{101,0}, bad_operation[]{50,3}, long_frame[]{50,0,0};
  if(!decode_cover_state_v1(closed,2,out) || out.position!=0 || out.operation!=0) return false;
  if(!decode_cover_state_v1(open,2,out) || out.position!=100) return false;
  if(!decode_cover_state_v1(opening,2,out) || out.operation!=1) return false;
  if(!decode_cover_state_v1(closing,2,out) || out.operation!=2) return false;
  if(decode_cover_state_v1(nullptr,2,out) || decode_cover_state_v1(closed,1,out) ||
     decode_cover_state_v1(long_frame,3,out) || decode_cover_state_v1(bad_position,2,out) ||
     decode_cover_state_v1(bad_operation,2,out)) return false;
  return out.position==75 && out.operation==2;
}
static_assert(cases(), "Cover state frame validation regression");
} // namespace cover_state_decoder_checks
