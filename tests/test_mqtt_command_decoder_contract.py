from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_mqtt_decoder_remains_receive_only():
    source = (ROOT / "components/communication_net_protocol/communication_net_protocol.cpp").read_text()
    decoder = (ROOT / "components/communication_net_protocol/mqtt_command_decoder.h").read_text()
    assert "decode_mqtt_command_probe" in source
    assert "canonical command validated" in source
    assert "not executed" in source
    assert "command_gate_.admit" not in source
    assert "encode_canonical_command" in decoder
    assert "std::strcmp(device, expected_device)" in decoder
