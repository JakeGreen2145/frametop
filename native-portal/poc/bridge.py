#!/usr/bin/env python3
"""Follow the live Frametop desk top face and portal intent for the POC layer."""
import argparse
import json
import math
import os
from pathlib import Path
import signal
import socket
import struct
import sys
import time

import control

STOP = False


def stopping(_signal, _frame):
    global STOP
    STOP = True


def sample():
    with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as wire:
        wire.bind('')
        wire.settimeout(0.2)
        wire.connect('\0ft_screens')
        wire.send(b'passthrough poc')
        reply = wire.recv(4096).decode()
    if not reply.startswith('ok poc '):
        raise RuntimeError('ft-screens needs the POC snapshot command: ' + reply[:120])
    state = json.loads(reply[7:])
    if type(state.get('wanted')) is not bool or type(state.get('valid')) is not bool:
        raise ValueError('invalid desktop intent/validity')
    vectors = [state[key] for key in ('center', 'u', 'v')]
    if any(not isinstance(v, list) or len(v) != 3 or
           any(type(n) not in (int, float) or not math.isfinite(n) for n in v) for v in vectors):
        raise ValueError('invalid desktop aperture')
    return vectors, state['wanted'] and state['valid']


def publish(path, vectors=None, enabled=False):
    values = [0.0] * 16
    if vectors:
        for i, vector in enumerate(vectors):
            values[4*i:4*i+3] = vector
    values[12] = float(enabled)
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_bytes(struct.pack('<16f', *values))
    temporary.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--geometry', type=Path, required=True)
    args = parser.parse_args()
    os.umask(0o077)
    args.geometry.parent.mkdir(parents=True, exist_ok=True)
    signal.signal(signal.SIGTERM, stopping)
    signal.signal(signal.SIGINT, stopping)
    last_enabled = None
    last_error = None
    try:
        while not STOP:
            started = time.monotonic()
            try:
                vectors, enabled = sample()
                publish(args.geometry, vectors, enabled)
                if enabled != last_enabled:
                    control.send(enabled)
                    last_enabled = enabled
                last_error = None
            except (OSError, RuntimeError, ValueError, struct.error) as error:
                publish(args.geometry)
                if last_enabled is not False:
                    try:
                        control.send(False)
                        last_enabled = False
                    except (OSError, RuntimeError):
                        pass
                message = str(error)
                if message != last_error:
                    print('POC geometry unavailable: ' + message, file=sys.stderr, flush=True)
                last_error = message
            time.sleep(max(0, 0.05 - (time.monotonic() - started)))
    finally:
        publish(args.geometry)
        try:
            control.send(False)
        except (OSError, RuntimeError):
            pass


if __name__ == '__main__':
    main()
