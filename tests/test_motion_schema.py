import ast
from pathlib import Path
import re
import pytest
import esphome.config_validation as cv
ROOT=Path(__file__).resolve().parents[1]

def function(path,name):
    tree=ast.parse((ROOT/path).read_text(encoding="utf-8"))
    node=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name==name)
    ns={"cv":cv,"re":re};exec(compile(ast.Module(body=[node],type_ignores=[]),str(path),"exec"),ns);return ns[name]

backend=function(Path("components/motion_cover/cover/__init__.py"),"validate_backend")
publications=function(Path("components/cover_motion_publisher/__init__.py"),"validate_publications")

def test_motion_uses_native_calibration_without_copying_durations_into_another_profile():
    cfg={"backend_id":"motor"};native={"id":"motor","platform":"time_based","internal":True,"open_duration":cv.TimePeriod(seconds=19),"close_duration":cv.TimePeriod(seconds=21)}
    assert backend(cfg,{"cover":[native]}) is cfg

@pytest.mark.parametrize("change",["public_backend","unsupported","two_owners","long_duration"])
def test_motion_backend_is_private_and_has_one_control_boundary(change):
    native={"id":"motor","platform":"time_based","internal":True,"open_duration":cv.TimePeriod(seconds=19),"close_duration":cv.TimePeriod(seconds=19)}
    covers=[native]
    if change=="public_backend":native["internal"]=False
    elif change=="unsupported":native["platform"]="template"
    elif change=="two_owners":covers.extend([{"platform":"motion_cover","backend_id":"motor"},{"platform":"motion_cover","backend_id":"motor"}])
    else:native["open_duration"]=cv.TimePeriod(hours=25)
    with pytest.raises(cv.Invalid):backend({"backend_id":"motor"},{"cover":covers})


def test_peer_interests_are_limited_to_declared_motion_resources():
    cfg={"protocol_id":"hub","sources":[{"cover_id":"blind","resource":"cover/blind","query_resource":"state/cover/blind","topic":"state/blind"}]}
    install={"communication_net_protocol":{"id":"hub","state_snapshot":{"observers":{"peers":[{"cover_resources":["cover/blind"]}]}}}}
    assert publications(cfg,install) is cfg
    install["communication_net_protocol"]["state_snapshot"]["observers"]["peers"][0]["cover_resources"]=["cover/other"]
    with pytest.raises(cv.Invalid):publications(cfg,install)
