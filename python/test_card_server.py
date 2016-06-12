#!/usr/bin/env python3

from collections import namedtuple
import itertools
import subprocess
import json

import zmq

IDENTITY_KEY = "identity"
ENDPOINT_KEY = "endpoint"
CONTROL_KEY = "control"
PeerEntry = namedtuple("PeerEntry", (IDENTITY_KEY, CONTROL_KEY))

INIT_COMMAND = b'init'
TERMINATE_COMMAND = b'terminate'

REPLY_SUCCESS = [b'success']
PEERS = [
    PeerEntry("peer1", "tcp://127.0.0.1:5501"),
    PeerEntry("peer2", "tcp://127.0.0.1:5502"),
    PeerEntry("peer3", "tcp://127.0.0.1:5503"),
    PeerEntry("peer4", "tcp://127.0.0.1:5504")]

port = 5601
ENDPOINTS = {}
for p in itertools.combinations(PEERS, 2):
    ENDPOINTS[frozenset(p)] = "tcp://127.0.0.1:%d" % port
    port = port + 1

zmqctx = zmq.Context.instance()
servers = [
    subprocess.Popen(["./cardserver", peer.control]) for peer in PEERS]
sockets = [zmqctx.socket(zmq.PAIR) for peer in PEERS]

print("Init...")

for (socket, peer) in zip(sockets, PEERS):
    def get_entry(p1, p2):
        if p1 != p2:
            return {
                IDENTITY_KEY: p1.identity,
                ENDPOINT_KEY: ENDPOINTS[frozenset((p1, p2))]}
        return None
    entries = [get_entry(peer, peer2) for peer2 in PEERS]
    socket.connect(peer.control)
    socket.send(INIT_COMMAND, flags=zmq.SNDMORE)
    socket.send_json(entries)

print("Receiving reply...")
    
for socket in sockets:
    assert(socket.recv_multipart() == REPLY_SUCCESS + [INIT_COMMAND])

print("Terminate...")

for socket in sockets:
    socket.send(TERMINATE_COMMAND)

print("Cleanup...")
    
for (server, socket) in zip(servers, sockets):
    server.wait()
    socket.close()

zmqctx.destroy()
