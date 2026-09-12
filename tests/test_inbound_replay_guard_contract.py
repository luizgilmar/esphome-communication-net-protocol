from pathlib import Path


def test_replay_guard_is_transport_neutral_and_bounded():
    source = Path("components/communication_net_protocol/inbound_replay_guard.h").read_text(
        encoding="utf-8"
    )
    assert "DUPLICATE_PENDING" in source
    assert "DUPLICATE_TERMINAL" in source
    assert "InboundDecision::CONFLICT" in source
    assert "InboundDecision::FULL" in source
    assert "clear_source_session" in source
    assert "std::memcmp(entry.command, command, command_length)" in source
    assert "terminal_data[MaxTerminal]" in source
    assert "get_terminal" in source
    assert "command_fingerprint" not in source
    assert "std::vector" not in source
    assert '#include "esphome/components/mqtt' not in source
    assert '#include "esphome/components/espnow' not in source
