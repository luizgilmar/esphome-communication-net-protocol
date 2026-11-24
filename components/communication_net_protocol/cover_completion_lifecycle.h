#pragma once
#include <cstdint>
namespace esphome::communication_net_protocol {
struct CoverCompletionProgress {bool moving_seen{false};uint8_t direction{0};uint32_t movement{0};};
constexpr bool cover_was_interrupted(CoverCompletionProgress &progress,uint8_t operation,uint32_t movement,bool completed) {
  if(operation!=0) {
    if(!progress.moving_seen){progress={true,operation,movement};return false;}
    return progress.direction!=operation || (progress.movement!=0 && movement!=0 && progress.movement!=movement);
  }
  return progress.moving_seen && !completed;
}
}
