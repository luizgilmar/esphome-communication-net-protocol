from pathlib import Path


COMPONENT = Path("components/communication_net_protocol")


def test_wire_adapter_does_not_take_application_lifecycle_ownership():
    source = (COMPONENT / "mqtt_wire_transport.h").read_text(encoding="utf-8")
    assert "mqtt::global_mqtt_client->subscribe(" in source
    assert "mqtt::global_mqtt_client->publish(" in source
    for forbidden in ("NetResultStatus", "IN_PROGRESS", "JsonObject"):
        assert forbidden not in source


def test_wire_methods_are_linkable_from_generated_component_translation_unit():
    header = (COMPONENT / "mqtt_wire_transport.h").read_text(encoding="utf-8")
    source = (COMPONENT / "mqtt_wire_transport.cpp").read_text(encoding="utf-8")
    assert "bool subscribe(const char *topic, uint8_t qos = 1) {" in header
    assert "bool publish(const char *topic, const uint8_t *payload, size_t length," in header
    assert "bool MqttWireTransport::subscribe" not in source


def test_foundation_does_not_unconditionally_subscribe_or_publish():
    source = (COMPONENT / "communication_net_protocol.cpp").read_text(encoding="utf-8")
    assert "FOUNDATION ONLY" in source
    assert "USE_COMMUNICATION_NET_MQTT_LISTENER" in source
    assert "mqtt_wire_.subscribe" in source
    gate = "#ifdef USE_COMMUNICATION_NET_ACTIVE_GATE\nusing NetCommand"
    start = source.index(gate)
    # The active executor contains optional nested feature gates. Anchor its
    # outer boundary at the component loop instead of the first nested endif.
    boundary = "\n#endif\n\nvoid CommunicationNetProtocolComponent::loop()"
    end = source.index(boundary, start) + len("\n#endif")
    active_executor = source[start:end]
    assert "this->mqtt_wire_.publish(this->mqtt_execution_reply_" in active_executor
    assert ".publish(" not in source[:start] + source[end:]


def test_mailbox_is_bounded_and_does_not_parse_in_callback():
    source = (COMPONENT / "mqtt_mailbox.h").read_text(encoding="utf-8")
    assert "MAX_TOPIC = 192" in source
    assert "MAX_PAYLOAD = 1280" in source
    assert "occupied_" in source and "dropped_" in source
    assert "std::vector" not in source
    assert "Json" not in source


def test_active_executor_failure_results_include_normalized_error_message():
    source = (COMPONENT / "communication_net_protocol.cpp").read_text(encoding="utf-8")
    assert '\\\"message\\\":\\\"command rejected by executor\\\"' in source
    assert 'const char *message = result.error.code ==' in source
    assert '\\\"message\\\":\\\"%s\\\"' in source
    assert '"operation interrupted by command"' in source
