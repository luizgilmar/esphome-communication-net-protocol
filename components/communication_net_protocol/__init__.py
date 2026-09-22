"""Declarative cross-transport application commands."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor, light
from esphome.const import CONF_ID, CONF_LIGHT_ID, CONF_TIMEOUT, CONF_TRIGGER_ID

CODEOWNERS = ["@project-maintainers"]

CONF_DEVICE_ID = "device_id"
CONF_MQTT = "mqtt"
CONF_MQTT_ID = "mqtt_id"
CONF_COMMAND_PREFIX = "command_prefix"
CONF_RESULT_PREFIX = "result_prefix"
CONF_SESSION_PREFIX = "session_prefix"
CONF_ESP_NOW = "esp_now"
CONF_ESPNOW_NET_PROTOCOL_ID = "espnow_net_protocol_id"
CONF_OBSERVE_INBOUND = "observe_inbound"
CONF_EXECUTE_INBOUND = "execute_inbound"
CONF_COMMAND_TARGET = "command_target"
CONF_SOURCE_ID = "source_id"
CONF_REPLY_TOPIC = "reply_topic"
CONF_POLICIES = "policies"
CONF_TRANSPORTS = "transports"
CONF_DESTINATIONS = "destinations"
CONF_POLICY = "policy"
CONF_ESPNOW_PEER = "espnow_peer"
CONF_MQTT_TARGET = "mqtt_target"
CONF_LISTEN_COMMANDS = "listen_commands"
CONF_LISTEN_RESULTS = "listen_results"
CONF_OBSERVE_OUTGOING_TARGET = "observe_outgoing_target"
CONF_INBOUND = "inbound"
CONF_BINDINGS = "bindings"
CONF_RESOURCE = "resource"
CONF_COMMAND = "command"
CONF_COMPLETION = "completion"
CONF_EXPECTED = "expected"
CONF_ADDITIONAL_LIGHT_IDS = "additional_light_ids"
CONF_TOGGLE_REFERENCE_LIGHT_IDS = "toggle_reference_light_ids"
CONF_RGB_LIGHT_IDS = "rgb_light_ids"
CONF_RESULT_RGB = "result_rgb"
CONF_RGB = "rgb"
CONF_BRIGHTNESS = "brightness"
CONF_BRIGHTNESS_LIGHT_IDS = "brightness_light_ids"
CONF_EFFECT = "effect"
CONF_EFFECT_LIGHT_IDS = "effect_light_ids"
CONF_STATE_SNAPSHOT = "state_snapshot"
CONF_TOPIC = "topic"
CONF_QOS = "qos"
CONF_INTERVAL = "interval"
CONF_FIELDS = "fields"
CONF_FIELD = "field"
CONF_LIGHT_IDS = "light_ids"
CONF_BINARY_SENSOR_ID = "binary_sensor_id"
CONF_DELAY = "delay"
CONF_INTERRUPTIBLE = "interruptible"
CONF_INTERRUPTS_ACTIVE = "interrupts_active"

communication_ns = cg.esphome_ns.namespace("communication_net_protocol")
CommunicationNetProtocolComponent = communication_ns.class_(
    "CommunicationNetProtocolComponent", cg.Component
)
DeclarativeInboundBinding = communication_ns.class_(
    "DeclarativeInboundBinding", automation.Trigger.template()
)
LightExpectedState = communication_ns.enum("LightExpectedState", is_class=True)
LIGHT_EXPECTED_STATES = {
    "on": LightExpectedState.ON,
    "off": LightExpectedState.OFF,
    "toggled": LightExpectedState.TOGGLED,
}


def _identifier(maximum, label):
    def validate(value):
        value = cv.string_strict(value)
        if not value or len(value) > maximum or any(c.isspace() for c in value):
            raise cv.Invalid(f"{label} must contain 1 to {maximum} non-whitespace characters")
        return value

    return validate


def _effect_name(value):
    value = cv.string_strict(value)
    if not value or len(value) > 63 or "\x00" in value:
        raise cv.Invalid("effect name must contain 1 to 63 characters")
    return value


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
    cv.Optional(CONF_EXECUTE_INBOUND, default=False): cv.boolean,
    cv.Optional(CONF_COMMAND_TARGET): _topic_segment(63, "command_target"),
    cv.Optional(CONF_SOURCE_ID): _topic_segment(63, "source_id"),
    cv.Optional(CONF_REPLY_TOPIC): _prefix,
    cv.Optional(CONF_LISTEN_RESULTS, default=False): cv.boolean,
    cv.Optional(CONF_OBSERVE_OUTGOING_TARGET): _topic_segment(63, "observe_outgoing_target"),
    cv.Optional(CONF_COMMAND_PREFIX, default="tx/commands"): _prefix,
    cv.Optional(CONF_RESULT_PREFIX, default="tx/results"): _prefix,
    cv.Optional(CONF_SESSION_PREFIX, default="tx/sessions"): _prefix,
})

ESPNOW_SCHEMA = cv.Schema({
    cv.Required(CONF_ESPNOW_NET_PROTOCOL_ID): cv.use_id(cg.Component),
    cv.Optional(CONF_OBSERVE_INBOUND, default=False): cv.boolean,
    cv.Optional(CONF_EXECUTE_INBOUND, default=False): cv.boolean,
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

def _validate_light_completion(config):
    targets = sum(key in config for key in (CONF_LIGHT_ID, CONF_BINARY_SENSOR_ID, CONF_DELAY))
    if targets != 1:
        raise cv.Invalid("completion requires exactly one of light_id, binary_sensor_id or delay")
    if CONF_BINARY_SENSOR_ID in config and any(key in config for key in (
        CONF_ADDITIONAL_LIGHT_IDS, CONF_TOGGLE_REFERENCE_LIGHT_IDS, CONF_RGB,
        CONF_RGB_LIGHT_IDS, CONF_BRIGHTNESS, CONF_BRIGHTNESS_LIGHT_IDS,
        CONF_EFFECT, CONF_EFFECT_LIGHT_IDS, CONF_RESULT_RGB,
    )):
        raise cv.Invalid("binary sensor completion cannot use light options")
    if CONF_DELAY in config and any(key in config for key in (
        CONF_EXPECTED, CONF_ADDITIONAL_LIGHT_IDS, CONF_TOGGLE_REFERENCE_LIGHT_IDS,
        CONF_RGB, CONF_RGB_LIGHT_IDS, CONF_BRIGHTNESS,
        CONF_BRIGHTNESS_LIGHT_IDS, CONF_EFFECT, CONF_EFFECT_LIGHT_IDS,
        CONF_RESULT_RGB,
    )):
        raise cv.Invalid("delay completion cannot use light options")
    if (CONF_RGB in config) != (CONF_RGB_LIGHT_IDS in config):
        raise cv.Invalid("rgb and rgb_light_ids must be configured together")
    if (CONF_BRIGHTNESS in config) != (CONF_BRIGHTNESS_LIGHT_IDS in config):
        raise cv.Invalid("brightness and brightness_light_ids must be configured together")
    if (CONF_EFFECT in config) != (CONF_EFFECT_LIGHT_IDS in config):
        raise cv.Invalid("effect and effect_light_ids must be configured together")
    # Run before cv.enum converts the YAML string to a C++ enum expression.
    if CONF_RGB in config and str(config.get(CONF_EXPECTED, "toggled")).lower() != "on":
        raise cv.Invalid("RGB completion requires expected: on")
    if CONF_BRIGHTNESS in config and str(config.get(CONF_EXPECTED, "toggled")).lower() != "on":
        raise cv.Invalid("brightness completion requires expected: on")
    if CONF_EFFECT in config and str(config.get(CONF_EXPECTED, "toggled")).lower() != "on":
        raise cv.Invalid("effect completion requires expected: on")
    return config


LIGHT_COMPLETION_SCHEMA = cv.All(_validate_light_completion, cv.Schema({
    cv.Optional(CONF_LIGHT_ID): cv.use_id(light.LightState),
    cv.Optional(CONF_BINARY_SENSOR_ID): cv.use_id(binary_sensor.BinarySensor),
    cv.Optional(CONF_DELAY): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_ADDITIONAL_LIGHT_IDS): cv.All(
        cv.ensure_list(cv.use_id(light.LightState)), cv.Length(min=1, max=3)
    ),
    cv.Optional(CONF_TOGGLE_REFERENCE_LIGHT_IDS): cv.All(
        cv.ensure_list(cv.use_id(light.LightState)), cv.Length(min=1, max=3)
    ),
    cv.Optional(CONF_EXPECTED, default="toggled"): cv.enum(
        LIGHT_EXPECTED_STATES, lower=True
    ),
    cv.Optional(CONF_TIMEOUT, default="2s"):
        cv.positive_time_period_milliseconds,
    cv.Optional(CONF_RGB): cv.Schema({
        cv.Required("red"): cv.int_range(min=0, max=255),
        cv.Required("green"): cv.int_range(min=0, max=255),
        cv.Required("blue"): cv.int_range(min=0, max=255),
    }),
    cv.Optional(CONF_RGB_LIGHT_IDS): cv.All(
        cv.ensure_list(cv.use_id(light.LightState)), cv.Length(min=1, max=3)
    ),
    cv.Optional(CONF_RESULT_RGB, default=False): cv.boolean,
    cv.Optional(CONF_BRIGHTNESS): cv.int_range(min=1, max=100),
    cv.Optional(CONF_BRIGHTNESS_LIGHT_IDS): cv.All(
        cv.ensure_list(cv.use_id(light.LightState)), cv.Length(min=1, max=3)
    ),
    cv.Optional(CONF_EFFECT): _effect_name,
    cv.Optional(CONF_EFFECT_LIGHT_IDS): cv.All(
        cv.ensure_list(cv.use_id(light.LightState)), cv.Length(min=1, max=3)
    ),
}))

INBOUND_BINDING_SCHEMA = automation.validate_automation({
    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DeclarativeInboundBinding),
    cv.Required(CONF_ID): _identifier(31, "inbound binding id"),
    cv.Required(CONF_RESOURCE): _identifier(63, "resource"),
    cv.Required(CONF_COMMAND): _identifier(63, "command"),
    cv.Optional(CONF_INTERRUPTIBLE, default=False): cv.boolean,
    cv.Optional(CONF_INTERRUPTS_ACTIVE, default=False): cv.boolean,
    cv.Optional(CONF_COMPLETION): LIGHT_COMPLETION_SCHEMA,
}, single=True)

INBOUND_SCHEMA = cv.Schema({
    cv.Required(CONF_BINDINGS): cv.All(
        cv.ensure_list(INBOUND_BINDING_SCHEMA), cv.Length(min=1, max=32)
    ),
})


def _validate_snapshot_field(config):
    if (CONF_LIGHT_IDS in config) == (CONF_BINARY_SENSOR_ID in config):
        raise cv.Invalid("snapshot field requires either light_ids or binary_sensor_id")
    if config.get(CONF_RGB, False) and CONF_LIGHT_IDS not in config:
        raise cv.Invalid("rgb snapshot requires light_ids")
    return config


def _snapshot_field_name(value):
    value = cv.string_strict(value)
    if (not value or len(value) > 31 or not value[0].isascii() or
            not value[0].islower() or
            any(not (char.isascii() and (char.islower() or char.isdigit() or
                                        char == "_")) for char in value)):
        raise cv.Invalid("snapshot field must be lowercase ASCII letters, digits or underscores")
    return value


SNAPSHOT_FIELD_SCHEMA = cv.All(cv.Schema({
    cv.Required(CONF_FIELD): _snapshot_field_name,
    cv.Optional(CONF_LIGHT_IDS): cv.All(
        cv.ensure_list(cv.use_id(light.LightState)), cv.Length(min=1, max=3)
    ),
    cv.Optional(CONF_BINARY_SENSOR_ID): cv.use_id(binary_sensor.BinarySensor),
    cv.Optional(CONF_RGB, default=False): cv.boolean,
}), _validate_snapshot_field)

STATE_SNAPSHOT_SCHEMA = cv.Schema({
    cv.Required(CONF_TOPIC): _prefix,
    cv.Optional(CONF_QOS, default=1): cv.int_range(min=0, max=2),
    cv.Optional(CONF_INTERVAL, default="500ms"): cv.positive_time_period_milliseconds,
    cv.Required(CONF_FIELDS): cv.All(
        cv.ensure_list(SNAPSHOT_FIELD_SCHEMA), cv.Length(min=1, max=4)
    ),
})


def _validate(config):
    esp_now = config.get(CONF_ESP_NOW)
    mqtt = config.get(CONF_MQTT)
    active = (esp_now and esp_now[CONF_EXECUTE_INBOUND]) or (mqtt and mqtt[CONF_EXECUTE_INBOUND])
    if active:
        if not esp_now or not mqtt or not mqtt[CONF_EXECUTE_INBOUND] or CONF_INBOUND not in config:
            raise cv.Invalid("execute_inbound currently requires MQTT, ESP-NOW and inbound.bindings")
        if mqtt and mqtt[CONF_EXECUTE_INBOUND] and (not mqtt[CONF_LISTEN_COMMANDS] or
                CONF_SOURCE_ID not in mqtt or CONF_REPLY_TOPIC not in mqtt or
                CONF_COMMAND_TARGET not in mqtt):
            raise cv.Invalid("MQTT execution requires listen_commands, command_target, source_id and reply_topic")
        if not esp_now[CONF_OBSERVE_INBOUND]:
            raise cv.Invalid("ESP-NOW execution requires observe_inbound: true for verified peer identity")
        for binding in config[CONF_INBOUND][CONF_BINDINGS]:
            if CONF_COMPLETION not in binding:
                raise cv.Invalid("active inbound bindings require completion")
    if esp_now and esp_now[CONF_OBSERVE_INBOUND] and CONF_INBOUND not in config:
        raise cv.Invalid("esp_now.observe_inbound requires inbound.bindings")
    if mqtt and CONF_OBSERVE_OUTGOING_TARGET in mqtt and not mqtt[CONF_LISTEN_RESULTS]:
        raise cv.Invalid("observe_outgoing_target requires listen_results: true")
    if CONF_STATE_SNAPSHOT in config:
        if mqtt is None:
            raise cv.Invalid("state_snapshot requires mqtt")
        fields = [field[CONF_FIELD] for field in config[CONF_STATE_SNAPSHOT][CONF_FIELDS]]
        if len(set(fields)) != len(fields):
            raise cv.Invalid("state_snapshot fields must be unique")
    if CONF_INBOUND in config:
        names, routes = set(), set()
        bindings = config[CONF_INBOUND][CONF_BINDINGS]
        for binding in bindings:
            name = binding[CONF_ID]
            route = (binding[CONF_RESOURCE], binding[CONF_COMMAND])
            completion = binding.get(CONF_COMPLETION, {})
            if name in names or route in routes:
                raise cv.Invalid(f"duplicate inbound binding: {name} {route}")
            names.add(name)
            routes.add(route)
            if binding[CONF_INTERRUPTIBLE] and binding[CONF_INTERRUPTS_ACTIVE]:
                raise cv.Invalid(f"binding {name} cannot be both interruptible and an interrupt")
            if binding[CONF_INTERRUPTS_ACTIVE] and binding[CONF_COMMAND] != "stop":
                raise cv.Invalid(f"interrupt binding {name} must use command: stop")
            if (binding[CONF_INTERRUPTS_ACTIVE] and
                    CONF_DELAY not in completion):
                raise cv.Invalid(
                    f"interrupt binding {name} requires completion.delay"
                )
            if (CONF_DELAY in completion and
                    completion[CONF_TIMEOUT].total_milliseconds <=
                    completion[CONF_DELAY].total_milliseconds):
                raise cv.Invalid(
                    f"binding {name} completion timeout must exceed delay"
                )
        for binding in bindings:
            if binding[CONF_INTERRUPTS_ACTIVE] and not any(
                candidate[CONF_INTERRUPTIBLE] and
                candidate[CONF_RESOURCE] == binding[CONF_RESOURCE]
                for candidate in bindings
            ):
                raise cv.Invalid(
                    f"interrupt binding {binding[CONF_ID]} has no interruptible route for its resource"
                )
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
        cv.Optional(CONF_INBOUND): INBOUND_SCHEMA,
        cv.Optional(CONF_STATE_SNAPSHOT): STATE_SNAPSHOT_SCHEMA,
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
    esp_now = config.get(CONF_ESP_NOW)
    mqtt_config = config.get(CONF_MQTT)
    interruptible_inbound = any(
        binding[CONF_INTERRUPTIBLE] or binding[CONF_INTERRUPTS_ACTIVE]
        for binding in config.get(CONF_INBOUND, {}).get(CONF_BINDINGS, [])
    )
    if CONF_STATE_SNAPSHOT in config:
        cg.add_define("USE_COMMUNICATION_NET_STATE_SNAPSHOT")
    if (esp_now and esp_now[CONF_EXECUTE_INBOUND]) or (mqtt_config and mqtt_config[CONF_EXECUTE_INBOUND]):
        cg.add_define("USE_COMMUNICATION_NET_ACTIVE_GATE")
    if CONF_INBOUND in config:
        cg.add_define("USE_COMMUNICATION_NET_INBOUND")
        cg.add_define("COMMUNICATION_NET_INBOUND_CAPACITY",
                      len(config[CONF_INBOUND][CONF_BINDINGS]))
        if interruptible_inbound:
            cg.add_define("USE_COMMUNICATION_NET_INTERRUPTIBLE_INBOUND")
            cg.add_define("USE_ESPNOW_NET_PROTOCOL_INTERRUPTIBLE_INBOUND")
    if esp_now and esp_now[CONF_OBSERVE_INBOUND]:
        cg.add_define("USE_COMMUNICATION_NET_ESPNOW_OBSERVER")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_device_id(config[CONF_DEVICE_ID]))
    if CONF_STATE_SNAPSHOT in config:
        snapshot = config[CONF_STATE_SNAPSHOT]
        cg.add(var.configure_state_snapshot(snapshot[CONF_TOPIC], snapshot[CONF_QOS],
                                            snapshot[CONF_INTERVAL].total_milliseconds))
        for field in snapshot[CONF_FIELDS]:
            if CONF_BINARY_SENSOR_ID in field:
                sensor = await cg.get_variable(field[CONF_BINARY_SENSOR_ID])
                cg.add(var.add_snapshot_binary_field(field[CONF_FIELD], sensor))
            else:
                cg.add(var.add_snapshot_light_field(field[CONF_FIELD], field[CONF_RGB]))
                for light_id in field[CONF_LIGHT_IDS]:
                    cg.add(var.add_snapshot_light(await cg.get_variable(light_id)))
    for binding in config.get(CONF_INBOUND, {}).get(CONF_BINDINGS, []):
        cg.add(var.add_inbound_route(binding[CONF_ID], binding[CONF_RESOURCE],
                                     binding[CONF_COMMAND]))
        trigger = cg.new_Pvariable(binding[CONF_TRIGGER_ID])
        cg.add(trigger.set_route_id(binding[CONF_ID]))
        cg.add(trigger.set_interruptible(binding[CONF_INTERRUPTIBLE]))
        cg.add(trigger.set_interrupts_active(binding[CONF_INTERRUPTS_ACTIVE]))
        if CONF_COMPLETION in binding:
            completion = binding[CONF_COMPLETION]
            if CONF_DELAY in completion:
                cg.add(trigger.set_completion_delay(
                    completion[CONF_DELAY].total_milliseconds))
            elif CONF_BINARY_SENSOR_ID in completion:
                sensor = await cg.get_variable(completion[CONF_BINARY_SENSOR_ID])
                cg.add(trigger.set_binary_sensor(sensor))
            else:
                state = await cg.get_variable(completion[CONF_LIGHT_ID])
                cg.add(trigger.set_light(state))
            for extra_id in completion.get(CONF_ADDITIONAL_LIGHT_IDS, []):
                cg.add(trigger.add_completion_light(await cg.get_variable(extra_id)))
            for reference_id in completion.get(CONF_TOGGLE_REFERENCE_LIGHT_IDS, []):
                cg.add(trigger.add_toggle_reference_light(
                    await cg.get_variable(reference_id)))
            cg.add(trigger.set_expected(completion[CONF_EXPECTED]))
            cg.add(trigger.set_result_rgb(completion[CONF_RESULT_RGB]))
            if CONF_RGB in completion:
                rgb = completion[CONF_RGB]
                cg.add(trigger.set_expected_rgb(rgb["red"], rgb["green"], rgb["blue"]))
                for rgb_light_id in completion[CONF_RGB_LIGHT_IDS]:
                    cg.add(trigger.add_rgb_light(await cg.get_variable(rgb_light_id)))
            if CONF_BRIGHTNESS in completion:
                cg.add(trigger.set_expected_brightness(completion[CONF_BRIGHTNESS]))
                for brightness_light_id in completion[CONF_BRIGHTNESS_LIGHT_IDS]:
                    cg.add(trigger.add_brightness_light(
                        await cg.get_variable(brightness_light_id)))
            if CONF_EFFECT in completion:
                cg.add(trigger.set_expected_effect(completion[CONF_EFFECT]))
                for effect_light_id in completion[CONF_EFFECT_LIGHT_IDS]:
                    cg.add(trigger.add_effect_light(
                        await cg.get_variable(effect_light_id)))
            cg.add(trigger.set_completion_timeout(
                completion[CONF_TIMEOUT].total_milliseconds))
        cg.add(var.add_inbound_binding(trigger))
        await automation.build_automation(trigger, [], binding)
    if esp_now and esp_now[CONF_OBSERVE_INBOUND]:
        protocol = await cg.get_variable(esp_now[CONF_ESPNOW_NET_PROTOCOL_ID])
        cg.add(var.set_espnow_observation_source(protocol))
        if esp_now[CONF_EXECUTE_INBOUND]:
            if interruptible_inbound:
                cg.add(var.activate_interruptible_inbound_executor(protocol))
            else:
                cg.add(var.activate_inbound_executor(protocol))
    if mqtt_config and mqtt_config[CONF_EXECUTE_INBOUND]:
        cg.add(var.set_mqtt_inbound_execution(mqtt_config[CONF_SOURCE_ID],
                                              mqtt_config[CONF_REPLY_TOPIC]))
    if mqtt_config and (mqtt_config[CONF_LISTEN_COMMANDS] or mqtt_config[CONF_LISTEN_RESULTS]):
        cg.add_define("USE_COMMUNICATION_NET_MQTT_LISTENER")
    if mqtt_config and mqtt_config[CONF_LISTEN_COMMANDS]:
        command_topic = (
            f"{mqtt_config[CONF_COMMAND_PREFIX]}/{mqtt_config.get(CONF_COMMAND_TARGET, config[CONF_DEVICE_ID])}/command"
        )
        if len(command_topic.encode("utf-8")) > 192:
            raise cv.Invalid("composed MQTT command topic exceeds 192 bytes")
        cg.add(var.set_mqtt_command_topic(command_topic))
    if mqtt_config and mqtt_config[CONF_LISTEN_RESULTS]:
        result_topic = f"{mqtt_config[CONF_RESULT_PREFIX]}/{config[CONF_DEVICE_ID]}"
        if len(result_topic.encode("utf-8")) > 192:
            raise cv.Invalid("composed MQTT result topic exceeds 192 bytes")
        cg.add(var.set_mqtt_result_topic(result_topic))
    if mqtt_config and CONF_OBSERVE_OUTGOING_TARGET in mqtt_config:
        target = mqtt_config[CONF_OBSERVE_OUTGOING_TARGET]
        command_topic = f"{mqtt_config[CONF_COMMAND_PREFIX]}/{target}/command"
        if len(command_topic.encode("utf-8")) > 192:
            raise cv.Invalid("composed MQTT outgoing command topic exceeds 192 bytes")
        cg.add(var.set_mqtt_outgoing_topic(command_topic))
        cg.add(var.set_mqtt_outgoing_target(target))
