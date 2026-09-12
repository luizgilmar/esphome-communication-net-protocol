"""Foundation validation; no runtime command dispatch is enabled yet."""

import pytest

from esphome import config_validation as cv

from components.communication_net_protocol import (
    CONF_DESTINATIONS,
    CONF_ESPNOW_PEER,
    CONF_ESP_NOW,
    CONF_ID,
    CONF_MQTT,
    CONF_MQTT_TARGET,
    CONF_POLICIES,
    CONF_POLICY,
    CONF_TRANSPORTS,
    _prefix,
    _topic_segment,
    _validate,
)


def config():
    return {
        CONF_MQTT: {},
        CONF_ESP_NOW: {},
        CONF_POLICIES: [{CONF_ID: "normal", CONF_TRANSPORTS: ["mqtt", "esp_now"]}],
        CONF_DESTINATIONS: [{
            CONF_ID: "quartogian", CONF_POLICY: "normal",
            CONF_MQTT_TARGET: "quartogian", CONF_ESPNOW_PEER: "quartogian",
        }],
    }


def test_ordered_transport_policy_and_destination_are_valid():
    assert _validate(config())[CONF_POLICIES][0][CONF_TRANSPORTS] == ["mqtt", "esp_now"]


@pytest.mark.parametrize("prefix", ["/tx", "tx/", "tx//commands", "tx/+", "tx/#", ""])
def test_prefix_rejects_wildcards_and_empty_levels(prefix):
    with pytest.raises(cv.Invalid):
        _prefix(prefix)


@pytest.mark.parametrize("segment", ["a/b", "a+", "a#"])
def test_topic_segment_rejects_ambiguous_destination(segment):
    with pytest.raises(cv.Invalid):
        _topic_segment(63, "destination id")(segment)


def test_duplicate_policy_and_duplicate_transport_are_rejected():
    duplicate = config()
    duplicate[CONF_POLICIES].append(duplicate[CONF_POLICIES][0].copy())
    with pytest.raises(cv.Invalid, match="duplicate policy"):
        _validate(duplicate)
    duplicate = config()
    duplicate[CONF_POLICIES][0][CONF_TRANSPORTS] = ["mqtt", "mqtt"]
    with pytest.raises(cv.Invalid, match="duplicate transport"):
        _validate(duplicate)


def test_missing_transport_target_and_unknown_policy_are_rejected():
    missing = config()
    del missing[CONF_DESTINATIONS][0][CONF_ESPNOW_PEER]
    with pytest.raises(cv.Invalid, match="requires espnow_peer"):
        _validate(missing)
    unknown = config()
    unknown[CONF_DESTINATIONS][0][CONF_POLICY] = "unknown"
    with pytest.raises(cv.Invalid, match="unknown policy"):
        _validate(unknown)


def test_unconfigured_transport_and_duplicate_destination_are_rejected():
    unconfigured = config()
    del unconfigured[CONF_MQTT]
    with pytest.raises(cv.Invalid, match="unconfigured transport"):
        _validate(unconfigured)
    duplicate = config()
    duplicate[CONF_DESTINATIONS].append(duplicate[CONF_DESTINATIONS][0].copy())
    with pytest.raises(cv.Invalid, match="duplicate destination"):
        _validate(duplicate)
