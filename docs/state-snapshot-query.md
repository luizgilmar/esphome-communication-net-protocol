# Declarative state snapshot query

When `state_snapshot` and active inbound execution are configured, the generic
component exposes one standard read-only application command on both MQTT and
ESP-NOW:

```text
resource: state/snapshot
command: get
payload: {}
```

The command does not run an ESPHome automation and does not mutate any entity.
It reads only the fields declared under `state_snapshot.fields` and returns a
successful `NetResult` whose remote state uses schema `state-fields/v2`.

Version 2 carries a persistent producer generation and an in-generation
revision. The generation advances once per producer boot; the revision advances
only when the bounded observed values change. MQTT retained JSON publishes the
same pair under `_meta`, so MQTT and ESP-NOW results can be ordered by the
consumer without using arrival time. The producer keeps decoding support for
version 1 at the result boundary during migration, but only emits version 2.

The bounded binary data is:

```text
version:u8, field_count:u8,
repeated(field_name_size:u8, field_name:bytes, flags:u8,
         red:u8, green:u8, blue:u8, brightness:u8)
```

Flags are `known=0x01`, `on=0x02`, `rgb=0x04`, and
`effect_active=0x08`. At most four fields and 31 bytes per field name are
accepted. The worst-case data is 150 bytes, below the 256-byte remote-state
limit and below a single reassembled ESP-NOW result's 640-byte limit.

ESP-NOW carries the bytes directly in the existing result codec. MQTT exposes
the identical bytes in `remote_state.data_hex`, preserving one logical schema
without adding a radio frame version.
