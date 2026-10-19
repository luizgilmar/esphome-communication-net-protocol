# Shared outbound adapter preparation

The existing destination registry and ESP-NOW model converter have been moved
from tx_ultimate into outbound_destination_registry.h and
outbound_espnow_model_adapter.h in communication_net_protocol::outbound.
TX headers preserve their public names as aliases, not copied class bodies.

The extraction changes namespace/includes and the converter's compile guard;
its method bodies, storage layout, validation, limits and wire mapping are
unchanged. The converter is available with USE_TX_ULTIMATE_ESPNOW for existing
TX builds or USE_COMMUNICATION_NET_OUTBOUND_ESPNOW for a generic runtime.
This step does not instantiate any transport or enable NSPanel submission.

Concrete transport extraction has now followed this first step. The shared
component owns outbound_http_json_codec, outbound_mqtt_json_codec,
outbound_mqtt_command_transport and outbound_espnow_command_transport (.h/.cpp).
The HTTP JSON codec is used internally by the MQTT wire codec and remains
available to existing HTTP callers. TX headers expose aliases and TX .cpp files
no longer define duplicate methods. All existing limits and wire encodings are
preserved; no per-device executor has been introduced.

Both concrete adapters now implement application_identity_matches. They require
a nonzero matching boot/session and the same source identity as configured on
the transport; unconfigured or mismatched adapters are rejected by the shared
service. This adds no instance storage. Existing TX callers do not use the new
admission method until connected to CommandSubmissionService.

Next: provision the common service and connect nspanel_resources. A separate
TX executor in the NSPanel is not the intended architecture. The ESP-NOW
endpoint currently stores two command-result observers: a three-lane service
must route results through a single owning observer, rather than silently
registering a third adapter. Physical endpoint admission/priority constraints
also require validation before enabling the interrupt lane for STOP.

These candidate changes are subsequent to the six-field snapshots pinned in
the NSPanel validation manifest. Consumers must update shared and TX component
refs together after compilation. Old TX Git pins are unaffected; new TX wrappers
require the new shared headers. No installed firmware is modified by the move.

Compilation preparation: tools/compilar-snapshot-capacity.ps1 -Role shared
compiles the generic headers without any TX feature define. -Role tx compiles
the TX candidate with static assertions confirming that legacy names are the
same types as the shared definitions. No upload is performed. Python contract
and schema checks passed (46 tests). Complete C++ compilation remains pending;
the agent's Windows cross-compiler failed to start due to DLL relocation.

After concrete extraction, 60 TX contract/alias checks and 24 shared
schema/snapshot checks passed. Existing native codec tests must link the new
shared .cpp implementations; the empty legacy .cpp wrappers no longer provide
symbols. ESPHome automatically builds the shared component sources. The shared
and TX compilation fixtures include the concrete headers and compatibility
type assertions. Complete C++ compilation of this subsequent extraction remains
pending; the earlier destination/model-only compilation does not cover it.
