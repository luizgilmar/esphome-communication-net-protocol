from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_result_observer_is_opt_in_bounded_and_receive_only():
    schema = (ROOT / "components/communication_net_protocol/__init__.py").read_text()
    source = (ROOT / "components/communication_net_protocol/communication_net_protocol.cpp").read_text()
    mailbox = (ROOT / "components/communication_net_protocol/mqtt_mailbox.h").read_text()
    bench = (ROOT / "examples/communication_net_protocol_foundation_bench.yaml").read_text()
    assert 'cv.Optional(CONF_LISTEN_RESULTS, default=False)' in schema
    assert 'len(result_topic.encode("utf-8")) > 192' in schema
    assert 'MAX_TOPIC = 192' in mailbox
    assert 'MAX_PAYLOAD = 1280' in mailbox
    assert 'listen_results: true' in bench
    assert 'std::strcmp(this->received_topic_, this->mqtt_result_topic_) == 0' in source
    assert 'decode_mqtt_result' in source
    assert 'observe_result' in source
    assert 'observe_outgoing_target' in schema
    assert 'this->transactions_.begin(' in source
    assert 'command_gate_.admit' not in source
