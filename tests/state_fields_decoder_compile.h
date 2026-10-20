#pragma once
#include "esphome/components/communication_net_protocol/state_fields_decoder.h"
namespace snapshot_decoder_regression {
using namespace esphome::communication_net_protocol;
constexpr bool regression(unsigned variant) {
  uint8_t frame[24]{2,1,0,0,0,1,0,0,0,2,1,'a',7,32,128,255,102,1,'b',1,0,0,0,0};
  size_t length = sizeof(frame);
  if (variant == 1) --length;
  if (variant == 2) frame[18] = 'a';
  if (variant == 3) frame[12] = 0x80;
  if (variant == 4) frame[1] = 0;
  if (variant == 5) frame[9] = 9;
  if (variant == 6) frame[11] = 0;
  if (variant == 7) frame[9] = 1;
  ObservedLightSnapshot result{};
  result.revision = 123;
  const bool ok = decode_state_fields_v2(frame, length, result);
  if (variant == 0) return ok && result.count == 2 && result.fields[0].blue == 255 && result.fields[0].brightness == 102;
  return !ok && result.revision == 123;  // No partial commit on invalid frames.
}
static_assert(regression(0), "valid multi-field snapshot");
static_assert(regression(1), "truncated snapshot rejected");
static_assert(regression(2), "duplicate field rejected");
static_assert(regression(3), "reserved flags rejected");
static_assert(regression(4), "zero generation rejected");
static_assert(regression(5), "over-capacity rejected");
static_assert(regression(6), "embedded NUL rejected");
static_assert(regression(7), "trailing bytes rejected");
constexpr bool capacity(unsigned count) {
  uint8_t frame[66]{};
  frame[0] = 2; frame[1] = 1; frame[5] = 1; frame[9] = count;
  for (unsigned i = 0; i < count; ++i) {
    const unsigned at = 10 + 7 * i;
    frame[at] = 1; frame[at + 1] = 'a' + i; frame[at + 2] = 1;
  }
  ObservedLightSnapshot result{};
  return decode_state_fields_v2(frame, 10 + 7 * count, result) && result.count == count;
}
static_assert(capacity(6), "six-field producer accepted");
static_assert(capacity(8), "eight-field capacity accepted");
static_assert(state_version_allowed(2, 1, 1, 999, false), "new generation allows revision restart");
static_assert(!state_version_allowed(1, 999, 2, 1, true), "old generation rejected");
static_assert(!state_version_allowed(2, 4, 2, 5, true), "old revision rejected");
static_assert(state_version_allowed(2, 5, 2, 5, true), "correlated equal revision refreshes freshness");
static_assert(!state_version_allowed(2, 5, 2, 5, false), "duplicate push cannot refresh freshness");
}
