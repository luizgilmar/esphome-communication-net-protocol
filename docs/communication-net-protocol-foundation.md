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
validates its format without executing an action.
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

After consuming one mailbox entry, the component accepts either the isolated
three-string diagnostic JSON or the current application MQTT envelope with
`transaction_id`, `reply_to`, `source` (`device_id`, `boot_id`), `target`
(`device_id`, `resource`) and `command` (`name`). Field order is arbitrary;
all fields must be present and bounded. Unknown/duplicate fields, escaped
strings, command arguments, malformed JSON, invalid numeric identifiers and
the wrong target device are rejected. Successful decoding produces the same
bounded canonical bytes for both forms and logs a counter, **without invoking
the replay gate or any executor**. The original `{"probe":true}` diagnostic
payload remains rejected. The live envelope can therefore be observed before
any application route changes; this is not yet an executable MQTT endpoint.
Do not infer sender authentication from `source` bytes supplied by MQTT.
Define broker publisher trust, argument normalization and transaction/session
handling before migrating a resource or enabling fallback.

`MqttWireTransport` uses the existing ESPHome MQTT client for a single
bounded subscription mailbox and byte publication. The mailbox neither parses
JSON nor interprets `IN_PROGRESS`/terminal status. Application correlation,
deduplication, retries and fallback remain unimplemented and must be added to
the shared protocol core. The foundation bench now explicitly opts into a
receive-only subscription; using `communication_net_protocol:` there cannot alter
the deployed Quartogian command route.

Host validation: compile `tests/mqtt_mailbox_test.cpp` and
`tests/mqtt_envelope_decoder_test.cpp` with a C++17 compiler
and run the resulting executable. Firmware validation remains a separate
`esphome config` and `esphome compile` of the foundation bench. No OTA upload.

## Correlation tracker (not yet bound to either transport)

The independent `TransactionTracker<4>` keeps an application transaction ID
and source boot ID in a fixed-capacity table. It can retain one coalesced
`IN_PROGRESS` event and the terminal outcome at the same time, rejecting
duplicate terminals and replies from a stale session. Its deadline applies to
the complete application operation: a failed MQTT or ESP-NOW *attempt* is not
itself a terminal application failure if the policy has another viable route.
This header has no MQTT/ESP-NOW includes and no dynamic allocation. Integrating
the ESP-NOW observer and activating cross-transport receiver deduplication are
**future gates**: do not connect a
HUB action yet. The tracker is testable on a host with
`tests/transaction_tracker_test.cpp`.

### Optional MQTT result observation

`mqtt.listen_results: true` subscribes to
`<result_prefix>/<device_id>` with the same bounded, non-retained MQTT mailbox
used by command observation. Both options default to `false`. The composed
topic is limited to 192 UTF-8 bytes and the receive payload to 1280 bytes;
oversized messages are dropped before parsing. The loop verifies the exact
topic, decodes the result and logs its ID, stage and tracker correlation.
Unknown IDs remain `UNKNOWN_TRANSACTION`: the isolated bench does not start
outbound transactions. Only a separately registered outstanding transaction
can accept a response, using its locally retained boot ID. This observation
does not authenticate a broker publisher, execute commands, publish replies or
alter the TX/HUB route. Do not use it as proof of effect confirmation.

With `listen_results: true`, the optional `observe_outgoing_target` subscribes
to `<command_prefix>/<target>/command`. It registers only envelopes whose
`source.device_id` equals the configured local device ID, whose target equals
the configured target, and whose `reply_to` equals the observed result topic.
It tracks the envelope transaction and boot IDs for at most 30 seconds and
four concurrent requests. This is passive MQTT observation, not publisher
authentication: a broker client can spoof these envelope fields. A result
can now log `ACCEPTED` for a tracked command, while the legacy TX executor
still owns the action and UX. The decoder accepts `command.payload: {}` and
a positive `timeout_ms` as emitted by the TX. Nonempty command arguments
remain rejected until they have a canonical encoding.

### Result correlation helper

`mqtt_result_decoder.h` recognizes the existing HUB result envelope, including
`in_progress`, terminal success/rejection/failure and `failed` with error code
`interrupted`. It extracts the decimal `transaction_id` and maps the result to
an application stage. The helper accepts the source boot ID from the local
outstanding request, never from result JSON; `TransactionTracker` rejects an
unknown transaction or a mismatch. It cannot prove publisher identity or the
effect of an action. The caller must verify the MQTT reply topic and broker
trust policy before invoking it. The optional isolated result observer uses
the configured topic; no existing TX/HUB runtime is changed. Host test:
`tests/mqtt_result_decoder_test.cpp`. Application result subscription,
publisher/session validation and dispatch remain separate migration gates.

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

### Declarative route names (compile-only)

The optional `inbound.bindings` list declares up to 16 unique `id`, `resource`
and `command` triples. The registry resolves a resource/command pair to its
configured binding ID without hard-coded device resources. Duplicate IDs or
pairs fail YAML validation; the C++ registry also rejects them and enforces
its bounded capacity. The current foundation bench declares `light/example`
only to compile this contract. Declaring a binding does **not** subscribe to
commands, invoke an ESPHome action, admit a transaction or take an existing
HUB/ESP-NOW binding. The same device-wide inbound gate must mediate both
transport adapters before any configured action is enabled. On the sender,
the intended production route order is MQTT then ESP-NOW; non-idempotent
actions require shared receiver deduplication before that fallback is live.

`RouteAdmission` now composes the declaration registry and the existing
device-wide replay gate. For a verified inbound command, it checks the local
device and declared resource/action before admitting the transaction; only
`NEW_COMMAND` returns a binding ID to an eventual executor. A duplicate
pending command stays pending; a duplicate terminal command may retrieve the
stored application outcome without re-execution. The two adapters must supply
the same authenticated source ID, application boot ID, transaction ID and
normalized command bytes. The separate ESP-NOW radio boot ID is not an
application boot ID. This code is compiled when `inbound` is declared, but no
transport calls it at runtime yet.

The declared binding now accepts a standard ESPHome `then` action list and an
optional `completion` block (`light_id`, `expected: on|off|toggled`, `timeout`).
ESPHome compiles the automation and resolves the light reference, with no
lambda needed in YAML. These are retained as configuration on a dormant
trigger. No inbound transport invokes the trigger yet, so neither the action
nor the completion runs in the current foundation bench. The completion
declaration currently supports lights; additional resource types need their
own typed completion adapters without changing the route/admission core.


`InboundCommandGate` combines the canonical encoder and replay guard behind
one `InboundCommandView` with source ID, source boot ID, transaction ID and
application intent. MQTT can supply the boot ID from its `source` envelope;
ESP-NOW can supply it from the authenticated frame envelope. The adapters
must verify sender identity *before* calling the gate, and call it in the
cooperative loop rather than a network callback. Only `NEW_COMMAND` allows
the device adapter to execute; a duplicate never invokes the executor again.
The gate includes neither transport nor device headers and has no hard-coded
device/resource IDs. With `execute_inbound: true`, MQTT and verified ESP-NOW
share the same route admission and terminal replay on the receiver. The replay
table is volatile across receiver reboot; session transitions and durable
deduplication remain explicit protocol limits. Host validation:
`tests/inbound_command_gate_test.cpp`.

The gate stores a bounded, transport-neutral terminal outcome (status and up
to 256 application bytes). An adapter can serialize it again for either
transport without re-executing the action. Only a verified *application*
outcome may call `complete`; an ACK or publish success cannot. Duplicate
pending commands remain pending, and a conflicting command identity cannot
retrieve another command's result. This is volatile and does not make
fallback safe across receiver reboot or stale sender session.
# Passive ESP-NOW route inspection

When `esp_now.observe_inbound: true` and `inbound.bindings` are declared, the
component accepts only commands whose application source matches the
configured encrypted radio peer. It checks the declared local device,
resource, action and empty JSON payload, then logs the route and length of
its canonical command. Existing ESP-NOW bindings remain responsible for
execution. No replay entries are reserved and no result is published in this
mode. Nonempty arguments require a normalized payload codec in a later step.

The passive build includes its bounded route table but omits the replay
table. The replay table must be enabled together with an executor that marks
the terminal outcome; admitting observational traffic would otherwise fill
the table and deny subsequent commands.

`InboundExecutionLifecycle` now tests reservation, executor start, terminal
completion and terminal replay against the same table without additional
permanent storage. This is a host-tested contract, not a live receiver yet.
The bounded table can now retire its oldest completed entry while preserving
the greatest retired transaction ID per source and boot session. Later
commands can continue after eight completions, while an old ID at or below
that watermark is rejected without re-executing a non-idempotent toggle.
Detailed result replay is available only while an entry remains cached;
transport adapters must return a terminal rejection for an older retired ID.
This ordering rule requires verified source sessions with monotonically
increasing transaction IDs, and session records remain bounded. Transport
result publication and source-session validation must be integrated before
enabling live admission.

## Grouped light completion

Active inbound bindings may declare `completion.additional_light_ids` with up
to three local light IDs alongside `completion.light_id`. The executor reports
success only when the primary light and every additional light have reached the
declared `expected` state. For `toggled`, the expected value is derived from
the primary light before the automation starts; grouped commands using
`toggled` should ensure all lights start in a consistent state. `on` and `off`
apply the same expected value to every light. The route and action remain YAML
declarations; the state check runs in C++ and is shared by MQTT and ESP-NOW.

For a grouped `toggled` binding, `completion.toggle_reference_light_ids`
optionally names additional lights whose initial ON state contributes to the
pre-action state. The expected final state is OFF if the primary light or any
reference was ON, and ON otherwise. This is useful when an existing toggle
means "turn the whole group off if any primary member is on." Auxiliary
lights such as a power supply can be placed in `additional_light_ids` without
changing the toggle decision. Both lists are bounded to three extra lights.

For RGB commands, set `expected: on`, `completion.rgb` with integer `red`,
`green` and `blue` values from 0 to 255, and `completion.rgb_light_ids` with
the local RGB lights to verify. Both RGB fields must be present together. The
executor also checks the primary and additional lights are ON; supply lights
can therefore be checked without requiring their color to match. Each RGB
channel allows one byte of rounding difference. The route succeeds only after
the declared RGB values appear on every RGB light, or fails on timeout.

For brightness commands, set `expected: on`, `completion.brightness` as an
integer percentage from 1 to 100, and `completion.brightness_light_ids` with
the lights whose brightness must match. Both brightness fields must be
specified together. The executor compares the current brightness of every
listed light to the configured percentage, allowing one 8-bit step of
rounding, and also checks the ON state of the primary and additional lights.
The bounded inbound route table supports 20 routes; only declared bindings
occupy entries, and increasing the table from 16 to 20 adds 640 bytes of
fixed route storage plus four binding pointers on 32-bit targets.

For light effects, set `expected: on`, `completion.effect` to the exact
effect name and `completion.effect_light_ids` to the local lights that must
report that effect. The two effect fields must be present together. The
executor confirms the ON state of the primary and additional lights and
checks the active effect on every listed light before returning success.
