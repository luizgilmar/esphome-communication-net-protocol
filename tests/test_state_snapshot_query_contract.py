from pathlib import Path


ROOT = Path(__file__).parents[1]
COMPONENT = ROOT / "components" / "communication_net_protocol"


def test_snapshot_query_reuses_bounded_net_result():
    header = (COMPONENT / "light_state_snapshot.h").read_text(encoding="utf-8")
    snapshot = (COMPONENT / "light_state_snapshot.cpp").read_text(encoding="utf-8")
    runtime = (COMPONENT / "communication_net_protocol.cpp").read_text(
        encoding="utf-8"
    )

    assert "write_remote_state" in header
    assert 'snapshot.schema.assign("state-fields/v2")' in snapshot
    assert "NetStateSnapshot::MAX_DATA_SIZE" in snapshot
    assert "generation_preference_" in header
    assert "uint32_t generation_" in header
    assert "uint32_t revision_" in header
    assert "state-fields/v2 binary encoding version" in snapshot
    assert 'std::strcmp(command.resource.c_str(), "state/snapshot") == 0' in runtime
    assert 'std::strcmp(command.name.c_str(), "get") == 0' in runtime
    assert "state_snapshot_.write_remote_state(result.remote_state)" in runtime


def test_mqtt_and_espnow_share_the_same_snapshot_schema():
    runtime = (COMPONENT / "communication_net_protocol.cpp").read_text(
        encoding="utf-8"
    )
    assert 'std::strcmp(remote.schema.c_str(), "state-fields/v1")' in runtime
    assert 'std::strcmp(remote.schema.c_str(), "state-fields/v2")' in runtime
    assert "data_hex" in runtime
    assert "char payload[704]" in runtime


def test_snapshot_version_advances_once_per_observed_state_change():
    header = (COMPONENT / "light_state_snapshot.h").read_text(encoding="utf-8")
    snapshot = (COMPONENT / "light_state_snapshot.cpp").read_text(encoding="utf-8")

    assert "Value last_values_[MAX_FIELDS]" in header
    assert "std::memcmp(values, this->last_values_, bytes) == 0" in snapshot
    assert "this->revision_++" in snapshot
    assert "published_generation_ == this->generation_" in snapshot
    assert "published_revision_ == this->revision_" in snapshot
    assert "Snapshot generation could not be persisted" in snapshot
