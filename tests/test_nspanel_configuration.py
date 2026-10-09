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
