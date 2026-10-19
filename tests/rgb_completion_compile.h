#pragma once
#include "esphome/components/communication_net_protocol/rgb_completion.h"
namespace rgb_completion_regression {
using esphome::communication_net_protocol::rgb_completion_matches;
static_assert(rgb_completion_matches(0.125f, 0.5f, 1.0f, 20, 80, 160), "non-full-scale request normalizes");
static_assert(!rgb_completion_matches(20.0f/255, 80.0f/255, 160.0f/255, 20, 80, 160), "raw unnormalized channels do not match");
static_assert(rgb_completion_matches(1.0f, 166.0f/255, 77.0f/255, 255, 166, 77), "existing full-scale presets preserved");
static_assert(!rgb_completion_matches(1.0f, 0.5f, 0.125f, 20, 80, 160), "wrong hue rejected");
static_assert(rgb_completion_matches(1.0f, 1.0f, 1.0f, 0, 0, 0), "ESPHome zero RGB normalization");
static_assert(rgb_completion_matches(1.0f, 0.0f, 0.0f, 1, 0, 0), "small nonzero primary normalizes");
static_assert(rgb_completion_matches(0.0f, 1.0f, 0.0f, 0, 255, 0), "zero channels preserved");
static_assert(!rgb_completion_matches(-0.1f, 0.5f, 1.0f, 20, 80, 160), "invalid observed channel rejected");
}
