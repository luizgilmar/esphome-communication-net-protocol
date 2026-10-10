import importlib.util
from pathlib import Path
import pytest

path = Path(__file__).parents[1] / "components/communication_net_protocol/snapshot_limits.py"
spec = importlib.util.spec_from_file_location("snapshot_limits_test", path)
limits = importlib.util.module_from_spec(spec)
spec.loader.exec_module(limits)


def test_current_hub_and_two_individual_fields_fit():
    fields = [{"field": "spot"}, {"field": "strips", "rgb": True},
              {"field": "panel1"}, {"field": "panel2"},
              {"field": "general"}, {"field": "superior", "rgb": True}]
    assert limits.validate_snapshot_budget(fields) is fields
    mqtt, radio = limits.snapshot_sizes(fields)
    assert mqtt <= 447 and radio <= 256


def test_eight_short_binary_fields_fit():
    fields = [{"field": f"f{i}"} for i in range(8)]
    limits.validate_snapshot_budget(fields)


def test_radio_budget_is_enforced_independently(monkeypatch):
    fields = [{"field": chr(97 + i) * 31} for i in range(7)]
    _, radio = limits.snapshot_sizes(fields)
    assert radio > 256
    # Even if MQTT capacity grows later, the radio limit still applies.
    monkeypatch.setattr(limits, "MAX_JSON_BYTES", 2048)
    with pytest.raises(ValueError, match="ESP-NOW"):
        limits.validate_snapshot_budget(fields)


def test_rgb_payload_rejected_even_when_radio_fits():
    fields = [{"field": f"f{i}", "rgb": True} for i in range(4)]
    mqtt, radio = limits.snapshot_sizes(fields)
    assert mqtt > 447 and radio <= 256
    with pytest.raises(ValueError, match="MQTT"):
        limits.validate_snapshot_budget(fields)


@pytest.mark.parametrize("count", [0, 9])
def test_field_count_is_bounded(count):
    with pytest.raises(ValueError, match="1..8"):
        limits.validate_snapshot_budget([{"field": f"f{i}"} for i in range(count)])
