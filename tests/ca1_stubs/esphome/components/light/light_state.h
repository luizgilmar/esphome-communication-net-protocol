#pragma once
#include <string>
namespace esphome { namespace light {
struct Values { bool is_on() const{return false;} float get_red()const{return 0;} float get_green()const{return 0;} float get_blue()const{return 0;} float get_brightness()const{return 0;} };
class LightState {public: Values current_values; std::string get_effect_name()const{return "None";} };
} }
