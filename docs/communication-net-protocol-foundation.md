# Communication NetProtocol — contract foundation

This is the **declarative foundation only**. Compiling this component must not
subscribe to MQTT, send ESP-NOW frames, take ownership of an inbound binding,
or change a deployed device's command route. The pilot resource migration is
a separate, explicitly gated step.

## Ownership

- Native ESPHome `mqtt:` owns broker connection, credentials and ordinary HA
  entity topics; `communication_net_protocol` will use its existing client.
- `espnow_net_protocol` owns channel, PMK, MAC/LMK peer registry, encrypted radio
  and reliable delivery. Peer IDs are referenced, not declared twice.
- `communication_net_protocol` will own the *application* transaction ID,
  boot/session correlation, deduplication across both transports, ordered
  fallback, IN_PROGRESS and terminal outcomes, and resource dispatch.
- Device adapters execute local actions and verify the relevant final state;
  received frames and broker publish acknowledgements are not proof of effect.
- A route cannot have both a direct `espnow_net_protocol.inbound` binding and
  a `communication_net_protocol` binding. Migration removes the old binding
  for that specific route only after the replacement is validated.

## Topic classes

| Kind | Quartogian compatibility | Retained? |
| --- | --- | --- |
| Command | `tx/commands/quartogian/command` | No: momentary intent |
| Result | `tx/results/tx-quartogian-integration` | No: correlate by session and transaction |
| Session | `tx/sessions/tx-quartogian-integration` | Define lifetime explicitly in runtime stage |
| Observed state | `tx/status/quartogian/lights` | Separate snapshot policy |
| HA sensors/entities | Native ESPHome MQTT topics | Native entity policy |

Only the command/result/session *prefixes* are admitted in this foundation.
State observation stays with the existing implementation until its own
migration. Prefixes cannot contain MQTT wildcards, leading/trailing slashes or
empty levels. The runtime stage must also validate the fully composed topic
length and reject unsafe/ambiguous destination IDs before publishing.

## YAML (foundation schema, no runtime routing)

```yaml
mqtt:
  id: device_mqtt
  broker: !secret mqtt_broker

espnow_net_protocol:
  id: espnow_network
  channel: 1
  pmk: !secret espnow_pmk
  peers:
    - id: quartogian
      address: !secret espnow_quartogian_mac
      lmk: !secret espnow_quartogian_lmk

communication_net_protocol:
  id: command_network
  device_id: tx-quartogian-integration
  mqtt:
    mqtt_id: device_mqtt
    command_prefix: tx/commands
    result_prefix: tx/results
    session_prefix: tx/sessions
  esp_now:
    espnow_net_protocol_id: espnow_network
  policies:
    - id: normal
      transports: [mqtt, esp_now]
  destinations:
    - id: quartogian
      policy: normal
      mqtt_target: quartogian
      espnow_peer: quartogian
```

This is now a **validated foundation syntax**, not yet a functional routing
example: the references are recorded as IDs but not resolved/used at runtime.
Do not install this alongside the existing Quartogian routes expecting it to
change behavior. The original TX/HUB firmware and YAML remain unmodified.

## Migration gates

1. Bind the MQTT/ESP-NOW component references and add bounded transport
   adapters. Preserve the current Quartogian topics and message compatibility.
2. Establish one application transaction identity across fallback. A second
   transport must never repeat a non-idempotent `toggle` whose execution is
   already known or uncertain without receiver-side cross-transport replay.
3. Pilot only `light/spot_chuveiro`; validate successes, retries, duplicates,
   no-response timeouts, offline recovery and effect confirmation on hardware.
4. Migrate strips, blinds (`open`/`close`/`stop` and IN_PROGRESS), then scenes
   according to local/HA dependencies. Delete each legacy YAML branch only
   after equivalent tests pass for that resource.

The generic component must not claim that full-mesh routing is implemented:
current peers are direct neighbors, without relaying through HUBs.
