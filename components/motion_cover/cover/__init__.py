import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import cover
from esphome.const import CONF_ID
from esphome.core import CORE
from .. import motion_cover_ns
MotionCover = motion_cover_ns.class_("MotionCover", cover.Cover, cg.Component)
DEPENDENCIES = ["communication_net_protocol"]
CONFIG_SCHEMA = cover.cover_schema(MotionCover).extend({cv.Required("backend_id"):cv.use_id(cover.Cover)}).extend(cv.COMPONENT_SCHEMA)


def validate_backend(config, installation):
    native=next((c for c in installation.get("cover",[]) if str(c.get("id"))==str(config["backend_id"])),None)
    if not native or native.get("platform")!="time_based" or not native.get("internal",False):
        raise cv.Invalid("Motion cover requires an internal native time_based backend; all controllers must use the public proxy")
    owners=[c for c in installation.get("cover",[]) if c.get("platform")=="motion_cover" and str(c.get("backend_id"))==str(config["backend_id"])]
    if len(owners)>1:raise cv.Invalid("Each internal cover must have exactly one public motion owner")
    for key in ("open_duration","close_duration"):
        if not 1<=native[key].total_milliseconds<=86400000:
            raise cv.Invalid("Motion calibration must be within 1ms..24h")
    return config
def final_validate(config):
    return validate_backend(config, fv.full_config.get())
FINAL_VALIDATE_SCHEMA=final_validate


async def to_code(config):
    var=await cover.new_cover(config)
    await cg.register_component(var,config)
    backend=await cg.get_variable(config["backend_id"])
    native=next(c for c in CORE.config.get("cover",[]) if str(c.get("id"))==str(config["backend_id"]))
    cg.add_define("USE_MOTION_COVER")
    cg.add(var.set_backend(backend,native["open_duration"].total_milliseconds,native["close_duration"].total_milliseconds))
