# Snapshot capacity candidate

The existing state-fields/v2 wire format supports a configured count of 1..8
fields. Configuration validates worst-case JSON against 447 bytes (448 including
NUL) and binary data against 256 bytes. These checks include maximum uint32
generation/revision, false booleans and maximum color/brightness channel values.
Existing runtime buffer checks remain in place. The number of radio messages,
publication intervals and subscription renewal rules are unchanged.

The C++ field arrays use the generated COMMUNICATION_NET_SNAPSHOT_CAPACITY:
max(4, configured count). Existing four-field builds retain their array capacity.
This does not enlarge the MQTT or ESP-NOW payload buffers. Long field names or
too many RGB fields can be rejected even when the field count is below eight.

The TX retained-state receiver previously used its four indicator mappings as
the maximum incoming binary field count. Its corresponding candidate separates
MAX_SNAPSHOT_FIELDS=8 from MAX_MAPPINGS=4. Existing fields are still required;
extra fields do not allocate indicators or change ownership mappings.

Deploy the updated receiver before increasing the producer field count.
Old TX receivers reject an ESP-NOW snapshot with more than four fields.
Keep producer YAML on its validated four fields until both changes have been
compiled and checked on the bench, published and pinned to Git revisions.

Validation: Python budget/schema tests and existing snapshot/observation
contract tests passed (34 total). Both ESP32 compilation fixtures subsequently
passed with ESPHome 2026.9.1, as reported by the user below. Runtime reception of
extra fields and hardware validation of the expanded snapshot remain pending.

## Compilation step

Run tools/compilar-snapshot-capacity.ps1 with -Role publisher and then -Role tx.
The script requires ESPHome 2026.9.1, stores build/data outside OneDrive and
never uploads. Publisher uses virtual outputs and six fields; TX inherits its
canonical configuration and overrides only source substitutions to read local
candidate repositories. Canonical source defaults retain the installed Git
SHAs. Both fixtures passed ESPHome 2026.9.1 configuration validation.

These are compilation fixtures, not bench deployment configurations. In
particular the TX fixture still has the installed TX network identities.
Do not flash it onto a bench node. After compilation, prepare separate bench
identities and a runtime check for extra-field reception before deployment.

## Compilation results reported by the user

| Fixture | Build time (-0300) | Config hash | Static RAM | Image bytes |
| --- | --- | --- | --- | --- |
| Six-field publisher | 2026-10-09 23:47:01 | 0x519793da | 68,488 / 180,736 (37.9%) | 877,015 |
| Candidate TX receiver | 2026-10-09 23:49:04 | 0xa50da556 | 151,792 / 180,736 (84.0%) | 1,128,671 |

Both logs reported Successfully compiled program and no upload. The TX static
RAM and image size match the previously compiled ESPHome 2026.9.1 candidate.
Equal image size does not imply identical firmware contents or prove reception.
Build roots are AppData/Local/esphome-snapshot-capacity-2026.9.1 and
AppData/Local/esphome-tx-snapshot-capacity-2026.9.1 respectively.

## Single-board runtime test

examples/snapshot_receiver_runtime.yaml and tests/snapshot_receiver_runtime.h
exercise the actual TX RetainedStateObserver through its public query/result
interface on a bare ESP32dev. Four mapped ambient indicator states are checked
in memory; no LED strip is required. Cases: four/six/eight fields, missing owned
field, duplicate owned field, truncated frame, trailing bytes and nine fields.
Rejected frames must leave all four sentinel indicator values unchanged.

Wi-Fi starts disabled. Snapshot inputs are injected binary test vectors, not
received radio frames; this test cannot prove MQTT/ESP-NOW delivery, subscription
handover or physical LED output. The fixture loads the TX component for its real
receiver code and must be used only on a bare bench board without attached loads.
It does not use installed-device network credentials or originate actuator commands.

tools/testar-snapshot-receiver.ps1 supports compile, upload and logs; serial port
is configurable (default COM4). No upload is performed by compile. Expected
serial result is snapshot.test END passed=8 total=8, repeated every 30 seconds
so attaching serial logs after flashing does not miss the result. ESPHome
2026.9.1 configuration validation and C++ source generation passed; C++
compilation and execution were subsequently confirmed below.

### Observed result — 10/10/2026

Firmware compiled 09/10/2026 23:58:27 -0300, config_hash 0x94117de3,
RAM 133,792 / 180,736 bytes (74.0%), image 976,511 bytes. The supplied log
confirms serial flashing with hash verification and ESP32 revision 3.0 boot.

Two complete cycles at 00:02:10 and 00:02:40 reported END passed=8 total=8.
All eight named cases passed in each cycle. Valid snapshots updated the four
mapped indicator states in memory; malformed snapshots preserved all four
sentinel states, as asserted by the test. No additional boot or panic appeared
in the supplied excerpt. This is a short functional check, not a soak test.

The inherited radio component logged reconnect-suppression confirmation
warnings while Wi-Fi was disabled. The receiver tests passed independently;
these logs do not establish radio readiness or validate reconnect behavior.
Radio delivery, producer-to-receiver integration, MQTT transitions and physical
LED output remain outside this injected test.

Source evidence: Codex attachment
3fbaa505-1ce7-4c6d-81cf-c5db07eee4b0/Texto colado.txt,
SHA256 4665059892798b61708e52eab497da5183890f2d1c73d5e87ccbd9d2a07d3823.
