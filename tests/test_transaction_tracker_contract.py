from pathlib import Path


def test_tracker_separates_transport_attempt_from_application_deadline():
    source = Path("components/communication_net_protocol/transaction_tracker.h").read_text(
        encoding="utf-8"
    )
    assert "TransactionTracker<4>" in Path(
        "components/communication_net_protocol/communication_net_protocol.h"
    ).read_text(encoding="utf-8")
    assert "slot.terminal_pending" in source
    assert "slot.progress_pending" in source
    assert "source_boot_id" in source
    assert "now_ms - slot.started_ms < slot.timeout_ms" in source
    assert "TransactionStage::EXPIRED" in source
    assert "TransactionStage::INTERRUPTED" in source
    assert "confirm_interruption" in source
    assert "std::vector" not in source
