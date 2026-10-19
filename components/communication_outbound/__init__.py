"""Provision shared command adapters without any device-specific executor."""
import re
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import mqtt
from esphome.const import CONF_ID

DEPENDENCIES = ["communication_net_protocol"]
ns = cg.esphome_ns.namespace("communication_net_protocol").namespace("outbound")
OutboundRuntime = ns.class_("OutboundRuntime", cg.Component)
Radio = cg.esphome_ns.namespace("espnow_net_protocol").class_("EspNowNetProtocolComponent", cg.Component)


def identity(value):
    value = cv.string_strict(value)
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,63}", value):
        raise cv.Invalid("Use 1..63 letters, digits, underscores or hyphens")
    return value


def topic(value):
    value = cv.string_strict(value)
    if not value or len(value.encode("utf-8")) > 160 or any(c in value for c in "+#") or any(ord(c) < 32 for c in value):
        raise cv.Invalid("Use a nonempty MQTT topic up to 160 bytes without wildcards")
    return value


def validate_runtime(config):
    configured = {k for k in ("mqtt", "esp_now") if k in config}
    if not configured:
        raise cv.Invalid("At least one command transport is required")
    if "mqtt" in config and config["mqtt"]["command_prefix"].endswith("/"):
        raise cv.Invalid("MQTT command_prefix must not end with a slash")
    policies = {p["id"]: p for p in config["policies"]}
    if len(policies) != len(config["policies"]):
        raise cv.Invalid("Duplicate outbound policy")
    for policy in policies.values():
        transports = policy["transports"]
        if len(set(transports)) != len(transports) or not set(transports) <= configured:
            raise cv.Invalid("Policy requires distinct, configured transports")
    ids = set()
    devices = set()
    for dest in config["destinations"]:
        if dest["id"] in ids:
            raise cv.Invalid("Duplicate outbound destination")
        ids.add(dest["id"])
        if dest["device_id"] in devices:
            raise cv.Invalid("Use one outbound destination per device_id")
        devices.add(dest["device_id"])
        policy = policies.get(dest["policy"])
        if policy is None:
            raise cv.Invalid("Unknown outbound policy")
        if "esp_now" in policy["transports"] and "espnow_peer" not in dest:
            raise cv.Invalid("ESP-NOW destination requires espnow_peer")
    return config


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(OutboundRuntime),
    cv.Required("source_id"): identity,
    cv.Optional("mqtt"): cv.Schema({
        cv.Required("mqtt_id"): cv.use_id(mqtt.MQTTClientComponent),
        cv.Required("command_prefix"): topic,
        cv.Required("reply_topic"): topic,
    }),
    cv.Optional("esp_now"): cv.Schema({
        cv.Required("espnow_net_protocol_id"): cv.use_id(Radio),
    }),
    cv.Required("policies"): cv.All(cv.ensure_list(cv.Schema({
        cv.Required("id"): cv.All(identity, cv.Length(max=31)),
        cv.Required("transports"): cv.All(cv.ensure_list(cv.one_of("mqtt", "esp_now")), cv.Length(min=1, max=2)),
    })), cv.Length(min=1, max=16)),
    cv.Required("destinations"): cv.All(cv.ensure_list(cv.Schema({
        cv.Required("id"): identity,
        cv.Required("device_id"): identity,
        cv.Required("policy"): identity,
        cv.Optional("espnow_peer"): identity,
    })), cv.Length(min=1, max=16)),
}).extend(cv.COMPONENT_SCHEMA), validate_runtime)


def validate_installation(config, installation):
    if "mqtt" in config:
        client = installation.get("mqtt", {})
        if str(client.get("id")) != str(config["mqtt"]["mqtt_id"]):
            raise cv.Invalid("Outbound MQTT must reference the configured native client")
    if "esp_now" not in config:
        return config
    radio_id = str(config["esp_now"]["espnow_net_protocol_id"])
    radio = installation.get("espnow_net_protocol", {})
    if str(radio.get("id")) != radio_id:
        raise cv.Invalid("Outbound radio must reference the configured endpoint")
    peers = {str(p["id"]): p for p in radio.get("peers", [])}
    if "mqtt" in config and config["mqtt"]["command_prefix"].endswith("/"):
        raise cv.Invalid("MQTT command_prefix must not end with a slash")
    policies = {p["id"]: p for p in config["policies"]}
    for dest in config["destinations"]:
        if "esp_now" not in policies[dest["policy"]]["transports"]:
            continue
        peer = peers.get(dest["espnow_peer"])
        if peer is None or peer.get("application_source_id") != dest["device_id"]:
            raise cv.Invalid("Outbound peer application_source_id must match destination device_id")
    tx = installation.get("tx_ultimate", {})
    diagnostic = installation.get("state_query_client", {})
    if str(tx.get("espnow_net_protocol_id")) == radio_id or str(diagnostic.get("radio_id")) == radio_id:
        raise cv.Invalid("Outbound runtime must be the sole outgoing command owner of its radio endpoint")
    return config


def final_validate(config):
    return validate_installation(config, fv.full_config.get())


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_source(config["source_id"]))
    if "mqtt" in config:
        cg.add_define("USE_COMMUNICATION_NET_OUTBOUND_MQTT")
        conf = config["mqtt"]
        await cg.get_variable(conf["mqtt_id"])
        cg.add(var.set_mqtt(conf["command_prefix"], conf["reply_topic"]))
    if "esp_now" in config:
        cg.add_define("USE_COMMUNICATION_NET_OUTBOUND_ESPNOW")
        cg.add_define("USE_ESPNOW_NET_PROTOCOL_DUAL_COMMAND_CLIENT")
        endpoint = await cg.get_variable(config["esp_now"]["espnow_net_protocol_id"])
        cg.add(var.set_radio(endpoint))
    for policy in config["policies"]:
        values = [{"mqtt": 2, "esp_now": 3}[t] for t in policy["transports"]]
        cg.add(var.add_policy(policy["id"], values[0], values[1] if len(values) > 1 else 0))
    for dest in config["destinations"]:
        cg.add(var.add_destination(dest["id"], dest["device_id"], dest["policy"], dest.get("espnow_peer", "")))
