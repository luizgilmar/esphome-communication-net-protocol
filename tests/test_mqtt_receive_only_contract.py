from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_mqtt_observation_is_explicit_and_does_not_execute():
    schema = (ROOT / "components/communication_net_protocol/__init__.py").read_text(
        encoding="utf-8"
    )
    source = (ROOT / "components/communication_net_protocol/communication_net_protocol.cpp").read_text(
        encoding="utf-8"
    )
    bench = (ROOT / "examples/communication_net_protocol_foundation_bench.yaml").read_text(
        encoding="utf-8"
    )
    assert 'cv.Optional(CONF_LISTEN_COMMANDS, default=False)' in schema
    assert 'cg.add_define("USE_COMMUNICATION_NET_MQTT_LISTENER")' in schema
    assert 'cg.add_define("USE_COMMUNICATION_NET_MQTT_COMMAND_PROBE")' in schema
    assert 'len(command_topic.encode("utf-8")) > 192' in schema
    assert "listen_commands: true" in bench
    assert "mqtt_wire_.subscribe" in source
    assert "mqtt_wire_.take_received" in source
    assert "not executed" in source
    assert "command_gate_.admit" not in source


def test_command_probe_storage_is_not_reserved_by_active_executor():
    header = (ROOT / "components/communication_net_protocol/communication_net_protocol.h").read_text(
        encoding="utf-8"
    )
    assert "#ifdef USE_COMMUNICATION_NET_MQTT_COMMAND_PROBE\n  uint32_t observed_commands_{0};" in header
    assert "defined(USE_COMMUNICATION_NET_MQTT_RESULT_OBSERVER)\n  uint8_t canonical_command_[192]{};" in header
