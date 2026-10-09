import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import espnow_net_protocol
from esphome.const import CONF_ID
import esphome.final_validate as fv
DEPENDENCIES = ["espnow_net_protocol"]
ns = cg.esphome_ns.namespace("state_query_client")
StateQueryClient = ns.class_("StateQueryClient", cg.Component)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(StateQueryClient),
    cv.Required("radio_id"): cv.use_id(espnow_net_protocol.EspNowNetProtocolComponent),
    cv.Required("peer"): cv.All(cv.string_strict, cv.Length(min=1,max=63)),
    cv.Required("source"): cv.All(cv.string_strict, cv.Length(min=1,max=63)),
    cv.Required("target"): cv.All(cv.string_strict, cv.Length(min=1,max=63)),
    cv.Optional("subscribe",default=False): cv.boolean,
    cv.Optional("interval",default="15s"): cv.All(cv.positive_time_period_milliseconds, cv.Range(min=cv.TimePeriod(milliseconds=5000),max=cv.TimePeriod(milliseconds=3600000))),
    cv.Optional("timeout",default="4s"): cv.All(cv.positive_time_period_milliseconds, cv.Range(min=cv.TimePeriod(milliseconds=500),max=cv.TimePeriod(milliseconds=10000))),
}).extend(cv.COMPONENT_SCHEMA)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_subscribe(config["subscribe"]))
    radio = await cg.get_variable(config["radio_id"])
    cg.add(var.configure(radio,config["peer"],config["source"],config["target"],config["interval"].total_milliseconds,config["timeout"].total_milliseconds))

def _final_validate(config):
    full = fv.full_config.get()
    radio = full.get("espnow_net_protocol", {})
    if radio.get(CONF_ID) != config["radio_id"]:
        raise cv.Invalid("state_query_client requires its configured radio endpoint")
    if config["peer"] not in {peer["id"] for peer in radio.get("peers", [])}:
        raise cv.Invalid("state_query_client.peer must name a configured peer")
    if not full.get("communication_net_protocol", {}).get("library_only", False):
        raise cv.Invalid("query fixture requires communication library_only; do not share its observer with runtime")
    return config
FINAL_VALIDATE_SCHEMA = _final_validate
