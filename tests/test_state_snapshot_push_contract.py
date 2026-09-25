from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
C = ROOT / "components" / "communication_net_protocol"


def test_push_is_declarative_coalesced_and_bounded() -> None:
    schema = (C / "__init__.py").read_text(encoding="utf-8")
    header = (C / "light_state_snapshot.h").read_text(encoding="utf-8")
    source = (C / "light_state_snapshot.cpp").read_text(encoding="utf-8")
    assert 'CONF_PUSH = "push"' in schema
    assert 'CONF_STARTUP_QUIET = "startup_quiet"' in schema
    assert 'CONF_STARTUP_SPREAD = "startup_spread"' in schema
    assert 'CONF_MIN_INTERVAL = "min_interval"' in schema
    assert "state_snapshot.push requires esp_now" in schema
    assert "push_dirty_" in header
    assert "push_inflight_" in header
    assert "std::queue" not in header + source
    assert "std::vector" not in header + source
    assert "start_background_result(peer, result)" in source


def test_mqtt_connectivity_suppresses_background_push() -> None:
    source = (C / "light_state_snapshot.cpp").read_text(encoding="utf-8")
    push = source[source.index("void LightStateSnapshot::try_push_(") :]
    push = push[:push.index("void LightStateSnapshot::loop(")]
    assert "if (mqtt_connected || !this->push_dirty_" in push
    assert "last_push_attempt_ms_" in push
    assert "push_min_interval_ms_" in push


def test_startup_spread_changes_each_persisted_generation() -> None:
    source = (C / "light_state_snapshot.cpp").read_text(encoding="utf-8")
    setup = source[source.index("void LightStateSnapshot::setup()") :]
    setup = setup[:setup.index("bool LightStateSnapshot::capture_")]
    assert "preference_key_() ^ this->generation_" in setup
    assert "startup_spread_ms_" in setup
    assert "startup_push_due_ms_" in setup
