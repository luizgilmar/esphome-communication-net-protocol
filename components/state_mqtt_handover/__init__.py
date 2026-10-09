import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
import re
from esphome.components import state_query_client
from esphome.const import CONF_ID

DEPENDENCIES = ["mqtt", "state_query_client"]
AUTO_LOAD = ["json"]
ns=cg.esphome_ns.namespace("state_mqtt_handover")
StateMqttHandover=ns.class_("StateMqttHandover",cg.Component)

def wire_text(value):
    value=cv.string_strict(value)
    if not value or len(value)>192 or any(c in value for c in '\\"+#\x00') or any(ord(c)<32 for c in value):
        raise cv.Invalid("MQTT topic must be nonempty, bounded and contain no wildcards or JSON escapes")
    return value

def timing(value):
    return cv.All(cv.positive_time_period_milliseconds,cv.Range(min=cv.TimePeriod(milliseconds=5000),max=cv.TimePeriod(milliseconds=300000)))(value)

def identifier(value):
    value=cv.string_strict(value)
    if len(value)>63 or not re.fullmatch(r"[a-zA-Z0-9_-]+",value):
        raise cv.Invalid("source/target must be 1..63 ASCII letters, digits, underscores or hyphens")
    return value

def validate(config):
    if config["freshness"].total_milliseconds < 2*config["query_interval"].total_milliseconds:
        raise cv.Invalid("freshness must be at least twice query_interval")
    if len({config[k] for k in ("state_topic","command_topic","reply_topic")})!=3:
        raise cv.Invalid("state, command and reply topics must be distinct")
    return config

CONFIG_SCHEMA=cv.All(cv.Schema({
    cv.GenerateID():cv.declare_id(StateMqttHandover),
    cv.Required("client_id"):cv.use_id(state_query_client.StateQueryClient),
    cv.Required("state_topic"):wire_text,
    cv.Required("command_topic"):wire_text,
    cv.Required("reply_topic"):wire_text,
    cv.Required("source"):identifier,
    cv.Required("target"):identifier,
    cv.Optional("query_interval",default="15s"):timing,
    cv.Optional("freshness",default="45s"):timing,
}).extend(cv.COMPONENT_SCHEMA),validate)

def final_validate(config):
    client=fv.full_config.get().get("state_query_client",{})
    if client.get(CONF_ID)!=config["client_id"] or not client.get("subscribe",False):
        raise cv.Invalid("handover requires its exclusive state_query_client with subscribe: true")
    if client["source"]!=config["source"] or client["target"]!=config["target"]:
        raise cv.Invalid("MQTT source/target must match the radio client")
    if "on_json_message" in fv.full_config.get().get("mqtt",{}):
        # No blanket ban: own exact reply/state topics are the only exclusive routes.
        for route in fv.full_config.get()["mqtt"]["on_json_message"]:
            if route.get("topic") in (config["reply_topic"],config["state_topic"]):
                raise cv.Invalid("handover owns its exact state and reply subscriptions")
    return config

FINAL_VALIDATE_SCHEMA=final_validate

async def to_code(config):
    var=cg.new_Pvariable(config[CONF_ID]);await cg.register_component(var,config)
    client=await cg.get_variable(config["client_id"])
    cg.add_define("USE_STATE_MQTT_HANDOVER")
    cg.add(var.configure(client,config["state_topic"],config["command_topic"],config["reply_topic"],
        config["source"],config["target"],config["query_interval"].total_milliseconds,config["freshness"].total_milliseconds))
