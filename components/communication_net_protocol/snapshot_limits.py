"""Worst-case budgets for the existing state-fields/v2 encodings."""
import json

MAX_FIELDS = 8
MAX_BINARY_BYTES = 256
MAX_JSON_BYTES = 767  # Must match MAX_SNAPSHOT_JSON_BYTES in snapshot_limits.h.


def snapshot_sizes(fields):
    payload = {"_meta": {"version": 2, "generation": 4294967295, "revision": 4294967295}}
    binary_bytes = 10
    for field in fields:
        name = field["field"]
        binary_bytes += 6 + len(name.encode("ascii"))
        value = {"known": False, "on": False}
        if field.get("rgb", False):
            value.update(red=255, green=255, blue=255, brightness=255, effect_active=False)
        payload[name] = value
    return len(json.dumps(payload, separators=(",", ":")).encode("ascii")), binary_bytes


def validate_snapshot_budget(fields):
    if not 1 <= len(fields) <= MAX_FIELDS:
        raise ValueError("snapshot requires 1..8 fields")
    json_bytes, binary_bytes = snapshot_sizes(fields)
    if json_bytes > MAX_JSON_BYTES:
        raise ValueError(f"snapshot worst-case MQTT payload {json_bytes} exceeds {MAX_JSON_BYTES} bytes")
    if binary_bytes > MAX_BINARY_BYTES:
        raise ValueError(f"snapshot ESP-NOW data {binary_bytes} exceeds {MAX_BINARY_BYTES} bytes")
    return fields
