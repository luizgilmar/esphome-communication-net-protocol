from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_route_declaration_supports_bounded_active_execution():
    schema = (ROOT / "components/communication_net_protocol/__init__.py").read_text()
    source = (ROOT / "components/communication_net_protocol/communication_net_protocol.cpp").read_text()
    assert 'cv.Optional(CONF_INBOUND): INBOUND_SCHEMA' in schema
    assert 'cv.Length(min=1, max=32)' in schema
    assert 'duplicate inbound binding' in schema
    assert 'var.add_inbound_route' in schema
    assert 'automation.validate_automation' in schema
    assert 'automation.build_automation(trigger, [], binding)' in schema
    assert 'cv.Optional(CONF_COMPLETION): LIGHT_COMPLETION_SCHEMA' in schema
    assert 'cv.Optional(CONF_LIGHT_ID): cv.use_id(light.LightState)' in schema
    assert 'cv.Optional(CONF_BINARY_SENSOR_ID): cv.use_id(binary_sensor.BinarySensor)' in schema
    assert 'completion requires exactly one of light_id, binary_sensor_id or delay' in schema
    assert 'cv.Optional(CONF_DELAY): cv.positive_time_period_milliseconds' in schema
    assert 'binary sensor completion cannot use light options' in schema
    assert 'trigger.set_completion_timeout' in schema
    binding = (ROOT / "components/communication_net_protocol/declarative_inbound_binding.h").read_text()
    assert 'class LightState;' in binding
    assert '#include "esphome/components/light/light_state.h"' not in binding
    assert 'this->route_admission_.admit(view, route)' in source
    assert 'this->route_admission_.complete(' in source
    assert 'sensor->has_state()' in source
    assert 'binary sensor state unavailable' in source


def test_interruptible_inbound_is_opt_in_and_transaction_aware():
    schema = (ROOT / "components/communication_net_protocol/__init__.py").read_text()
    source = (ROOT / "components/communication_net_protocol/communication_net_protocol.cpp").read_text()
    header = (ROOT / "components/communication_net_protocol/communication_net_protocol.h").read_text()
    binding = (ROOT / "components/communication_net_protocol/declarative_inbound_binding.h").read_text()
    assert 'CONF_INTERRUPTIBLE = "interruptible"' in schema
    assert 'CONF_INTERRUPTS_ACTIVE = "interrupts_active"' in schema
    assert 'interrupt binding {name} must use command: stop' in schema
    assert 'interrupt binding {name} requires completion.delay' in schema
    assert 'USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND' in schema
    assert 'set_interruptible(binding[CONF_INTERRUPTIBLE])' in schema
    assert 'set_interrupts_active(binding[CONF_INTERRUPTS_ACTIVE])' in schema
    assert 'bool interruptible() const' in binding
    assert 'bool interrupts_active() const' in binding
    assert 'has_result(uint64_t transaction_id) const override' in header
    assert 'take_result(uint64_t transaction_id,' in header
    assert 'NetErrorCode::INTERRUPTED' in source
