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
