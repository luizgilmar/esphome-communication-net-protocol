# Interruptible inbound commands

An inbound binding can keep one bounded operation active while a `stop`
binding for the same resource is accepted on MQTT or ESP-NOW. The sender,
source boot, target and resource must match the active operation.

The configuration is entirely declarative:

```yaml
communication_net_protocol:
  # Existing MQTT, ESP-NOW, policy and destination configuration omitted.
  inbound:
    bindings:
      - id: blind_open
        resource: cover/bedroom
        command: open
        interruptible: true
        then:
          - cover.open: bedroom_blind
        completion:
          delay: 19s
          timeout: 20s

      - id: blind_close
        resource: cover/bedroom
        command: close
        interruptible: true
        then:
          - cover.close: bedroom_blind
        completion:
          delay: 19s
          timeout: 20s

      - id: blind_stop
        resource: cover/bedroom
        command: stop
        interrupts_active: true
        then:
          - cover.stop: bedroom_blind
        completion:
          delay: 50ms
          timeout: 1s
```

`completion.delay` is a bounded timer completion intended for operations whose
executor does not expose a directly observable final entity state. Its timeout
must be greater than the delay. A successful stop completes its own transaction
and terminates the original operation with the `INTERRUPTED` error code.

Declaring an interruptible binding automatically enables the optional second
ESP-NOW inbound dispatcher. Configurations without these keys retain the
single-command runtime and do not allocate the extra dispatcher.
