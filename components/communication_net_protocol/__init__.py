"""Declarative foundation for cross-transport application commands.

No transport is activated in this milestone; existing MQTT and ESP-NOW
command paths retain ownership until the pilot migration is complete.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@project-maintainers"]

CONF_DEVICE_ID = "device_id"
CONF_MQTT = "mqtt"
CONF_MQTT_ID = "mqtt_id"
CONF_COMMAND_PREFIX = "command_prefix"
CONF_RESULT_PREFIX = "result_prefix"
CONF_SESSION_PREFIX = "session_prefix"
CONF_ESP_NOW = "esp_now"
CONF_ESPNOW_NET_PROTOCOL_ID = "espnow_net_protocol_id"
CONF_POLICIES = "policies"
CONF_TRANSPORTS = "transports"
CONF_DESTINATIONS = "destinations"
CONF_POLICY = "policy"
CONF_ESPNOW_PEER = "espnow_peer"
CONF_MQTT_TARGET = "mqtt_target"
CONF_LISTEN_COMMANDS = "listen_commands"

communication_ns = cg.esphome_ns.namespace("communication_net_protocol")
CommunicationNetProtocolComponent = communication_ns.class_(
    "CommunicationNetProtocolComponent", cg.Component
)


def _identifier(maximum, label):
    def validate(value):
        value = cv.string_strict(value)
        if not value or len(value) > maximum or any(c.isspace() for c in value):
            raise cv.Invalid(f"{label} must contain 1 to {maximum} non-whitespace characters")
        return value

    return validate


def _prefix(value):
    value = cv.string_strict(value)
    if (not value or len(value) > 95 or value.startswith("/") or
            value.endswith("/") or "//" in value or
            any(c in value for c in ("+", "#", "\x00"))):
        raise cv.Invalid("MQTT prefix must be 1 to 95 characters without wildcards or empty levels")
    return value


def _topic_segment(maximum, label):
    identifier = _identifier(maximum, label)

    def validate(value):
        value = identifier(value)
        if any(c in value for c in ("/", "+", "#", "\x00")):
            raise cv.Invalid(f"{label} must be a single MQTT topic segment")
        return value

    return validate


def _mqtt_id(value):
    # ESPHome owns MQTT client creation. Resolve its ID in the runtime-binding
    # milestone; accepting a string here cannot accidentally start a client.
    return _identifier(63, "mqtt_id")(value)


MQTT_SCHEMA = cv.Schema({
    cv.Required(CONF_MQTT_ID): _mqtt_id,
    cv.Optional(CONF_LISTEN_COMMANDS, default=False): cv.boolean,
    cv.Optional(CONF_COMMAND_PREFIX, default="tx/commands"): _prefix,
    cv.Optional(CONF_RESULT_PREFIX, default="tx/results"): _prefix,
    cv.Optional(CONF_SESSION_PREFIX, default="tx/sessions"): _prefix,
})

ESPNOW_SCHEMA = cv.Schema({
    cv.Required(CONF_ESPNOW_NET_PROTOCOL_ID): _identifier(63, CONF_ESPNOW_NET_PROTOCOL_ID),
})

POLICY_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): _identifier(31, "policy id"),
    cv.Required(CONF_TRANSPORTS): cv.All(
        cv.ensure_list(cv.one_of(CONF_MQTT, CONF_ESP_NOW, lower=True)),
        cv.Length(min=1, max=2),
    ),
})

DESTINATION_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): _topic_segment(63, "destination id"),
    cv.Required(CONF_POLICY): _identifier(31, "policy id"),
    cv.Optional(CONF_MQTT_TARGET): _topic_segment(63, "mqtt_target"),
    cv.Optional(CONF_ESPNOW_PEER): _identifier(63, "espnow_peer"),
})


def _validate(config):
    configured = {name for name in (CONF_MQTT, CONF_ESP_NOW) if name in config}
    policies = {}
    for policy in config[CONF_POLICIES]:
        name = policy[CONF_ID]
        if name in policies:
            raise cv.Invalid(f"duplicate policy id: {name}")
        transports = policy[CONF_TRANSPORTS]
        if len(set(transports)) != len(transports):
            raise cv.Invalid(f"duplicate transport in policy: {name}")
        if not set(transports).issubset(configured):
            raise cv.Invalid(f"policy {name} uses an unconfigured transport")
        policies[name] = transports

    destinations = set()
    for destination in config[CONF_DESTINATIONS]:
        name = destination[CONF_ID]
        if name in destinations:
            raise cv.Invalid(f"duplicate destination id: {name}")
        destinations.add(name)
        policy_name = destination[CONF_POLICY]
        if policy_name not in policies:
            raise cv.Invalid(f"destination {name} references unknown policy: {policy_name}")
        for transport in policies[policy_name]:
            target_key = CONF_MQTT_TARGET if transport == CONF_MQTT else CONF_ESPNOW_PEER
            if target_key not in destination:
                raise cv.Invalid(f"destination {name} requires {target_key} for policy {policy_name}")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(CommunicationNetProtocolComponent),
        cv.Required(CONF_DEVICE_ID): _topic_segment(63, "device_id"),
        cv.Optional(CONF_MQTT): MQTT_SCHEMA,
        cv.Optional(CONF_ESP_NOW): ESPNOW_SCHEMA,
        cv.Required(CONF_POLICIES): cv.All(
            cv.ensure_list(POLICY_SCHEMA), cv.Length(min=1, max=16)
        ),
        cv.Required(CONF_DESTINATIONS): cv.All(
            cv.ensure_list(DESTINATION_SCHEMA), cv.Length(min=1, max=16)
        ),
    }).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_device_id(config[CONF_DEVICE_ID]))
    mqtt_config = config.get(CONF_MQTT)
    if mqtt_config and mqtt_config[CONF_LISTEN_COMMANDS]:
        cg.add_define("USE_COMMUNICATION_NET_MQTT_LISTENER")
        command_topic = (
            f"{mqtt_config[CONF_COMMAND_PREFIX]}/{config[CONF_DEVICE_ID]}/command"
        )
        if len(command_topic.encode("utf-8")) > 192:
            raise cv.Invalid("composed MQTT command topic exceeds 192 bytes")
        cg.add(var.set_mqtt_command_topic(command_topic))
