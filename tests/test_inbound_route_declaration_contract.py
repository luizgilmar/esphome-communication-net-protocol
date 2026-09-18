from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_route_declaration_has_no_live_dispatch():
    schema = (ROOT / "components/communication_net_protocol/__init__.py").read_text()
    source = (ROOT / "components/communication_net_protocol/communication_net_protocol.cpp").read_text()
    assert 'cv.Optional(CONF_INBOUND): INBOUND_SCHEMA' in schema
    assert 'cv.Length(min=1, max=16)' in schema
    assert 'duplicate inbound binding' in schema
    assert 'var.add_inbound_route' in schema
    assert 'automation.validate_automation' in schema
    assert 'automation.build_automation(trigger, [], binding)' in schema
    assert 'cv.Optional(CONF_COMPLETION): LIGHT_COMPLETION_SCHEMA' in schema
    assert 'cv.Required(CONF_LIGHT_ID): cv.use_id(light.LightState)' in schema
    assert 'trigger.set_completion_timeout' in schema
    binding = (ROOT / "components/communication_net_protocol/declarative_inbound_binding.h").read_text()
    assert 'class LightState;' in binding
    assert '#include "esphome/components/light/light_state.h"' not in binding
    assert 'dispatch disabled' in source
    assert 'command_gate_.admit' not in source
