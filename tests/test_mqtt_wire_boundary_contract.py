from pathlib import Path


COMPONENT = Path("components/communication_net_protocol")


def test_wire_adapter_does_not_take_application_lifecycle_ownership():
    source = (COMPONENT / "mqtt_wire_transport.cpp").read_text(encoding="utf-8")
    assert "mqtt::global_mqtt_client->subscribe(" in source
    assert "mqtt::global_mqtt_client->publish(" in source
    for forbidden in ("NetResultStatus", "IN_PROGRESS", "fallback", "JsonObject"):
        assert forbidden not in source


def test_foundation_does_not_auto_subscribe_or_publish():
    source = (COMPONENT / "communication_net_protocol.cpp").read_text(encoding="utf-8")
    assert "FOUNDATION ONLY" in source
    assert ".subscribe(" not in source
    assert ".publish(" not in source


def test_mailbox_is_bounded_and_does_not_parse_in_callback():
    source = (COMPONENT / "mqtt_mailbox.h").read_text(encoding="utf-8")
    assert "MAX_TOPIC = 192" in source
    assert "MAX_PAYLOAD = 1280" in source
    assert "occupied_" in source and "dropped_" in source
    assert "std::vector" not in source
    assert "Json" not in source
