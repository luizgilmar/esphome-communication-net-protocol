import zlib
import re
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.components.motion_cover.cover import MotionCover
DEPENDENCIES=["communication_net_protocol","mqtt","espnow_net_protocol"]
ns=cg.esphome_ns.namespace("cover_motion_publisher")
CoverMotionPublisher=ns.class_("CoverMotionPublisher",cg.Component)
Protocol=cg.esphome_ns.namespace("communication_net_protocol").class_("CommunicationNetProtocolComponent",cg.Component)

def bounded(value):
    value=cv.string_strict(value)
    if not value or len(value.encode())>63 or any(c in value for c in '+#'):
        raise cv.Invalid("Use a nonempty bounded resource/topic without wildcards")
    return value

CONFIG_SCHEMA=cv.Schema({cv.GenerateID():cv.declare_id(CoverMotionPublisher),cv.Required("protocol_id"):cv.use_id(Protocol),
    cv.Required("sources"):cv.All(cv.ensure_list(cv.Schema({cv.Required("cover_id"):cv.use_id(MotionCover),cv.Required("resource"):bounded,
      cv.Required("query_resource"):bounded,cv.Required("topic"):bounded,
      cv.Optional("heartbeat",default="60s"):cv.All(cv.positive_time_period_milliseconds,cv.Range(min=cv.TimePeriod(seconds=30),max=cv.TimePeriod(seconds=300)))})),cv.Length(min=1,max=8))}).extend(cv.COMPONENT_SCHEMA)

def validate_publications(config, installation):
    protocol=installation.get("communication_net_protocol",{})
    if str(protocol.get("id"))!=str(config["protocol_id"]) or "state_snapshot" not in protocol:
        raise cv.Invalid("Motion publisher requires the active protocol and existing snapshot server")
    for key in ("resource","query_resource","topic","cover_id"):
        values=[str(s[key]) for s in config["sources"]]
        if len(values)!=len(set(values)):raise cv.Invalid("Motion source resources/topics/covers must be unique")
    for source in config["sources"]:
        if not re.fullmatch(r"[A-Za-z0-9_./-]{1,63}",source["resource"]) or not re.fullmatch(r"state/cover/[A-Za-z0-9_./-]+",source["query_resource"]):
            raise cv.Invalid("Use a bounded motion resource and a dedicated read-only query route")
    known={s["resource"] for s in config["sources"]}
    for peer in protocol.get("state_snapshot",{}).get("observers",{}).get("peers",[]):
        if not set(peer.get("cover_resources",[]))<=known:raise cv.Invalid("Observer cover resource must reference a configured motion stream")
    return config
def final_validate(config):
    return validate_publications(config, fv.full_config.get())
FINAL_VALIDATE_SCHEMA=final_validate

async def to_code(config):
    protocol=await cg.get_variable(config["protocol_id"])
    definition=CORE.config["communication_net_protocol"]
    key=zlib.crc32(("cover-motion:"+definition["device_id"]).encode())
    cg.add_define("USE_COVER_MOTION_PUBLISHER")
    var=cg.new_Pvariable(config[CONF_ID],key);await cg.register_component(var,config)
    for source in config["sources"]:
        cg.add(var.add_source(await cg.get_variable(source["cover_id"]),source["resource"],source["query_resource"],source["topic"],source["heartbeat"].total_milliseconds))
    cg.add(protocol.set_cover_motion_publisher(var))
    for peer in definition.get("state_snapshot",{}).get("observers",{}).get("peers",[]):
        for resource in peer.get("cover_resources",[]):
            index=next(i for i,s in enumerate(config["sources"]) if s["resource"]==resource)
            cg.add(protocol.add_observer_cover_stream(peer["peer"],index))
