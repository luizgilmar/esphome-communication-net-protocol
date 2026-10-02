#pragma once
namespace esphome { template<class... T> class Trigger {public: unsigned calls=0; void trigger(T...){++calls;} }; }
