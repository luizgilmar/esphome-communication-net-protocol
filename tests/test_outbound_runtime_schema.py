import copy
import importlib.util
from pathlib import Path
import pytest
import esphome.config_validation as cv

path = Path(__file__).parents[1] / 'components/communication_outbound/__init__.py'
spec = importlib.util.spec_from_file_location('outbound_runtime_schema', path)
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


def configuration():
    return dict(source_id='panel', mqtt=dict(mqtt_id='mqtt_client', command_prefix='tx/commands', reply_topic='tx/results/panel'),
                esp_now=dict(espnow_net_protocol_id='radio'),
                policies=[dict(id='auto', transports=['mqtt', 'esp_now'])],
                destinations=[dict(id='hub', device_id='hub_wire', policy='auto', espnow_peer='hub_peer')])


def installation():
    return dict(mqtt=dict(id='mqtt_client'), espnow_net_protocol=dict(id='radio', peers=[dict(id='hub_peer', application_source_id='hub_wire')]))


def test_valid_transport_and_authenticated_peer():
    config = configuration()
    assert runtime.validate_runtime(config) is config
    assert runtime.validate_installation(config, installation()) is config


@pytest.mark.parametrize('mutation,match', [
    (lambda c: c.pop('mqtt') or None, 'configured transports'),
    (lambda c: c['policies'].append(copy.deepcopy(c['policies'][0])), 'Duplicate outbound policy'),
    (lambda c: c['policies'][0].update(transports=['mqtt', 'mqtt']), 'distinct'),
    (lambda c: c['destinations'][0].update(policy='missing'), 'Unknown outbound policy'),
    (lambda c: c['destinations'][0].pop('espnow_peer'), 'requires espnow_peer'),
    (lambda c: c['destinations'].append(dict(id='another', device_id='hub_wire', policy='auto', espnow_peer='hub_peer')), 'one outbound destination'),
    (lambda c: c['mqtt'].update(command_prefix='tx/commands/'), 'must not end'),
])
def test_invalid_routing_rejected_before_codegen(mutation, match):
    config = configuration()
    mutation(config)
    with pytest.raises(cv.Invalid, match=match):
        runtime.validate_runtime(config)


@pytest.mark.parametrize('mutation,match', [
    (lambda i: i['espnow_net_protocol']['peers'][0].update(application_source_id='other'), 'application_source_id'),
    (lambda i: i.update(tx_ultimate=dict(espnow_net_protocol_id='radio')), 'sole outgoing command owner'),
    (lambda i: i.update(state_query_client=dict(radio_id='radio')), 'sole outgoing command owner'),
    (lambda i: i['mqtt'].update(id='other'), 'native client'),
])
def test_incompatible_installation_rejected(mutation, match):
    current = installation()
    mutation(current)
    with pytest.raises(cv.Invalid, match=match):
        runtime.validate_installation(configuration(), current)


@pytest.mark.parametrize('value', ['', 'bad/#', 'bad/+', 'a' * 161, 'bad\nvalue'])
def test_invalid_topic_rejected(value):
    with pytest.raises(cv.Invalid):
        runtime.topic(value)
