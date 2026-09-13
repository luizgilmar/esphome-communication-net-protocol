# Confirmed interruption of an in-flight command

`TransactionStage::INTERRUPTED` is a terminal application outcome for the
*original* command. It does not mean that the original target position was
reached. The stop command has its own transaction and its own result.

The executor must verify that the stop applies to the same resource and that
stopping succeeded before calling `confirm_interruption(original_id,
source_boot_id, now_ms)`. Delivery acknowledgements alone never confirm a
stop. An unknown, stale-session, or already-terminal original must not be
reopened. A late completion for an interrupted original must be ignored.

`confirm_interruption` discards any buffered progress for the original and
publishes only its terminal interruption event. Consuming that event releases
the bounded transaction slot. This foundation does **not** yet wire a HUB
executor, MQTT response publisher or ESP-NOW sender to the tracker: the
receiver must explicitly send a terminal response for the original command,
and its sender must decode that response before the field issue is closed.
