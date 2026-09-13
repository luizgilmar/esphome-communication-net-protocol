# Communication NetProtocol — contract foundation

This is the **declarative foundation only**. Without the explicit receive-only
probe, using this component must not subscribe to MQTT, send ESP-NOW frames,
take ownership of an inbound binding,
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

## Opt-in MQTT command observation (isolated bench)

`mqtt.listen_commands: true` subscribes to the full topic composed as
`<command_prefix>/<device_id>/command`; the default is `false`. The isolated
foundation bench opts in for compilation. The callback only fills a bounded
mailbox; the component consumes at most one message per cooperative loop and
records its size/count without parsing its content or executing an action.
The listener's additional buffers are compiled only when explicitly enabled,
and live on the component, not the loopTask stack. No deployed TX/HUB
configuration is changed. An incoming MQTT `source.device_id` and `boot_id`
must **not** be treated as authenticated identity simply because the JSON
contains those fields: the trust policy for broker publishers is still open.
This probe is not proof of end-to-end execution, result correlation or
safe cross-transport fallback.

The MQTT wire methods are inline in their header because the isolated ESPHome
build may compile the component consumer without linking the separate wire
translation unit. A host link test checks both the consumer alone and the
consumer with the optional `.cpp` file. The firmware compile still verifies
the actual ESPHome MQTT API and linker behavior.

## MQTT wire boundary (next incremental step)

### Receive-only canonical probe

After consuming one mailbox entry, the component now accepts only a small
three-string JSON object: `{"device":"quartogian_probe","resource":"light/spot_chuveiro","action":"toggle"}`.
Fields may be reordered. Unknown or duplicate fields, escaped strings,
arguments, malformed JSON and a device other than the configured `device_id`
are rejected. Successful decoding generates the bounded canonical command
bytes and logs a counter, **without invoking the replay gate or any executor**.
`{"probe":true}` remains an observed but rejected diagnostic payload. This
deliberately narrow wire format is not the existing production MQTT envelope;
no production topic should be routed to this probe. Do not infer sender
authentication from these bytes. Define trusted publisher identity, full
production envelope and normalized arguments before migration or fallback.

`MqttWireTransport` uses the existing ESPHome MQTT client for a single
bounded subscription mailbox and byte publication. The mailbox neither parses
JSON nor interprets `IN_PROGRESS`/terminal status. Application correlation,
deduplication, retries and fallback remain unimplemented and must be added to
the shared protocol core. The foundation bench now explicitly opts into a
receive-only subscription; using `communication_net_protocol:` there cannot alter
the deployed Quartogian command route.

Host validation: compile `tests/mqtt_mailbox_test.cpp` with a C++17 compiler
and run the resulting executable. Firmware validation remains a separate
`esphome config` and `esphome compile` of the foundation bench. No OTA upload.

## Correlation tracker (not yet bound to either transport)

The independent `TransactionTracker<4>` keeps an application transaction ID
and source boot ID in a fixed-capacity table. It can retain one coalesced
`IN_PROGRESS` event and the terminal outcome at the same time, rejecting
duplicate terminals and replies from a stale session. Its deadline applies to
the complete application operation: a failed MQTT or ESP-NOW *attempt* is not
itself a terminal application failure if the policy has another viable route.
This header has no MQTT/ESP-NOW includes and no dynamic allocation. Parsing
the actual MQTT JSON/result envelope, integrating the ESP-NOW observer, and
cross-transport receiver deduplication are **future gates**: do not connect a
HUB action yet. The tracker is testable on a host with
`tests/transaction_tracker_test.cpp`.

## Inbound replay guard (not yet bound to command dispatch)

`InboundReplayGuard` reserves a bounded entry by sender ID, sender boot ID and
transaction ID **before** executing an inbound action. A second arrival over
either transport is reported as pending/terminal duplicate; a reused identity
with different canonical command bytes is rejected as a conflict, without a
collision-prone fingerprint. The parser must produce the **same bounded byte
representation** from MQTT and ESP-NOW for the same device, resource, action
and arguments; comparing raw JSON with an ESP-NOW frame is not sufficient.
Entries are not
evicted on transport reconnect, timeout or table pressure: a full table rejects
new commands rather than risking a second `toggle`. Only a confirmed new sender
session may clear **terminal** entries from the old boot ID; pending entries
remain reserved. This is an in-memory building
block, **not yet active in firmware dispatch** and **not durable over receiver
reboot**. The bounded `encode_canonical_command` now encodes device, resource,
action and normalized arguments independently of transport. Both adapters
must still produce equivalent semantic fields; raw MQTT JSON and radio frame
bytes are not interchangeable. Authenticated sender identity, session fencing
and replaying a stored terminal result are required before enabling automatic
fallback of non-idempotent commands. Host tests:
`tests/inbound_replay_guard_test.cpp` and `tests/canonical_command_test.cpp`.

## Transport-neutral inbound admission (still no live routing)

`InboundCommandGate` combines the canonical encoder and replay guard behind
one `InboundCommandView` with source ID, source boot ID, transaction ID and
application intent. MQTT can supply the boot ID from its `source` envelope;
ESP-NOW can supply it from the authenticated frame envelope. The adapters
must verify sender identity *before* calling the gate, and call it in the
cooperative loop rather than a network callback. Only `NEW_COMMAND` allows
the device adapter to execute; a duplicate never invokes the executor again.
The gate includes neither transport nor device headers and has no hard-coded
device/resource IDs. **This is not wired into either transport yet.** Before
automatic fallback, fence stale sessions and define behavior after a receiver
reboot. Host validation:
`tests/inbound_command_gate_test.cpp`.

The gate stores a bounded, transport-neutral terminal outcome (status and up
to 256 application bytes). An adapter can serialize it again for either
transport without re-executing the action. Only a verified *application*
outcome may call `complete`; an ACK or publish success cannot. Duplicate
pending commands remain pending, and a conflicting command identity cannot
retrieve another command's result. This is volatile and does not make
fallback safe across receiver reboot or stale sender session.
