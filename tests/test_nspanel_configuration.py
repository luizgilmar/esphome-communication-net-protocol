from copy import deepcopy
from pathlib import Path
import sys
import pytest
from esphome.core import CORE
from esphome.config import read_config
import esphome.config_validation as cv

@pytest.fixture(scope="module")
def schema():
    CORE.config_path = Path("examples/nspanel_hub_schema_check.yaml").resolve()
    config = read_config({})
    assert config is not None
    module = sys.modules["esphome.components.communication_net_protocol"]
    return module, config["communication_net_protocol"]

def test_legacy_pair_and_second_origin_remain_declarative(schema):
    module, config = schema
    assert module._validate(deepcopy(config))["inbound"]["normal_executions"] == 2
    assert config["mqtt"]["source_id"] != config["mqtt"]["sources"][0]["source_id"]


def test_snapshot_schema_accepts_six_fields_with_individual_rgb(schema):
    module, _ = schema
    fields = [{"field": "spot", "light_ids": ["spot"]},
              {"field": "strips", "light_ids": ["strips"], "rgb": True},
              {"field": "panel1", "binary_sensor_id": "panel1"},
              {"field": "panel2", "binary_sensor_id": "panel2"},
              {"field": "general", "light_ids": ["general"]},
              {"field": "superior", "light_ids": ["superior"], "rgb": True}]
    result = module.STATE_SNAPSHOT_SCHEMA({"topic": "test/state", "fields": fields})
    assert len(result["fields"]) == 6


def test_snapshot_schema_rejects_payload_overflow(schema):
    module, _ = schema
    fields = [{"field": f"f{i}", "light_ids": [f"light{i}"], "rgb": True}
              for i in range(4)]
    with pytest.raises(cv.Invalid, match="MQTT payload"):
        module.STATE_SNAPSHOT_SCHEMA({"topic": "test/state", "fields": fields})

@pytest.mark.parametrize("change", ["duplicate_source", "duplicate_reply", "missing_legacy_reply", "wildcard_reply"])
def test_invalid_origin_configuration_is_rejected(schema, change):
    module, original = schema
    config = deepcopy(original)
    if change == "duplicate_source":
        config["mqtt"]["sources"][0]["source_id"] = config["mqtt"]["source_id"]
    elif change == "duplicate_reply":
        config["mqtt"]["sources"][0]["reply_topic"] = config["mqtt"]["reply_topic"]
    elif change == "missing_legacy_reply":
        del config["mqtt"]["reply_topic"]
    else:
        config["mqtt"]["sources"][0]["reply_topic"] = "results/#"
    with pytest.raises(cv.Invalid):
        module._validate(config)

@pytest.mark.parametrize("change", ["reverse_range", "brightness_overflow", "wrong_rgb_type", "position_overflow"])
def test_argument_completion_configuration_is_checked(schema, change):
    module, original = schema
    config = deepcopy(original)
    bindings = {b["id"]: b for b in config["inbound"]["bindings"]}
    if change == "reverse_range":
        bindings["check_position"]["arguments"].update(min=80, max=20)
    elif change == "brightness_overflow":
        bindings["check_brightness"]["arguments"]["max"] = 101
    elif change == "wrong_rgb_type":
        bindings["check_rgb"]["arguments"]["type"] = "value"
    else:
        bindings["check_position"]["arguments"]["max"] = 101
    with pytest.raises(cv.Invalid):
        module._validate(config)


@pytest.mark.parametrize("action,arguments", [
    ("apply_rgb", {"type": "value", "min": 0, "max": 100}),
    ("apply_position", {"type": "rgb", "min": 0, "max": 100}),
    ("apply_brightness", {"type": "value", "min": 0, "max": 100}),
    ("apply_position", {"type": "value", "min": 0, "max": 101}),
])
def test_argument_actions_reject_incompatible_binding_even_when_nested(schema, action, arguments):
    module, _ = schema
    node = [{"if": {"then": [{"communication_net_protocol." + action: {"id": "target"}}]}}]
    with pytest.raises(cv.Invalid):
        module._validate_argument_actions(node, arguments)


@pytest.mark.parametrize("action,arguments", [
    ("apply_rgb", {"type": "rgb", "min": 0, "max": 65535}),
    ("apply_position", {"type": "value", "min": 0, "max": 100}),
    ("apply_brightness", {"type": "value", "min": 1, "max": 100}),
])
def test_argument_actions_accept_their_typed_binding(schema, action, arguments):
    module, _ = schema
    module._validate_argument_actions([{"communication_net_protocol." + action: {"id": "target"}}], arguments)
