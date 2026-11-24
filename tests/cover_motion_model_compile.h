#pragma once
#include "esphome/components/communication_net_protocol/cover_motion_model.h"
#include "esphome/components/communication_net_protocol/cover_completion_lifecycle.h"
namespace cover_motion_checks {
using namespace esphome::communication_net_protocol;
constexpr CoverMotionFrame start() {
  CoverMotionFrame f;f.generation=2;f.revision=10;f.movement=4;f.position=0;f.target=10000;f.remaining_ms=19000;f.operation=1;
  constexpr char resource[]="cover/bench";for(size_t i=0;i<sizeof(resource);++i)f.resource[i]=resource[i];return f;
}
constexpr bool wire() {
  auto f=start();uint8_t bytes[96]{};size_t size=0;
  if(!encode_cover_motion(f,bytes,sizeof(bytes),size) || size!=35)return false;
  if(bytes[0]!=1 || bytes[1]!=2 || bytes[5]!=10 || bytes[9]!=4 || bytes[15]!=0x10 || bytes[16]!=0x27 || bytes[17]!=0x38 || bytes[18]!=0x4a || bytes[21]!=1 || bytes[22]!=1 || bytes[23]!=11)return false;
  CoverMotionFrame decoded;
  if(!decode_cover_motion(bytes,size,decoded) || decoded.target!=10000 || decoded.remaining_ms!=19000 || decoded.movement!=4)return false;
  auto unchanged=decoded;
  bytes[22]=2;if(decode_cover_motion(bytes,size,decoded) || decoded.movement!=unchanged.movement)return false;bytes[22]=1;
  if(decode_cover_motion(bytes,size-1,decoded) || decode_cover_motion(nullptr,size,decoded))return false;
  bytes[23]=0;if(decode_cover_motion(bytes,size,decoded))return false;
  f.target=10001;if(valid_cover_motion(f))return false;
  f=start();f.operation=2;if(valid_cover_motion(f))return false;
  f=start();f.remaining_ms=0;if(valid_cover_motion(f))return false;
  f=start();f.remaining_ms=COVER_MOTION_MAX_MS+1;if(valid_cover_motion(f))return false;
  f=start();f.planned=false;f.target=f.position;f.remaining_ms=0;if(!valid_cover_motion(f))return false;
  f=start();f.resource[0]=' ';if(valid_cover_motion(f))return false;
  return true;
}
constexpr bool interpolation_is_not_confirmation() {
  CoverMotionProjection p;auto f=start();if(!p.accept(f,100))return false;
  if(p.visual(100)!=0 || p.visual(9600)!=5000 || p.visual(19100)!=10000)return false;
  if(!p.awaiting_final(19100) || p.confirmed_sample().position!=0 || p.confirmed_sample().operation!=1)return false;
  if(p.recovery_due(20000,2000) || !p.recovery_due(21100,2000))return false;
  if(p.accept(f,20000))return false; // Retained/repeated start must not rewind animation.
  auto stop=f;stop.revision=11;stop.operation=0;stop.position=4200;stop.target=4200;stop.remaining_ms=0;
  if(!p.accept(stop,10000) || p.visual(10001)!=4200 || p.awaiting_final(999999))return false;
  if(p.accept(f,11000) || p.visual(11000)!=4200)return false;
  auto next=stop;next.revision=12;next.movement=5;next.operation=2;next.target=2000;next.remaining_ms=4400;
  if(!p.accept(next,12000) || p.visual(14200)!=3100)return false;
  auto invalid=next;invalid.target=10001;if(p.accept(invalid,13000) || p.visual(14200)!=3100)return false;
  auto reboot=stop;reboot.generation=3;reboot.revision=1;reboot.movement=0;
  if(!p.accept(reboot,15000) || p.accept(next,16000))return false;
  p.invalidate();return !p.known() && !p.visual_known() && !p.accept(next,17000) && !p.accept(reboot,17000) && p.accept(reboot,18000,true);
}
constexpr bool recovery_and_clock_wrap() {
  auto f=start();f.remaining_ms=10000;
  CoverMotionProjection p;if(!p.accept(f,UINT32_MAX-500) || p.visual(499)!=1000)return false;
  auto fresh=f;fresh.position=5000;fresh.remaining_ms=5000;
  if(!p.accept(fresh,5000,true) || p.visual(7500)!=7500)return false;
  auto unknown=f;unknown.revision=11;unknown.planned=false;unknown.target=unknown.position;unknown.remaining_ms=0;
  return p.accept(unknown,8000) && !p.visual_known() && !p.awaiting_final(999999);
}
constexpr bool completion() {
  CoverCompletionProgress p;
  if(cover_was_interrupted(p,0,0,false))return false;
  if(cover_was_interrupted(p,1,7,false))return false;
  if(!cover_was_interrupted(p,0,7,false))return false;
  if(cover_was_interrupted(p,0,7,true))return false;
  if(!cover_was_interrupted(p,1,8,false))return false;
  if(!cover_was_interrupted(p,2,7,false))return false;
  return true;
}
static_assert(completion(), "External controllers must release obsolete commands");
static_assert(wire(), "Motion wire contract regression");
static_assert(interpolation_is_not_confirmation(), "Visual estimate must not become confirmed state");
static_assert(recovery_and_clock_wrap(), "Resynchronization and unsigned elapsed time regression");
}
