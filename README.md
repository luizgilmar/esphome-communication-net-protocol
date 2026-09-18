# ESPHome Communication NetProtocol

Independent ESPHome external component for application-level transactions
across transport adapters. The current package is **a declarative foundation
with an optional receive-only MQTT observer**: it validates policies,
destinations and MQTT topic prefixes. A subscription occurs only with
`mqtt.listen_commands: true`; it does not send or execute commands or provide
fallback yet.

## Repository boundaries

- This repository owns `components/communication_net_protocol` and its tests.
- `esphome-espnow-net-protocol` remains an independent radio/peer protocol
  repository; do not copy its implementation into this one.
- `esphome-sonoff-txultimate` remains the device adapter and UX owner.
- Native ESPHome MQTT remains the broker/entity client.

See [the foundation contract](docs/communication-net-protocol-foundation.md)
for YAML syntax, compatibility topics and the staged migration of the HUB.
Do not enable the new block in the production TX or HUB firmware until its
runtime binding and first resource migration have passed hardware testing.

Run tests from the repository root with `python -m pytest -q` in an environment
with ESPHome and pytest installed. No git history or remote is included in
this ZIP; create the repository only after validation.

The opt-in receive-only MQTT observer can now validate the existing application
command envelope as well as the small diagnostic probe. It does not dispatch
commands, authenticate publishers, or modify the current HUB MQTT package.
The bounded MQTT result decoder and tracker correlation helper are available
for host validation; the isolated bench optionally subscribes to its result
topic and logs decoded responses without sending or executing commands.
