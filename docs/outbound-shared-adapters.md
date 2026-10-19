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

Next: extract concrete transport adapters/codecs with compatibility wrappers,
provide consistent application_identity_matches for shared service admission,
then provision the common service and connect nspanel_resources. Do not pass
unconfigured legacy adapters to CommandSubmissionService: they still use the
default false identity check. A separate TX executor in the NSPanel is not the
intended architecture.

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
