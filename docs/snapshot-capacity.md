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

Validation so far: Python budget/schema tests and existing snapshot/observation
contract tests. No native C++ execution, ESP32 compilation or hardware validation
of the expanded snapshot has been performed.
