#pragma once
#include "esphome/components/communication_net_protocol/state_observation_client.h"
namespace observation_timing_regression {
using Client = esphome::communication_net_protocol::StateObservationClient;
static_assert(Client::renewal_interval_for(60000, 0) == 20000, "TX default unchanged");
static_assert(Client::renewal_interval_for(60000, 15000) == 15000, "consumer freshness bound");
static_assert(Client::renewal_interval_for(30000, 60000) == 10000, "server validity remains authoritative");
static_assert(Client::renewal_interval_for(300000, 15000) == 15000, "long server validity cannot age consumer cache");
static_assert(Client::renewal_interval_for(60000, 5000) == 5000, "minimum configured renewal cadence");
}
