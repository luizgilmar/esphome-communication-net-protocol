from pathlib import Path


def test_inbound_gate_is_transport_and_device_independent():
    source = Path("components/communication_net_protocol/inbound_command_gate.h").read_text(
        encoding="utf-8"
    )
    assert "encode_canonical_command" in source
    assert "guard_.begin" in source
    assert "guard_.finish" in source
    assert "guard_.get_terminal" in source
    assert "NEW_COMMAND" in source
    assert "#include \"esphome/components/tx_ultimate" not in source
    assert "#include \"esphome/components/espnow_net_protocol" not in source
    assert "quartogian" not in source.lower()
