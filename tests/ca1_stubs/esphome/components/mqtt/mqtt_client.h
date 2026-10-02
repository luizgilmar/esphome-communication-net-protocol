#pragma once
#include <functional>
#include <string>
#include <vector>
namespace esphome { namespace mqtt {
class MQTTClientComponent {public:
 using Callback=std::function<void(const std::string&,const std::string&)>;
 bool connected=true; Callback callback; std::string topic; std::vector<std::string> published;
 bool is_connected()const{return connected;}
 void subscribe(const std::string&t,Callback c,uint8_t){topic=t;callback=c;}
 void unsubscribe(const std::string&){}
 bool publish(const std::string&,const char*p,size_t n,uint8_t,bool){if(!connected)return false;published.emplace_back(p,n);return true;}
};
inline MQTTClientComponent *global_mqtt_client=nullptr;
} }
