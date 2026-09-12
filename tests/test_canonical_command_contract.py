from pathlib import Path


def test_identity_encoding_is_shared_and_bounded():
    source = Path("components/communication_net_protocol/canonical_command.h").read_text(
        encoding="utf-8"
    )
    assert "encode_canonical_command" in source
    assert "view.arguments_length > 128" in source
    assert "required > 192" in source
    assert "identity format version" in source
    assert "std::vector" not in source
