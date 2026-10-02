#pragma once
#include <cstdint>
namespace esphome { inline uint32_t test_time=0; inline uint32_t millis(){return test_time;} }
